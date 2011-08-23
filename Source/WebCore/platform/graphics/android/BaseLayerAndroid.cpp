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

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "BaseLayerAndroid", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

using namespace android;

BaseLayerAndroid::BaseLayerAndroid()
#if USE(ACCELERATED_COMPOSITING)
    : m_glWebViewState(0),
      m_color(Color::white)
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
bool BaseLayerAndroid::drawBasePictureInGL(SkRect& viewport, float scale,
                                           double currentTime, bool* pagesSwapped)
{
    ZoomManager* zoomManager = m_glWebViewState->zoomManager();

    bool goingDown = m_glWebViewState->goingDown();
    bool goingLeft = m_glWebViewState->goingLeft();

    const SkIRect& viewportTileBounds = m_glWebViewState->viewportTileBounds();
    XLOG("drawBasePicture, TX: %d, TY: %d scale %.2f", viewportTileBounds.fLeft,
            viewportTileBounds.fTop, scale);

    // Query the resulting state from the zoom manager
    bool prepareNextTiledPage = zoomManager->needPrepareNextTiledPage();
    bool zooming = zoomManager->zooming();

    // Display the current page
    TiledPage* tiledPage = m_glWebViewState->frontPage();
    TiledPage* nextTiledPage = m_glWebViewState->backPage();
    tiledPage->setScale(zoomManager->currentScale());

    // Let's prepare the page if needed
    if (prepareNextTiledPage) {
        nextTiledPage->setScale(scale);
        m_glWebViewState->setFutureViewport(viewportTileBounds);
        m_glWebViewState->lockBaseLayerUpdate();
        nextTiledPage->prepare(goingDown, goingLeft, viewportTileBounds, TiledPage::kVisibleBounds);
        // Cancel pending paints for the foreground page
        TilesManager::instance()->removePaintOperationsForPage(tiledPage, false);
    }

    // If we fired a request, let's check if it's ready to use
    if (zoomManager->didFireRequest()) {
        if (nextTiledPage->ready(viewportTileBounds, zoomManager->futureScale()))
            zoomManager->setReceivedRequest(); // transition to received request state
    }

    float transparency = 1;
    bool doSwap = false;

    // If the page is ready, display it. We do a short transition between
    // the two pages (current one and future one with the new scale factor)
    if (zoomManager->didReceivedRequest()) {
        float nextTiledPageTransparency = 1;
        zoomManager->processTransition(currentTime, scale, &doSwap,
                                       &nextTiledPageTransparency, &transparency);
        nextTiledPage->draw(nextTiledPageTransparency, viewportTileBounds);
    }

    const SkIRect& preZoomBounds = m_glWebViewState->preZoomBounds();

    bool needsRedraw = false;

    static bool waitOnScrollFinish = false;

    if (m_glWebViewState->isScrolling()) {
        if (!waitOnScrollFinish) {
            waitOnScrollFinish = true;

            //started scrolling, lock updates
            m_glWebViewState->lockBaseLayerUpdate();
        }
    } else {
        // wait until all tiles are rendered before anything else
        if (waitOnScrollFinish) {
            //wait for the page to finish rendering, then go into swap mode
            if (tiledPage->ready(preZoomBounds, zoomManager->currentScale())) {
                m_glWebViewState->resetFrameworkInval();
                m_glWebViewState->unlockBaseLayerUpdate();
                waitOnScrollFinish = false;
            }
            //should be prepared, simply draw
        }

        if (!waitOnScrollFinish) {
            //completed page post-scroll
            if (!tiledPage->ready(preZoomBounds, zoomManager->currentScale())) {
                m_glWebViewState->lockBaseLayerUpdate();
            }
        }
    }

    if (!prepareNextTiledPage || tiledPage->ready(preZoomBounds, zoomManager->currentScale()))
        tiledPage->prepare(goingDown, goingLeft, preZoomBounds, TiledPage::kExpandedBounds);
    tiledPage->draw(transparency, preZoomBounds);

    if (zoomManager->scaleRequestState() != ZoomManager::kNoScaleRequest
        || !tiledPage->ready(preZoomBounds, zoomManager->currentScale()))
        needsRedraw = true;

    if (doSwap) {
        zoomManager->setCurrentScale(scale);
        m_glWebViewState->swapPages();
        if (pagesSwapped)
            *pagesSwapped = true;
    }

    // if no longer trailing behind invalidates, unlock (so invalidates can
    // go directly to the the TiledPages without deferral)
    if (!needsRedraw && !waitOnScrollFinish)
        m_glWebViewState->unlockBaseLayerUpdate();

    m_glWebViewState->paintExtras();
    return needsRedraw;
}
#endif // USE(ACCELERATED_COMPOSITING)

bool BaseLayerAndroid::drawGL(double currentTime, LayerAndroid* compositedRoot,
                              IntRect& viewRect, SkRect& visibleRect, float scale,
                              bool* pagesSwapped)
{
    bool needsRedraw = false;
#if USE(ACCELERATED_COMPOSITING)

    needsRedraw = drawBasePictureInGL(visibleRect, scale, currentTime,
                                      pagesSwapped);

    if (!needsRedraw)
        m_glWebViewState->resetFrameworkInval();

    if (compositedRoot) {
        TransformationMatrix ident;

        bool animsRunning = compositedRoot->evaluateAnimations();
        if (animsRunning)
            needsRedraw = true;

        compositedRoot->updateFixedLayersPositions(visibleRect);
        FloatRect clip(0, 0, viewRect.width(), viewRect.height());
        compositedRoot->updateGLPositions(ident, clip, 1);
        SkMatrix matrix;
        matrix.setTranslate(viewRect.x(), viewRect.y());

        // get the scale factor from the zoom manager
        compositedRoot->setScale(m_glWebViewState->zoomManager()->layersScale());

#ifdef DEBUG
        compositedRoot->showLayer(0);
        XLOG("We have %d layers, %d textured",
              compositedRoot->nbLayers(),
              compositedRoot->nbTexturedLayers());
#endif

        // Clean up GL textures for video layer.
        TilesManager::instance()->videoLayerManager()->deleteUnusedTextures();

        if (compositedRoot->drawGL(m_glWebViewState, matrix))
            needsRedraw = true;
        else if (!animsRunning)
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
