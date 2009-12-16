/*
 * Copyright 2009, The Android Open Source Project
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "webcore_test"
#include "config.h"

#include "Base64.h"
#include "CString.h"
#include "HTTPParsers.h"
#include "Intercept.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "StringHash.h"
#include "TextEncoding.h"
#include <utils/Log.h>
#include <wtf/HashMap.h>

PassRefPtr<WebCore::ResourceLoaderAndroid> MyResourceLoader::create(
        ResourceHandle* handle, String url)
{
    return adoptRef<WebCore::ResourceLoaderAndroid>(
            new MyResourceLoader(handle, url));
}

void MyResourceLoader::handleRequest()
{
    if (protocolIs(m_url, "data"))
        loadData(m_url.substring(5)); // 5 for data:
    else if (protocolIs(m_url, "file"))
        loadFile(m_url.substring(7)); // 7 for file://
}

void MyResourceLoader::loadData(const String& data)
{
    LOGD("Loading data (%s) ...", data.latin1().data());
    ResourceHandleClient* client = m_handle->client();
    int index = data.find(',');
    if (index == -1) {
        client->cannotShowURL(m_handle);
        return;
    }

    String mediaType = data.substring(0, index);
    String base64 = data.substring(index + 1);

    bool decode = mediaType.endsWith(";base64", false);
    if (decode)
        mediaType = mediaType.left(mediaType.length() - 7); // 7 for base64;

    if (mediaType.isEmpty())
        mediaType = "text/plain;charset=US-ASCII";

    String mimeType = extractMIMETypeFromMediaType(mediaType);
    String charset = extractCharsetFromMediaType(mediaType);

    ResourceResponse response;
    response.setMimeType(mimeType);

    if (decode) {
        base64 = decodeURLEscapeSequences(base64);
        response.setTextEncodingName(charset);
        client->didReceiveResponse(m_handle, response);

        // FIXME: This is annoying. WebCore's Base64 decoder chokes on spaces.
        // That is correct with strict decoding but html authors (particularly
        // the acid3 authors) put spaces in the data which should be ignored.
        // Remove them here before sending to the decoder.
        Vector<char> in;
        CString str = base64.latin1();
        const char* chars = str.data();
        unsigned i = 0;
        while (i < str.length()) {
            char c = chars[i];
            // Don't send spaces or control characters.
            if (c != ' ' && c != '\n' && c != '\t' && c != '\b'
                    && c != '\f' && c != '\r')
                in.append(chars[i]);
            i++;
        }
        Vector<char> out;
        if (base64Decode(in, out) && out.size() > 0)
            client->didReceiveData(m_handle, out.data(), out.size(), 0);
    } else {
        base64 = decodeURLEscapeSequences(base64, TextEncoding(charset));
        response.setTextEncodingName("UTF-16");
        client->didReceiveResponse(m_handle, response);
        if (base64.length() > 0)
            client->didReceiveData(m_handle, (const char*)base64.characters(),
                    base64.length() * sizeof(UChar), 0);
    }
    client->didFinishLoading(m_handle);
}
static String mimeTypeForExtension(const String& file)
{
    static HashMap<String, String, CaseFoldingHash> extensionToMime;
    if (extensionToMime.isEmpty()) {
        extensionToMime.set("txt", "text/plain");
        extensionToMime.set("html", "text/html");
        extensionToMime.set("htm", "text/html");
        extensionToMime.set("png", "image/png");
        extensionToMime.set("jpeg", "image/jpeg");
        extensionToMime.set("jpg", "image/jpeg");
        extensionToMime.set("gif", "image/gif");
        extensionToMime.set("ico", "image/x-icon");
        extensionToMime.set("js", "text/javascript");
    }
    int dot = file.reverseFind('.');
    String mime("text/plain");
    if (dot != -1) {
        String ext = file.substring(dot + 1);
        if (extensionToMime.contains(ext))
            mime = extensionToMime.get(ext);
    }
    return mime;
}

void MyResourceLoader::loadFile(const String& file)
{
    LOGD("Loading file (%s) ...", file.latin1().data());
    FILE* f = fopen(file.latin1().data(), "r");
    ResourceHandleClient* client = m_handle->client();
    if (!f) {
        client->didFail(m_handle,
                ResourceError("", -14, file, "Could not open file"));
    } else {
        ResourceResponse response;
        response.setTextEncodingName("utf-8");
        response.setMimeType(mimeTypeForExtension(file));
        client->didReceiveResponse(m_handle, response);
        char buf[512];
        while (true) {
            int res = fread(buf, 1, sizeof(buf), f);
            if (res <= 0)
                break;
            client->didReceiveData(m_handle, buf, res, 0);
        }
        fclose(f);
        client->didFinishLoading(m_handle);
    }
}

PassRefPtr<WebCore::ResourceLoaderAndroid> MyWebFrame::startLoadingResource(
        ResourceHandle* handle, const ResourceRequest& req, bool ignore,
        bool ignore2)
{
    RefPtr<WebCore::ResourceLoaderAndroid> loader =
            MyResourceLoader::create(handle, req.url().string());
    m_requests.append(loader);
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
    return loader.release();
}

void MyWebFrame::timerFired(Timer<MyWebFrame>*)
{
    LOGD("Handling requests...");
    Vector<RefPtr<WebCore::ResourceLoaderAndroid> > reqs;
    reqs.swap(m_requests);
    Vector<RefPtr<WebCore::ResourceLoaderAndroid> >::iterator i = reqs.begin();
    Vector<RefPtr<WebCore::ResourceLoaderAndroid> >::iterator end = reqs.end();
    for (; i != end; i++)
        static_cast<MyResourceLoader*>((*i).get())->handleRequest();

    LOGD("...done");
}
