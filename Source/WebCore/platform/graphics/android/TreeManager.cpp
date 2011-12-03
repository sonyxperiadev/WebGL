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
#include "TreeManager.h"

#include "Layer.h"
#include "BaseLayerAndroid.h"
#include "ScrollableLayerAndroid.h"
#include "TilesManager.h"

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "TreeManager", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "TreeManager", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

TreeManager::TreeManager()
    : m_drawingTree(0)
    , m_paintingTree(0)
    , m_queuedTree(0)
    , m_fastSwapMode(false)
{
}

TreeManager::~TreeManager()
{
    clearTrees();
}

// the painting tree has finished painting:
//   discard the drawing tree
//   swap the painting tree in place of the drawing tree
//   and start painting the queued tree
void TreeManager::swap()
{
    // swap can't be called unless painting just finished
    ASSERT(m_paintingTree);

    android::Mutex::Autolock lock(m_paintSwapLock);

    XLOG("SWAPPING, D %p, P %p, Q %p", m_drawingTree, m_paintingTree, m_queuedTree);

    // if we have a drawing tree, discard it since the painting tree is done
    if (m_drawingTree) {
        XLOG("destroying drawing tree %p", m_drawingTree);
        m_drawingTree->setIsDrawing(false);
        SkSafeUnref(m_drawingTree);
    }

    // painting tree becomes the drawing tree
    XLOG("drawing tree %p", m_paintingTree);
    m_paintingTree->setIsDrawing(true);
    if (m_paintingTree->countChildren())
        static_cast<LayerAndroid*>(m_paintingTree->getChild(0))->initAnimations();

    if (m_queuedTree) {
        // start painting with the queued tree
        XLOG("now painting tree %p", m_queuedTree);
        m_queuedTree->setIsPainting(m_paintingTree);
    }
    m_drawingTree = m_paintingTree;
    m_paintingTree = m_queuedTree;
    m_queuedTree = 0;

    TilesManager::instance()->paintedSurfacesCleanup();

    XLOG("SWAPPING COMPLETE, D %p, P %p, Q %p", m_drawingTree, m_paintingTree, m_queuedTree);
}

// clear all of the content in the three trees held by the tree manager
void TreeManager::clearTrees()
{
    // remove painted surfaces from any tree in this view, and set trees as no
    // longer drawing, to clear ptrs from surfaces to layers
    GLWebViewState* oldState = 0;
    if (m_drawingTree) {
        oldState = m_drawingTree->state();
        m_drawingTree->setIsDrawing(false);
    }
    if (m_paintingTree) {
        oldState = m_paintingTree->state();
        m_paintingTree->setIsDrawing(false);
    }

    XLOG("TreeManager %p removing PS from state %p", this, oldState);
    TilesManager::instance()->paintedSurfacesCleanup(oldState);

    SkSafeUnref(m_drawingTree);
    m_drawingTree = 0;
    SkSafeUnref(m_paintingTree);
    m_paintingTree = 0;
    SkSafeUnref(m_queuedTree);
    m_queuedTree = 0;
}

// a new layer tree has arrived, queue it if we're painting something already,
// or start painting it if we aren't
void TreeManager::updateWithTree(Layer* newTree, bool brandNew)
{
    XLOG("updateWithTree - %p, has children %d, has animations %d",
         newTree, newTree && newTree->countChildren(),
         newTree && newTree->countChildren()
             ? static_cast<LayerAndroid*>(newTree->getChild(0))->hasAnimations() : 0);

    // can't have a queued tree unless have a painting tree too
    ASSERT(m_paintingTree || !m_queuedTree);

    SkSafeRef(newTree);

    android::Mutex::Autolock lock(m_paintSwapLock);

    if (!newTree || brandNew) {
        clearTrees();
        if (brandNew) {
            m_paintingTree = newTree;
            m_paintingTree->setIsPainting(m_drawingTree);
        }
        return;
    }

    if (m_queuedTree || m_paintingTree) {
        // currently painting, so defer this new tree
        if (m_queuedTree) {
            // have a queued tree, copy over invals so the regions are
            // eventually repainted
            m_queuedTree->mergeInvalsInto(newTree);

            XLOG("DISCARDING tree - %p, has children %d, has animations %d",
                 newTree, newTree && newTree->countChildren(),
                 newTree && newTree->countChildren()
                     ? static_cast<LayerAndroid*>(newTree->getChild(0))->hasAnimations() : 0);
        }
        SkSafeUnref(m_queuedTree);
        m_queuedTree = newTree;
        return;
    }

    // don't have painting tree, paint this one!
    m_paintingTree = newTree;
    m_paintingTree->setIsPainting(m_drawingTree);
}

void TreeManager::updateScrollableLayerInTree(Layer* tree, int layerId, int x, int y)
{
    LayerAndroid* layer;
    if (tree && tree->countChildren()) {
        layer = static_cast<LayerAndroid*>(tree->getChild(0))->findById(layerId);
        if (layer && layer->contentIsScrollable())
            static_cast<ScrollableLayerAndroid*>(layer)->scrollTo(x, y);
    }
}

void TreeManager::updateScrollableLayer(int layerId, int x, int y)
{
    updateScrollableLayerInTree(m_queuedTree, layerId, x, y);
    updateScrollableLayerInTree(m_paintingTree, layerId, x, y);
    updateScrollableLayerInTree(m_drawingTree, layerId, x, y);
}

bool TreeManager::drawGL(double currentTime, IntRect& viewRect,
                         SkRect& visibleRect, float scale,
                         bool enterFastSwapMode, bool* treesSwappedPtr, bool* newTreeHasAnimPtr,
                         TexturesResult* texturesResultPtr)
{
    m_fastSwapMode |= enterFastSwapMode;

    XLOG("drawGL, D %p, P %p, Q %p, fastSwap %d",
         m_drawingTree, m_paintingTree, m_queuedTree, m_fastSwapMode);

    bool ret = false;
    bool didTreeSwap = false;
    if (m_paintingTree) {
        XLOG("preparing painting tree %p", m_paintingTree);

        LayerAndroid* laTree = 0;
        if (m_paintingTree->countChildren()) {
            laTree = static_cast<LayerAndroid*>(m_paintingTree->getChild(0));
            ret |= laTree->evaluateAnimations(currentTime);
        }

        ret |= m_paintingTree->prepare(currentTime, viewRect,
                                       visibleRect, scale);

        if (laTree)
            laTree->computeTexturesAmount(texturesResultPtr);

        if (/*!m_fastSwapMode && */ m_paintingTree->isReady()) {
            XLOG("have painting tree %p ready, swapping!", m_paintingTree);
            didTreeSwap = true;
            swap();
            if (treesSwappedPtr)
                *treesSwappedPtr = true;
            if (laTree && newTreeHasAnimPtr)
                *newTreeHasAnimPtr = laTree->hasAnimations();
        }
    } else if (m_drawingTree) {
        XLOG("preparing drawing tree %p", m_drawingTree);
        ret |= m_drawingTree->prepare(currentTime, viewRect,
                                      visibleRect, scale);
        if (m_drawingTree->countChildren()) {
            LayerAndroid* laTree = static_cast<LayerAndroid*>(m_drawingTree->getChild(0));
            laTree->computeTexturesAmount(texturesResultPtr);
        }
    }


    if (m_drawingTree) {
        bool drawingReady = didTreeSwap || m_drawingTree->isReady();

        if (didTreeSwap || m_fastSwapMode || (drawingReady && !m_paintingTree))
            m_drawingTree->swapTiles();

        if (drawingReady) {
            // exit fast swap mode, as content is up to date
            m_fastSwapMode = false;
        } else {
            // drawing isn't ready, must redraw
            ret = true;
        }

        if (m_drawingTree->countChildren()) {
            LayerAndroid* laTree = static_cast<LayerAndroid*>(m_drawingTree->getChild(0));
            ret |= laTree->evaluateAnimations(currentTime);
        }
        XLOG("drawing tree %p", m_drawingTree);
        ret |= m_drawingTree->drawGL(viewRect, visibleRect, scale);
    } else if (m_paintingTree && m_paintingTree->state()) {
        // Dont have a drawing tree, draw white background
        Color defaultBackground = Color::white;
        m_paintingTree->state()->drawBackground(defaultBackground);
    }

    if (m_paintingTree) {
        XLOG("still have painting tree %p", m_paintingTree);
        return true;
    }

    return ret;
}

int TreeManager::getTotalPaintedSurfaceCount()
{
    return TilesManager::instance()->getPaintedSurfaceCount();
}

// draw for base tile - called on TextureGeneration thread
void TreeManager::drawCanvas(SkCanvas* canvas, bool drawLayers)
{
    BaseLayerAndroid* paintingTree = 0;
    m_paintSwapLock.lock();
    if (m_paintingTree)
        paintingTree = static_cast<BaseLayerAndroid*>(m_paintingTree);
    else
        paintingTree = static_cast<BaseLayerAndroid*>(m_drawingTree);
    SkSafeRef(paintingTree);
    m_paintSwapLock.unlock();

    if (!paintingTree)
        return;


    paintingTree->drawCanvas(canvas);

    if (drawLayers && paintingTree->countChildren()) {
        // draw the layers onto the canvas as well
        Layer* layers = paintingTree->getChild(0);
        static_cast<LayerAndroid*>(layers)->drawCanvas(canvas);
    }

    SkSafeUnref(paintingTree);
}

int TreeManager::baseContentWidth()
{
    if (m_paintingTree) {
        return static_cast<BaseLayerAndroid*>(m_paintingTree)->content()->width();
    } else if (m_drawingTree) {
        return static_cast<BaseLayerAndroid*>(m_drawingTree)->content()->width();
    }
    return 0;
}

int TreeManager::baseContentHeight()
{
    if (m_paintingTree) {
        return static_cast<BaseLayerAndroid*>(m_paintingTree)->content()->height();
    } else if (m_drawingTree) {
        return static_cast<BaseLayerAndroid*>(m_drawingTree)->content()->height();
    }
    return 0;
}

} // namespace WebCore
