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

#include "config.h"
#include "TextureInfo.h"

#include "WebCoreJni.h"

#include <JNIUtility.h>
#include <android/native_window.h>
#include <gui/SurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>

namespace WebCore {

TextureInfo::TextureInfo(SharedTextureMode mode)
{
    m_textureId = GL_NO_TEXTURE;
    m_width = 0;
    m_height = 0;
    m_internalFormat = 0;
    m_sharedTextureMode = mode;
    m_eglSurface = EGL_NO_SURFACE;
    m_pictureCount = 0;
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

GLenum TextureInfo::getTextureTarget()
{
#if DEPRECATED_SURFACE_TEXTURE_MODE
    if (m_surfaceTexture.get()) {
        GLenum target = m_surfaceTexture->getCurrentTextureTarget();
        // TODO: remove this translation when TEXTURE_2D+RGBA surface texture
        // support is deprecated.
        if (target == GL_TEXTURE_2D)
            return 0;
        return target;
    }
#endif
    return GL_TEXTURE_2D;
}

} // namespace WebCore
