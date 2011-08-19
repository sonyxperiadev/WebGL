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

#ifndef ZoomManager_h
#define ZoomManager_h

#if USE(ACCELERATED_COMPOSITING)

#include "SkRect.h"

namespace WebCore {

class GLWebViewState;

class ZoomManager {
public:
    enum GLScaleStates {
        kNoScaleRequest = 0,
        kWillScheduleRequest = 1,
        kRequestNewScale = 2,
        kReceivedNewScale = 3
    };
    typedef int32_t GLScaleState;

    ZoomManager(GLWebViewState* state);

    void scheduleUpdate(const double& currentTime, const SkIRect& viewport, float scale);

    GLScaleState scaleRequestState() const { return m_scaleRequestState; }
    void setScaleRequestState(GLScaleState state) { m_scaleRequestState = state; }
    float currentScale() const { return m_currentScale; }
    void setCurrentScale(float scale) { m_currentScale = scale; }
    float futureScale() const { return m_futureScale; }
    void setFutureScale(float scale) { m_futureScale = scale; }
    float layersScale() const { return m_layersScale; }
    double zoomInTransitionTime(double currentTime);
    double zoomOutTransitionTime(double currentTime);
    float zoomInTransparency(double currentTime);
    float zoomOutTransparency(double currentTime);
    void resetTransitionTime() { m_transitionTime = -1; }
    double updateTime() const { return m_updateTime; }
    void setUpdateTime(double value) { m_updateTime = value; }

    // state used by BaseLayerAndroid
    bool needPrepareNextTiledPage() { return m_prepareNextTiledPage; }
    bool zooming() { return m_zooming; }

    bool didFireRequest() { return m_scaleRequestState == ZoomManager::kRequestNewScale; }
    void setReceivedRequest() {
        m_scaleRequestState = ZoomManager::kReceivedNewScale;
        m_layersScale = m_futureScale;
    }
    bool didReceivedRequest() { return m_scaleRequestState == ZoomManager::kReceivedNewScale; }

    double transitionTime(double currentTime, float scale) {
        return (scale < m_currentScale) ? zoomOutTransitionTime(currentTime)
            : zoomInTransitionTime(currentTime);
    }
    void processTransition(double currentTime, float scale, bool* doSwap,
                            float* backPageTransparency, float* frontPageTransparency);

    bool swapPages();

    void processNewScale(double currentTime, float scale);

private:
    // Delay between scheduling a new page when the scale
    // factor changes (i.e. zooming in or out)
    static const double s_updateInitialDelay = 0.3; // 300 ms
    // If the scale factor continued to change and we completed
    // the original delay, we push back the update by this value
    static const double s_updateDelay = 0.1; // 100 ms

    // Delay for the transition between the two pages
    static const double s_zoomInTransitionDelay = 0.1; // 100 ms
    static const double s_invZoomInTransitionDelay = 10;
    static const double s_zoomOutTransitionDelay = 0.2; // 200 ms
    static const double s_invZoomOutTransitionDelay = 5;

    GLScaleState m_scaleRequestState;
    float m_currentScale;
    float m_futureScale;
    float m_layersScale;
    double m_updateTime;
    double m_transitionTime;

    bool m_prepareNextTiledPage;
    bool m_zooming;

    GLWebViewState* m_glWebViewState;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // ZoomManager_h
