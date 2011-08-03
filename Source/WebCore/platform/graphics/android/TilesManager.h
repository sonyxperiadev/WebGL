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

#ifndef TilesManager_h
#define TilesManager_h

#if USE(ACCELERATED_COMPOSITING)

#include "BaseTile.h"
#include "BaseTileTexture.h"
#include "LayerAndroid.h"
#include "ShaderProgram.h"
#include "TexturesGenerator.h"
#include "TiledPage.h"
#include "TilesProfiler.h"
#include "TilesTracker.h"
#include "VideoLayerManager.h"
#include <utils/threads.h>
#include <wtf/HashMap.h>

namespace WebCore {

class PaintedSurface;

class TilesManager {
public:
    static TilesManager* instance();
    static GLint getMaxTextureSize();
    static int getMaxTextureAllocation();

    static bool hardwareAccelerationEnabled()
    {
        return gInstance != 0;
    }

    void removeOperationsForFilter(OperationFilter* filter, bool waitForRunning = false)
    {
        m_pixmapsGenerationThread->removeOperationsForFilter(filter, waitForRunning);
    }

    void removeOperationsForPage(TiledPage* page)
    {
        m_pixmapsGenerationThread->removeOperationsForPage(page);
    }

    void removeOperationsForPainter(TilePainter* painter, bool waitForCompletion)
    {
        m_pixmapsGenerationThread->removeOperationsForPainter(painter, waitForCompletion);
    }

    void removePaintOperationsForPage(TiledPage* page, bool waitForCompletion)
    {
        m_pixmapsGenerationThread->removePaintOperationsForPage(page, waitForCompletion);
    }

    void scheduleOperation(QueuedOperation* operation)
    {
        m_pixmapsGenerationThread->scheduleOperation(operation);
    }

    void swapLayersTextures(LayerAndroid* newTree, LayerAndroid* oldTree);
    void addPaintedSurface(PaintedSurface* surface);

    ShaderProgram* shader() { return &m_shader; }
    VideoLayerManager* videoLayerManager() { return &m_videoLayerManager; }

    BaseTileTexture* getAvailableTexture(BaseTile* owner);

    void cleanupTilesTextures();

    void markGeneratorAsReady()
    {
        {
            android::Mutex::Autolock lock(m_generatorLock);
            m_generatorReady = true;
        }
        m_generatorReadyCond.signal();
    }

    void printTextures();

    void resetTextureUsage(TiledPage* page);

    int maxLayersAllocation();
    int maxLayerAllocation();
    int maxTextureCount();
    void setMaxTextureCount(int max);
    static float tileWidth();
    static float tileHeight();
    static float layerTileWidth();
    static float layerTileHeight();
    int expandedTileBoundsX();
    int expandedTileBoundsY();
    void registerGLWebViewState(GLWebViewState* state);
    void unregisterGLWebViewState(GLWebViewState* state);

    void allocateTiles();

    void setExpandedTileBounds(bool enabled) {
        m_expandedTileBounds = enabled;
    }

    bool getShowVisualIndicator() {
        return m_showVisualIndicator;
    }

    void setShowVisualIndicator(bool showVisualIndicator) {
        m_showVisualIndicator = showVisualIndicator;
    }

    SharedTextureMode getSharedTextureMode() {
        return SurfaceTextureMode;
    }

    TilesProfiler* getProfiler() {
        return &m_profiler;
    }

    TilesTracker* getTilesTracker() {
        return &m_tilesTracker;
    }

    bool invertedScreen() {
        return m_invertedScreen;
    }

    void setInvertedScreen(bool invert) {
        m_invertedScreen = invert;
    }

    void setInvertedScreenContrast(float contrast) {
        m_shader.setContrast(contrast);
    }

private:

    TilesManager();

    void waitForGenerator()
    {
        android::Mutex::Autolock lock(m_generatorLock);
        while (!m_generatorReady)
            m_generatorReadyCond.wait(m_generatorLock);
    }

    Vector<BaseTileTexture*> m_textures;
    Vector<BaseTileTexture*> m_tilesTextures;
    Vector<PaintedSurface*> m_paintedSurfaces;

    unsigned int m_layersMemoryUsage;

    int m_maxTextureCount;
    bool m_expandedTileBounds;

    bool m_generatorReady;

    bool m_showVisualIndicator;
    bool m_invertedScreen;

    sp<TexturesGenerator> m_pixmapsGenerationThread;

    android::Mutex m_texturesLock;
    android::Mutex m_generatorLock;
    android::Condition m_generatorReadyCond;

    static TilesManager* gInstance;

    ShaderProgram m_shader;
    VideoLayerManager m_videoLayerManager;

    HashMap<GLWebViewState*, unsigned int> m_glWebViewStateMap;
    unsigned int m_drawRegistrationCount;

    unsigned int getGLWebViewStateDrawCount(GLWebViewState* state);

    TilesProfiler m_profiler;
    TilesTracker m_tilesTracker;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TilesManager_h
