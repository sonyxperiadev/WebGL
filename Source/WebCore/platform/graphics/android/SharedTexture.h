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

#ifndef SharedTexture_h
#define SharedTexture_h

#include "TextureInfo.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <utils/threads.h>

namespace WebCore {

/**
 * SharedTexture is a class that encapsulates all the necessary variables
 * needed to share a single texture across threads. In the case that threads
 * communicate using EGL's sharedContext mechanism or do not support the proper
 * EGLImage extensions the targetTexture, eglImage, and isNewImage variables are
 * not used.
 */
class SharedTexture {
public:
    // consumer thread functions
    SharedTexture(SharedTextureMode mode);
    ~SharedTexture();

    TextureInfo* lockSource();
    void releaseSource();

    TextureInfo* lockTarget();
    void releaseTarget();

    // these locks are only used for the methods below
    void lock() { m_lock.lock(); }
    void unlock() { m_lock.unlock(); }

    void initSourceTexture(); // producer thread only
    void deleteSourceTexture(); // producer thread only
    void deleteTargetTexture(); // consumer thread only
    GLuint getSourceTextureId() { return m_sourceTexture->m_textureId; }
    GLuint getTargetTextureId() { return m_targetTexture->m_textureId; }
    EGLImageKHR getEGLImage() { return m_eglImage; }

private:
    /**
     * The mutex is used to ensure that the contents of the struct are current across
     * threads and that only one thread is manipulating the texture at a given time.
     */
    android::Mutex m_lock;
    /**
     * The texture and its associated metadata that is used by the producer. The
     * texture is created in the producer's thread and can only be read by the
     * consumer when the consumer shares the same context as the producer. The
     * metadata is used to track changes to the texture that would orphan the
     * target texture and require a new EGLImage to be constructed.
     */
    TextureInfo* m_sourceTexture;
    /**
     * The target texture stores the id and metadata of the texture that is to be
     * used by the consumer. In the case where EGLImages are supported this hold
     * the current eglImage target.
     */
    TextureInfo* m_targetTexture;
    /**
     * The EGLImage is used to share the texture between EGLContexts on two
     * different threads. This serves as an alternative to sharing the contexts
     * but is only used if GL and EGL support the required extensions.
     */
    EGLImageKHR m_eglImage;
    /**
     * This flag is used to determine if the eglImage has been updated. This
     * signals the consumer thread to rebind the targetTexture to the new image.
     */
    bool m_isNewImage;
    /**
     * The sync allows the consumer to release the lock prior to the commands
     * executing on the GPU. Prior to releasing the lock the consumer creates
     * a sync object and stores it here. After locking the texture the client
     * must check that the sync has completed prior to manipulating the texture.
     * This value is only used if the proper EGL extensions are supported.
     */
    EGLSyncKHR m_syncObject;

    EGLDisplay m_display;

    bool m_supportsEGLImage;
    bool m_supportsEGLFenceSyncKHR;

    SharedTextureMode m_sharedTextureMode;
};

} // namespace WebCore

#endif // SharedTexture_h
