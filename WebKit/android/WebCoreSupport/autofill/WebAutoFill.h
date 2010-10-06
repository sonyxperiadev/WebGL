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

class AutoFillManager;
class AutoFillProfile;
class AutoFillHost;

namespace WebCore {
class Document;
class HTMLFormControlElement;
}

namespace android
{
class FormManager;
class WebViewCore;

class WebAutoFill : public Noncopyable
{
public:
    WebAutoFill();
    virtual ~WebAutoFill();

    void searchDocument(WebCore::Document*);
    void formFieldFocused(WebCore::HTMLFormControlElement*);
    void fillFormFields(int queryId);
    void querySuccessful(int queryId, const string16& value, const string16& label, int uniqueId);
    void fillFormInPage(int queryId, const webkit_glue::FormData& form);
    void setWebViewCore(WebViewCore* webViewCore) { mWebViewCore = webViewCore; }
    bool enabled() const;

    void setProfile(const string16& fullName, const string16& emailAddress);

private:
    OwnPtr<FormManager> mFormManager;
    OwnPtr<AutoFillManager> mAutoFillManager;
    OwnPtr<AutoFillHost> mAutoFillHost;
    OwnPtr<TabContents> mTabContents;

    typedef std::vector<webkit_glue::FormData, std::allocator<webkit_glue::FormData> > FormList;
    FormList mForms;

    typedef std::map<int, webkit_glue::FormData*> AutoFillQueryFormDataMap;
    AutoFillQueryFormDataMap mQueryMap;

    typedef struct {
        string16 value;
        string16 label;
        int uniqueId;
    } AutoFillSuggestion;
    typedef std::map<int, AutoFillSuggestion> AutoFillQuerySuggestionMap;
    AutoFillQuerySuggestionMap mSuggestionMap;
    int mQueryId;

    WebViewCore* mWebViewCore;
};

}

#endif // ENABLE(WEB_AUTOFILL)
#endif // WebAutoFill_h
