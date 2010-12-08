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

#ifndef DeleteTextureOperation_h
#define DeleteTextureOperation_h

#include "GLUtils.h"
#include "QueuedOperation.h"

namespace WebCore {

class DeleteTextureOperation : public QueuedOperation {
 public:
    DeleteTextureOperation(GLuint textureId, EGLImageKHR eglImage)
        : QueuedOperation(QueuedOperation::DeleteTexture, 0)
        , m_textureId(textureId)
        , m_eglImage(eglImage) {}
    virtual bool operator==(const QueuedOperation* operation)
    {
        if (operation->type() != type())
            return false;
        const DeleteTextureOperation* op = static_cast<const DeleteTextureOperation*>(operation);
        return op->m_textureId == m_textureId;
    }
    virtual void run()
    {
        if (m_textureId)
            GLUtils::deleteTexture(&m_textureId);
        if (m_eglImage)
            eglDestroyImageKHR(eglGetCurrentDisplay(), m_eglImage);
    }
 private:
    GLuint m_textureId;
    EGLImageKHR m_eglImage;
};

}

#endif // DeleteTextureOperation_h
