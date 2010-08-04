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

#ifndef DeviceOrientationManager_h
#define DeviceOrientationManager_h

#include <DeviceOrientationClient.h>
#include <PassRefPtr.h>
#include <OwnPtr.h>

namespace WebCore {
class DeviceOrientation;
}

namespace android {

// This class handles providing a client for DeviceOrientation. This is
// non-trivial because of the need to be able to use and configure a mock
// client. This class is owned by WebViewCore and exists to keep cruft out of
// that class.
class DeviceOrientationManager {
public:
    DeviceOrientationManager();

    void useMock();
    void setMockOrientation(PassRefPtr<WebCore::DeviceOrientation>);
    WebCore::DeviceOrientationClient* client();

private:
    bool m_useMock;
    OwnPtr<WebCore::DeviceOrientationClient> m_client;
};

} // namespace android

#endif // DeviceOrientationManager_h
