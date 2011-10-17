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
    : m_glWebViewState(0)
    , m_color(Color::white)
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

void BaseLayerAndroid::setExtra(SkPicture& src)
{
#if USE(ACCELERATED_COMPOSITING)
    android::Mutex::Autolock lock(m_drawLock);
#endif
    m_extra.swap(src);
}

void BaseLayerAndroid::drawCanvas(SkCanvas* canvas)
{
#if USE(ACCELERATED_COMPOSITING)
    android::Mutex::Autolock lock(m_drawLock);
#endif
    if (!m_content.isEmpty())
        m_content.draw(canvas);
    // TODO : replace with !m_extra.isEmpty() once such a call exists
    if (m_extra.width() > 0)
        m_extra.draw(canvas);
}

#if USE(ACCELERATED_COMPOSITING)

void BaseLayerAndroid::prefetchBasePicture(SkRect& viewport, float currentScale,
                                           TiledPage* prefetchTiledPage)
{
    SkIRect bounds;
    float prefetchScale = currentScale * PREFETCH_SCALE_MODIFIER;

    float invTileWidth = (prefetchScale)
        / TilesManager::instance()->tileWidth();
    float invTileHeight = (prefetchScale)
        / TilesManager::instance()->tileHeight();
    bool goingDown = m_glWebViewState->goingDown();
    bool goingLeft = m_glWebViewState->goingLeft();


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
                                          prefetchScale,
                                          TiledPage::SwapWhateverIsReady);
    prefetchTiledPage->draw(PREFETCH_OPACITY, bounds);
}

bool BaseLayerAndroid::drawBasePictureInGL(SkRect& viewport, float scale,
                                           double currentTime, bool* buffersSwappedPtr)
{
    ZoomManager* zoomManager = m_glWebViewState->zoomManager();

    bool goingDown = m_glWebViewState->goingDown();
    bool goingLeft = m_glWebViewState->goingLeft();

    const SkIRect& viewportTileBounds = m_glWebViewState->viewportTileBounds();
    XLOG("drawBasePicture, TX: %d, TY: %d scale %.2f", viewportTileBounds.fLeft,
            viewportTileBounds.fTop, scale);

    // Query the resulting state from the zoom manager
    bool prepareNextTiledPage = zoomManager->needPrepareNextTiledPage();

    // Display the current page
    TiledPage* tiledPage = m_glWebViewState->frontPage();
    TiledPage* nextTiledPage = m_glWebViewState->backPage();
    tiledPage->setScale(zoomManager->currentScale());

    // Let's prepare the page if needed so that it will start painting
    if (prepareNextTiledPage) {
        nextTiledPage->setScale(scale);
        m_glWebViewState->setFutureViewport(viewportTileBounds);
        m_glWebViewState->lockBaseLayerUpdate();

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
                                              zoomManager->futureScale(),
                                              TiledPage::SwapWholePage))
            zoomManager->setReceivedRequest(); // transition to received request state
    }

    float transparency = 1;
    bool doZoomPageSwap = false;

    // If the page is ready, display it. We do a short transition between
    // the two pages (current one and future one with the new scale factor)
    if (zoomManager->didReceivedRequest()) {
        float nextTiledPageTransparency = 1;
        zoomManager->processTransition(currentTime, scale, &doZoomPageSwap,
                                       &nextTiledPageTransparency, &transparency);
        nextTiledPage->draw(nextTiledPageTransparency, viewportTileBounds);
    }

    const SkIRect& preZoomBounds = m_glWebViewState->preZoomBounds();

    // update scrolling state machine by querying glwebviewstate - note that the
    // NotScrolling state is only set below
    if (m_glWebViewState->isScrolling())
        m_scrollState = Scrolling;
    else if (m_scrollState == Scrolling)
        m_scrollState = ScrollingFinishPaint;

    bool scrolling = m_scrollState != NotScrolling;
    bool zooming = ZoomManager::kNoScaleRequest != zoomManager->scaleRequestState();

    // When we aren't zooming, we should TRY and swap tile buffers if they're
    // ready. When scrolling, we swap whatever's ready. Otherwise, buffer until
    // the entire page is ready and then swap.
    bool buffersSwapped = false;
    if (!zooming) {
        TiledPage::SwapMethod swapMethod;
        if (scrolling)
            swapMethod = TiledPage::SwapWhateverIsReady;
        else
            swapMethod = TiledPage::SwapWholePage;

        buffersSwapped = tiledPage->swapBuffersIfReady(preZoomBounds,
                                                       zoomManager->currentScale(),
                                                       swapMethod);

        if (buffersSwappedPtr && buffersSwapped)
            *buffersSwappedPtr = true;
        if (buffersSwapped) {
            if (m_scrollState == ScrollingFinishPaint) {
                m_scrollState = NotScrolling;
                scrolling = false;
            }
        }
    }

    if (doZoomPageSwap) {
        zoomManager->setCurrentScale(scale);
        m_glWebViewState->swapPages();
        if (buffersSwappedPtr)
            *buffersSwappedPtr = true;
    }


    bool needsRedraw = scrolling || zooming || !buffersSwapped;

    // if we don't expect to redraw, unlock the invals
    if (!needsRedraw)
        m_glWebViewState->unlockBaseLayerUpdate();

    // if applied invals mark tiles dirty, need to redraw
    needsRedraw |= tiledPage->updateTileDirtiness(preZoomBounds);

    if (needsRedraw) {
        // lock and paint what's needed unless we're zooming, since the new
        // tiles won't be relevant soon anyway
        m_glWebViewState->lockBaseLayerUpdate();
        if (!zooming)
            tiledPage->prepare(goingDown, goingLeft, preZoomBounds,
                               TiledPage::ExpandedBounds);
    }

    XLOG("scrolling %d, zooming %d, buffersSwapped %d, needsRedraw %d",
         scrolling, zooming, buffersSwapped, needsRedraw);

    // prefetch in the nextTiledPage if unused by zooming (even if not scrolling
    // since we want the tiles to be ready before they're needed)
    bool usePrefetchPage = !zooming;
    nextTiledPage->setIsPrefetchPage(usePrefetchPage);
    if (usePrefetchPage)
        prefetchBasePicture(viewport, scale, nextTiledPage);

    tiledPage->draw(transparency, preZoomBounds);

    return needsRedraw;
}
#endif // USE(ACCELERATED_COMPOSITING)

bool BaseLayerAndroid::drawGL(double currentTime, LayerAndroid* compositedRoot,
                              IntRect& viewRect, SkRect& visibleRect, float scale,
                              bool* buffersSwappedPtr)
{
    bool needsRedraw = false;
#if USE(ACCELERATED_COMPOSITING)

    needsRedraw = drawBasePictureInGL(visibleRect, scale, currentTime,
                                      buffersSwappedPtr);

    if (!needsRedraw)
        m_glWebViewState->resetFrameworkInval();

    if (compositedRoot) {
        TransformationMatrix ident;

        bool animsRunning = compositedRoot->evaluateAnimations();
        if (animsRunning)
            needsRedraw = true;

        compositedRoot->updateFixedLayersPositions(visibleRect);
        FloatRect clip(0, 0, viewRect.width(), viewRect.height());
        compositedRoot->updateGLPositionsAndScale(
            ident, clip, 1, m_glWebViewState->zoomManager()->layersScale());
        SkMatrix matrix;
        matrix.setTranslate(viewRect.x(), viewRect.y());

#ifdef DEBUG
        compositedRoot->showLayer(0);
        XLOG("We have %d layers, %d textured",
              compositedRoot->nbLayers(),
              compositedRoot->nbTexturedLayers());
#endif

        // Clean up GL textures for video layer.
        TilesManager::instance()->videoLayerManager()->deleteUnusedTextures();

        compositedRoot->prepare(m_glWebViewState);
        if (compositedRoot->drawGL(m_glWebViewState, matrix)) {
            if (TilesManager::instance()->layerTexturesRemain()) {
                // only try redrawing for layers if layer textures remain,
                // otherwise we'll repaint without getting anything done
                needsRedraw = true;
            }
        } else if (!animsRunning)
            m_glWebViewState->resetLayersDirtyArea();

    }

    m_previousVisible = visibleRect;

#endif // USE(ACCELERATED_COMPOSITING)
#ifdef DEBUG
    ClassTracker::instance()->show();
#endif
    return needsRedraw;
}

} // namespace WebCore
