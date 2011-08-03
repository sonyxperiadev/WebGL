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

    // This queue can be accessed from UI and TexGen thread, therefore, we need
    // a lock to protect its access
    TileTransferData* m_transferQueue;
    android::Mutex m_transferQueueLock;

    void initSharedSurfaceTextures(int width, int height);
    sp<ANativeWindow> m_ANW;

    bool getHasGLContext();
    void setHasGLContext(bool hasContext);

    int getNextTransferQueueIndex();

    void addItemInTransferQueue(const TileRenderInfo* info, int index);

    // Check if the item @ index is ready for update.
    // The lock will be done when returning true.
    bool lockForUpdate(int index, const TileRenderInfo* renderInfo);

    void discardQueue();

    // Store info for currentOpWaitingRemoval() to tell which operation
    // will be removed.
    TilePainter* m_currentRemovingPaint;

    // Return the buffer number.
    int size();

private:
    // Note that the m_transferQueueIndex only changed in the TexGen thread
    // where we are going to move on to update the next item in the queue.
    int m_transferQueueIndex;

    GLuint m_fboID; // The FBO used for copy the SurfTex to each tile

    GLuint m_sharedSurfaceTextureId;

    // GLContext can be lost when WebView destroyed.
    bool m_hasGLContext;
    android::Mutex m_hasGLContextLock;

    // Save and restore the GL State while switching from/to FBO.
    void saveGLState();
    void setGLStateForCopy(int width, int height);
    void restoreGLState();
    GLState m_GLStateBeforeBlit;

    // Check the current transfer queue item is obsolete or not.
    bool checkObsolete(int index);

    // Before each draw call and the blit operation, clean up all the
    // pendingDiscard items.
    void cleanupTransportQueue();

    // This function can tell Tex Gen thread whether or not the current
    // operation need to be removed now.
    bool currentOpWaitingRemoval(const TileRenderInfo* renderInfo);

    void blitTileFromQueue(GLuint fboID, BaseTileTexture* destTex, GLuint srcTexId, GLenum srcTexTarget);

    // Each element in the queue has its own lock, basically lock before update
    // and unlock at drawGL time.
    // This is similar to the internal lock from SurfaceTexture, but we can't
    // naively rely on them since when tearing down, UI thread need to clean up
    // the pending jobs in that Tex Gen thread, if the Tex Gen is waiting for
    // Surface Texture, then we are stuck.
    android::Mutex* m_transferQueueItemLocks;

    sp<android::SurfaceTexture> m_sharedSurfaceTexture;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TransferQueue_h
