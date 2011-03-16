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

#include "config.h"
#include "SharedTexture.h"

#include "GLUtils.h"

#define LOG_NDEBUG 1
#define LOG_TAG "SharedTexture.cpp"
#include <utils/Log.h>

namespace WebCore {

TextureInfo::TextureInfo()
{
    m_textureId = GL_NO_TEXTURE;
    m_width = 0;
    m_height = 0;
    m_internalFormat = 0;
}

bool TextureInfo::equalsAttributes(const TextureInfo* otherTexture)
{
    return otherTexture->m_width == m_width
        && otherTexture->m_height == m_height
        && otherTexture->m_internalFormat == m_internalFormat;
}

void TextureInfo::copyAttributes(const TextureInfo* sourceTexture)
{
    m_width = sourceTexture->m_width;
    m_height = sourceTexture->m_height;
    m_internalFormat = sourceTexture->m_internalFormat;
}

bool TextureInfo::operator==(const TextureInfo& otherTexture)
{
    return otherTexture.m_textureId == m_textureId && equalsAttributes(&otherTexture);
}

SharedTexture::SharedTexture()
{
    m_eglImage = EGL_NO_IMAGE_KHR;
    m_isNewImage = true;
    m_syncObject = EGL_NO_SYNC_KHR;

    // Defer initialization of these values until we initialize the source
    // texture. This ensures that this initialization happens in the appropriate
    // thread.
    m_display = 0;
    m_supportsEGLImage = false;
    m_supportsEGLFenceSyncKHR = false;
}

// called by the consumer when it no longer wants to consume and after it has
// terminated all providers. If EGLImages are used, the deletion of the
// source texture and EGLImage is the responsibility of the caller. In the case
// of double buffered textures this is handled in the onDestroy(...) method.
SharedTexture::~SharedTexture()
{
    if (m_supportsEGLImage)
        GLUtils::deleteTexture(&m_targetTexture.m_textureId);
    else
        GLUtils::deleteTexture(&m_sourceTexture.m_textureId);
}

void SharedTexture::initSourceTexture()
{

    m_display = eglGetCurrentDisplay();
    m_supportsEGLImage = GLUtils::isEGLImageSupported();
    m_supportsEGLFenceSyncKHR = GLUtils::isEGLFenceSyncSupported();

    // TODO temporarily disable fence sync until the EGL framework fixes
    // performance issues that result from consistently adding/removing fences.
    m_supportsEGLFenceSyncKHR = false;

    LOGI("imageEGL: %d syncKHR: %d", m_supportsEGLImage, m_supportsEGLFenceSyncKHR);

    glGenTextures(1, &m_sourceTexture.m_textureId);
}

TextureInfo* SharedTexture::lockSource()
{
    m_lock.lock();

    if (m_supportsEGLFenceSyncKHR && m_syncObject != EGL_NO_SYNC_KHR) {

        EGLint status = eglClientWaitSyncKHR(m_display, m_syncObject, 0, 1000000);

        if (status == EGL_TIMEOUT_EXPIRED_KHR)
            LOGE("Sync timeout for shared texture (%d)", m_sourceTexture.m_textureId);

        eglDestroySyncKHR(m_display, m_syncObject);
        m_syncObject = EGL_NO_SYNC_KHR;
    }

    return &m_sourceTexture;
}

void SharedTexture::releaseSource()
{
    if (m_supportsEGLImage) {
        // delete the existing image if needed
        if (!m_sourceTexture.equalsAttributes(&m_targetTexture)) {
            if (m_eglImage != EGL_NO_IMAGE_KHR) {
                eglDestroyImageKHR(m_display, m_eglImage);
                m_eglImage = EGL_NO_IMAGE_KHR;
                m_isNewImage = true;
            }
            m_targetTexture.copyAttributes(&m_sourceTexture);
        }

        // create an image from the texture
        if (m_eglImage == EGL_NO_IMAGE_KHR) {
            GLUtils::createEGLImageFromTexture(m_sourceTexture.m_textureId, &m_eglImage);
            LOGV("Generating Image (%d) 0x%x", m_sourceTexture.m_textureId, m_eglImage);

            glFinish(); // ensures the texture is ready to be used by the consumer
        }

    } else {

        m_targetTexture = m_sourceTexture;

        // in the case of shared contexts we must flush the texture edits to the
        // GPU. This ensures the edits complete prior to allowing the texture to
        // be bound on the producers context.
        glFlush();
    }

    m_lock.unlock();
}

TextureInfo* SharedTexture::lockTarget()
{
    m_lock.lock();

    if ((!m_supportsEGLImage && m_targetTexture.m_textureId == GL_NO_TEXTURE)
        || (m_supportsEGLImage && m_eglImage == EGL_NO_IMAGE_KHR)) {
        m_lock.unlock();
        return 0;
    }

    if (m_supportsEGLImage && (m_isNewImage || m_targetTexture.m_textureId == GL_NO_TEXTURE)) {
        if (m_targetTexture.m_textureId == GL_NO_TEXTURE)
            glGenTextures(1, &m_targetTexture.m_textureId);

        GLUtils::createTextureFromEGLImage(m_targetTexture.m_textureId, m_eglImage);
        LOGV("Generating Consumer Texture from 0x%x", m_eglImage);
        m_isNewImage = false;
    }

    return &m_targetTexture;
}

void SharedTexture::releaseTarget()
{

    if (m_supportsEGLFenceSyncKHR) {
        if (m_syncObject != EGL_NO_SYNC_KHR)
            eglDestroySyncKHR(m_display, m_syncObject);
        m_syncObject = eglCreateSyncKHR(m_display, EGL_SYNC_FENCE_KHR, 0);
    } else {
        // TODO the flush currently prevents the screen from getting partial
        // updates but the only way to guarantee this is to call glFinish. Until
        // we support an EGL sync we will leave flush enable in order to test
        // with modest performance.
        glFlush();
    }

    m_lock.unlock();
}

} // namespace WebCore
