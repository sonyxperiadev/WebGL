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
#include "WebRequestContext.h"

#include "ChromiumIncludes.h"
#include "ChromiumLogging.h"
#include "WebCache.h"
#include "WebCookieJar.h"

#include <dirent.h>
#include <wtf/text/CString.h>

namespace {
// TODO: The userAgent should not be a static, as it can be set per WebView.
// http://b/3113804
std::string userAgent("");
Lock userAgentLock;

std::string acceptLanguageStdString("");
WTF::String acceptLanguageWtfString("");
WTF::Mutex acceptLanguageMutex;
}

using namespace WTF;

namespace android {

static scoped_refptr<WebRequestContext> privateBrowsingContext(0);
static WTF::Mutex privateBrowsingContextMutex;

WebRequestContext::WebRequestContext()
{
    // Also hardcoded in FrameLoader.java
    accept_charset_ = "utf-8, iso-8859-1, utf-16, *;q=0.7";
}

WebRequestContext::~WebRequestContext()
{
}

void WebRequestContext::setUserAgent(String string)
{
    // The useragent is set on the WebCore thread and read on the network
    // stack's IO thread.
    AutoLock aLock(userAgentLock);
    userAgent = string.utf8().data();
}

const std::string& WebRequestContext::GetUserAgent(const GURL& url) const
{
    // The useragent is set on the WebCore thread and read on the network
    // stack's IO thread.
    AutoLock aLock(userAgentLock);
    ASSERT(userAgent != "");
    return userAgent;
}

void WebRequestContext::setAcceptLanguage(const String& string)
{
    MutexLocker lock(acceptLanguageMutex);
    acceptLanguageStdString = string.utf8().data();
    acceptLanguageWtfString = string;
}

const std::string& WebRequestContext::GetAcceptLanguage() const
{
    MutexLocker lock(acceptLanguageMutex);
    return acceptLanguageStdString;
}

const String& WebRequestContext::acceptLanguage()
{
    MutexLocker lock(acceptLanguageMutex);
    return acceptLanguageWtfString;
}

WebRequestContext* WebRequestContext::getImpl(bool isPrivateBrowsing)
{
    WebRequestContext* context = new WebRequestContext();

    WebCache* cache = WebCache::get(isPrivateBrowsing);
    context->host_resolver_ = cache->hostResolver();
    context->http_transaction_factory_ = cache->cache();

    WebCookieJar* cookieJar = WebCookieJar::get(isPrivateBrowsing);
    context->cookie_store_ = cookieJar->cookieStore();
    context->cookie_policy_ = cookieJar;

    return context;
}

WebRequestContext* WebRequestContext::getRegularContext()
{
    static scoped_refptr<WebRequestContext> regularContext(0);
    if (!regularContext)
        regularContext = getImpl(false);
    return regularContext;
}

WebRequestContext* WebRequestContext::getPrivateBrowsingContext()
{
    MutexLocker lock(privateBrowsingContextMutex);

    // TODO: Where is the right place to put the temporary db? Should it be
    // kept in memory?
    if (!privateBrowsingContext)
        privateBrowsingContext = getImpl(true);
    return privateBrowsingContext;
}

WebRequestContext* WebRequestContext::get(bool isPrivateBrowsing)
{
    // Initialize chromium logging, needs to be done before any chromium code is called
    initChromiumLogging();

    return isPrivateBrowsing ? getPrivateBrowsingContext() : getRegularContext();
}

void WebRequestContext::removeFileOrDirectory(const char* filename)
{
    struct stat filetype;
    if (stat(filename, &filetype) != 0)
        return;
    if (S_ISDIR(filetype.st_mode)) {
        DIR* directory = opendir(filename);
        if (directory) {
            while (struct dirent* entry = readdir(directory)) {
                if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
                    continue;
                std::string entryName(filename);
                entryName.append("/");
                entryName.append(entry->d_name);
                removeFileOrDirectory(entryName.c_str());
            }
            closedir(directory);
            rmdir(filename);
        }
        return;
    }
    unlink(filename);
}

bool WebRequestContext::cleanupPrivateBrowsingFiles()
{
    // This is called on the UI thread.
    MutexLocker lock(privateBrowsingContextMutex);

    if (!privateBrowsingContext || privateBrowsingContext->HasOneRef()) {
        privateBrowsingContext = 0;

        WebCookieJar::get(true)->cleanupFiles();
        WebCache::get(true)->cleanupFiles();
        return true;
    }
    return false;
}

} // namespace android
