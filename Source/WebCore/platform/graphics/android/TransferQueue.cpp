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
#include "TransferQueue.h"

#if USE(ACCELERATED_COMPOSITING)

#include "BaseTile.h"
#include "PaintedSurface.h"
#include <android/native_window.h>
#include <gui/SurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>

#ifdef DEBUG
#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "TransferQueue", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

#define ST_BUFFER_NUMBER 4

namespace WebCore {

TransferQueue::TransferQueue()
    : m_currentRemovingPaint(0)
    , m_transferQueueIndex(0)
    , m_fboID(0)
    , m_sharedSurfaceTextureId(0)
    , m_hasGLContext(true)
{
    memset(&m_GLStateBeforeBlit, 0, sizeof(m_GLStateBeforeBlit));

    m_transferQueueItemLocks = new android::Mutex[ST_BUFFER_NUMBER];

    m_transferQueue = new TileTransferData[ST_BUFFER_NUMBER];
    for (int i = 0; i < ST_BUFFER_NUMBER; i++) {
        m_transferQueue[i].savedBaseTilePtr = 0;
        m_transferQueue[i].status = emptyItem;
    }
}

TransferQueue::~TransferQueue()
{
    glDeleteFramebuffers(1, &m_fboID);
    m_fboID = 0;
    glDeleteTextures(1, &m_sharedSurfaceTextureId);
    m_sharedSurfaceTextureId = 0;

    delete[] m_transferQueueItemLocks;
    delete[] m_transferQueue;
}

int TransferQueue::size()
{
    return ST_BUFFER_NUMBER;
}

void TransferQueue::initSharedSurfaceTextures(int width, int height)
{
    if (!m_sharedSurfaceTextureId) {
        glGenTextures(1, &m_sharedSurfaceTextureId);
        m_sharedSurfaceTexture =
            new android::SurfaceTexture(m_sharedSurfaceTextureId);
        m_ANW = new android::SurfaceTextureClient(m_sharedSurfaceTexture);
        m_sharedSurfaceTexture->setSynchronousMode(true);
        m_sharedSurfaceTexture->setBufferCount(ST_BUFFER_NUMBER+1);

        int result = native_window_set_buffers_geometry(m_ANW.get(),
                width, height, HAL_PIXEL_FORMAT_RGBA_8888);
        GLUtils::checkSurfaceTextureError("native_window_set_buffers_geometry", result);
        result = native_window_set_usage(m_ANW.get(),
                GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
        GLUtils::checkSurfaceTextureError("native_window_set_usage", result);
    }

    if (!m_fboID)
        glGenFramebuffers(1, &m_fboID);
}

// When bliting, if the item from the transfer queue is mismatching b/t the
// BaseTile and the content, then the item is considered as obsolete, and
// the content is discarded.
bool TransferQueue::checkObsolete(int index)
{
    BaseTile* baseTilePtr = m_transferQueue[index].savedBaseTilePtr;
    if (!baseTilePtr) {
        XLOG("Invalid savedBaseTilePtr , such that the tile is obsolete");
        return true;
    }

    BaseTileTexture* baseTileTexture = baseTilePtr->texture();
    if (!baseTileTexture) {
        XLOG("Invalid baseTileTexture , such that the tile is obsolete");
        return true;
    }

    const TextureTileInfo* tileInfo = &m_transferQueue[index].tileInfo;

    if (tileInfo->m_x != baseTilePtr->x()
        || tileInfo->m_y != baseTilePtr->y()
        || tileInfo->m_scale != baseTilePtr->scale()
        || tileInfo->m_painter != baseTilePtr->painter()) {
        XLOG("Mismatching x, y, scale or painter , such that the tile is obsolete");
        return true;
    }

    return false;
}

void TransferQueue::blitTileFromQueue(GLuint fboID, BaseTileTexture* destTex, GLuint srcTexId, GLenum srcTexTarget)
{
    // Then set up the FBO and copy the SurfTex content in.
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           destTex->m_ownTextureId,
                           0);
    setGLStateForCopy(destTex->getSize().width(),
                      destTex->getSize().height());
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        XLOG("Error: glCheckFramebufferStatus failed");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    // Use empty rect to set up the special matrix to draw.
    SkRect rect  = SkRect::MakeEmpty();
    TilesManager::instance()->shader()->drawQuad(rect, srcTexId, 1.0,
                       srcTexTarget);

    // Need to WAR a driver bug to add a sync point here
    GLubyte readBackPixels[4];
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, readBackPixels);

    // Clean up FBO setup.
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // rebind the standard FBO
    GLUtils::checkGlError("copy the surface texture into the normal one");
}

bool TransferQueue::currentOpWaitingRemoval(const TileRenderInfo* renderInfo)
{
    if (m_currentRemovingPaint
        && m_currentRemovingPaint == renderInfo->tilePainter)
        return true;
    return false;
}

bool TransferQueue::lockForUpdate(int index, const TileRenderInfo* renderInfo)
{
    // Before dequeuing the buffer from Surface Texture, make sure this item
    // has been consumed. We can't simply rely on dequeueBuffer, due to the
    // UI thread may want to remove the current operation, so we have to add
    // our own locking mechanism.
    // For blocking case, the item's status should not be emptyItem.
    while (m_transferQueueItemLocks[index].tryLock()) {
        // If the current operation is on an resource which is going to be
        // removed from UI thread, then early return.
        if (currentOpWaitingRemoval(renderInfo)) {
            XLOG("Quit bitmap update: operation is removed from UI thread!"
                 " x y %d %d",
                 renderInfo->x, renderInfo->y);
            return false;
        }
    }

    // And if we have lost GL Context, then we can just early return here.
    if (!getHasGLContext()) {
        m_transferQueue[index].status = pendingDiscard;
        m_transferQueueItemLocks[index].unlock();
        XLOG("Quit bitmap update: No GL Context! x y %d %d",
             renderInfo->x, renderInfo->y);
        return false;
    }

    return true;
}

bool TransferQueue::getHasGLContext()
{
    android::Mutex::Autolock lock(m_hasGLContextLock);
    bool hasContext = m_hasGLContext;
    return hasContext;
}

void TransferQueue::setHasGLContext(bool hasContext)
{
    android::Mutex::Autolock lock(m_hasGLContextLock);
    m_hasGLContext = hasContext;
}

void TransferQueue::discardQueue()
{
    // Unblock the Tex Gen thread first before Tile Page deletion.
    // Otherwise, there will be a deadlock while removing operations.
    setHasGLContext(false);

    for (int i = 0 ; i < ST_BUFFER_NUMBER; i++) {
        m_transferQueue[i].status = pendingDiscard;
        m_transferQueueItemLocks[i].unlock();
    }
}

// Call on UI thread to copy from the shared Surface Texture to the BaseTile's texture.
void TransferQueue::updateDirtyBaseTiles()
{
    bool unlockFlag[ST_BUFFER_NUMBER];
    memset(unlockFlag, 0, sizeof(unlockFlag));

    saveGLState();
    m_transferQueueLock.lock();

    cleanupTransportQueue();
    if (!getHasGLContext())
        setHasGLContext(true);

    // Start from the oldest item, we call the updateTexImage to retrive
    // the texture and blit that into each BaseTile's texture.
    const int nextItemIndex = getNextTransferQueueIndex();
    int index = nextItemIndex;
    for (int k = 0; k < ST_BUFFER_NUMBER ; k++) {
        if (m_transferQueue[index].status == pendingBlit) {
            bool obsoleteBaseTile = checkObsolete(index);
            // Save the needed info, update the Surf Tex, clean up the item in
            // the queue. Then either move on to next item or copy the content.
            BaseTileTexture* destTexture = 0;
            if (!obsoleteBaseTile)
                destTexture = m_transferQueue[index].savedBaseTilePtr->texture();

            m_sharedSurfaceTexture->updateTexImage();

            unlockFlag[index] = true;
            m_transferQueue[index].savedBaseTilePtr = 0;
            m_transferQueue[index].status = emptyItem;
            if (obsoleteBaseTile) {
                XLOG("Warning: the texture is obsolete for this baseTile");
                continue;
            }

            blitTileFromQueue(m_fboID, destTexture,
                              m_sharedSurfaceTextureId,
                              m_sharedSurfaceTexture->getCurrentTextureTarget());

            // After the base tile copied into the GL texture, we need to
            // update the texture's info such that at draw time, readyFor
            // will find the latest texture's info
            // We don't need a map any more, each texture contains its own
            // texturesTileInfo.
            destTexture->setOwnTextureTileInfoFromQueue(&m_transferQueue[index].tileInfo);

            XLOG("Blit tile x, y %d %d to destTexture->m_ownTextureId %d",
                 m_transferQueue[index].tileInfo.m_x,
                 m_transferQueue[index].tileInfo.m_y,
                 destTexture->m_ownTextureId);
        }
        index = (index + 1) % ST_BUFFER_NUMBER;
    }

    restoreGLState();

    int unlockIndex = nextItemIndex;
    for (int k = 0; k < ST_BUFFER_NUMBER; k++) {
        // Check the same order as the updateTexImage.
        if (unlockFlag[unlockIndex])
            m_transferQueueItemLocks[unlockIndex].unlock();
        unlockIndex = (unlockIndex + 1) % ST_BUFFER_NUMBER;
    }

    m_transferQueueLock.unlock();
}

// Note that there should be lock/unlock around this function call.
// Currently only called by GLUtils::updateSharedSurfaceTextureWithBitmap.
void TransferQueue::addItemInTransferQueue(const TileRenderInfo* renderInfo,
                                          int index)
{
    m_transferQueueIndex = index;

    if (m_transferQueue[index].savedBaseTilePtr
        || m_transferQueue[index].status != emptyItem) {
        XLOG("ERROR update a tile which is dirty already @ index %d", index);
    }

    m_transferQueue[index].savedBaseTilePtr = renderInfo->baseTile;
    m_transferQueue[index].status = pendingBlit;

    // Now fill the tileInfo.
    TextureTileInfo* textureInfo = &m_transferQueue[index].tileInfo;

    textureInfo->m_x = renderInfo->x;
    textureInfo->m_y = renderInfo->y;
    textureInfo->m_scale = renderInfo->scale;
    textureInfo->m_painter = renderInfo->tilePainter;

    textureInfo->m_picture = renderInfo->textureInfo->m_pictureCount;
}

// Note: this need to be called within th m_transferQueueLock.
void TransferQueue::cleanupTransportQueue()
{
    // We should call updateTexImage to the items first,
    // then unlock. And we should start from the next item.
    const int nextItemIndex = getNextTransferQueueIndex();
    bool needUnlock[ST_BUFFER_NUMBER];
    int index = nextItemIndex;

    for (int i = 0 ; i < ST_BUFFER_NUMBER; i++) {
        needUnlock[index] = false;
        if (m_transferQueue[index].status == pendingDiscard) {
            m_sharedSurfaceTexture->updateTexImage();

            m_transferQueue[index].savedBaseTilePtr = 0;
            m_transferQueue[index].status == emptyItem;
            needUnlock[index] = true;
        }
        index = (index + 1) % ST_BUFFER_NUMBER;
    }
    index = nextItemIndex;
    for (int i = 0 ; i < ST_BUFFER_NUMBER; i++) {
        if (needUnlock[index])
            m_transferQueueItemLocks[index].unlock();
        index = (index + 1) % ST_BUFFER_NUMBER;
    }
}

void TransferQueue::saveGLState()
{
    glGetIntegerv(GL_VIEWPORT, m_GLStateBeforeBlit.viewport);
    glGetBooleanv(GL_SCISSOR_TEST, m_GLStateBeforeBlit.scissor);
    glGetBooleanv(GL_DEPTH_TEST, m_GLStateBeforeBlit.depth);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, m_GLStateBeforeBlit.clearColor);
}

void TransferQueue::setGLStateForCopy(int width, int height)
{
    // Need to match the texture size.
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
}

void TransferQueue::restoreGLState()
{
    glViewport(m_GLStateBeforeBlit.viewport[0],
               m_GLStateBeforeBlit.viewport[1],
               m_GLStateBeforeBlit.viewport[2],
               m_GLStateBeforeBlit.viewport[3]);

    if (m_GLStateBeforeBlit.scissor[0])
        glEnable(GL_SCISSOR_TEST);

    if (m_GLStateBeforeBlit.depth)
        glEnable(GL_DEPTH_TEST);

    glClearColor(m_GLStateBeforeBlit.clearColor[0],
                 m_GLStateBeforeBlit.clearColor[1],
                 m_GLStateBeforeBlit.clearColor[2],
                 m_GLStateBeforeBlit.clearColor[3]);
}

int TransferQueue::getNextTransferQueueIndex()
{
    return (m_transferQueueIndex + 1) % ST_BUFFER_NUMBER;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING
