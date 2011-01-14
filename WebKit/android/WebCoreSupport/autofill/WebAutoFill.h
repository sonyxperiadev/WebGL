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

#ifndef WebAutoFill_h
#define WebAutoFill_h

#if ENABLE(WEB_AUTOFILL)

#include "ChromiumIncludes.h"

#include <map>
#include <vector>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/ThreadingPrimitives.h>

class AutoFillManager;
class AutoFillProfile;
class AutoFillHost;

namespace WebCore {
class Frame;
class HTMLFormControlElement;
}

namespace android
{
class FormManager;
class WebViewCore;

class FormDataAndField {
public:
    FormDataAndField(webkit_glue::FormData* form, webkit_glue::FormField* field)
        : mForm(form)
        , mField(field)
    {
    }

    webkit_glue::FormData* form() { return mForm.get(); }
    webkit_glue::FormField* field() { return mField.get(); }

private:
    OwnPtr<webkit_glue::FormData> mForm;
    OwnPtr<webkit_glue::FormField> mField;
};

class WebAutoFill : public Noncopyable
{
public:
    WebAutoFill();
    virtual ~WebAutoFill();
    void formFieldFocused(WebCore::HTMLFormControlElement*);
    void fillFormFields(int queryId);
    void querySuccessful(const string16& value, const string16& label, int uniqueId);
    void fillFormInPage(int queryId, const webkit_glue::FormData& form);
    void setWebViewCore(WebViewCore* webViewCore) { mWebViewCore = webViewCore; }
    bool enabled() const;

    void setProfile(const string16& fullName, const string16& emailAddress, const string16& companyName,
                    const string16& addressLine1, const string16& addressLine2, const string16& city,
                    const string16& state, const string16& zipCode, const string16& country, const string16& phoneNumber);
    void clearProfiles();

    bool updateProfileLabel();

    void reset() { mLastSearchDomVersion = 0; }

private:
    void init();
    void searchDocument(WebCore::Frame*);
    void setEmptyProfile();
    void formsSeenImpl();
    void cleanUpQueryMap();

    OwnPtr<FormManager> mFormManager;
    OwnPtr<AutoFillManager> mAutoFillManager;
    OwnPtr<AutoFillHost> mAutoFillHost;
    OwnPtr<TabContents> mTabContents;
    OwnPtr<AutoFillProfile> mAutoFillProfile;

    typedef std::vector<webkit_glue::FormData, std::allocator<webkit_glue::FormData> > FormList;
    FormList mForms;

    typedef std::map<int, FormDataAndField*> AutoFillQueryFormDataMap;
    AutoFillQueryFormDataMap mQueryMap;

    typedef std::map<int, int> AutoFillQueryToUniqueIdMap;
    AutoFillQueryToUniqueIdMap mUniqueIdMap;
    int mQueryId;

    WebViewCore* mWebViewCore;

    unsigned mLastSearchDomVersion;

    WTF::Mutex mFormsSeenMutex; // Guards mFormsSeenCondition and mParsingForms.
    WTF::ThreadCondition mFormsSeenCondition;
    bool volatile mParsingForms;
};

}

DISABLE_RUNNABLE_METHOD_REFCOUNT(android::WebAutoFill);

#endif // ENABLE(WEB_AUTOFILL)
#endif // WebAutoFill_h
