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
#include "GLWebViewState.h"

#if USE(ACCELERATED_COMPOSITING)

#include "BaseLayerAndroid.h"
#include "ClassTracker.h"
#include "GLUtils.h"
#include "LayerAndroid.h"
#include "TilesManager.h"
#include <wtf/CurrentTime.h>

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "GLWebViewState", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "GLWebViewState", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

#define FIRST_TILED_PAGE_ID 1
#define SECOND_TILED_PAGE_ID 2

#define FRAMERATE_CAP 0.01666 // We cap at 60 fps

namespace WebCore {

using namespace android;

GLWebViewState::GLWebViewState(android::Mutex* buttonMutex)
    : m_scaleRequestState(kNoScaleRequest)
    , m_currentScale(1)
    , m_futureScale(1)
    , m_updateTime(-1)
    , m_transitionTime(-1)
    , m_baseLayer(0)
    , m_currentBaseLayer(0)
    , m_previouslyUsedRoot(0)
    , m_currentPictureCounter(0)
    , m_usePageA(true)
    , m_frameworkInval(0, 0, 0, 0)
    , m_frameworkLayersInval(0, 0, 0, 0)
    , m_globalButtonMutex(buttonMutex)
    , m_baseLayerUpdate(true)
    , m_backgroundColor(SK_ColorWHITE)
    , m_prevDrawTime(0)
    , m_displayRings(false)
    , m_focusRingTexture(-1)
{
    m_viewport.setEmpty();
    m_previousViewport.setEmpty();
    m_futureViewportTileBounds.setEmpty();
    m_viewportTileBounds.setEmpty();
    m_preZoomBounds.setEmpty();

    m_tiledPageA = new TiledPage(FIRST_TILED_PAGE_ID, this);
    m_tiledPageB = new TiledPage(SECOND_TILED_PAGE_ID, this);
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("GLWebViewState");
#endif
#ifdef MEASURES_PERF
    m_timeCounter = 0;
    m_totalTimeCounter = 0;
    m_measurePerfs = false;
#endif
}

GLWebViewState::~GLWebViewState()
{
    // We have to destroy the two tiled pages first as their destructor
    // may depend on the existence of this GLWebViewState and some of its
    // instance variables in order to complete.
    // Explicitely, currently we need to have the m_currentBaseLayer around
    // in order to complete any pending paint operations (the tiled pages
    // will remove any pending operations, and wait if one is underway).
    delete m_tiledPageA;
    delete m_tiledPageB;
    SkSafeUnref(m_currentBaseLayer);
    SkSafeUnref(m_baseLayer);
    m_baseLayer = 0;
    m_currentBaseLayer = 0;
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("GLWebViewState");
#endif
}

void GLWebViewState::setBaseLayer(BaseLayerAndroid* layer, const SkRegion& inval,
                                  bool showVisualIndicator, bool isPictureAfterFirstLayout)
{
    android::Mutex::Autolock lock(m_baseLayerLock);
    if (!layer || isPictureAfterFirstLayout) {
        m_tiledPageA->setUsable(false);
        m_tiledPageB->setUsable(false);
    }
    if (isPictureAfterFirstLayout) {
        m_baseLayerUpdate = true;
        m_invalidateRegion.setEmpty();
    }
    if (m_baseLayer && layer)
        m_baseLayer->swapExtra(layer);

    SkSafeRef(layer);
    SkSafeUnref(m_baseLayer);
    m_baseLayer = layer;
    if (m_baseLayer)
        m_baseLayer->setGLWebViewState(this);

    // We only update the layers if we are not currently
    // waiting for a tiledPage to be painted
    if (m_baseLayerUpdate) {
        SkSafeRef(layer);
        SkSafeUnref(m_currentBaseLayer);
        m_currentBaseLayer = layer;
    }
    m_displayRings = false;
    invalRegion(inval);

#ifdef MEASURES_PERF
    if (m_measurePerfs && !showVisualIndicator)
        dumpMeasures();
    m_measurePerfs = showVisualIndicator;
#endif

    TilesManager::instance()->setShowVisualIndicator(showVisualIndicator);
}

void GLWebViewState::setRings(Vector<IntRect>& rings, bool isPressed)
{
    android::Mutex::Autolock lock(m_baseLayerLock);
    m_displayRings = true;
    m_rings = rings;
    m_ringsIsPressed = isPressed;
}

void GLWebViewState::invalRegion(const SkRegion& region)
{
    SkRegion::Iterator iterator(region);
    while (!iterator.done()) {
        SkIRect r = iterator.rect();
        IntRect ir(r.fLeft, r.fTop, r.width(), r.height());
        inval(ir);
        iterator.next();
    }
}

void GLWebViewState::unlockBaseLayerUpdate() {
    if (m_baseLayerUpdate)
        return;

    m_baseLayerUpdate = true;
    android::Mutex::Autolock lock(m_baseLayerLock);
    SkSafeRef(m_baseLayer);
    SkSafeUnref(m_currentBaseLayer);
    m_currentBaseLayer = m_baseLayer;

    invalRegion(m_invalidateRegion);
    m_invalidateRegion.setEmpty();
}

void GLWebViewState::setExtra(BaseLayerAndroid* layer, SkPicture& picture,
    const IntRect& rect, bool allowSame)
{
    android::Mutex::Autolock lock(m_baseLayerLock);
    if (!m_baseLayerUpdate)
        return;

    layer->setExtra(picture);

    if (!allowSame && m_lastInval == rect)
        return;

    if (!rect.isEmpty())
        inval(rect);
    if (!m_lastInval.isEmpty())
        inval(m_lastInval);
    m_lastInval = rect;
    m_displayRings = false;
}

void GLWebViewState::inval(const IntRect& rect)
{
    if (m_baseLayerUpdate) {
        m_currentPictureCounter++;
        if (!rect.isEmpty()) {
            // find which tiles fall within the invalRect and mark them as dirty
            m_tiledPageA->invalidateRect(rect, m_currentPictureCounter);
            m_tiledPageB->invalidateRect(rect, m_currentPictureCounter);
            if (m_frameworkInval.isEmpty())
                m_frameworkInval = rect;
            else
                m_frameworkInval.unite(rect);
            XLOG("intermediate invalRect(%d, %d, %d, %d) after unite with rect %d %d %d %d", m_frameworkInval.x(),
                 m_frameworkInval.y(), m_frameworkInval.right(), m_frameworkInval.bottom(),
                 rect.x(), rect.y(), rect.right(), rect.bottom());
        }
    } else {
        m_invalidateRegion.op(rect.x(), rect.y(), rect.right(), rect.bottom(), SkRegion::kUnion_Op);
    }
}

void GLWebViewState::resetRings()
{
    m_displayRings = false;
}

void GLWebViewState::drawFocusRing(IntRect& srcRect)
{
    // TODO: use a 9-patch texture to draw the focus ring
    // instead of plain colors
    const float alpha = 0.3;
    float borderAlpha = 0.40;

    const int r = 104;
    const int g = 153;
    const int b = 255;

    int padding = 4;
    int border = 1;
    int fuzzyBorder = border * 2;
    if (!m_ringsIsPressed) {
        padding = 0;
        border = 2;
        fuzzyBorder = 3;
        borderAlpha = 0.2;
    }
    if (m_focusRingTexture == -1)
        m_focusRingTexture = GLUtils::createSampleColorTexture(r, g, b);

    SkRect rLeft, rTop, rRight, rBottom, rOverlay;

    IntRect rect(srcRect.x() - padding, srcRect.y() - padding,
                 srcRect.width() + (padding * 2), srcRect.height() + (padding * 2));
    rLeft.set(rect.x() - border, rect.y(),
              rect.x(), rect.y() + rect.height());
    rTop.set(rect.x() - border, rect.y() - border,
             rect.x() + rect.width() + border, rect.y());
    rRight.set(rect.x() + rect.width(), rect.y(),
               rect.x() + rect.width() + border,
               rect.y() + rect.height());
    rBottom.set(rect.x() - border, rect.y() + rect.height(),
                rect.x() + rect.width() + border,
                rect.y() + rect.height() + border);
    rOverlay.set(rect.x() - fuzzyBorder, rect.y() - fuzzyBorder,
              rect.x() + rect.width() + fuzzyBorder,
              rect.y() + rect.height() + fuzzyBorder);

    TilesManager::instance()->shader()->drawQuad(rLeft, m_focusRingTexture, borderAlpha);
    TilesManager::instance()->shader()->drawQuad(rTop, m_focusRingTexture, borderAlpha);
    TilesManager::instance()->shader()->drawQuad(rRight, m_focusRingTexture, borderAlpha);
    TilesManager::instance()->shader()->drawQuad(rBottom, m_focusRingTexture, borderAlpha);
    if (m_ringsIsPressed) {
        TilesManager::instance()->shader()->drawQuad(rOverlay, m_focusRingTexture, alpha);
    } else {
        rLeft.set(rect.x() - fuzzyBorder, rect.y(),
                  rect.x(), rect.y() + rect.height());
        rTop.set(rect.x() - fuzzyBorder, rect.y() - fuzzyBorder,
                 rect.x() + rect.width() + fuzzyBorder, rect.y());
        rRight.set(rect.x() + rect.width(), rect.y(),
                   rect.x() + rect.width() + fuzzyBorder,
                   rect.y() + rect.height());
        rBottom.set(rect.x() - fuzzyBorder, rect.y() + rect.height(),
                    rect.x() + rect.width() + fuzzyBorder,
                    rect.y() + rect.height() + fuzzyBorder);
        TilesManager::instance()->shader()->drawQuad(rLeft, m_focusRingTexture, alpha);
        TilesManager::instance()->shader()->drawQuad(rTop, m_focusRingTexture, alpha);
        TilesManager::instance()->shader()->drawQuad(rRight, m_focusRingTexture, alpha);
        TilesManager::instance()->shader()->drawQuad(rBottom, m_focusRingTexture, alpha);
    }
}

void GLWebViewState::paintExtras()
{
    if (m_displayRings) {
        // TODO: handles correctly the multi-rings case
        for (int i=0; i<m_rings.size(); i++) {
            IntRect rect = m_rings.at(i);
            drawFocusRing(rect);
        }
    }
}

unsigned int GLWebViewState::paintBaseLayerContent(SkCanvas* canvas)
{
    android::Mutex::Autolock lock(m_baseLayerLock);
    if (m_currentBaseLayer) {
        m_globalButtonMutex->lock();
        m_currentBaseLayer->drawCanvas(canvas);
        m_globalButtonMutex->unlock();
    }
    return m_currentPictureCounter;
}

void GLWebViewState::scheduleUpdate(const double& currentTime,
                                    const SkIRect& viewport, float scale)
{
    // if no update time, set it
    if (updateTime() == -1) {
        m_scaleRequestState = kWillScheduleRequest;
        setUpdateTime(currentTime + s_updateInitialDelay);
        setFutureScale(scale);
        setFutureViewport(viewport);
        return;
    }

    if (currentTime < updateTime())
        return;

    // we reached the scheduled update time, check if we can update
    if (futureScale() == scale) {
        // we are still with the previous scale, let's go
        // with the update
        m_scaleRequestState = kRequestNewScale;
        setUpdateTime(-1);
    } else {
        // we reached the update time, but the planned update was for
        // a different scale factor -- meaning the user is still probably
        // in the process of zooming. Let's push the update time a bit.
        setUpdateTime(currentTime + s_updateDelay);
        setFutureScale(scale);
        setFutureViewport(viewport);
    }
}

double GLWebViewState::zoomInTransitionTime(double currentTime)
{
    if (m_transitionTime == -1)
        m_transitionTime = currentTime + s_zoomInTransitionDelay;
    return m_transitionTime;
}

double GLWebViewState::zoomOutTransitionTime(double currentTime)
{
    if (m_transitionTime == -1)
        m_transitionTime = currentTime + s_zoomOutTransitionDelay;
    return m_transitionTime;
}


float GLWebViewState::zoomInTransparency(double currentTime)
{
    float t = zoomInTransitionTime(currentTime) - currentTime;
    t *= s_invZoomInTransitionDelay;
    return fmin(1, fmax(0, t));
}

float GLWebViewState::zoomOutTransparency(double currentTime)
{
    float t = zoomOutTransitionTime(currentTime) - currentTime;
    t *= s_invZoomOutTransitionDelay;
    return fmin(1, fmax(0, t));
}

TiledPage* GLWebViewState::sibling(TiledPage* page)
{
    return (page == m_tiledPageA) ? m_tiledPageB : m_tiledPageA;
}

TiledPage* GLWebViewState::frontPage()
{
    android::Mutex::Autolock lock(m_tiledPageLock);
    return m_usePageA ? m_tiledPageA : m_tiledPageB;
}

TiledPage* GLWebViewState::backPage()
{
    android::Mutex::Autolock lock(m_tiledPageLock);
    return m_usePageA ? m_tiledPageB : m_tiledPageA;
}

void GLWebViewState::swapPages()
{
    android::Mutex::Autolock lock(m_tiledPageLock);
    m_usePageA ^= true;
    TiledPage* working = m_usePageA ? m_tiledPageB : m_tiledPageA;
    if (m_scaleRequestState != kNoScaleRequest)
        TilesManager::instance()->resetTextureUsage(working);

    m_scaleRequestState = kNoScaleRequest;
}

int GLWebViewState::baseContentWidth()
{
    return m_currentBaseLayer ? m_currentBaseLayer->getWidth() : 0;

}
int GLWebViewState::baseContentHeight()
{
    return m_currentBaseLayer ? m_currentBaseLayer->getHeight() : 0;
}

void GLWebViewState::setViewport(SkRect& viewport, float scale)
{
    m_previousViewport = m_viewport;
    if ((m_viewport == viewport) &&
        (m_futureScale == scale))
        return;

    m_viewport = viewport;
    XLOG("New VIEWPORT %.2f - %.2f %.2f - %.2f (w: %2.f h: %.2f scale: %.2f currentScale: %.2f futureScale: %.2f)",
         m_viewport.fLeft, m_viewport.fTop, m_viewport.fRight, m_viewport.fBottom,
         m_viewport.width(), m_viewport.height(), scale, m_currentScale, m_futureScale);

    const float invTileContentWidth = scale / TilesManager::tileWidth();
    const float invTileContentHeight = scale / TilesManager::tileHeight();

    m_viewportTileBounds.set(
            static_cast<int>(floorf(viewport.fLeft * invTileContentWidth)),
            static_cast<int>(floorf(viewport.fTop * invTileContentHeight)),
            static_cast<int>(ceilf(viewport.fRight * invTileContentWidth)),
            static_cast<int>(ceilf(viewport.fBottom * invTileContentHeight)));

    int maxTextureCount = (m_viewportTileBounds.width() + TilesManager::instance()->expandedTileBoundsX() * 2 + 1) *
            (m_viewportTileBounds.height() + TilesManager::instance()->expandedTileBoundsY() * 2 + 1) * 2;
    TilesManager::instance()->setMaxTextureCount(maxTextureCount);
    m_tiledPageA->updateBaseTileSize();
    m_tiledPageB->updateBaseTileSize();
}

#ifdef MEASURES_PERF
void GLWebViewState::dumpMeasures()
{
    for (int i = 0; i < m_timeCounter; i++) {
        XLOGC("%d delay: %d ms", m_totalTimeCounter + i,
             static_cast<int>(m_delayTimes[i]*1000));
        m_delayTimes[i] = 0;
    }
    m_totalTimeCounter += m_timeCounter;
    m_timeCounter = 0;
}
#endif // MEASURES_PERF

void GLWebViewState::resetFrameworkInval()
{
    m_frameworkInval.setX(0);
    m_frameworkInval.setY(0);
    m_frameworkInval.setWidth(0);
    m_frameworkInval.setHeight(0);
}

void GLWebViewState::addDirtyArea(const IntRect& rect)
{
    if (rect.isEmpty())
        return;

    IntRect inflatedRect = rect;
    inflatedRect.inflate(8);
    if (m_frameworkLayersInval.isEmpty())
        m_frameworkLayersInval = inflatedRect;
    else
        m_frameworkLayersInval.unite(inflatedRect);
}

void GLWebViewState::resetLayersDirtyArea()
{
    m_frameworkLayersInval.setX(0);
    m_frameworkLayersInval.setY(0);
    m_frameworkLayersInval.setWidth(0);
    m_frameworkLayersInval.setHeight(0);
}

bool GLWebViewState::drawGL(IntRect& rect, SkRect& viewport, IntRect* invalRect,
                            IntRect& webViewRect, int titleBarHeight,
                            IntRect& clip, float scale, SkColor color)
{
    glFinish();

    double currentTime = WTF::currentTime();
    double delta = currentTime - m_prevDrawTime;

    if (delta < FRAMERATE_CAP) {
        unsigned int usecs = (FRAMERATE_CAP - delta) * 1E6;
        usleep(usecs);
    }

    m_prevDrawTime = currentTime;

    m_baseLayerLock.lock();
    BaseLayerAndroid* baseLayer = m_currentBaseLayer;
    SkSafeRef(baseLayer);
    BaseLayerAndroid* baseForComposited = m_baseLayer;
    SkSafeRef(baseForComposited);
    m_baseLayerLock.unlock();
    if (!baseLayer) {
        SkSafeUnref(baseForComposited);
        return false;
    }

    XLOG("drawGL, rect(%d, %d, %d, %d), viewport(%.2f, %.2f, %.2f, %.2f)",
         rect.x(), rect.y(), rect.right(), rect.bottom(),
         viewport.fLeft, viewport.fTop, viewport.fRight, viewport.fBottom);

    resetLayersDirtyArea();

    if (!baseForComposited ||
        (baseForComposited && !baseForComposited->countChildren())) {
        SkSafeRef(baseLayer);
        SkSafeUnref(baseForComposited);
        baseForComposited = baseLayer;
    }

    LayerAndroid* compositedRoot = 0;
    if (baseForComposited && baseForComposited->countChildren() >= 1)
        compositedRoot = static_cast<LayerAndroid*>(baseForComposited->getChild(0));

    bool ret = baseLayer->drawGL(compositedRoot, rect, viewport, webViewRect, titleBarHeight, clip, scale, color);
    m_previouslyUsedRoot = compositedRoot;
    if (ret) {
        FloatRect frameworkInval = TilesManager::instance()->shader()->rectInInvScreenCoord(m_frameworkInval);
        IntRect inval(frameworkInval.x(), frameworkInval.y(), frameworkInval.width(), frameworkInval.height());

        inval.unite(m_frameworkLayersInval);

        invalRect->setX(inval.x());
        invalRect->setY(inval.y());
        invalRect->setWidth(inval.width());
        invalRect->setHeight(inval.height());

        XLOG("invalRect(%d, %d, %d, %d)", inval.x(),
             inval.y(), inval.right(), inval.bottom());
    } else {
        resetFrameworkInval();
    }

#ifdef MEASURES_PERF
    if (m_measurePerfs) {
        m_delayTimes[m_timeCounter++] = delta;
        if (m_timeCounter >= MAX_MEASURES_PERF)
            dumpMeasures();
    }
#endif

    SkSafeUnref(baseForComposited);
    SkSafeUnref(baseLayer);
    return ret;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
