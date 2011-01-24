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
#include "LayerAndroid.h"
#include "TilesManager.h"

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "GLWebViewState", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

#define FIRST_TILED_PAGE_ID 1
#define SECOND_TILED_PAGE_ID 2

namespace WebCore {

using namespace android;

#ifdef DEBUG_COUNT
static int gGLWebViewStateCount = 0;
int GLWebViewState::count()
{
    return gGLWebViewStateCount;
}
#endif

GLWebViewState::GLWebViewState(android::Mutex* buttonMutex)
    : m_scaleRequestState(kNoScaleRequest)
    , m_currentScale(1)
    , m_futureScale(1)
    , m_updateTime(-1)
    , m_transitionTime(-1)
    , m_baseLayer(0)
    , m_currentBaseLayer(0)
    , m_currentPictureCounter(0)
    , m_usePageA(true)
    , m_globalButtonMutex(buttonMutex)
    , m_baseLayerUpdate(true)
{
    m_viewport.setEmpty();
    m_previousViewport.setEmpty();
    m_futureViewportTileBounds.setEmpty();
    m_viewportTileBounds.setEmpty();
    m_preZoomBounds.setEmpty();

    m_tiledPageA = new TiledPage(FIRST_TILED_PAGE_ID, this);
    m_tiledPageB = new TiledPage(SECOND_TILED_PAGE_ID, this);
#ifdef DEBUG_COUNT
    gGLWebViewStateCount++;
#endif
}

GLWebViewState::~GLWebViewState()
{
    m_currentBaseLayer->safeUnref();
    delete m_tiledPageA;
    delete m_tiledPageB;
#ifdef DEBUG_COUNT
    gGLWebViewStateCount--;
#endif
}

void GLWebViewState::setBaseLayer(BaseLayerAndroid* layer, const IntRect& rect)
{
    android::Mutex::Autolock lock(m_baseLayerLock);
    if (!layer) {
        m_tiledPageA->setUsable(false);
        m_tiledPageB->setUsable(false);
    }
    if (m_baseLayer && layer)
        m_baseLayer->swapExtra(layer);
    m_baseLayer = layer;
    if (m_baseLayer) {
        m_baseLayer->setGLWebViewState(this);
    }
    // We only update the layers if we are not currently
    // waiting for a tiledPage to be painted
    if (m_baseLayerUpdate) {
        m_currentBaseLayer->safeUnref();
        m_currentBaseLayer = layer;
        m_currentBaseLayer->safeRef();
        inval(rect);
    } else {
        m_invalidateRect.unite(rect);
    }
}

void GLWebViewState::unlockBaseLayerUpdate() {
    m_baseLayerUpdate = true;
    android::Mutex::Autolock lock(m_baseLayerLock);
    m_currentBaseLayer->safeUnref();
    m_currentBaseLayer = m_baseLayer;
    m_currentBaseLayer->safeRef();
    inval(m_invalidateRect);
    IntRect empty;
    m_invalidateRect = empty;
}

void GLWebViewState::setExtra(BaseLayerAndroid* layer, SkPicture& picture,
    const IntRect& rect)
{
    android::Mutex::Autolock lock(m_baseLayerLock);
    layer->setExtra(picture);
    if (!rect.isEmpty())
        inval(rect);
    else if (!m_lastInval.isEmpty())
        inval(m_lastInval);
    m_lastInval = rect;
}

void GLWebViewState::inval(const IntRect& rect)
{
    m_currentPictureCounter++;
    if (!rect.isEmpty()) {
        // find which tiles fall within the invalRect and mark them as dirty
        m_tiledPageA->invalidateRect(rect, m_currentPictureCounter);
        m_tiledPageB->invalidateRect(rect, m_currentPictureCounter);
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

double GLWebViewState::transitionTime(double currentTime)
{
    if (m_transitionTime == -1)
        m_transitionTime = currentTime + s_transitionDelay;
    return m_transitionTime;
}

float GLWebViewState::transparency(double currentTime)
{
    float t = transitionTime(currentTime) - currentTime;
    t *= s_invTransitionDelay;
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
    if (m_viewport == viewport)
        return;

    m_viewport = viewport;
    XLOG("New VIEWPORT %.2f - %.2f %.2f - %.2f (w: %2.f h: %.2f scale: %.2f)",
         m_viewport.fLeft, m_viewport.fTop, m_viewport.fRight, m_viewport.fBottom,
         m_viewport.width(), m_viewport.height(), scale);

    const float invTileContentWidth = scale / TilesManager::tileWidth();
    const float invTileContentHeight = scale / TilesManager::tileHeight();

    m_viewportTileBounds.set(
            static_cast<int>(floorf(viewport.fLeft * invTileContentWidth)),
            static_cast<int>(floorf(viewport.fTop * invTileContentHeight)),
            static_cast<int>(ceilf(viewport.fRight * invTileContentWidth)),
            static_cast<int>(ceilf(viewport.fBottom * invTileContentHeight)));
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
