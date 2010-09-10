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
#include "WebFrame.h"
#include "WebRequestContext.h"
#include "WebUrlLoaderClient.h"
#include "WebViewCore.h"

namespace android
{

WebAutoFill::WebAutoFill()
{
    mFormManager = new FormManager();
    mQueryId = 1;

    AndroidURLRequestContextGetter::Get()->SetURLRequestContext(WebRequestContext::GetAndroidContext());
    AndroidURLRequestContextGetter::Get()->SetIOThread(WebUrlLoaderClient::ioThread());
    TabContents* tabContents = new TabContents();
    mAutoFillManager = new AutoFillManager(tabContents);

    // FIXME: For testing use a precanned profile. This should come from Java land!
    mAutoFillProfile = new AutoFillProfile();
    mAutoFillProfile->SetInfo(AutoFillType(NAME_FULL), string16(ASCIIToUTF16("John Smith")));
    mAutoFillProfile->SetInfo(AutoFillType(EMAIL_ADDRESS), string16(ASCIIToUTF16("jsmith@gmail.com")));
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
}

void WebAutoFill::searchDocument(WebCore::Document* document)
{
    // TODO: This method is called when the main frame finishes loading and
    // scans the document for forms and tries to work out the type of the
    // fields in those forms. It's currently synchronous and might be slow
    // if the page has many or complex forms. Might want to make this an
    // async method.
    if (!mFormManager)
        return;

    mQueryMap.clear();
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
    // Get the FormField from the Node.
    webkit_glue::FormField formField;
    FormManager::HTMLFormControlElementToFormField(*formFieldElement, false, &formField);
    formField.set_label(FormManager::LabelForElement(*formFieldElement));

    webkit_glue::FormData* form = new webkit_glue::FormData;
    mFormManager->FindFormWithFormControlElement(*formFieldElement, FormManager::REQUIRE_AUTOCOMPLETE, form);
    mQueryMap[mQueryId] = form;

    mAutoFillManager->GetAutoFillSuggestions(mQueryId, false, formField);
    mQueryId++;
}

void WebAutoFill::fillFormFields(int queryId, const string16& value, const string16& label, int uniqueId)
{
    webkit_glue::FormData* form = mQueryMap[queryId];
    mAutoFillManager->FillAutoFillFormData(queryId, *form, value, label, uniqueId);
}

void WebAutoFill::fillFormInPage(int queryId, const webkit_glue::FormData& form)
{
    mFormManager->FillForm(form);
}

}

#endif
