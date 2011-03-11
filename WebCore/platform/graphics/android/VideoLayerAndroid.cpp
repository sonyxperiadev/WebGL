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

#include "config.h"
#include "VideoLayerAndroid.h"

#include "TilesManager.h"
#include <GLES2/gl2.h>
#include <gui/SurfaceTexture.h>

#if USE(ACCELERATED_COMPOSITING)

#ifdef DEBUG
#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "VideoLayerAndroid", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {
GLuint VideoLayerAndroid::m_pauseTextureId = 0;
bool VideoLayerAndroid::m_createdPauseTexture = false;

VideoLayerAndroid::VideoLayerAndroid()
    : LayerAndroid((RenderLayer*)0)
{
    init();
}

VideoLayerAndroid::VideoLayerAndroid(const VideoLayerAndroid& layer)
    : LayerAndroid(layer)
{
    init();
}

void VideoLayerAndroid::init()
{
    // m_surfaceTexture is only useful on UI thread, no need to copy.
    // And it will be set at setBaseLayer timeframe

    // m_useSurfTex will be true only after the player is prepared
    m_useSurfTex = false;
    m_textureId = 0;
}

// We can use this function to set the Layer to point to surface texture.
void VideoLayerAndroid::setSurfaceTexture(sp<SurfaceTexture> texture,
                                          int textureName, bool useSurfTex)
{
    m_surfaceTexture = texture;
    m_textureId = textureName;
    m_useSurfTex = useSurfTex;
}

GLuint VideoLayerAndroid::createPauseTexture()
{
    GLuint texture;
    glGenTextures(1, &texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLubyte pixels[4 *3] = {
        128, 128, 128,
        128, 128, 128,
        128, 128, 128,
        128, 128, 128
    };
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    GLUtils::checkGlError("glTexImage2D");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

bool VideoLayerAndroid::drawGL(SkMatrix& matrix)
{
    // Lazy allocated the paused texture.
    if (!m_createdPauseTexture) {
        m_pauseTextureId = createPauseTexture();
        m_createdPauseTexture = true;
    }

    SkRect rect = SkRect::MakeSize(getSize());
    GLfloat surfaceMatrix[16];

    // Draw the paused image or the Video
    if (!m_surfaceTexture.get() || !m_useSurfTex) {
        TransformationMatrix identity;
        GLUtils::toGLMatrix(surfaceMatrix, identity);
        TilesManager::instance()->shader()->drawLayerQuad(drawTransform(), rect,
                                                          m_pauseTextureId,
                                                          0.5, true);
    } else {
        m_surfaceTexture->updateTexImage();
        m_surfaceTexture->getTransformMatrix(surfaceMatrix);
        TilesManager::instance()->shader()->drawVideoLayerQuad(drawTransform(),
                                                               surfaceMatrix,
                                                               rect, m_textureId);
    }

    return drawChildrenGL(matrix);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
