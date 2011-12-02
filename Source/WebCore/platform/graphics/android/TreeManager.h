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

#ifndef TreeManager_h
#define TreeManager_h

#include "TestExport.h"
#include <utils/threads.h>
#include "PerformanceMonitor.h"

class Layer;
class SkRect;
class SkCanvas;

namespace WebCore {

class IntRect;
class TexturesResult;

class TEST_EXPORT TreeManager {
public:
    TreeManager();

    ~TreeManager();

    void updateWithTree(Layer* tree, bool brandNew);

    void updateScrollableLayer(int layerId, int x, int y);

    bool drawGL(double currentTime, IntRect& viewRect,
                SkRect& visibleRect, float scale,
                bool enterFastSwapMode, bool* treesSwappedPtr, bool* newTreeHasAnimPtr,
                TexturesResult* texturesResultPtr);

    void drawCanvas(SkCanvas* canvas, bool drawLayers);

    // used in debugging (to avoid exporting TilesManager symbols)
    static int getTotalPaintedSurfaceCount();

    int baseContentWidth();
    int baseContentHeight();

private:
    static void updateScrollableLayerInTree(Layer* tree, int layerId, int x, int y);

    void swap();
    void clearTrees();

    android::Mutex m_paintSwapLock;

    Layer* m_drawingTree;
    Layer* m_paintingTree;
    Layer* m_queuedTree;

    bool m_fastSwapMode;
    PerformanceMonitor m_perf;
};

} // namespace WebCore

#endif //#define TreeManager_h
