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
#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkPaint.h"
#include <cutils/atomic.h>

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

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
#define TILE_WIDTH 300
#define TILE_HEIGHT 300

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

LayerTexture* TilesManager::getExistingTextureForLayer(LayerAndroid* layer,
                                                       const IntRect& rect,
                                                       bool any,
                                                       LayerTexture* texture)
{
    android::Mutex::Autolock lock(m_texturesLock);
    LayerTexture* best = 0;
    unsigned newestPictureUsed = 0;
    for (unsigned int i = 0; i< m_layersTextures.size(); i++) {
        if (m_layersTextures[i]->id() != layer->uniqueId())
            continue;
        if (!any && rect != m_layersTextures[i]->rect())
            continue;
        if (!any && layer->getScale() != m_layersTextures[i]->scale())
            continue;
        if (any && texture == m_layersTextures[i])
            continue;

        if (m_layersTextures[i]->ready()) {
            unsigned int pictureUsed = m_layersTextures[i]->pictureUsed();
            if (pictureUsed >= newestPictureUsed) {
                newestPictureUsed = pictureUsed;
                best = m_layersTextures[i];
            }
        }

        XLOG("return layer %d (%x) for tile %d (%x)",
             i, m_layersTextures[i],
             layer->uniqueId(), layer);
    }

    if (best && best->acquire(layer, any))
        return best;
    return 0;
}

void TilesManager::printLayersTextures(const char* s)
{
#ifdef DEBUG
    XLOG(">>> print layers textures (%s)", s);
    for (unsigned int i = 0; i< m_layersTextures.size(); i++) {
        XLOG("[%d] %s, texture %x for layer %d (w: %.2f, h: %.2f), owner: %x",
             i, s, m_layersTextures[i],
             m_layersTextures[i]->id(),
             m_layersTextures[i]->getSize().fWidth,
             m_layersTextures[i]->getSize().fHeight,
             m_layersTextures[i]->owner());
    }
    XLOG("<<< print layers textures (%s)", s);
#endif
}

void TilesManager::cleanupLayersTextures(LayerAndroid* layer, bool forceCleanup)
{
    android::Mutex::Autolock lock(m_texturesLock);
    Layer* rootLayer = 0;
    if (layer)
        rootLayer = layer->getRootLayer();
#ifdef DEBUG
    if (forceCleanup)
        XLOG("FORCE cleanup");
    XLOG("before cleanup, memory %d", m_layersMemoryUsage);
    printLayersTextures("before cleanup");
#endif
    for (unsigned int i = 0; i< m_layersTextures.size();) {
        LayerTexture* texture = m_layersTextures[i];

        if (forceCleanup && texture->owner()) {
            LayerAndroid* textureLayer =
                static_cast<LayerAndroid*>(texture->owner());
            if (textureLayer->getRootLayer() != rootLayer) {
                // We only want to force destroy layers
                // that are not used by the current page
                XLOG("force removing texture %x for layer %d",
                     texture, textureLayer->uniqueId());
                textureLayer->removeTexture(texture);
            }
        }

        // We only try to destroy textures that have no owners.
        // This could be due to:
        // 1) - the LayerAndroid dtor has been called (i.e. when swapping
        // a LayerAndroid tree with a new one)
        // 2) - or due to the above code, forcing a destroy.
        // If the texture has been forced to be released (case #2), it
        // could still be in use (in the middle of being painted). So we
        // need to check that's not the case by checking busy(). See
        // LayerAndroid::paintBitmapGL().
        if (!texture->owner() && !texture->busy()) {
            m_layersMemoryUsage -= (int) texture->getSize().fWidth
                * (int) texture->getSize().fHeight * BYTES_PER_PIXEL;
            m_layersTextures.remove(i);
            // We can destroy the texture. We first remove it from the textures
            // list, and then remove any queued drawing. At this point we know
            // the texture has been removed from the layer, and that it's not
            // busy, so it's safe to delete.
            m_pixmapsGenerationThread->removeOperationsForTexture(texture);
            XLOG("delete texture %x", texture);
            delete texture;
        } else {
            // only iterate if we don't delete (if we delete, no need to as we
            // remove the element from the array)
            i++;
        }
    }
#ifdef DEBUG
    printLayersTextures("after cleanup");
    XLOG("after cleanup, memory %d", m_layersMemoryUsage);
#endif
}

LayerTexture* TilesManager::createTextureForLayer(LayerAndroid* layer, const IntRect& rect)
{
    int w = rect.width() * layer->getScale();
    int h = rect.height() * layer->getScale();
    unsigned int size = w * h * BYTES_PER_PIXEL;

    // We will not allocate textures that:
    // 1) cannot be handled by the graphic card (maxTextureSize &
    // totalMaxTextureSize)
    // 2) will make us go past our texture limit (MAX_LAYERS_ALLOCATION)

    GLint maxTextureSize = getMaxTextureSize();
    unsigned totalMaxTextureSize = maxTextureSize * maxTextureSize * BYTES_PER_PIXEL;
    bool large = w > maxTextureSize || h > maxTextureSize || size > totalMaxTextureSize;
    XLOG("createTextureForLayer(%d) @scale %.2f => %d, %d (too large? %x)", layer->uniqueId(),
         layer->getScale(), w, h, large);

    // For now just return 0 if too large
    if (large)
        return 0;

    if (w == 0 || h == 0) // empty layer
        return 0;

    if (m_layersMemoryUsage + size > MAX_LAYERS_ALLOCATION)
        cleanupLayersTextures(layer, true);

    // If the cleanup can't achieve the goal, then don't create a layerTexture.
    if (m_layersMemoryUsage + size > MAX_LAYERS_ALLOCATION)
        return 0;

    LayerTexture* texture = new LayerTexture(w, h);
    texture->setId(layer->uniqueId());
    texture->setRect(rect);
    texture->setScale(layer->getScale());

    android::Mutex::Autolock lock(m_texturesLock);
    m_layersTextures.append(texture);
    texture->acquire(layer);
    m_layersMemoryUsage += size;
    return texture;
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
