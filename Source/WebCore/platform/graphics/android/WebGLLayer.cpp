/*
 * Copyright (C) 2011, 2012, Sony Ericsson Mobile Communications AB
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

#if ENABLE(WEBGL) && USE(ACCELERATED_COMPOSITING)

#include "WebGLLayer.h"

#include "GraphicsContext3DInternal.h"
#include "SkRect.h"
#include "TilesManager.h"
#include "TransformationMatrix.h"

namespace WebCore {

WebGLLayer::WebGLLayer(GraphicsContext3DProxy* proxy)
    : LayerAndroid((RenderLayer*)0)
    , m_proxy(proxy)
{
    LOGWEBGL("WebGLLayer::WebGLLayer(GraphicsContext3DProxy* %p), this = %p", proxy, this);
    for (int i = 0; i < 2; i++) {
        m_eglImages[i] = 0;
        m_textures[i] = 0;
    }
}

WebGLLayer::WebGLLayer(const WebGLLayer& layer)
    : LayerAndroid(layer)
    , m_proxy(layer.m_proxy)
{
    LOGWEBGL("WebGLLayer::WebGLLayer(const WebGLLayer& %p), this = %p", &layer, this);
    for (int i = 0; i < 2; i++) {
        m_eglImages[i] = layer.m_eglImages[i];
        m_textures[i] = layer.m_textures[i];
    }
}

WebGLLayer::~WebGLLayer()
{
    LOGWEBGL("WebGLLayer::~WebGLLayer(), this = %p", this);
    for (int i = 0; i < 2; i++) {
        if (m_textures[i] != 0) {
            LOGWEBGL("glDeleteTextures(1, %d)", m_textures[i]);
            glDeleteTextures(1, &m_textures[i]);
            LOGWEBGL("glGetError() = %d", glGetError());
        }
    }
}

GLuint WebGLLayer::createTexture(EGLImageKHR image, int width, int height)
{
    LOGWEBGL("WebGLLayer::createTexture(image = %p)", image);
    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image);
    glBindTexture(GL_TEXTURE_2D, 0);

    return texture;
}

bool WebGLLayer::drawGL()
{
    bool askScreenUpdate = false;

    askScreenUpdate |= LayerAndroid::drawGL();

    if (m_proxy.get()) {
        EGLImageKHR eglImage;
        int width;
        int height;
        SkRect localBounds;
        bool requestUpdate;
        bool locked = m_proxy->lockFrontBuffer(eglImage, width, height, localBounds, requestUpdate);
        if (locked) {
            GLuint texture;
            LOGWEBGL("WebGLLayer::drawGL(), this = %p, m_proxy = %p, eglImage = %d",
                     this, m_proxy.get(), eglImage);
            if (m_eglImages[0] == eglImage) {
                texture = m_textures[0];
            }
            else if (m_eglImages[1] == eglImage) {
                texture = m_textures[1];
            }
            else {
                // New buffer
                int idx = 0;
                if (m_eglImages[idx] != 0)
                    idx++;
                if (m_eglImages[idx] != 0)
                    return false;
                m_eglImages[idx] = eglImage;
                m_textures[idx] = createTexture(eglImage, width, height);
                texture = m_textures[idx];
            }

            // Flip the y-coordinate
            TransformationMatrix transform = m_drawTransform;
            transform = transform.translate(0, 2 * localBounds.top() + localBounds.height());
            transform = transform.scale3d(1.0, -1.0, 1.0);
            TilesManager::instance()->shader()->drawLayerQuad(transform, localBounds,
                                                              texture, 1.0f, true);
            m_proxy->releaseFrontBuffer();
            askScreenUpdate |= requestUpdate;
        }
    }

    return askScreenUpdate;
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(ACCELERATED_COMPOSITING)
