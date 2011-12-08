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
#include "BaseLayerAndroid.h"

#if USE(ACCELERATED_COMPOSITING)
#include "ClassTracker.h"
#include "GLUtils.h"
#include "ShaderProgram.h"
#include "SkCanvas.h"
#include "TilesManager.h"
#include <GLES2/gl2.h>
#include <wtf/CurrentTime.h>
#endif // USE(ACCELERATED_COMPOSITING)

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "BaseLayerAndroid", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "BaseLayerAndroid", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

// TODO: dynamically determine based on DPI
#define PREFETCH_SCALE_MODIFIER 0.3
#define PREFETCH_OPACITY 1
#define PREFETCH_X_DIST 0
#define PREFETCH_Y_DIST 1

namespace WebCore {

using namespace android;

BaseLayerAndroid::BaseLayerAndroid()
#if USE(ACCELERATED_COMPOSITING)
    : m_color(Color::white)
    , m_scrollState(NotScrolling)
#endif
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("BaseLayerAndroid");
#endif
}

BaseLayerAndroid::~BaseLayerAndroid()
{
    m_content.clear();
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("BaseLayerAndroid");
#endif
}

void BaseLayerAndroid::setContent(const PictureSet& src)
{
#if USE(ACCELERATED_COMPOSITING)
    // FIXME: We lock here because we do not want
    // to paint and change the m_content concurrently.
    // We should instead refactor PictureSet to use
    // an atomic refcounting scheme and use atomic operations
    // to swap PictureSets.
    android::Mutex::Autolock lock(m_drawLock);
#endif
    m_content.set(src);
    // FIXME: We cannot set the size of the base layer because it will screw up
    // the matrix used.  We need to fix matrix computation for the base layer
    // and then we can set the size.
    // setSize(src.width(), src.height());
}

bool BaseLayerAndroid::drawCanvas(SkCanvas* canvas)
{
#if USE(ACCELERATED_COMPOSITING)
    android::Mutex::Autolock lock(m_drawLock);
#endif
    if (!m_content.isEmpty())
        m_content.draw(canvas);
    return true;
}

#if USE(ACCELERATED_COMPOSITING)

void BaseLayerAndroid::prefetchBasePicture(SkRect& viewport, float currentScale,
                                           TiledPage* prefetchTiledPage, bool draw)
{
    SkIRect bounds;
    float prefetchScale = currentScale * PREFETCH_SCALE_MODIFIER;

    float invTileWidth = (prefetchScale)
        / TilesManager::instance()->tileWidth();
    float invTileHeight = (prefetchScale)
        / TilesManager::instance()->tileHeight();
    bool goingDown = m_state->goingDown();
    bool goingLeft = m_state->goingLeft();


    XLOG("fetch rect %f %f %f %f, scale %f",
         viewport.fLeft,
         viewport.fTop,
         viewport.fRight,
         viewport.fBottom,
         scale);

    bounds.fLeft = static_cast<int>(floorf(viewport.fLeft * invTileWidth)) - PREFETCH_X_DIST;
    bounds.fTop = static_cast<int>(floorf(viewport.fTop * invTileHeight)) - PREFETCH_Y_DIST;
    bounds.fRight = static_cast<int>(ceilf(viewport.fRight * invTileWidth)) + PREFETCH_X_DIST;
    bounds.fBottom = static_cast<int>(ceilf(viewport.fBottom * invTileHeight)) + PREFETCH_Y_DIST;

    XLOG("prefetch rect %d %d %d %d, scale %f, preparing page %p",
         bounds.fLeft, bounds.fTop,
         bounds.fRight, bounds.fBottom,
         scale * PREFETCH_SCALE,
         prefetchTiledPage);

    prefetchTiledPage->setScale(prefetchScale);
    prefetchTiledPage->updateTileDirtiness(bounds);
    prefetchTiledPage->prepare(goingDown, goingLeft, bounds,
                               TiledPage::ExpandedBounds);
    prefetchTiledPage->swapBuffersIfReady(bounds,
                                          prefetchScale);
    if (draw)
        prefetchTiledPage->prepareForDrawGL(PREFETCH_OPACITY, bounds);
}

bool BaseLayerAndroid::isReady()
{
    ZoomManager* zoomManager = m_state->zoomManager();
    if (ZoomManager::kNoScaleRequest != zoomManager->scaleRequestState()) {
        XLOG("base layer not ready, still zooming");
        return false; // still zooming
    }

    if (!m_state->frontPage()->isReady(m_state->preZoomBounds())) {
        XLOG("base layer not ready, front page not done painting");
        return false;
    }

    LayerAndroid* compositedRoot = static_cast<LayerAndroid*>(getChild(0));
    if (compositedRoot) {
        XLOG("base layer is ready, how about children?");
        return compositedRoot->isReady();
    }

    return true;
}

void BaseLayerAndroid::swapTiles()
{
    if (countChildren())
        getChild(0)->swapTiles(); // TODO: move to parent impl

    m_state->frontPage()->swapBuffersIfReady(m_state->preZoomBounds(),
                                             m_state->zoomManager()->currentScale());

    m_state->backPage()->swapBuffersIfReady(m_state->preZoomBounds(),
                                            m_state->zoomManager()->currentScale());
}

void BaseLayerAndroid::setIsDrawing(bool isDrawing)
{
    if (countChildren())
        getChild(0)->setIsDrawing(isDrawing); // TODO: move to parent impl
}

void BaseLayerAndroid::setIsPainting(Layer* drawingTree)
{
    XLOG("BLA %p painting, dirty %d", this, isDirty());
    if (drawingTree)
        drawingTree = drawingTree->getChild(0);

    if (countChildren())
        getChild(0)->setIsPainting(drawingTree); // TODO: move to parent impl

    m_state->invalRegion(m_dirtyRegion);
    m_dirtyRegion.setEmpty();
}

void BaseLayerAndroid::mergeInvalsInto(Layer* replacementTree)
{
    XLOG("merging invals (empty=%d) from BLA %p to %p", m_dirtyRegion.isEmpty(), this, replacementTree);
    if (countChildren() && replacementTree->countChildren())
        getChild(0)->mergeInvalsInto(replacementTree->getChild(0));

    replacementTree->markAsDirty(m_dirtyRegion);
}

bool BaseLayerAndroid::prepareBasePictureInGL(SkRect& viewport, float scale,
                                              double currentTime)
{
    ZoomManager* zoomManager = m_state->zoomManager();

    bool goingDown = m_state->goingDown();
    bool goingLeft = m_state->goingLeft();

    const SkIRect& viewportTileBounds = m_state->viewportTileBounds();
    XLOG("drawBasePicture, TX: %d, TY: %d scale %.2f", viewportTileBounds.fLeft,
            viewportTileBounds.fTop, scale);

    // Query the resulting state from the zoom manager
    bool prepareNextTiledPage = zoomManager->needPrepareNextTiledPage();

    // Display the current page
    TiledPage* tiledPage = m_state->frontPage();
    TiledPage* nextTiledPage = m_state->backPage();
    tiledPage->setScale(zoomManager->currentScale());

    // Let's prepare the page if needed so that it will start painting
    if (prepareNextTiledPage) {
        nextTiledPage->setScale(scale);
        m_state->setFutureViewport(viewportTileBounds);

        // ignore dirtiness return value since while zooming we repaint regardless
        nextTiledPage->updateTileDirtiness(viewportTileBounds);

        nextTiledPage->prepare(goingDown, goingLeft, viewportTileBounds,
                               TiledPage::VisibleBounds);
        // Cancel pending paints for the foreground page
        TilesManager::instance()->removePaintOperationsForPage(tiledPage, false);
    }

    // If we fired a request, let's check if it's ready to use
    if (zoomManager->didFireRequest()) {
        if (nextTiledPage->swapBuffersIfReady(viewportTileBounds,
                                              zoomManager->futureScale()))
            zoomManager->setReceivedRequest(); // transition to received request state
    }

    float transparency = 1;
    bool doZoomPageSwap = false;

    // If the page is ready, display it. We do a short transition between
    // the two pages (current one and future one with the new scale factor)
    if (zoomManager->didReceivedRequest()) {
        float nextTiledPageTransparency = 1;
        m_state->resetFrameworkInval();
        zoomManager->processTransition(currentTime, scale, &doZoomPageSwap,
                                       &nextTiledPageTransparency, &transparency);
        nextTiledPage->prepareForDrawGL(nextTiledPageTransparency, viewportTileBounds);
    }

    const SkIRect& preZoomBounds = m_state->preZoomBounds();

    bool zooming = ZoomManager::kNoScaleRequest != zoomManager->scaleRequestState();

    if (doZoomPageSwap) {
        zoomManager->setCurrentScale(scale);
        m_state->swapPages();
    }

    bool needsRedraw = zooming;

    // if applied invals mark tiles dirty, need to redraw
    needsRedraw |= tiledPage->updateTileDirtiness(preZoomBounds);

    // paint what's needed unless we're zooming, since the new tiles won't
    // be relevant soon anyway
    if (!zooming)
        tiledPage->prepare(goingDown, goingLeft, preZoomBounds,
                           TiledPage::ExpandedBounds);

    XLOG("scrolling %d, zooming %d, needsRedraw %d",
         scrolling, zooming, needsRedraw);

    // prefetch in the nextTiledPage if unused by zooming (even if not scrolling
    // since we want the tiles to be ready before they're needed)
    bool usePrefetchPage = !zooming;
    nextTiledPage->setIsPrefetchPage(usePrefetchPage);
    if (usePrefetchPage) {
        // if the non-prefetch page isn't missing tiles, don't bother drawing
        // prefetch page
        bool drawPrefetchPage = tiledPage->hasMissingContent(preZoomBounds);
        prefetchBasePicture(viewport, scale, nextTiledPage, drawPrefetchPage);
    }

    tiledPage->prepareForDrawGL(transparency, preZoomBounds);

    return needsRedraw;
}

void BaseLayerAndroid::drawBasePictureInGL()
{
    m_state->backPage()->drawGL();
    m_state->frontPage()->drawGL();
}

#endif // USE(ACCELERATED_COMPOSITING)

void BaseLayerAndroid::updateLayerPositions(SkRect& visibleRect)
{
    LayerAndroid* compositedRoot = static_cast<LayerAndroid*>(getChild(0));
    TransformationMatrix ident;
    compositedRoot->updateFixedLayersPositions(visibleRect);
    FloatRect clip(0, 0, content()->width(), content()->height());
    compositedRoot->updateGLPositionsAndScale(
        ident, clip, 1, m_state->zoomManager()->layersScale());

#ifdef DEBUG
    compositedRoot->showLayer(0);
    XLOG("We have %d layers, %d textured",
         compositedRoot->nbLayers(),
         compositedRoot->nbTexturedLayers());
#endif
}

bool BaseLayerAndroid::prepare(double currentTime, IntRect& viewRect,
                               SkRect& visibleRect, float scale)
{
    XLOG("preparing BLA %p", this);

    // base layer is simply drawn in prepare, since there is always a base layer it doesn't matter
    bool needsRedraw = prepareBasePictureInGL(visibleRect, scale, currentTime);

    LayerAndroid* compositedRoot = static_cast<LayerAndroid*>(getChild(0));
    if (compositedRoot) {
        updateLayerPositions(visibleRect);

        XLOG("preparing BLA %p, root %p", this, compositedRoot);
        compositedRoot->prepare();
    }

    return needsRedraw;
}

bool BaseLayerAndroid::drawGL(IntRect& viewRect, SkRect& visibleRect,
                              float scale)
{
    XLOG("drawing BLA %p", this);

    // TODO: consider moving drawBackground outside of prepare (into tree manager)
    m_state->drawBackground(m_color);
    drawBasePictureInGL();

    bool needsRedraw = false;

#if USE(ACCELERATED_COMPOSITING)

    LayerAndroid* compositedRoot = static_cast<LayerAndroid*>(getChild(0));
    if (compositedRoot) {
        updateLayerPositions(visibleRect);
        // For now, we render layers only if the rendering mode
        // is kAllTextures or kClippedTextures
        if (compositedRoot->drawGL()) {
            if (TilesManager::instance()->layerTexturesRemain()) {
                // only try redrawing for layers if layer textures remain,
                // otherwise we'll repaint without getting anything done
                needsRedraw = true;
            }
        }
    }

#endif // USE(ACCELERATED_COMPOSITING)
#ifdef DEBUG
    ClassTracker::instance()->show();
#endif
    return needsRedraw;
}

} // namespace WebCore
