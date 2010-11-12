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
#include "WebResponse.h"

#include "ResourceResponse.h"
#include "ResourceError.h"

using namespace std;

namespace android {

WebResponse::WebResponse(URLRequest* request)
    : m_httpStatusCode(0)
{
    // The misleadingly-named os_error() is actually a net::Error enum constant.
    m_error = net::Error(request->status().os_error());

    m_url = request->url().spec();
    m_host = request->url().HostNoBrackets();
    request->GetMimeType(&m_mime);
    request->GetCharset(&m_encoding);
    m_expectedSize = request->GetExpectedContentSize();

    net::HttpResponseHeaders* responseHeaders = request->response_headers();
    if (!responseHeaders)
        return;

    m_httpStatusCode = responseHeaders->response_code();
    m_httpStatusText = responseHeaders->GetStatusText();

    string value;
    string name;
    void* iter = 0;
    while (responseHeaders->EnumerateHeaderLines(&iter, &name, &value))
        m_headerFields[name] = value;
}

WebResponse::WebResponse(const string &url, const string &mimeType, long long expectedSize, const string &encoding, int httpStatusCode)
    : m_error(net::OK)
    , m_encoding(encoding)
    , m_httpStatusCode(httpStatusCode)
    , m_expectedSize(expectedSize)
    , m_mime(mimeType)
    , m_url(url)
{
}

WebCore::ResourceResponse WebResponse::createResourceResponse()
{
    WebCore::ResourceResponse resourceResponse(createKurl(), m_mime.c_str(), m_expectedSize, m_encoding.c_str(), "");
    resourceResponse.setHTTPStatusCode(m_httpStatusCode);
    resourceResponse.setHTTPStatusText(m_httpStatusText.c_str());

    map<string, string>::const_iterator it;
    for (it = m_headerFields.begin(); it != m_headerFields.end(); ++it)
        resourceResponse.setHTTPHeaderField(it->first.c_str(), it->second.c_str());

    return resourceResponse;
}

WebCore::ResourceError WebResponse::createResourceError()
{
    WebCore::ResourceError error(m_host.c_str(), ToWebViewClientError(m_error), m_url.c_str(), WTF::String());
    return error;
}


WebCore::KURL WebResponse::createKurl()
{
    WebCore::KURL kurl(WebCore::ParsedURLString, m_url.c_str());
    return kurl;
}

const string& WebResponse::getUrl() const
{
    return m_url;
}

void WebResponse::setUrl(const string& url)
{
    m_url = url;
}

const string& WebResponse::getMimeType() const
{
    return m_mime;
}

bool WebResponse::getHeader(const string& header, string* result) const
{
    map<string, string>::const_iterator iter = m_headerFields.find(header);
    if (iter == m_headerFields.end())
        return false;
    if (result)
        *result = iter->second;
    return true;
}

long long WebResponse::getExpectedSize() const
{
    return m_expectedSize;
}

} // namespace android
