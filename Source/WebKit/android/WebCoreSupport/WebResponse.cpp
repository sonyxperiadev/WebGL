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

#include "MIMETypeRegistry.h"
#include "PlatformString.h"
#include "ResourceResponse.h"
#include "ResourceError.h"

#include <wtf/text/CString.h>

using namespace std;

namespace android {

WebResponse::WebResponse(net::URLRequest* request)
    : m_httpStatusCode(0)
{
    // The misleadingly-named os_error() is actually a net::Error enum constant.
    m_error = net::Error(request->status().os_error());

    m_url = request->url().spec();
    m_host = request->url().HostNoBrackets();
    request->GetMimeType(&m_mime);

    request->GetCharset(&m_encoding);
    m_expectedSize = request->GetExpectedContentSize();

    m_sslInfo = request->ssl_info();

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
    WebCore::ResourceResponse resourceResponse(createKurl(), getMimeType().c_str(), m_expectedSize, m_encoding.c_str(), "");
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

// Calls WebCore APIs so should only be called from the WebCore thread.
// TODO: can we return a WTF::String directly? Need to check all callsites.
const string& WebResponse::getMimeType()
{
    if (!m_url.length())
        return m_mime;

    if (!m_mime.length() || !m_mime.compare("text/plain") || !m_mime.compare("application/octet-stream"))
        m_mime = resolveMimeType(m_url, m_mime);

    return m_mime;
}

const string WebResponse::resolveMimeType(const string& url, const string& old_mime)
{
    // Use "text/html" as a default (matching the behaviour of the Apache
    // HTTP stack -- see guessMimeType() in LoadListener.java).
    string mimeType = old_mime.length() ? old_mime : "text/html";
    // Try to guess a better MIME type from the URL. We call
    // getMIMETypeForExtension rather than getMIMETypeForPath because the
    // latter defaults to "application/octet-stream" on failure.
    WebCore::KURL kurl(WebCore::ParsedURLString, url.c_str());
    WTF::String path = kurl.path();
    size_t extensionPos = path.reverseFind('.');
    if (extensionPos != WTF::notFound) {
        // We found a file extension.
        path.remove(0, extensionPos + 1);
        // TODO: Should use content-disposition instead of url if it is there
        WTF::String mime = WebCore::MIMETypeRegistry::getMIMETypeForExtension(path);
        if (!mime.isEmpty()) {
            // Great, we found a MIME type.
            mimeType = std::string(mime.utf8().data(), mime.length());
        }
    }
    return mimeType;
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
