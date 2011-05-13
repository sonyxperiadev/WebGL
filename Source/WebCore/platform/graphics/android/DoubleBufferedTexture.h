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

#ifndef DoubleBufferedTexture_h
#define DoubleBufferedTexture_h

#include "SharedTexture.h"
#include <EGL/egl.h>

namespace WebCore {

class DoubleBufferedTexture {
public:
    // consumer thread functions
    DoubleBufferedTexture(EGLContext sharedContext);
    virtual ~DoubleBufferedTexture();

    // provider thread functions
    virtual TextureInfo* producerLock();
    virtual void producerRelease();
    virtual void producerReleaseAndSwap();
    EGLContext producerAcquireContext();
    void producerDeleteTextures();

    // consumer thread functions
    TextureInfo* consumerLock();
    void consumerRelease();
    void consumerDeleteTextures();

protected:
    SharedTexture* getReadableTexture();
    SharedTexture* getWriteableTexture();

    SharedTexture m_textureA;
    SharedTexture m_textureB;

private:

    SharedTexture* m_writeableTexture;
    SharedTexture* m_lockedConsumerTexture; // only used by the consumer

    EGLDisplay m_display;
    EGLContext m_pContext;
    EGLContext m_cContext;

    bool m_supportsEGLImage;
};

} // namespace WebCore

#endif // DoubleBufferedTexture_h
