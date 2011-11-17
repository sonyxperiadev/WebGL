/*
 * Copyright 2011 The Android Open Source Project
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

#ifndef VideoLayerAndroid_h
#define VideoLayerAndroid_h

#if USE(ACCELERATED_COMPOSITING)

#include "GLUtils.h"
#include "LayerAndroid.h"
#include <jni.h>

namespace android {
class SurfaceTexture;
}

namespace WebCore {

// state get from UI thread to decide which image to draw.
// PREPARING should be the progressing image
// PLAYING will be the Video (Surface Texture).
// Otherwise will draw a static image.
// NOTE: These values are matching the ones in HTML5VideoView.java
// Please keep them in sync when changed here.
typedef enum {INITIALIZED, PREPARING, PREPARED, PLAYING, RELEASED} PlayerState;

class VideoLayerAndroid : public LayerAndroid {

public:
    VideoLayerAndroid();
    explicit VideoLayerAndroid(const VideoLayerAndroid& layer);

    virtual bool isVideo() const { return true; }
    virtual LayerAndroid* copy() const { return new VideoLayerAndroid(*this); }

    // The following 3 functions are called in UI thread only.
    virtual bool drawGL();
    void setSurfaceTexture(sp<SurfaceTexture> texture, int textureName, PlayerState playerState);
    GLuint createBackgroundTexture();
    GLuint createSpinnerOuterTexture();
    GLuint createSpinnerInnerTexture();
    GLuint createPosterTexture();

private:
    GLuint createTextureFromImage(int buttonType);
    void init();
    // Surface texture for showing the video is actually allocated in Java side
    // and passed into this native code.
    sp<android::SurfaceTexture> m_surfaceTexture;

    PlayerState m_playerState;

    // Texture for showing the static image will be created at native side.
    static bool m_createdTexture;
    static GLuint m_backgroundTextureId;
    static GLuint m_posterTextureId;
    static GLuint m_spinnerOuterTextureId;
    static GLuint m_spinnerInnerTextureId;

    static double m_rotateDegree;

    static const int ROTATESTEP = 12;
    static const int IMAGESIZE = 64;
    static const IntRect buttonRect;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // VideoLayerAndroid_h
