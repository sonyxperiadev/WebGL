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

GLWebViewState::GLWebViewState()
    : m_scaleRequestState(kNoScaleRequest)
    , m_currentScale(1)
    , m_futureScale(1)
    , m_updateTime(-1)
    , m_transitionTime(-1)
    , m_originalTilesPosX(0)
    , m_originalTilesPosY(0)
    , m_nbTilesWidth(0)
    , m_nbTilesHeight(0)
    , m_firstTileX(0)
    , m_firstTileY(0)
    , m_baseLayer(0)
    , m_currentPictureCounter(0)
    , m_usePageA(true)
    , m_extra(0)
    , m_navLayer(0)
{
    m_invalidatedRect.setEmpty();
    m_tiledPageA = new TiledPage(FIRST_TILED_PAGE_ID, this);
    m_tiledPageB = new TiledPage(SECOND_TILED_PAGE_ID, this);
#ifdef DEBUG_COUNT
    gGLWebViewStateCount++;
#endif
}

GLWebViewState::~GLWebViewState()
{
    delete m_tiledPageA;
    delete m_tiledPageB;
    delete m_navLayer;
#ifdef DEBUG_COUNT
    gGLWebViewStateCount--;
#endif
}

void GLWebViewState::setBaseLayer(BaseLayerAndroid* layer, IntRect& rect)
{
    android::Mutex::Autolock lock(m_baseLayerLock);
    m_baseLayer = layer;
    m_extra = 0;
    delete m_navLayer;
    m_navLayer = 0;
    if (m_baseLayer) {
        m_baseLayer->setGLWebViewState(this);
        m_invalidatedRect.set(rect);
        m_currentPictureCounter++;
    }
}

void GLWebViewState::setExtra(android::DrawExtra* extra, LayerAndroid* navLayer)
{
    android::Mutex::Autolock lock(m_baseLayerLock);
    m_extra = extra;
    delete m_navLayer;
    m_navLayer = navLayer;
    m_currentPictureCounter++;
}

void GLWebViewState::resetExtra(bool repaint)
{
    android::Mutex::Autolock lock(m_baseLayerLock);
    if (m_extra && repaint)
        m_currentPictureCounter++;
    m_extra = 0;
    delete m_navLayer;
    m_navLayer = 0;
}

void GLWebViewState::paintBaseLayerContent(SkCanvas* canvas)
{
    android::Mutex::Autolock lock(m_baseLayerLock);
    if (m_baseLayer) {
        m_baseLayer->drawCanvas(canvas);
        if (m_extra && m_navLayer)
            m_extra->draw(canvas, m_navLayer);
    }
}

void GLWebViewState::scheduleUpdate(const double& currentTime, float scale)
{
    // if no update time, set it
    if (updateTime() == -1) {
        m_scaleRequestState = kWillScheduleRequest;
        setUpdateTime(currentTime + s_updateInitialDelay);
        setFutureScale(scale);
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

void GLWebViewState::setViewport(SkRect& viewport, float scale)
{
    if (m_viewport == viewport)
        return;

    m_viewport = viewport;
    float fnbw = m_viewport.width() * scale / TilesManager::instance()->tileWidth();
    int nbw = static_cast<int>(ceilf(fnbw));
    float fnbh = m_viewport.height() * scale / TilesManager::instance()->tileHeight();
    int nbh = static_cast<int>(ceilf(fnbh));
    m_nbTilesWidth = nbw + 1;
    m_nbTilesHeight = nbh + 1;
    XLOG("New VIEWPORT %.2f - %.2f %.2f - %.2f (w: %2.f h: %.2f scale: %.2f), nbw: %d nbh: %d",
         m_viewport.fLeft, m_viewport.fTop, m_viewport.fRight, m_viewport.fBottom,
         m_viewport.width(), m_viewport.height(), scale,
         m_nbTilesWidth, m_nbTilesHeight);
    m_firstTileX = static_cast<int>(m_viewport.fLeft * scale / TilesManager::instance()->tileWidth());
    m_firstTileY = static_cast<int>(m_viewport.fTop * scale / TilesManager::instance()->tileHeight());
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
