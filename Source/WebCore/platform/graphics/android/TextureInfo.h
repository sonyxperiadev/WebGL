/*
 * Copyright 2011, The Android Open Source Project
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

#ifndef TextureInfo_h
#define TextureInfo_h

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <jni.h>
#include <ui/GraphicBuffer.h>
#include <utils/RefBase.h>
#include "BaseTile.h"
using android::sp;

#define DEPRECATED_SURFACE_TEXTURE_MODE 0

namespace android {
class SurfaceTexture;
}

namespace WebCore {

static const GLuint GL_NO_TEXTURE = 0;
/**
 * TextureInfo is a class that stores both the texture and metadata about the
 * texture.
 */
enum SharedTextureMode {
    EglImageMode,
    SurfaceTextureMode
};

class TextureInfo {
public:

    TextureInfo(SharedTextureMode mode);

    bool equalsAttributes(const TextureInfo* otherTexture);
    void copyAttributes(const TextureInfo* sourceTexture);

    SharedTextureMode getSharedTextureMode() { return m_sharedTextureMode; }
    bool operator==(const TextureInfo& otherTexture);

    GLuint m_textureId;
    int32_t m_width;
    int32_t m_height;
    GLenum m_internalFormat;

    // Surface Texture specific data
#if DEPRECATED_SURFACE_TEXTURE_MODE
    sp<android::SurfaceTexture> m_surfaceTexture;
#endif
    // TODO: Delete this after the Ganesh code path get fixed.
    sp<ANativeWindow> m_ANW;
    // The EGLSurface wraps the m_ANW to enable direct OpenGL rendering (e.g. Ganesh)
    EGLSurface m_eglSurface;

    int m_pictureCount;
private:
    SharedTextureMode m_sharedTextureMode;
};

} // namespace WebCore

#endif // TextureInfo_h
