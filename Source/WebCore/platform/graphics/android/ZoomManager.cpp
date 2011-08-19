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
#include "ZoomManager.h"

#include "GLWebViewState.h"

#if USE(ACCELERATED_COMPOSITING)

#include <wtf/CurrentTime.h>

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "ZoomManager", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "ZoomManager", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

using namespace android;

ZoomManager::ZoomManager(GLWebViewState* state)
    : m_scaleRequestState(kNoScaleRequest)
    , m_currentScale(-1)
    , m_futureScale(-1)
    , m_layersScale(-1)
    , m_updateTime(-1)
    , m_transitionTime(-1)
    , m_glWebViewState(state)
{
}

void ZoomManager::scheduleUpdate(const double& currentTime,
                                 const SkIRect& viewport, float scale)
{
    // if no update time, set it
    if (updateTime() == -1) {
        m_scaleRequestState = kWillScheduleRequest;
        setUpdateTime(currentTime + s_updateInitialDelay);
        setFutureScale(scale);
        m_glWebViewState->setFutureViewport(viewport);
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
        m_glWebViewState->setFutureViewport(viewport);
    }
}

double ZoomManager::zoomInTransitionTime(double currentTime)
{
    if (m_transitionTime == -1)
        m_transitionTime = currentTime + s_zoomInTransitionDelay;
    return m_transitionTime;
}

double ZoomManager::zoomOutTransitionTime(double currentTime)
{
    if (m_transitionTime == -1)
        m_transitionTime = currentTime + s_zoomOutTransitionDelay;
    return m_transitionTime;
}

float ZoomManager::zoomInTransparency(double currentTime)
{
    float t = zoomInTransitionTime(currentTime) - currentTime;
    t *= s_invZoomInTransitionDelay;
    return fmin(1, fmax(0, t));
}

float ZoomManager::zoomOutTransparency(double currentTime)
{
    float t = zoomOutTransitionTime(currentTime) - currentTime;
    t *= s_invZoomOutTransitionDelay;
    return fmin(1, fmax(0, t));
}

bool ZoomManager::swapPages()
{
    bool reset = m_scaleRequestState != kNoScaleRequest;
    m_scaleRequestState = kNoScaleRequest;
    return reset;
}

void ZoomManager::processNewScale(double currentTime, float scale)
{
    m_prepareNextTiledPage = false;
    m_zooming = false;
    const SkIRect& viewportTileBounds = m_glWebViewState->viewportTileBounds();

    if (scale == m_currentScale
        || m_glWebViewState->preZoomBounds().isEmpty())
      m_glWebViewState->setPreZoomBounds(viewportTileBounds);

    // If we have a different scale than the current one, we have to
    // decide what to do. The current behaviour is to delay an update,
    // so that we do not slow down zooming unnecessarily.
    if ((m_currentScale != scale
        && (m_scaleRequestState == ZoomManager::kNoScaleRequest
            || m_futureScale != scale))
        || m_scaleRequestState == ZoomManager::kWillScheduleRequest) {

        // schedule the new Zoom request
        scheduleUpdate(currentTime, viewportTileBounds, scale);

        // If it's a new request, we will have to prepare the page.
        if (m_scaleRequestState == ZoomManager::kRequestNewScale)
            m_prepareNextTiledPage = true;
    }

    // If the viewport has changed since we scheduled the request, we also need
    // to prepare.
    if ((m_scaleRequestState == ZoomManager::kRequestNewScale
         || m_scaleRequestState == ZoomManager::kReceivedNewScale)
        && m_glWebViewState->futureViewport() != viewportTileBounds)
        m_prepareNextTiledPage = true;

    // Checking if we are zooming...
    if (m_scaleRequestState != ZoomManager::kNoScaleRequest) {
        m_prepareNextTiledPage = true;
        m_zooming = true;
    }

    // Get the current scale; if we are zooming, we don't change the scale
    // factor immediately (see BaseLayerAndroid::drawBasePictureInGL()), but
    // we change the scaleRequestState. When the state is kReceivedNewScale
    // (see setReceivedRequest()), we can use the future scale instead of
    // the current scale to request new textures. After a transition time,
    // the scaleRequestState will be reset and the current scale will be set
    // to the future scale.
    m_layersScale = m_currentScale;
}

void ZoomManager::processTransition(double currentTime, float scale,
                                     bool* doSwap, float* backPageTransparency,
                                     float* frontPageTransparency)
{
    if (scale < m_currentScale)
        *backPageTransparency = 1 - zoomOutTransparency(currentTime);
    else
        *frontPageTransparency = zoomInTransparency(currentTime);

    // The transition between the two page is finished
    if (currentTime > transitionTime(currentTime, scale)) {
        resetTransitionTime();
        *doSwap = true;
    }
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
