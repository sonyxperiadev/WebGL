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

#ifndef WebResourceRequest_h
#define WebResourceRequest_h

#include "ChromiumIncludes.h"

#include <string>

namespace WebCore {
class ResourceRequest;
}

namespace android {

class WebResourceRequest {

public:
    WebResourceRequest(const WebCore::ResourceRequest&);

    const std::string& method()
    {
        return m_method;
    }

    const std::string& referrer()
    {
        return m_referrer;
    }

    const net::HttpRequestHeaders& requestHeaders()
    {
        return m_requestHeaders;
    }

    const std::string& url()
    {
        return m_url;
    }

    const bool isAndroidUrl()
    {
        return m_specialAndroidFileType != 0;
    }

    const int androidFileType()
    {
        return m_specialAndroidFileType;
    }

private:
    std::string m_method;
    std::string m_referrer;
    net::HttpRequestHeaders m_requestHeaders;
    int m_specialAndroidFileType;
    std::string m_url;
};

} // namespace android


#endif
