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
#include "WebAutoFill.h"

#if ENABLE(WEB_AUTOFILL)

#include "AutoFillHostAndroid.h"
#include "Document.h"
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

#define FORM_NOT_AUTOFILLABLE -1

namespace android
{

WebAutoFill::WebAutoFill()
    : mWebViewCore(0)
{
    mFormManager = new FormManager();
    mQueryId = 1;

    AndroidURLRequestContextGetter::Get()->SetURLRequestContextGetterFunction(&WebRequestContext::GetAndroidContext);
    AndroidURLRequestContextGetter::Get()->SetIOThread(WebUrlLoaderClient::ioThread());
    TabContents* tabContents = new TabContents();
    mAutoFillManager = new AutoFillManager(tabContents);

    // FIXME: For testing use a precanned profile. This should come from Java land!
    mAutoFillProfile = new AutoFillProfile();
    mAutoFillProfile->SetInfo(AutoFillType(NAME_FULL), string16(ASCIIToUTF16("John Smith")));
    mAutoFillProfile->SetInfo(AutoFillType(EMAIL_ADDRESS), string16(ASCIIToUTF16("jsmith@gmail.com")));
    mAutoFillProfile->SetInfo(AutoFillType(ADDRESS_HOME_LINE1), string16(ASCIIToUTF16("123 Fake Street")));
    mAutoFillProfile->SetInfo(AutoFillType(ADDRESS_HOME_LINE2), string16(ASCIIToUTF16("Somewhere")));
    mAutoFillProfile->SetInfo(AutoFillType(ADDRESS_HOME_CITY), string16(ASCIIToUTF16("Faketown")));
    mAutoFillProfile->SetInfo(AutoFillType(ADDRESS_HOME_STATE), string16(ASCIIToUTF16("CA")));
    mAutoFillProfile->SetInfo(AutoFillType(ADDRESS_HOME_COUNTRY), string16(ASCIIToUTF16("Germany")));
    mAutoFillProfile->SetInfo(AutoFillType(ADDRESS_HOME_ZIP), string16(ASCIIToUTF16("AB12 3DE")));
    mAutoFillProfile->SetInfo(AutoFillType(PHONE_HOME_WHOLE_NUMBER), string16(ASCIIToUTF16("0123456789")));

    std::vector<AutoFillProfile> profiles;
    profiles.push_back(*mAutoFillProfile);
    tabContents->profile()->GetPersonalDataManager()->SetProfiles(&profiles);
    mAutoFillHost = new AutoFillHostAndroid(this);
    tabContents->SetAutoFillHost(mAutoFillHost.get());
}

WebAutoFill::~WebAutoFill()
{
    mQueryMap.clear();
    mSuggestionMap.clear();
}

void WebAutoFill::searchDocument(WebCore::Document* document)
{
    // TODO: This method is called when the main frame finishes loading and
    // scans the document for forms and tries to work out the type of the
    // fields in those forms. It's currently synchronous and might be slow
    // if the page has many or complex forms. Might want to make this an
    // async method.

    if (!enabled())
        return;

    if (!mFormManager)
        return;

    mQueryMap.clear();
    mSuggestionMap.clear();
    mQueryId = 1;
    mAutoFillManager->Reset();
    mFormManager->Reset();
    mFormManager->ExtractForms(document);
    mForms.clear();
    mFormManager->GetForms(FormManager::REQUIRE_AUTOCOMPLETE, &mForms);
    mAutoFillManager->FormsSeen(mForms);
}

void WebAutoFill::formFieldFocused(WebCore::HTMLFormControlElement* formFieldElement)
{
    if (!enabled()) {
        // In case that we've just been disabled and the last time we got autofill
        // suggestions and told Java about them, clear that bit Java side now
        // we're disabled.
        mWebViewCore->setWebTextViewAutoFillable(FORM_NOT_AUTOFILLABLE);
        return;
    }

    // Get the FormField from the Node.
    webkit_glue::FormField formField;
    FormManager::HTMLFormControlElementToFormField(*formFieldElement, false, &formField);
    formField.set_label(FormManager::LabelForElement(*formFieldElement));

    webkit_glue::FormData* form = new webkit_glue::FormData;
    mFormManager->FindFormWithFormControlElement(*formFieldElement, FormManager::REQUIRE_AUTOCOMPLETE, form);
    mQueryMap[mQueryId] = form;

    bool suggestions = mAutoFillManager->GetAutoFillSuggestions(mQueryId, false, formField);
    mQueryId++;
    if (!suggestions) {
        ASSERT(mWebViewCore);
        // Tell Java no autofill suggestions for this form.
        mWebViewCore->setWebTextViewAutoFillable(FORM_NOT_AUTOFILLABLE);
        return;
    }
}

void WebAutoFill::querySuccessful(int queryId, const string16& value, const string16& label, int uniqueId)
{
    if (!enabled())
        return;

    // Store the results for the query and inform java that autofill suggestions for this form are available.
    // Pass java the queryId so that it can pass it back if the user decides to use autofill.
    AutoFillSuggestion suggestion;
    suggestion.value = value;
    suggestion.label = label;
    suggestion.uniqueId = uniqueId;
    mSuggestionMap[queryId] = AutoFillSuggestion();

    ASSERT(mWebViewCore);
    mWebViewCore->setWebTextViewAutoFillable(queryId);
}

void WebAutoFill::fillFormFields(int queryId)
{
    if (!enabled())
        return;

    webkit_glue::FormData* form = mQueryMap[queryId];
    AutoFillQuerySuggestionMap::iterator iter = mSuggestionMap.find(queryId);
    ASSERT(iter != mSuggestionMap.end());
    AutoFillSuggestion* suggestion = &iter->second;
    mAutoFillManager->FillAutoFillFormData(queryId, *form, suggestion->value, suggestion->label, suggestion->uniqueId);
    mSuggestionMap.erase(iter);
}

void WebAutoFill::fillFormInPage(int queryId, const webkit_glue::FormData& form)
{
    if (!enabled())
        return;

    mFormManager->FillForm(form);
}

bool WebAutoFill::enabled() const
{
    Page* page = mWebViewCore->mainFrame()->page();
    return page ? page->settings()->autoFillEnabled() : false;
}

}

#endif
