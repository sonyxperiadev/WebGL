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

#ifndef CachedFramePlatformDatatAndroid_h
#define CachedFramePlatformDatatAndroid_h

#include "CachedFramePlatformData.h"

namespace WebCore {
    class Settings;
}

namespace android {

class CachedFramePlatformDataAndroid : public WebCore::CachedFramePlatformData {
public:
    CachedFramePlatformDataAndroid(WebCore::Settings* settings);

#ifdef ANDROID_META_SUPPORT
    void restoreMetadata(WebCore::Settings* settings);
#endif

private:
#ifdef ANDROID_META_SUPPORT
    // meta data of the frame
    int m_viewport_width;
    int m_viewport_height;
    int m_viewport_initial_scale;
    int m_viewport_minimum_scale;
    int m_viewport_maximum_scale;
    int m_viewport_target_densitydpi;
    bool m_viewport_user_scalable : 1;
    bool m_format_detection_address : 1;
    bool m_format_detection_email : 1;
    bool m_format_detection_telephone : 1;
#endif
};

}

#endif