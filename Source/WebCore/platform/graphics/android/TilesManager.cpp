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
#include "TilesManager.h"

#if USE(ACCELERATED_COMPOSITING)

#include "BaseTile.h"
#include "PaintedSurface.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkPaint.h"
#include <android/native_window.h>
#include <cutils/atomic.h>
#include <gui/SurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>


#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "TilesManager", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "TilesManager", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

// Important: We need at least twice as many textures as is needed to cover
// one viewport, otherwise the allocation may stall.
// We need n textures for one TiledPage, and another n textures for the
// second page used when scaling.
// In our case, we use 256*256 textures. On the tablet, this equates to
// at least 60 textures, or 112 with expanded tile boundaries.
// 112(tiles)*256*256*4(bpp)*2(pages) = 56MB
// It turns out the viewport dependent value m_maxTextureCount is a reasonable
// number to cap the layer tile texturs, it worked on both phones and tablets.
// TODO: after merge the pool of base tiles and layer tiles, we should revisit
// the logic of allocation management.
#define MAX_TEXTURE_ALLOCATION ((6+TILE_PREFETCH_DISTANCE*2)*(5+TILE_PREFETCH_DISTANCE*2)*4)
#define TILE_WIDTH 256
#define TILE_HEIGHT 256
#define LAYER_TILE_WIDTH 256
#define LAYER_TILE_HEIGHT 256

#define BYTES_PER_PIXEL 4 // 8888 config

#define LAYER_TEXTURES_DESTROY_TIMEOUT 60 // If we do not need layers for 60 seconds, free the textures

namespace WebCore {

GLint TilesManager::getMaxTextureSize()
{
    static GLint maxTextureSize = 0;
    if (!maxTextureSize)
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    return maxTextureSize;
}

int TilesManager::getMaxTextureAllocation()
{
    return MAX_TEXTURE_ALLOCATION;
}

TilesManager::TilesManager()
    : m_layerTexturesRemain(true)
    , m_maxTextureCount(0)
    , m_maxLayerTextureCount(0)
    , m_generatorReady(false)
    , m_showVisualIndicator(false)
    , m_invertedScreen(false)
    , m_invertedScreenSwitch(false)
    , m_useMinimalMemory(true)
    , m_drawGLCount(1)
    , m_lastTimeLayersUsed(0)
    , m_hasLayerTextures(false)
{
    XLOG("TilesManager ctor");
    m_textures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    m_availableTextures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    m_tilesTextures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    m_availableTilesTextures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    m_pixmapsGenerationThread = new TexturesGenerator();
    m_pixmapsGenerationThread->run("TexturesGenerator", android::PRIORITY_BACKGROUND);
}

void TilesManager::allocateTiles()
{
    int nbTexturesToAllocate = m_maxTextureCount - m_textures.size();
    XLOG("%d tiles to allocate (%d textures planned)", nbTexturesToAllocate, m_maxTextureCount);
    int nbTexturesAllocated = 0;
    for (int i = 0; i < nbTexturesToAllocate; i++) {
        BaseTileTexture* texture = new BaseTileTexture(
            tileWidth(), tileHeight());
        // the atomic load ensures that the texture has been fully initialized
        // before we pass a pointer for other threads to operate on
        BaseTileTexture* loadedTexture =
            reinterpret_cast<BaseTileTexture*>(
            android_atomic_acquire_load(reinterpret_cast<int32_t*>(&texture)));
        m_textures.append(loadedTexture);
        nbTexturesAllocated++;
    }

    int nbLayersTexturesToAllocate = m_maxLayerTextureCount - m_tilesTextures.size();
    XLOG("%d layers tiles to allocate (%d textures planned)",
         nbLayersTexturesToAllocate, m_maxLayerTextureCount);
    int nbLayersTexturesAllocated = 0;
    for (int i = 0; i < nbLayersTexturesToAllocate; i++) {
        BaseTileTexture* texture = new BaseTileTexture(
            layerTileWidth(), layerTileHeight());
        // the atomic load ensures that the texture has been fully initialized
        // before we pass a pointer for other threads to operate on
        BaseTileTexture* loadedTexture =
            reinterpret_cast<BaseTileTexture*>(
            android_atomic_acquire_load(reinterpret_cast<int32_t*>(&texture)));
        m_tilesTextures.append(loadedTexture);
        nbLayersTexturesAllocated++;
    }
    XLOG("allocated %d textures for base (total: %d, %d Mb), %d textures for layers (total: %d, %d Mb)",
         nbTexturesAllocated, m_textures.size(),
         m_textures.size() * TILE_WIDTH * TILE_HEIGHT * 4 / 1024 / 1024,
         nbLayersTexturesAllocated, m_tilesTextures.size(),
         m_tilesTextures.size() * LAYER_TILE_WIDTH * LAYER_TILE_HEIGHT * 4 / 1024 / 1024);
}

void TilesManager::deallocateTextures(bool allTextures)
{
    const unsigned int max = m_textures.size();

    unsigned long long sparedDrawCount = ~0; // by default, spare no textures
    if (!allTextures) {
        // if we're not deallocating all textures, spare those with max drawcount
        sparedDrawCount = 0;
        for (unsigned int i = 0; i < max; i++) {
            TextureOwner* owner = m_textures[i]->owner();
            if (owner)
                sparedDrawCount = std::max(sparedDrawCount, owner->drawCount());
        }
    }
    deallocateTexturesVector(sparedDrawCount, m_textures);
    deallocateTexturesVector(sparedDrawCount, m_tilesTextures);
}

void TilesManager::deallocateTexturesVector(unsigned long long sparedDrawCount,
                                            WTF::Vector<BaseTileTexture*>& textures)
{
    const unsigned int max = textures.size();
    int dealloc = 0;
    for (unsigned int i = 0; i < max; i++) {
        TextureOwner* owner = textures[i]->owner();
        if (!owner || owner->drawCount() < sparedDrawCount) {
            textures[i]->discardGLTexture();
            dealloc++;
        }
    }
    XLOG("Deallocated %d gl textures (out of %d base tiles and %d layer tiles)",
         dealloc, max, maxLayer);
}

void TilesManager::gatherTexturesNumbers(int* nbTextures, int* nbAllocatedTextures,
                                        int* nbLayerTextures, int* nbAllocatedLayerTextures)
{
    *nbTextures = m_textures.size();
    for (unsigned int i = 0; i < m_textures.size(); i++) {
        BaseTileTexture* texture = m_textures[i];
        if (texture->m_ownTextureId)
            *nbAllocatedTextures += 1;
    }
    *nbLayerTextures = m_tilesTextures.size();
    for (unsigned int i = 0; i < m_tilesTextures.size(); i++) {
        BaseTileTexture* texture = m_tilesTextures[i];
        if (texture->m_ownTextureId)
            *nbAllocatedLayerTextures += 1;
    }
}

void TilesManager::printTextures()
{
#ifdef DEBUG
    XLOG("++++++");
    for (unsigned int i = 0; i < m_textures.size(); i++) {
        BaseTileTexture* texture = m_textures[i];
        BaseTile* o = 0;
        if (texture->owner())
            o = (BaseTile*) texture->owner();
        int x = -1;
        int y = -1;
        if (o) {
            x = o->x();
            y = o->y();
        }
        XLOG("[%d] texture %x  busy: %d owner: %x (%d, %d) page: %x scale: %.2f",
               i, texture,
               texture->busy(), o, x, y, o ? o->page() : 0, o ? o->scale() : 0);
    }
    XLOG("------");
#endif // DEBUG
}

void TilesManager::addPaintedSurface(PaintedSurface* surface)
{
    m_paintedSurfaces.append(surface);
}

void TilesManager::gatherTextures()
{
    android::Mutex::Autolock lock(m_texturesLock);
    m_availableTextures = m_textures;
}

void TilesManager::gatherLayerTextures()
{
    android::Mutex::Autolock lock(m_texturesLock);
    m_availableTilesTextures = m_tilesTextures;
    m_layerTexturesRemain = true;
}

BaseTileTexture* TilesManager::getAvailableTexture(BaseTile* owner)
{
    android::Mutex::Autolock lock(m_texturesLock);

    // Sanity check that the tile does not already own a texture
    if (owner->backTexture() && owner->backTexture()->owner() == owner) {
        XLOG("same owner (%d, %d), getAvailableBackTexture(%x) => texture %x",
             owner->x(), owner->y(), owner, owner->backTexture());
        if (owner->isLayerTile())
            m_availableTilesTextures.remove(m_availableTilesTextures.find(owner->backTexture()));
        else
            m_availableTextures.remove(m_availableTextures.find(owner->backTexture()));
        return owner->backTexture();
    }

    WTF::Vector<BaseTileTexture*>* availableTexturePool;
    if (owner->isLayerTile()) {
        availableTexturePool = &m_availableTilesTextures;
    } else {
        availableTexturePool = &m_availableTextures;
    }

    // The heuristic for selecting a texture is as follows:
    //  1. Skip textures currently being painted, they can't be painted while
    //         busy anyway
    //  2. If a tile isn't owned, break with that one
    //  3. Don't let tiles acquire their front textures
    //  4. If we find a tile in the same page with a different scale,
    //         it's old and not visible. Break with that one
    //  5. Otherwise, use the least recently prepared tile, but ignoring tiles
    //         drawn in the last frame to avoid flickering

    BaseTileTexture* farthestTexture = 0;
    unsigned long long oldestDrawCount = getDrawGLCount() - 1;
    const unsigned int max = availableTexturePool->size();
    for (unsigned int i = 0; i < max; i++) {
        BaseTileTexture* texture = (*availableTexturePool)[i];
        BaseTile* currentOwner = static_cast<BaseTile*>(texture->owner());

        if (texture->busy()) {
            // don't bother, since the acquire() will likely fail
            continue;
        }

        if (!currentOwner) {
            // unused texture! take it!
            farthestTexture = texture;
            break;
        }

        if (currentOwner == owner) {
            // Don't let a tile acquire its own front texture, as the
            // acquisition logic doesn't handle that
            continue;
        }

        if (currentOwner->painter() == owner->painter() && texture->scale() != owner->scale()) {
            // if we render the back page with one scale, then another while
            // still zooming, we recycle the tiles with the old scale instead of
            // taking ones from the front page
            farthestTexture = texture;
            break;
        }

        unsigned long long textureDrawCount = currentOwner->drawCount();
        if (oldestDrawCount > textureDrawCount) {
            farthestTexture = texture;
            oldestDrawCount = textureDrawCount;
        }
    }

    if (farthestTexture) {
        BaseTile* previousOwner = static_cast<BaseTile*>(farthestTexture->owner());
        if (farthestTexture->acquire(owner)) {
            if (previousOwner) {
                previousOwner->removeTexture(farthestTexture);

                XLOG("%s texture %p stolen from tile %d, %d for %d, %d, drawCount was %llu (now %llu)",
                     owner->isLayerTile() ? "LAYER" : "BASE",
                     farthestTexture, previousOwner->x(), previousOwner->y(),
                     owner->x(), owner->y(),
                     oldestDrawCount, getDrawGLCount());
            }

            availableTexturePool->remove(availableTexturePool->find(farthestTexture));
            return farthestTexture;
        }
    } else {
        if (owner->isLayerTile()) {
            // couldn't find a tile for a layer, layers shouldn't request redraw
            // TODO: once we do layer prefetching, don't set this for those
            // tiles
            m_layerTexturesRemain = false;
        }
    }

    XLOG("Couldn't find an available texture for %s tile %x (%d, %d) out of %d available",
          owner->isLayerTile() ? "LAYER" : "BASE",
          owner, owner->x(), owner->y(), max);
#ifdef DEBUG
    printTextures();
#endif // DEBUG
    return 0;
}

int TilesManager::maxTextureCount()
{
    android::Mutex::Autolock lock(m_texturesLock);
    return m_maxTextureCount;
}

int TilesManager::maxLayerTextureCount()
{
    android::Mutex::Autolock lock(m_texturesLock);
    return m_maxLayerTextureCount;
}

void TilesManager::setMaxTextureCount(int max)
{
    XLOG("setMaxTextureCount: %d (current: %d, total:%d)",
         max, m_maxTextureCount, MAX_TEXTURE_ALLOCATION);
    if (m_maxTextureCount == MAX_TEXTURE_ALLOCATION ||
         max <= m_maxTextureCount)
        return;

    android::Mutex::Autolock lock(m_texturesLock);

    if (max < MAX_TEXTURE_ALLOCATION)
        m_maxTextureCount = max;
    else
        m_maxTextureCount = MAX_TEXTURE_ALLOCATION;

    allocateTiles();
}

void TilesManager::setMaxLayerTextureCount(int max)
{
    XLOG("setMaxLayerTextureCount: %d (current: %d, total:%d)",
         max, m_maxLayerTextureCount, MAX_TEXTURE_ALLOCATION);
    if (!max && m_hasLayerTextures) {
        double secondsSinceLayersUsed = WTF::currentTime() - m_lastTimeLayersUsed;
        if (secondsSinceLayersUsed > LAYER_TEXTURES_DESTROY_TIMEOUT) {
            unsigned long long sparedDrawCount = ~0; // by default, spare no textures
            deallocateTexturesVector(sparedDrawCount, m_tilesTextures);
            m_hasLayerTextures = false;
        }
        return;
    }
    m_lastTimeLayersUsed = WTF::currentTime();
    if (m_maxLayerTextureCount == MAX_TEXTURE_ALLOCATION ||
         max <= m_maxLayerTextureCount)
        return;

    android::Mutex::Autolock lock(m_texturesLock);

    if (max < MAX_TEXTURE_ALLOCATION)
        m_maxLayerTextureCount = max;
    else
        m_maxLayerTextureCount = MAX_TEXTURE_ALLOCATION;

    allocateTiles();
    m_hasLayerTextures = true;
}


float TilesManager::tileWidth()
{
    return TILE_WIDTH;
}

float TilesManager::tileHeight()
{
    return TILE_HEIGHT;
}

float TilesManager::layerTileWidth()
{
    return LAYER_TILE_WIDTH;
}

float TilesManager::layerTileHeight()
{
    return LAYER_TILE_HEIGHT;
}

void TilesManager::paintedSurfacesCleanup(GLWebViewState* state)
{
    // PaintedSurfaces are created by LayerAndroid with a refcount of 1,
    // and just transferred to new (corresponding) layers when a new layer tree
    // is received.
    // PaintedSurface also keep a reference on the Layer it currently has, so
    // when we unref the tree of layer, those layers with a PaintedSurface will
    // still be around if we do nothing.
    // Here, if the surface does not have any associated layer, it means that we
    // received a new layer tree without a corresponding layer (i.e. a layer
    // using a texture has been removed by webkit).
    // In that case, we remove the PaintedSurface from our list, and unref it.
    // If the surface does have a layer, but the GLWebViewState associated to
    // that layer is different from the one passed in parameter, it means we can
    // also remove the surface (and we also remove/unref any layer that surface
    // has). We do this when we deallocate GLWebViewState (i.e. the webview has
    // been destroyed) and also when we switch to a page without
    // composited layers.

    WTF::Vector<PaintedSurface*> collect;
    for (unsigned int i = 0; i < m_paintedSurfaces.size(); i++) {
        PaintedSurface* surface = m_paintedSurfaces[i];

        Layer* drawing = surface->drawingLayer();
        Layer* painting = surface->paintingLayer();

        XLOG("considering PS %p, drawing %p, painting %p", surface, drawing, painting);

        bool drawingMatchesState = state && drawing && (drawing->state() == state);
        bool paintingMatchesState = state && painting && (painting->state() == state);

        if ((!painting && !drawing) || drawingMatchesState || paintingMatchesState) {
            XLOG("trying to remove PS %p, painting %p, drawing %p, DMS %d, PMS %d",
                 surface, painting, drawing, drawingMatchesState, paintingMatchesState);
            collect.append(surface);
        }
    }
    for (unsigned int i = 0; i < collect.size(); i++) {
        PaintedSurface* surface = collect[i];
        m_paintedSurfaces.remove(m_paintedSurfaces.find(surface));
        SkSafeUnref(surface);
    }
}

void TilesManager::unregisterGLWebViewState(GLWebViewState* state)
{
    // Discard the whole queue b/c we lost GL context already.
    // Note the real updateTexImage will still wait for the next draw.
    transferQueue()->discardQueue();
}

TilesManager* TilesManager::instance()
{
    if (!gInstance) {
        gInstance = new TilesManager();
        XLOG("instance(), new gInstance is %x", gInstance);
        XLOG("Waiting for the generator...");
        gInstance->waitForGenerator();
        XLOG("Generator ready!");
    }
    return gInstance;
}

TilesManager* TilesManager::gInstance = 0;

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
