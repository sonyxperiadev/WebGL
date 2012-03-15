/*
 * Copyright (C) 2012 Sony Ericsson Mobile Communications AB
 * Copyright (C) 2012 Sony Mobile Communications AB
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

#if ENABLE(WEBGL)

#include "GraphicsContext3DProxy.h"
#include "GraphicsContext3DInternal.h"

namespace WebCore {

GraphicsContext3DProxy::GraphicsContext3DProxy()
{
    LOGWEBGL("GraphicsContext3DProxy::GraphicsContext3DProxy(), this = %p", this);
}

GraphicsContext3DProxy::~GraphicsContext3DProxy()
{
    LOGWEBGL("GraphicsContext3DProxy::~GraphicsContext3DProxy(), this = %p", this);
}

void GraphicsContext3DProxy::setGraphicsContext(GraphicsContext3DInternal* context)
{
    MutexLocker lock(m_mutex);
    m_context = context;
}

void GraphicsContext3DProxy::incr()
{
    MutexLocker lock(m_mutex);
    m_refcount++;
}

void GraphicsContext3DProxy::decr()
{
    MutexLocker lock(m_mutex);
    m_refcount--;
    if (m_refcount == 0) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
}

bool GraphicsContext3DProxy::lockFrontBuffer(GLuint& texture, SkRect& rect)
{
    MutexLocker lock(m_mutex);
    if (!m_context) {
        return false;
    }
    EGLImageKHR image;
    bool locked = m_context->lockFrontBuffer(image, rect);
    if (locked) {
        if (m_texture == 0)
            glGenTextures(1, &m_texture);

        glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_texture);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image);
        texture = m_texture;
    }

    return locked;
}

void GraphicsContext3DProxy::releaseFrontBuffer()
{
    MutexLocker lock(m_mutex);
    if (!m_context) {
        return;
    }
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    m_context->releaseFrontBuffer();
}
}
#endif // ENABLE(WEBGL)
