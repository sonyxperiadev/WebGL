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
#include "ImageTexture.h"
#include "LayerAndroid.h"
#include "ShaderProgram.h"
#include "SkBitmapRef.h"
#include "TexturesGenerator.h"
#include "TiledPage.h"
#include "TilesProfiler.h"
#include "TilesTracker.h"
#include "TransferQueue.h"
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
    TransferQueue* transferQueue() { return &m_queue; }
    VideoLayerManager* videoLayerManager() { return &m_videoLayerManager; }

    void gatherLayerTextures();
    void gatherTextures();
    bool layerTexturesRemain() { return m_layerTexturesRemain; }
    void gatherTexturesNumbers(int* nbTextures, int* nbAllocatedTextures,
                               int* nbLayerTextures, int* nbAllocatedLayerTextures);

    BaseTileTexture* getAvailableTexture(BaseTile* owner);

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

    int maxTextureCount();
    int maxLayerTextureCount();
    void setMaxTextureCount(int max);
    void setMaxLayerTextureCount(int max);
    static float tileWidth();
    static float tileHeight();
    static float layerTileWidth();
    static float layerTileHeight();
    void paintedSurfacesCleanup(GLWebViewState* state = 0);
    void unregisterGLWebViewState(GLWebViewState* state);

    void allocateTiles();

    // Called when webview is hidden to discard graphics memory
    void deallocateTextures(bool allTextures);

    bool getShowVisualIndicator()
    {
        return m_showVisualIndicator;
    }

    void setShowVisualIndicator(bool showVisualIndicator)
    {
        m_showVisualIndicator = showVisualIndicator;
    }

    SharedTextureMode getSharedTextureMode()
    {
        return SurfaceTextureMode;
    }

    TilesProfiler* getProfiler()
    {
        return &m_profiler;
    }

    TilesTracker* getTilesTracker()
    {
        return &m_tilesTracker;
    }

    bool invertedScreen()
    {
        return m_invertedScreen;
    }

    bool invertedScreenSwitch()
    {
        return m_invertedScreenSwitch;
    }

    void setInvertedScreen(bool invert)
    {
        if (m_invertedScreen != invert)
            m_invertedScreenSwitch = true;
        m_invertedScreen = invert;
    }

    void setInvertedScreenSwitch(bool invertedSwitch)
    {
        m_invertedScreenSwitch = invertedSwitch;
    }

    void setInvertedScreenContrast(float contrast)
    {
        m_shader.setContrast(contrast);
    }

    void setUseMinimalMemory(bool useMinimalMemory)
    {
        m_useMinimalMemory = useMinimalMemory;
    }

    bool useMinimalMemory()
    {
        return m_useMinimalMemory;
    }

    void incDrawGLCount()
    {
        m_drawGLCount++;
    }

    unsigned long long getDrawGLCount()
    {
        return m_drawGLCount;
    }

    int getPaintedSurfaceCount()
    {
        return m_paintedSurfaces.size();
    }

private:
    TilesManager();

    void waitForGenerator()
    {
        android::Mutex::Autolock lock(m_generatorLock);
        while (!m_generatorReady)
            m_generatorReadyCond.wait(m_generatorLock);
    }

    void deallocateTexturesVector(unsigned long long sparedDrawCount,
                                  WTF::Vector<BaseTileTexture*>& textures);

    Vector<BaseTileTexture*> m_textures;
    Vector<BaseTileTexture*> m_availableTextures;

    Vector<BaseTileTexture*> m_tilesTextures;
    Vector<BaseTileTexture*> m_availableTilesTextures;
    bool m_layerTexturesRemain;

    Vector<PaintedSurface*> m_paintedSurfaces;

    int m_maxTextureCount;
    int m_maxLayerTextureCount;

    bool m_generatorReady;

    bool m_showVisualIndicator;
    bool m_invertedScreen;
    bool m_invertedScreenSwitch;

    bool m_useMinimalMemory;

    sp<TexturesGenerator> m_pixmapsGenerationThread;

    android::Mutex m_texturesLock;
    android::Mutex m_generatorLock;
    android::Condition m_generatorReadyCond;

    static TilesManager* gInstance;

    ShaderProgram m_shader;
    TransferQueue m_queue;

    VideoLayerManager m_videoLayerManager;

    TilesProfiler m_profiler;
    TilesTracker m_tilesTracker;
    unsigned long long m_drawGLCount;
    double m_lastTimeLayersUsed;
    bool m_hasLayerTextures;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TilesManager_h
