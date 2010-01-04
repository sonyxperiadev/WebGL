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
#include "PlatformBridge.h"

#include "JavaSharedClient.h"
#include "KeyGeneratorClient.h"
#include "WebViewCore.h"

using namespace android;

namespace WebCore {

#if USE(ACCELERATED_COMPOSITING)

void PlatformBridge::setRootLayer(const WebCore::FrameView* view, int layer)
{
    android::WebViewCore* core = android::WebViewCore::getWebViewCore(view);
    core->setRootLayer(layer);
}

void PlatformBridge::immediateRepaint(const WebCore::FrameView* view)
{
    android::WebViewCore* core = android::WebViewCore::getWebViewCore(view);
    core->immediateRepaint();
}

#endif // USE(ACCELERATED_COMPOSITING)

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

}
