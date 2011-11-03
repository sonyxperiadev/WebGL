/*
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebAutofill.h"

#if ENABLE(WEB_AUTOFILL)

#include "AutoFillHostAndroid.h"
#include "Frame.h"
#include "FormData.h"
#include "FormManagerAndroid.h"
#include "FrameLoader.h"
#include "HTMLFormControlElement.h"
#include "MainThreadProxy.h"
#include "Node.h"
#include "Page.h"
#include "Settings.h"
#include "WebFrame.h"
#include "WebRequestContext.h"
#include "WebUrlLoaderClient.h"
#include "WebViewCore.h"

#define NO_PROFILE_SET 0
#define FORM_NOT_AUTOFILLABLE -1

namespace android
{
WebAutofill::WebAutofill()
    : mQueryId(1)
    , mWebViewCore(0)
    , mLastSearchDomVersion(0)
    , mParsingForms(false)
{
    mTabContents = new TabContents();
    setEmptyProfile();
}

void WebAutofill::init()
{
    if (mAutofillManager)
        return;

    mFormManager = new FormManager();
    // We use the WebView's WebRequestContext, which may be a private browsing context.
    ASSERT(mWebViewCore);
    mAutofillManager = new AutofillManager(mTabContents.get());
    mAutofillHost = new AutoFillHostAndroid(this);
    mTabContents->SetProfileRequestContext(new AndroidURLRequestContextGetter(mWebViewCore->webRequestContext(), WebUrlLoaderClient::ioThread()));
    mTabContents->SetAutoFillHost(mAutofillHost.get());
}

WebAutofill::~WebAutofill()
{
    cleanUpQueryMap();
    mUniqueIdMap.clear();
}

void WebAutofill::cleanUpQueryMap()
{
    for (AutofillQueryFormDataMap::iterator it = mQueryMap.begin(); it != mQueryMap.end(); it++)
        delete it->second;
    mQueryMap.clear();
}

void WebAutofill::searchDocument(WebCore::Frame* frame)
{
    if (!enabled())
        return;

    MutexLocker lock(mFormsSeenMutex);

    init();

    cleanUpQueryMap();
    mUniqueIdMap.clear();
    mForms.clear();
    mQueryId = 1;

    ASSERT(mFormManager);
    ASSERT(mAutofillManager);

    mAutofillManager->Reset();
    mFormManager->Reset();

    mFormManager->ExtractForms(frame);
    mFormManager->GetFormsInFrame(frame, FormManager::REQUIRE_AUTOCOMPLETE, &mForms);

    // Needs to be done on a Chrome thread as it will make a URL request to the Autofill server.
    // TODO: Use our own Autofill thread instead of the IO thread.
    // TODO: For now, block here. Would like to make this properly async.
    base::Thread* thread = WebUrlLoaderClient::ioThread();
    mParsingForms = true;
    thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this, &WebAutofill::formsSeenImpl));
    while (mParsingForms)
        mFormsSeenCondition.wait(mFormsSeenMutex);
}

// Called on the Chromium IO thread.
void WebAutofill::formsSeenImpl()
{
    MutexLocker lock(mFormsSeenMutex);
    mAutofillManager->OnFormsSeenWrapper(mForms);
    mParsingForms = false;
    mFormsSeenCondition.signal();
}

void WebAutofill::formFieldFocused(WebCore::HTMLFormControlElement* formFieldElement)
{
    if (!enabled()) {
        // In case that we've just been disabled and the last time we got autofill
        // suggestions we told Java about them, clear that bit Java side now
        // we're disabled.
        mWebViewCore->setWebTextViewAutoFillable(FORM_NOT_AUTOFILLABLE, string16());
        return;
    }

    ASSERT(formFieldElement);

    Document* doc = formFieldElement->document();
    Frame* frame = doc->frame();

    // FIXME: Autofill only works in main frame for now. Should consider
    // child frames.
    if (frame != frame->page()->mainFrame())
        return;

    unsigned domVersion = doc->domTreeVersion();
    ASSERT(domVersion > 0);

    if (mLastSearchDomVersion != domVersion) {
        // Need to extract forms as DOM version has changed since the last time
        // we searched.
        searchDocument(formFieldElement->document()->frame());
        mLastSearchDomVersion = domVersion;
    }

    ASSERT(mFormManager);

    // Get the FormField from the Node.
    webkit_glue::FormField* formField = new webkit_glue::FormField;
    FormManager::HTMLFormControlElementToFormField(formFieldElement, FormManager::EXTRACT_NONE, formField);
    formField->label = FormManager::LabelForElement(*formFieldElement);

    webkit_glue::FormData* form = new webkit_glue::FormData;
    mFormManager->FindFormWithFormControlElement(formFieldElement, FormManager::REQUIRE_AUTOCOMPLETE, form);
    mQueryMap[mQueryId] = new FormDataAndField(form, formField);

    bool suggestions = mAutofillManager->OnQueryFormFieldAutoFillWrapper(*form, *formField);

    mQueryId++;
    if (!suggestions) {
        ASSERT(mWebViewCore);
        // Tell Java no autofill suggestions for this form.
        mWebViewCore->setWebTextViewAutoFillable(FORM_NOT_AUTOFILLABLE, string16());
        return;
    }
}

void WebAutofill::querySuccessful(const string16& value, const string16& label, int uniqueId)
{
    if (!enabled())
        return;

    // Store the unique ID for the query and inform java that autofill suggestions for this form are available.
    // Pass java the queryId so that it can pass it back if the user decides to use autofill.
    mUniqueIdMap[mQueryId] = uniqueId;

    ASSERT(mWebViewCore);
    mWebViewCore->setWebTextViewAutoFillable(mQueryId, mAutofillProfile->Label());
}

void WebAutofill::fillFormFields(int queryId)
{
    if (!enabled())
        return;

    webkit_glue::FormData* form = mQueryMap[queryId]->form();
    webkit_glue::FormField* field = mQueryMap[queryId]->field();
    ASSERT(form);
    ASSERT(field);

    AutofillQueryToUniqueIdMap::iterator iter = mUniqueIdMap.find(queryId);
    if (iter == mUniqueIdMap.end()) {
        // The user has most likely tried to Autofill the form again without
        // refocussing the form field. The UI should protect against this
        // but stop here to be certain.
        return;
    }
    mAutofillManager->OnFillAutoFillFormDataWrapper(queryId, *form, *field, iter->second);
    mUniqueIdMap.erase(iter);
}

void WebAutofill::fillFormInPage(int queryId, const webkit_glue::FormData& form)
{
    if (!enabled())
        return;

    // FIXME: Pass a pointer to the Node that triggered the Autofill flow here instead of 0.
    // The consquence of passing 0 is that we should always fail the test in FormManader::ForEachMathcingFormField():169
    // that says "only overwrite an elements current value if the user triggered autofill through that element"
    // for elements that have a value already. But by a quirk of Android text views we are OK. We should still
    // fix this though.
    mFormManager->FillForm(form, 0);
}

bool WebAutofill::enabled() const
{
    Page* page = mWebViewCore->mainFrame()->page();
    return page ? page->settings()->autoFillEnabled() : false;
}

void WebAutofill::setProfile(const string16& fullName, const string16& emailAddress, const string16& companyName,
                             const string16& addressLine1, const string16& addressLine2, const string16& city,
                             const string16& state, const string16& zipCode, const string16& country, const string16& phoneNumber)
{
    if (!mAutofillProfile)
        mAutofillProfile.set(new AutofillProfile());

    // Update the profile.
    // Constants for Autofill field types are found in external/chromium/chrome/browser/autofill/field_types.h.
    mAutofillProfile->SetInfo(AutofillFieldType(NAME_FULL), fullName);
    mAutofillProfile->SetInfo(AutofillFieldType(EMAIL_ADDRESS), emailAddress);
    mAutofillProfile->SetInfo(AutofillFieldType(COMPANY_NAME), companyName);
    mAutofillProfile->SetInfo(AutofillFieldType(ADDRESS_HOME_LINE1), addressLine1);
    mAutofillProfile->SetInfo(AutofillFieldType(ADDRESS_HOME_LINE2), addressLine2);
    mAutofillProfile->SetInfo(AutofillFieldType(ADDRESS_HOME_CITY), city);
    mAutofillProfile->SetInfo(AutofillFieldType(ADDRESS_HOME_STATE), state);
    mAutofillProfile->SetInfo(AutofillFieldType(ADDRESS_HOME_ZIP), zipCode);
    mAutofillProfile->SetInfo(AutofillFieldType(ADDRESS_HOME_COUNTRY), country);
    mAutofillProfile->SetInfo(AutofillFieldType(PHONE_HOME_WHOLE_NUMBER), phoneNumber);

    std::vector<AutofillProfile> profiles;
    profiles.push_back(*mAutofillProfile);
    updateProfileLabel();
    mTabContents->profile()->GetPersonalDataManager()->SetProfiles(&profiles);
}

bool WebAutofill::updateProfileLabel()
{
    std::vector<AutofillProfile*> profiles;
    profiles.push_back(mAutofillProfile.get());
    return AutofillProfile::AdjustInferredLabels(&profiles);
}

void WebAutofill::clearProfiles()
{
    if (!mAutofillProfile)
        return;
    // For now Chromium only ever knows about one profile, so we can just
    // remove it. If we support multiple profiles in the future
    // we need to remove them all here.
    std::string profileGuid = mAutofillProfile->guid();
    mTabContents->profile()->GetPersonalDataManager()->RemoveProfile(profileGuid);
    setEmptyProfile();
}

void WebAutofill::setEmptyProfile()
{
    // Set an empty profile. This will ensure that when autofill is enabled,
    // we will still search the document for autofillable forms and inform
    // java of their presence so we can invite the user to set up
    // their own profile.

    // Chromium code will strip the values sent into the profile so we need them to be
    // at least one non-whitespace character long. We need to set all fields of the
    // profile to a non-empty string so that any field type can trigger the autofill
    // suggestion. Autofill will not detect form fields if the profile value for that
    // field is an empty string.
    static const string16 empty = string16(ASCIIToUTF16("a"));
    setProfile(empty, empty, empty, empty, empty, empty, empty, empty, empty, empty);
}

}

#endif
