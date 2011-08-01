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

// Important: We need at least twice as much textures as is needed to cover
// one viewport, otherwise the allocation may stall.
// We need n textures for one TiledPage, and another n textures for the
// second page used when scaling.
// In our case, we use 300x300 textures. On the tablet, this equates to
// at least 24 textures. That is consuming almost 50MB GPU memory.
// 24*300*300*4(bpp)*2(pages)*3(triple buffering in Surface Texture) = 49.4MB
// In order to avoid OOM issue, we limit the bounds number to 0 for now.
// TODO: We should either dynamically change the outer bound by detecting the
// HW limit or save further in the GPU memory consumption.
#define EXPANDED_TILE_BOUNDS_X 0
#define EXPANDED_TILE_BOUNDS_Y 0
#define MAX_TEXTURE_ALLOCATION 3+(6+EXPANDED_TILE_BOUNDS_X*2)*(4+EXPANDED_TILE_BOUNDS_Y*2)*2
#define TILE_WIDTH 256
#define TILE_HEIGHT 256
#define LAYER_TILE_WIDTH 256
#define LAYER_TILE_HEIGHT 256
#define LAYER_TILES 10

// Define a maximum amount of ram used by layers
#define MAX_LAYERS_ALLOCATION 33554432 // 32Mb
// Define a maximum amount of ram used by one layer
#define MAX_LAYER_ALLOCATION 8388608 // 8Mb
#define BYTES_PER_PIXEL 4 // 8888 config

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
    : m_layersMemoryUsage(0)
    , m_maxTextureCount(0)
    , m_expandedTileBounds(false)
    , m_generatorReady(false)
    , m_showVisualIndicator(false)
    , m_invertedScreen(false)
    , m_drawRegistrationCount(0)
{
    XLOG("TilesManager ctor");
    m_textures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    m_pixmapsGenerationThread = new TexturesGenerator();
    m_pixmapsGenerationThread->run("TexturesGenerator");
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

    for (int i = 0; i < LAYER_TILES; i++) {
        BaseTileTexture* texture = new BaseTileTexture(
            layerTileWidth(), layerTileHeight());
        // the atomic load ensures that the texture has been fully initialized
        // before we pass a pointer for other threads to operate on
        BaseTileTexture* loadedTexture =
            reinterpret_cast<BaseTileTexture*>(
            android_atomic_acquire_load(reinterpret_cast<int32_t*>(&texture)));
        m_tilesTextures.append(loadedTexture);
    }
    XLOG("allocated %d textures", nbTexturesAllocated);
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
        XLOG("[%d] texture %x usedLevel: %d busy: %d owner: %x (%d, %d) page: %x scale: %.2f",
               i, texture, texture->usedLevel(),
               texture->busy(), o, x, y, o ? o->page() : 0, o ? o->scale() : 0);
    }
    XLOG("------");
#endif // DEBUG
}

void TilesManager::resetTextureUsage(TiledPage* page)
{
    android::Mutex::Autolock lock(m_texturesLock);
    for (unsigned int i = 0; i < m_textures.size(); i++) {
        BaseTileTexture* texture = m_textures[i];
        TextureOwner* owner = texture->owner();
        if (owner) {
            if (owner->page() == page)
                texture->setUsedLevel(-1);
        }
    }
}

void TilesManager::swapLayersTextures(LayerAndroid* oldTree, LayerAndroid* newTree)
{
    if (newTree)
        newTree->assignTexture(oldTree);
}

void TilesManager::addPaintedSurface(PaintedSurface* surface)
{
    m_paintedSurfaces.append(surface);
}

void TilesManager::cleanupTilesTextures()
{
    // release existing surfaces without layers
    WTF::Vector<PaintedSurface*> collect;
    for (unsigned int i = 0; i < m_paintedSurfaces.size(); i++) {
        PaintedSurface* surface = m_paintedSurfaces[i];
        if (!surface->layer())
            collect.append(surface);
    }
    XLOG("remove %d / %d PaintedSurfaces", collect.size(), m_paintedSurfaces.size());
    for (unsigned int i = 0; i < collect.size(); i++) {
        m_paintedSurfaces.remove(m_paintedSurfaces.find(collect[i]));
        TilePainter* painter = collect[i]->texture();
        // Mark the current painter to destroy!!
        transferQueue()->m_currentRemovingPaint = painter;
        m_pixmapsGenerationThread->removeOperationsForPainter(painter, true);
        transferQueue()->m_currentRemovingPaint = 0;
        XLOG("destroy %x (%x)", collect[i], painter);
        delete collect[i];
    }
    for (unsigned int i = 0; i < m_tilesTextures.size(); i++) {
        BaseTileTexture* texture = m_tilesTextures[i];
        texture->setUsedLevel(-1);
        if (texture->owner()) {
            bool keep = false;
            for (unsigned int j = 0; j < m_paintedSurfaces.size(); j++) {
                if (m_paintedSurfaces[j]->owns(texture))
                    keep = true;
            }
            if (!keep) {
                texture->release(texture->owner());
            }
        }
    }
}

BaseTileTexture* TilesManager::getAvailableTexture(BaseTile* owner)
{
    android::Mutex::Autolock lock(m_texturesLock);

    // Sanity check that the tile does not already own a texture
    if (owner->texture() && owner->texture()->owner() == owner) {
        owner->texture()->setUsedLevel(0);
        XLOG("same owner (%d, %d), getAvailableTexture(%x) => texture %x",
             owner->x(), owner->y(), owner, owner->texture());
        return owner->texture();
    }

    if (owner->isLayerTile()) {
        unsigned int max = m_tilesTextures.size();
        for (unsigned int i = 0; i < max; i++) {
            BaseTileTexture* texture = m_tilesTextures[i];
            if (!texture->owner() && texture->acquire(owner)) {
                return texture;
            }
            if (texture->usedLevel() != 0 && texture->acquire(owner)) {
                return texture;
            }
            if (texture->scale() != owner->scale() && texture->acquire(owner)) {
                return texture;
            }
        }
        return 0;
    }

    // The heuristic for selecting a texture is as follows:
    //  1. If usedLevel == -1, break with that one
    //  2. Otherwise, select the highest usedLevel available
    //  3. Break ties with the lowest LRU(RecentLevel) valued GLWebViewState

    BaseTileTexture* farthestTexture = 0;
    int farthestTextureLevel = 0;
    unsigned int lowestDrawCount = ~0; //maximum uint
    const unsigned int max = m_textures.size();
    for (unsigned int i = 0; i < max; i++) {
        BaseTileTexture* texture = m_textures[i];

        if (texture->usedLevel() == -1) { // found an unused texture, grab it
            farthestTexture = texture;
            break;
        }

        int textureLevel = texture->usedLevel();
        unsigned int textureDrawCount = getGLWebViewStateDrawCount(texture->owner()->state());

        // if (higher distance or equal distance but less recently rendered)
        if (farthestTextureLevel < textureLevel
            || ((farthestTextureLevel == textureLevel) && (lowestDrawCount > textureDrawCount))) {
            farthestTexture = texture;
            farthestTextureLevel = textureLevel;
            lowestDrawCount = textureDrawCount;
        }
    }

    if (farthestTexture && farthestTexture->acquire(owner)) {
        XLOG("farthest texture, getAvailableTexture(%x) => texture %x (level %d, drawCount %d)",
             owner, farthestTexture, farthestTextureLevel, lowestDrawCount);
        farthestTexture->setUsedLevel(0);
        return farthestTexture;
    }

    XLOG("Couldn't find an available texture for BaseTile %x (%d, %d) !!!",
         owner, owner->x(), owner->y());
#ifdef DEBUG
    printTextures();
#endif // DEBUG
    return 0;
}

int TilesManager::maxLayersAllocation()
{
    return MAX_LAYERS_ALLOCATION;
}

int TilesManager::maxLayerAllocation()
{
    return MAX_LAYER_ALLOCATION;
}

int TilesManager::maxTextureCount()
{
    android::Mutex::Autolock lock(m_texturesLock);
    return m_maxTextureCount;
}

void TilesManager::setMaxTextureCount(int max)
{
    XLOG("setMaxTextureCount: %d (current: %d, total:%d)",
         max, m_maxTextureCount, MAX_TEXTURE_ALLOCATION);
    if (m_maxTextureCount &&
        (max > MAX_TEXTURE_ALLOCATION ||
         max <= m_maxTextureCount))
        return;

    android::Mutex::Autolock lock(m_texturesLock);

    if (max < MAX_TEXTURE_ALLOCATION)
        m_maxTextureCount = max;
    else
        m_maxTextureCount = MAX_TEXTURE_ALLOCATION;

    allocateTiles();
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

int TilesManager::expandedTileBoundsX() {
    return m_expandedTileBounds ? EXPANDED_TILE_BOUNDS_X : 0;
}

int TilesManager::expandedTileBoundsY() {
    return m_expandedTileBounds ? EXPANDED_TILE_BOUNDS_Y : 0;
}

void TilesManager::registerGLWebViewState(GLWebViewState* state)
{
    m_glWebViewStateMap.set(state, m_drawRegistrationCount);
    m_drawRegistrationCount++;
    XLOG("now state %p, total of %d states", state, m_glWebViewStateMap.size());
}

void TilesManager::unregisterGLWebViewState(GLWebViewState* state)
{
    // Discard the whole queue b/c we lost GL context already.
    // Note the real updateTexImage will still wait for the next draw.
    transferQueue()->discardQueue();

    m_glWebViewStateMap.remove(state);
    XLOG("state %p now removed, total of %d states", state, m_glWebViewStateMap.size());
}

unsigned int TilesManager::getGLWebViewStateDrawCount(GLWebViewState* state)
{
    XLOG("looking up state %p, contains=%s", state, m_glWebViewStateMap.contains(state) ? "TRUE" : "FALSE");
    return m_glWebViewStateMap.find(state)->second;
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
