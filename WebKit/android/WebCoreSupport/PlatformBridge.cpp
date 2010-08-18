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
#include <PlatformBridge.h>

#include "CookieClient.h"
#include "Document.h"
#include "FileSystemClient.h"
#include "FrameView.h"
#include "JavaSharedClient.h"
#include "KeyGeneratorClient.h"
#include "PluginView.h"
#include "Settings.h"
#include "WebCoreFrameBridge.h"
#include "WebRequestContext.h"
#include "WebViewCore.h"
#include "npruntime.h"
#include <wtf/android/AndroidThreading.h>
#include <wtf/MainThread.h>

using namespace android;

namespace WebCore {

WTF::Vector<String> PlatformBridge::getSupportedKeyStrengthList()
{
    KeyGeneratorClient* client = JavaSharedClient::GetKeyGeneratorClient();
    if (!client)
        return Vector<String>();

    return client->getSupportedKeyStrengthList();
}

String PlatformBridge::getSignedPublicKeyAndChallengeString(unsigned index, const String& challenge, const KURL& url)
{
    KeyGeneratorClient* client = JavaSharedClient::GetKeyGeneratorClient();
    if (!client)
        return String();

    return client->getSignedPublicKeyAndChallengeString(index, challenge, url);
}

void PlatformBridge::setCookies(const Document* document, const KURL& url, const String& value)
{
#if USE(CHROME_NETWORK_STACK)
    std::string cookieValue(value.utf8().data());
    GURL cookieGurl(url.string().utf8().data());
    WebRequestContext* androidContext;
    if (document->settings() && document->settings()->privateBrowsingEnabled())
        androidContext = WebRequestContext::GetAndroidPrivateBrowsingContext();
    else
        androidContext = WebRequestContext::GetAndroidContext();
    androidContext->cookie_store()->SetCookie(cookieGurl, cookieValue);
#else
    CookieClient* client = JavaSharedClient::GetCookieClient();
    if (!client)
        return;

    client->setCookies(url, value);
#endif
}

String PlatformBridge::cookies(const Document* document, const KURL& url)
{
#if USE(CHROME_NETWORK_STACK)
    GURL cookieGurl(url.string().utf8().data());
    WebRequestContext* androidContext;
    if (document->settings() && document->settings()->privateBrowsingEnabled())
        androidContext = WebRequestContext::GetAndroidPrivateBrowsingContext();
    else
        androidContext = WebRequestContext::GetAndroidContext();
    std::string cookies = androidContext->cookie_store()->GetCookies(cookieGurl);
    String cookieString(cookies.c_str());
    return cookieString;
#else
    CookieClient* client = JavaSharedClient::GetCookieClient();
    if (!client)
        return String();

    return client->cookies(url);
#endif
}

bool PlatformBridge::cookiesEnabled(const Document* document)
{
    CookieClient* client = JavaSharedClient::GetCookieClient();
    if (!client)
        return false;

    return client->cookiesEnabled();
}

NPObject* PlatformBridge::pluginScriptableObject(Widget* widget)
{
#if USE(V8)
    if (!widget->isPluginView())
        return 0;

    PluginView* pluginView = static_cast<PluginView*>(widget);
    return pluginView->getNPObject();
#else
    return 0;
#endif
}

bool PlatformBridge::isWebViewPaused(const WebCore::FrameView* frameView)
{
    android::WebViewCore* webViewCore = android::WebViewCore::getWebViewCore(frameView);
    return webViewCore->isPaused();
}

bool PlatformBridge::canScroll(const WebCore::FrameView* frameView)
{
    // We want to ignore requests to scroll that were not initiated by the
    // user.  An example of this is when text is inserted into a
    // textfield/area, which results in a scroll.  We ignore this because
    // we know how to do this ourselves in the UI thread.
    // An example of it being initiated by the user is if the user clicks
    // an anchor element which simply scrolls the page.
    return android::WebFrame::getWebFrame(frameView->frame())->userInitiatedClick();
}

bool PlatformBridge::popupsAllowed(NPP)
{
    return false;
}

String PlatformBridge::resolveFilePathForContentUri(const String& contentUri)
{
    FileSystemClient* client = JavaSharedClient::GetFileSystemClient();
    return client->resolveFilePathForContentUri(contentUri);
}

}  // namespace WebCore


// This is the implementation of AndroidThreading, which is declared in
// JavaScriptCore/wtf/android/AndroidThreading.h. It is provided here, rather
// than in its own source file, to avoid linker problems.
//
// By default, when building a shared library, the linker strips from static
// libraries any compilation units which do not contain any code referenced from
// that static library. Since
// AndroidThreading::scheduleDispatchFunctionsOnMainThread is not referenced
// from libwebcore.a, implementing it in its own compilation unit results in it
// being stripped. This stripping can be avoided by using the linker option
// --whole-archive for libwebcore.a, but this adds considerably to the size of
// libwebcore.so.

namespace WTF {

// Callback in the main thread.
static void timeoutFired(void*)
{
    dispatchFunctionsFromMainThread();
}

void AndroidThreading::scheduleDispatchFunctionsOnMainThread()
{
    JavaSharedClient::EnqueueFunctionPtr(timeoutFired, 0);
}

}  // namespace WTF
