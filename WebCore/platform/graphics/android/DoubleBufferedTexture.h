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

#ifndef DoubleBufferedTexture__DEFINED
#define DoubleBufferedTexture__DEFINED

#include <utils/threads.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "SharedTexture.h"

namespace WebCore {

class DoubleBufferedTexture {
public:
    // consumer thread functions
    DoubleBufferedTexture(EGLContext sharedContext);

    // provider thread functions
    TextureInfo* producerLock();
    void producerRelease();
    EGLContext producerAcquireContext();

    // consumer thread functions
    TextureInfo* consumerLock();
    void consumerRelease();

private:
    SharedTexture* getFrontTexture();
    SharedTexture* getBackTexture();

    EGLDisplay m_display;

    EGLContext m_pContext;
    EGLContext m_cContext;

    SharedTexture m_textureA;
    SharedTexture m_textureB;
    SharedTexture* m_frontTexture;
    SharedTexture* m_lockedConsumerTexture; // only used by the consumer

    android::Mutex m_varLock;

    bool m_supportsEGLImage;
};


} // namespace WebCore

#endif // DoubleBufferedTexture__DEFINED
