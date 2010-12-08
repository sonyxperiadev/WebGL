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
// In our case, we use 300x300 textures. On the tablet, this equals to
// at least 24 (6 * 4) textures, hence 48.
#define MAX_TEXTURE_ALLOCATION 48
#define TILE_WIDTH 300
#define TILE_HEIGHT 300

// Define a maximum amount of ram used by layers
#define MAX_LAYERS_ALLOCATION 20971520 // 20Mb
#define BYTES_PER_PIXEL 4 // 8888 config

namespace WebCore {

TilesManager::TilesManager()
    : m_layersMemoryUsage(0)
    , m_generatorReady(false)
{
    XLOG("TilesManager ctor");
    m_textures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    for (int i = 0; i < MAX_TEXTURE_ALLOCATION; i++) {
        BackedDoubleBufferedTexture* texture = new BackedDoubleBufferedTexture(
            tileWidth(), tileHeight());
        // the atomic load ensures that the texture has been fully initialized
        // before we pass a pointer for other threads to operate on
        m_textures.append(reinterpret_cast<BackedDoubleBufferedTexture*>(
            android_atomic_acquire_load(reinterpret_cast<int32_t*>(&texture))));
    }
    XLOG("TilesManager ctor");

    m_pixmapsGenerationThread = new TexturesGenerator();
    m_pixmapsGenerationThread->run("TexturesGenerator");
}

// Has to be run on the texture generation threads
void TilesManager::enableTextures()
{
    android::Mutex::Autolock lock(m_texturesLock);
    for (unsigned int i = 0; i < m_textures.size(); i++) {
        BackedDoubleBufferedTexture* texture = m_textures[i];
        texture->producerAcquireContext();
    }
    XLOG("enableTextures");
}

void TilesManager::printTextures()
{
#ifdef DEBUG
    XLOG("++++++");
    for (unsigned int i = 0; i < m_textures.size(); i++) {
        BackedDoubleBufferedTexture* texture = m_textures[i];
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

void TilesManager::paintTexturesDefault()
{
    android::Mutex::Autolock lock(m_texturesLock);
    for (unsigned int i = 0; i < m_textures.size(); i++) {
        for (int j = 0; j < 2; j++) {
            BackedDoubleBufferedTexture* texture = m_textures[i];
            TextureInfo* textureInfo = texture->producerLock();
            SkCanvas* canvas = texture->canvas();
#ifdef DEBUG
            if (j)
                canvas->drawARGB(255, 0, 0, 255);
            else
                canvas->drawARGB(255, 255, 0, 255);
            SkPaint paint;
            paint.setARGB(128, 255, 0, 0);
            paint.setStrokeWidth(3);
            canvas->drawLine(0, 0, tileWidth(), tileHeight(), paint);
            paint.setARGB(128, 0, 255, 0);
            canvas->drawLine(0, tileHeight(), tileWidth(), 0, paint);
            paint.setARGB(128, 0, 0, 255);
            canvas->drawLine(0, 0, tileWidth(), 0, paint);
            canvas->drawLine(tileWidth(), 0, tileWidth(), tileHeight(), paint);
#else
            canvas->drawARGB(255, 255, 255, 255);
#endif // DEBUG
            texture->producerUpdate(textureInfo);
        }
    }
}

void TilesManager::resetTextureUsage(TiledPage* page)
{
    android::Mutex::Autolock lock(m_texturesLock);
    for (unsigned int i = 0; i < m_textures.size(); i++) {
        BackedDoubleBufferedTexture* texture = m_textures[i];
        if (texture->owner()) {
            if (texture->owner()->page() == page)
                texture->setUsedLevel(-1);
        }
    }
}

BackedDoubleBufferedTexture* TilesManager::getAvailableTexture(BaseTile* owner)
{
    android::Mutex::Autolock lock(m_texturesLock);

    // Sanity check that the tile does not already own a texture
    if (owner->texture() && owner->texture()->owner() == owner) {
        owner->texture()->setUsedLevel(0);
        XLOG("same owner, getAvailableTexture(%x) => texture %x", owner, owner->texture());
        return owner->texture();
    }

    // The heuristic for selecting a texture is as follows:
    //  1. return an unused texture if one exists
    //  2. return the farthest texture from the viewport (from any tiled page)
    //  3. return any texture not used by the tile's page or the page's sibiling
    //
    // The texture level indicates a tiles closeness to the current viewport
    BackedDoubleBufferedTexture* farthestTexture = 0;
    int farthestTextureLevel = 0;
    const unsigned int max = m_textures.size();
    for (unsigned int i = 0; i < max; i++) {
        BackedDoubleBufferedTexture* texture = m_textures[i];
        if (texture->usedLevel() == -1) { // found an unused texture, grab it
            farthestTexture = texture;
            break;
        }
        if (farthestTextureLevel < texture->usedLevel()) {
            farthestTextureLevel = texture->usedLevel();
            farthestTexture = texture;
        }
    }
    if (farthestTexture && farthestTexture->acquire(owner)) {
        XLOG("farthest texture, getAvailableTexture(%x) => texture %x (level %d)",
             owner, farthestTexture, farthestTexture->usedLevel());
        farthestTexture->setUsedLevel(0);
        return farthestTexture;
    }

    // At this point, all textures are used or we failed to aquire the farthest
    // texture. Now let's just grab a texture not in use by either of the two
    // tiled pages associated with this view.
    TiledPage* currentPage = owner->page();
    TiledPage* nextPage = currentPage->sibling();
    for (unsigned int i = 0; i < max; i++) {
        BackedDoubleBufferedTexture* texture = m_textures[i];
        if (texture->owner()
            && texture->owner()->page() != currentPage
            && texture->owner()->page() != nextPage
            && texture->acquire(owner)) {
            XLOG("grab a texture that wasn't ours, (%x != %x) at %d => texture %x",
                 owner->page(), texture->owner()->page(), i, texture);
            texture->setUsedLevel(0);
            return texture;
        }
    }

    XLOG("Couldn't find an available texture for BaseTile %x (%d, %d) !!!",
         owner, owner->x(), owner->y());
#ifdef DEBUG
    printTextures();
#endif // DEBUG
    return 0;
}

LayerTexture* TilesManager::getExistingTextureForLayer(LayerAndroid* layer)
{
    android::Mutex::Autolock lock(m_texturesLock);
    for (unsigned int i = 0; i< m_layersTextures.size(); i++) {
        if (m_layersTextures[i]->id() != layer->uniqueId())
            continue;
        if (layer->getSize() != m_layersTextures[i]->getSize())
            continue;

        XLOG("return layer %d (%x) for tile %d (%x)",
             i, m_layersTextures[i],
             layer->uniqueId(), layer);

        if (m_layersTextures[i]->acquire(layer))
            return m_layersTextures[i];
    }
    return 0;
}

void TilesManager::printLayersTextures(const char* s)
{
#ifdef DEBUG
    for (unsigned int i = 0; i< m_layersTextures.size(); i++) {
        XLOG("[%d] %s, texture %x for layer %d, owner: %x", i, s, m_layersTextures[i],
             m_layersTextures[i]->id(), m_layersTextures[i]->owner());
    }
#endif
}

void TilesManager::cleanupLayersTextures(bool forceCleanup)
{
    android::Mutex::Autolock lock(m_texturesLock);
#ifdef DEBUG
    printLayersTextures("cleanup");
#endif
    for (unsigned int i = 0; i< m_layersTextures.size(); i++) {
        LayerTexture* texture = m_layersTextures[i];

        if (forceCleanup)
            texture->setOwner(0);

        if (!texture->owner()) {
            m_layersMemoryUsage -= (int) texture->getSize().fWidth
                * (int) texture->getSize().fHeight * BYTES_PER_PIXEL;
            m_layersTextures.remove(i);
            delete texture;
        }
    }
}

LayerTexture* TilesManager::createTextureForLayer(LayerAndroid* layer)
{
    int w = layer->getWidth();
    int h = layer->getHeight();
    int size = w * h * BYTES_PER_PIXEL;

    if (m_layersMemoryUsage + size > MAX_LAYERS_ALLOCATION)
        cleanupLayersTextures(true);

    android::Mutex::Autolock lock(m_texturesLock);
    LayerTexture* texture = new LayerTexture(w, h);
    texture->setId(layer->uniqueId());
    m_layersTextures.append(texture);
    texture->acquire(layer);
    m_layersMemoryUsage += size;
    return texture;
}

int TilesManager::maxTextureCount()
{
    return MAX_TEXTURE_ALLOCATION;
}

float TilesManager::tileWidth()
{
    return TILE_WIDTH;
}

float TilesManager::tileHeight()
{
    return TILE_HEIGHT;
}

TilesManager* TilesManager::instance()
{
    if (!gInstance) {
        gInstance = new TilesManager();
        XLOG("Waiting for the generator...");
        gInstance->waitForGenerator();
        XLOG("Generator ready!");
    }
    return gInstance;
}

TilesManager* TilesManager::gInstance = 0;

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
