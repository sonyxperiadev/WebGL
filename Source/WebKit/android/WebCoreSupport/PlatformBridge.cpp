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
#include "JNIUtility.h"
#include "JavaSharedClient.h"
#include "KeyGeneratorClient.h"
#include "MemoryUsage.h"
#include "PluginView.h"
#include "Settings.h"
#include "WebCookieJar.h"
#include "WebRequestContext.h"
#include "WebViewCore.h"
#include "npruntime.h"

#include <surfaceflinger/SurfaceComposerClient.h>
#include <ui/DisplayInfo.h>
#include <ui/PixelFormat.h>
#include <wtf/android/AndroidThreading.h>
#include <wtf/MainThread.h>

using namespace android;

namespace WebCore {

WTF::Vector<String> PlatformBridge::getSupportedKeyStrengthList()
{
    KeyGeneratorClient* client = JavaSharedClient::GetKeyGeneratorClient();
    if (!client)
        return WTF::Vector<String>();

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
    bool isPrivateBrowsing = document->settings() && document->settings()->privateBrowsingEnabled();
    WebCookieJar::get(isPrivateBrowsing)->cookieStore()->SetCookie(cookieGurl, cookieValue);
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
    bool isPrivateBrowsing = document->settings() && document->settings()->privateBrowsingEnabled();
    std::string cookies = WebCookieJar::get(isPrivateBrowsing)->cookieStore()->GetCookies(cookieGurl);
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
#if USE(CHROME_NETWORK_STACK)
    bool isPrivateBrowsing = document->settings() && document->settings()->privateBrowsingEnabled();
    return WebCookieJar::get(isPrivateBrowsing)->allowCookies();
#else
    CookieClient* client = JavaSharedClient::GetCookieClient();
    if (!client)
        return false;

    return client->cookiesEnabled();
#endif
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

bool PlatformBridge::popupsAllowed(NPP)
{
    return false;
}

String PlatformBridge::resolveFilePathForContentUri(const String& contentUri)
{
    FileSystemClient* client = JavaSharedClient::GetFileSystemClient();
    return client->resolveFilePathForContentUri(contentUri);
}

int PlatformBridge::PlatformBridge::screenDepth()
{
    android::DisplayInfo info;
    android::SurfaceComposerClient::getDisplayInfo(android::DisplayID(0), &info);
    return info.pixelFormatInfo.bitsPerPixel;
}

FloatRect PlatformBridge::screenRect()
{
    android::DisplayInfo info;
    android::SurfaceComposerClient::getDisplayInfo(android::DisplayID(0), &info);
    return FloatRect(0.0, 0.0, info.w, info.h);
}

// The visible size on screen in document coordinate
int PlatformBridge::screenWidthInDocCoord(const WebCore::FrameView* frameView)
{
    android::WebViewCore* webViewCore = android::WebViewCore::getWebViewCore(frameView);
    return webViewCore->screenWidth();
}

int PlatformBridge::screenHeightInDocCoord(const WebCore::FrameView* frameView)
{
    android::WebViewCore* webViewCore = android::WebViewCore::getWebViewCore(frameView);
    return webViewCore->screenHeight();
}

String PlatformBridge::computeDefaultLanguage()
{
#if USE(CHROME_NETWORK_STACK)
    String acceptLanguages = WebRequestContext::acceptLanguage();
    size_t length = acceptLanguages.find(',');
    if (length == std::string::npos)
        length = acceptLanguages.length();
    return acceptLanguages.substring(0, length);
#else
    return "en";
#endif
}

void PlatformBridge::updateViewport(FrameView* frameView)
{
    android::WebViewCore* webViewCore = android::WebViewCore::getWebViewCore(frameView);
    webViewCore->updateViewport();
}

void PlatformBridge::updateTextfield(FrameView* frameView, Node* nodePtr, bool changeToPassword, const WTF::String& text)
{
    android::WebViewCore* webViewCore = android::WebViewCore::getWebViewCore(frameView);
    webViewCore->updateTextfield(nodePtr, changeToPassword, text);
}

void PlatformBridge::setScrollPosition(ScrollView* scrollView, int x, int y) {
    // Check to make sure the view is the main FrameView.
    android::WebViewCore *webViewCore = android::WebViewCore::getWebViewCore(scrollView);
    if (webViewCore->mainFrame()->view() == scrollView)
        webViewCore->scrollTo(x, y);
}

int PlatformBridge::lowMemoryUsageMB()
{
    return MemoryUsage::lowMemoryUsageMb();
}

int PlatformBridge::highMemoryUsageMB()
{
    return MemoryUsage::highMemoryUsageMb();
}

int PlatformBridge::highUsageDeltaMB()
{
    return MemoryUsage::highUsageDeltaMb();
}

int PlatformBridge::memoryUsageMB()
{
    return MemoryUsage::memoryUsageMb(false);
}

int PlatformBridge::actualMemoryUsageMB()
{
    return MemoryUsage::memoryUsageMb(true);
}

bool PlatformBridge::canSatisfyMemoryAllocation(long bytes)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jclass bridgeClass = env->FindClass("android/webkit/JniUtil");
    jmethodID method = env->GetStaticMethodID(bridgeClass, "canSatisfyMemoryAllocation", "(J)Z");
    jboolean canAllocate = env->CallStaticBooleanMethod(bridgeClass, method, static_cast<jlong>(bytes));
    env->DeleteLocalRef(bridgeClass);

    return canAllocate == JNI_TRUE;
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
