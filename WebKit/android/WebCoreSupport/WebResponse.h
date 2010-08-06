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

#ifndef WebResponse_h
#define WebResponse_h

#include "KURL.h"

#include <map>
#include <net/url_request/url_request.h>
#include <string>

namespace WebCore {
class ResourceResponse;
}

namespace android {

class WebResponse {

public:
    WebResponse() {}
    WebResponse(URLRequest*);
    WebResponse(const std::string &url, const std::string &mimeType, const long long length, const std::string &encoding, const int httpStatusCode);
    WebCore::KURL url();
    void setUrl(std::string);

    // Only use on the WebCore thread!
    WebCore::ResourceResponse createResourceResponse();

private:
    std::string m_encoding;
    int m_httpStatusCode;
    std::string m_httpStatusText;
    long long m_length;
    std::string m_mime;
    std::string m_url;

    std::map<std::string, std::string> m_headerFields;
};

} // namespace android

#endif
