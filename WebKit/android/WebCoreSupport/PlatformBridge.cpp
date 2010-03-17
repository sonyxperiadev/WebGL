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
#include "JavaSharedClient.h"
#include "KeyGeneratorClient.h"
#include "PluginView.h"
#include "WebViewCore.h"
#include "npruntime.h"
#include <wtf/android/AndroidThreading.h>
#include <wtf/MainThread.h>

#if USE(ACCELERATED_COMPOSITING)
#include "LayerAndroid.h"
#endif

using namespace android;

namespace WebCore {

#if USE(ACCELERATED_COMPOSITING)

void PlatformBridge::setUIRootLayer(const WebCore::FrameView* view, const LayerAndroid* layer)
{
    android::WebViewCore* core = android::WebViewCore::getWebViewCore(view);
    core->setUIRootLayer(layer);
}

void PlatformBridge::immediateRepaint(const WebCore::FrameView* view)
{
    android::WebViewCore* core = android::WebViewCore::getWebViewCore(view);
    core->immediateRepaint();
}

#endif // USE(ACCELERATED_COMPOSITING)

int PlatformBridge::screenWidth(const WebCore::FrameView* view)
{
    android::WebViewCore* core = android::WebViewCore::getWebViewCore(view);
    return static_cast<int>((core->screenWidthScale() * core->screenWidth()) / core->scale());
}

int PlatformBridge::screenHeight(const WebCore::FrameView* view)
{
    android::WebViewCore* core = android::WebViewCore::getWebViewCore(view);
    return core->screenHeight();
}

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

void PlatformBridge::setCookies(const KURL& url, const String& value)
{
    CookieClient* client = JavaSharedClient::GetCookieClient();
    if (!client)
        return;

    client->setCookies(url, value);
}

String PlatformBridge::cookies(const KURL& url)
{
    CookieClient* client = JavaSharedClient::GetCookieClient();
    if (!client)
        return String();

    return client->cookies(url);
}

bool PlatformBridge::cookiesEnabled()
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

bool PlatformBridge::popupsAllowed(NPP)
{
    return false;
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
