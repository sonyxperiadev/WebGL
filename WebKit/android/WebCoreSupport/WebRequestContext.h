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

#ifndef WebRequestContext_h
#define WebRequestContext_h

// Cannot forward declare the chrome classes since this is
// a subclass of a chrome class.
#include "ChromiumIncludes.h"
#include "PlatformString.h"

namespace android {

class WebRequestContext : public URLRequestContext {
public:
    virtual const std::string& GetUserAgent(const GURL& url) const;
    virtual const std::string& GetAcceptLanguage() const;
    static scoped_refptr<WebRequestContext> GetAndroidContext();
    static scoped_refptr<WebRequestContext> GetAndroidPrivateBrowsingContext();
    static bool CleanupAndroidPrivateBrowsingFiles(std::string dataDirectory);

    static void SetUserAgent(WTF::String);
    static void SetAcceptLanguage(WTF::String);

private:
    static const std::string* GetDataDirectory();
    static scoped_refptr<WebRequestContext> GetAndroidContextForPath(const char* cookiePath, const char* cachePath);
    WebRequestContext();
    ~WebRequestContext();

    // Caching this query from java
    static std::string* s_dataDirectory;
    static std::string* s_userAgent;
};

} // namespace android

#endif
