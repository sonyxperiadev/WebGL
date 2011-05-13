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

#include "ChromiumIncludes.h"
#include "KURL.h"
#include "WebViewClientError.h"

#include <map>
#include <string>

namespace WebCore {
class ResourceResponse;
class ResourceError;
}

namespace android {

class WebResponse {

public:
    WebResponse() {}
    WebResponse(net::URLRequest*);
    WebResponse(const std::string &url, const std::string &mimeType, long long expectedSize, const std::string &encoding, int httpStatusCode);

    const std::string& getUrl() const;
    void setUrl(const std::string&);

    const std::string& getMimeType(); // Use only on WebCore thread.
    bool getHeader(const std::string& header, std::string* result) const;
    long long getExpectedSize() const;

    const net::SSLInfo& getSslInfo() const { return m_sslInfo; }

    // The create() methods create WebCore objects. They must only be called on the WebKit thread.
    WebCore::KURL createKurl();
    WebCore::ResourceResponse createResourceResponse();
    WebCore::ResourceError createResourceError();

    static const std::string resolveMimeType(const std::string& url, const std::string& old_mime);

private:
    net::Error m_error;
    std::string m_encoding;
    int m_httpStatusCode;
    std::string m_host;
    std::string m_httpStatusText;
    long long m_expectedSize;
    std::string m_mime;
    std::string m_url;
    net::SSLInfo m_sslInfo;

    struct CaseInsensitiveLessThan {
        bool operator()(const std::string& lhs, const std::string& rhs) const {
            return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
        }
    };

    // Header fields are case insensitive, so we use a case-insensitive map.
    // See RFC 822, 3.4.7, "CASE INDEPENDENCE".
    std::map<std::string, std::string, CaseInsensitiveLessThan> m_headerFields;

};

} // namespace android

#endif
