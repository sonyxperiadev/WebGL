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

#ifndef DeviceMotionClientAndroid_h
#define DeviceMotionClientAndroid_h

#include <DeviceMotionClient.h>

namespace WebCore {
class DeviceMotionController;
}

namespace android {

class WebViewCore;

// The Android implementation of DeviceMotionClient. Acts as a proxy to
// the real or mock impl, which is owned by the WebViewCore.
class DeviceMotionClientAndroid : public WebCore::DeviceMotionClient {
public:
    DeviceMotionClientAndroid();

    void setWebViewCore(WebViewCore*);

    // DeviceMotionClient methods
    virtual void setController(WebCore::DeviceMotionController*);
    virtual void startUpdating();
    virtual void stopUpdating();
    virtual WebCore::DeviceMotionData* currentDeviceMotion() const;
    virtual void deviceMotionControllerDestroyed();

private:
    WebCore::DeviceMotionClient* client() const;

    WebViewCore* m_webViewCore;
    WebCore::DeviceMotionController* m_controller;
    // Lazily obtained cache of the client owned by the WebViewCore.
    mutable WebCore::DeviceMotionClient* m_client;
};

} // namespace android

#endif // DeviceMotionClientAndroid_h
