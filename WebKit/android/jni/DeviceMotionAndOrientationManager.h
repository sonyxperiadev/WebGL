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

#ifndef DeviceMotionAndOrientationManager_h
#define DeviceMotionAndOrientationManager_h

#include <DeviceMotionData.h>
#include <DeviceMotionClient.h>
#include <DeviceOrientation.h>
#include <DeviceOrientationClient.h>
#include <OwnPtr.h>
#include <PassRefPtr.h>
#include <RefPtr.h>

namespace android {

class WebViewCore;

// This class takes care of the fact that the clients used for DeviceMotion and
// DeviceOrientation may be either the real implementations or mocks. It also
// handles setting the data on both the real and mock clients. This class is
// owned by WebViewCore and exists to keep cruft out of that class.
class DeviceMotionAndOrientationManager {
public:
    DeviceMotionAndOrientationManager(WebViewCore*);

    void useMock();
    void setMockMotion(PassRefPtr<WebCore::DeviceMotionData>);
    void onMotionChange(PassRefPtr<WebCore::DeviceMotionData>);
    void setMockOrientation(PassRefPtr<WebCore::DeviceOrientation>);
    void onOrientationChange(PassRefPtr<WebCore::DeviceOrientation>);
    void maybeSuspendClients();
    void maybeResumeClients();
    WebCore::DeviceMotionClient* motionClient();
    WebCore::DeviceOrientationClient* orientationClient();

private:
    bool m_useMock;
    WebViewCore* m_webViewCore;
    OwnPtr<WebCore::DeviceMotionClient> m_motionClient;
    OwnPtr<WebCore::DeviceOrientationClient> m_orientationClient;
};

} // namespace android

#endif // DeviceMotionAndOrientationManager_h
