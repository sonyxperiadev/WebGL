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

#ifndef TransferQueue_h
#define TransferQueue_h

#if USE(ACCELERATED_COMPOSITING)

#include "BaseTile.h"
#include "BaseTileTexture.h"
#include "ShaderProgram.h"
#include "TiledPage.h"

namespace WebCore {

struct GLState {
    GLint viewport[4];
    GLboolean scissor[1];
    GLboolean depth[1];
    GLfloat clearColor[4];
};

class TransferQueue {
public:
    TransferQueue();
    ~TransferQueue();

    void updateDirtyBaseTiles();

    void initSharedSurfaceTextures(int width, int height);

    void updateQueueWithBitmap(const TileRenderInfo* renderInfo, int x, int y,
                               const SkBitmap& bitmap);

    void discardQueue();

    void addItemInTransferQueue(const TileRenderInfo* info);

    // Check if the item @ index is ready for update.
    // The lock will be done when returning true.
    bool readyForUpdate();

    void interruptTransferQueue(bool);

    void lockQueue() { m_transferQueueItemLocks.lock(); }
    void unlockQueue() { m_transferQueueItemLocks.unlock(); }

    // This queue can be accessed from UI and TexGen thread, therefore, we need
    // a lock to protect its access
    TileTransferData* m_transferQueue;

    sp<ANativeWindow> m_ANW;

    // EGL wrapper around m_ANW for use by the GaneshRenderer
    EGLSurface m_eglSurface;

private:
    bool getHasGLContext();
    void setHasGLContext(bool hasContext);

    int getNextTransferQueueIndex();

    // Save and restore the GL State while switching from/to FBO.
    void saveGLState();
    void setGLStateForCopy(int width, int height);
    void restoreGLState();

    // Check the current transfer queue item is obsolete or not.
    bool checkObsolete(int index);

    // Before each draw call and the blit operation, clean up all the
    // pendingDiscard items.
    void cleanupTransportQueue();

    void blitTileFromQueue(GLuint fboID, BaseTileTexture* destTex,
                           GLuint srcTexId, GLenum srcTexTarget);

    // Note that the m_transferQueueIndex only changed in the TexGen thread
    // where we are going to move on to update the next item in the queue.
    int m_transferQueueIndex;

    GLuint m_fboID; // The FBO used for copy the SurfTex to each tile

    GLuint m_sharedSurfaceTextureId;

    // GLContext can be lost when WebView destroyed.
    bool m_hasGLContext;

    GLState m_GLStateBeforeBlit;
    sp<android::SurfaceTexture> m_sharedSurfaceTexture;

    int m_emptyItemCount;

    bool m_interruptedByRemovingOp;

    // We are using wait/signal to handle our own queue sync.
    // First of all, if we don't have our own lock, then while WebView is
    // destroyed, the UI thread will wait for the Tex Gen to get out from
    // dequeue operation, which will not succeed. B/c at this moment, we
    // already lost the GL Context.
    // Now we maintain a counter, which is m_emptyItemCount. When this reach
    // 0, then we need the Tex Gen thread to wait. UI thread can signal this
    // wait after calling updateTexImage at the draw call , or after WebView
    // is destroyed.
    android::Mutex m_transferQueueItemLocks;
    android::Condition m_transferQueueItemCond;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TransferQueue_h
