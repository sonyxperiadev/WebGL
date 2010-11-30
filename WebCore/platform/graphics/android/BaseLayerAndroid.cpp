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

#ifdef DEBUG_COUNT
static int gBaseLayerAndroidCount = 0;
int BaseLayerAndroid::count()
{
    return gBaseLayerAndroidCount;
}
#endif

BaseLayerAndroid::BaseLayerAndroid()
#if USE(ACCELERATED_COMPOSITING)
    : m_glWebViewState(0),
      m_color(Color::white)
#endif
{
#ifdef DEBUG_COUNT
    gBaseLayerAndroidCount++;
#endif
}

BaseLayerAndroid::~BaseLayerAndroid()
{
    m_content.clear();
#ifdef DEBUG_COUNT
    gBaseLayerAndroidCount--;
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
    setSize(src.width(), src.height());
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
bool BaseLayerAndroid::drawBasePictureInGL(SkRect& viewport, float scale)
{
    if (m_content.isEmpty())
        return false;
    if (!m_glWebViewState)
        return false;

    double currentTime = WTF::currentTime();
    bool goingDown = m_previousVisible.fTop - viewport.fTop <= 0;
    bool goingLeft = m_previousVisible.fLeft - viewport.fLeft >= 0;

    m_glWebViewState->setViewport(viewport, scale);

    const SkIRect& viewportTileBounds = m_glWebViewState->viewportTileBounds();
    XLOG("drawBasePicture, TX: %d, TY: %d scale %.2f", viewportTileBounds.fLeft,
            viewportTileBounds.fTop, scale);

    if (scale == m_glWebViewState->currentScale()
        || m_glWebViewState->preZoomBounds().isEmpty())
        m_glWebViewState->setPreZoomBounds(viewportTileBounds);

    bool prepareNextTiledPage = false;
    // If we have a different scale than the current one, we have to
    // decide what to do. The current behaviour is to delay an update,
    // so that we do not slow down zooming unnecessarily.
    if (m_glWebViewState->currentScale() != scale
        && (m_glWebViewState->scaleRequestState() == GLWebViewState::kNoScaleRequest
            || m_glWebViewState->scaleRequestState() == GLWebViewState::kWillScheduleRequest
            || m_glWebViewState->futureScale() != scale)) {

        // schedule the new request
        m_glWebViewState->scheduleUpdate(currentTime, viewportTileBounds, scale);

        // If it's a new request, we will have to prepare the page.
        if (m_glWebViewState->scaleRequestState() == GLWebViewState::kRequestNewScale)
            prepareNextTiledPage = true;
    }

    // If the viewport has changed since we scheduled the request, we also need to prepare.
    if (((m_glWebViewState->scaleRequestState() == GLWebViewState::kRequestNewScale)
         || (m_glWebViewState->scaleRequestState() == GLWebViewState::kReceivedNewScale))
         && (m_glWebViewState->futureViewport() != viewportTileBounds))
        prepareNextTiledPage = true;

    // Let's prepare the page if needed
    if (prepareNextTiledPage) {
        TiledPage* nextTiledPage = m_glWebViewState->backPage();
        nextTiledPage->setScale(scale);
        m_glWebViewState->setFutureViewport(viewportTileBounds);
        nextTiledPage->prepare(goingDown, goingLeft, viewportTileBounds);
    }

    float transparency = 1;
    bool doSwap = false;

    // If we fired a request, let's check if it's ready to use
    if (m_glWebViewState->scaleRequestState() == GLWebViewState::kRequestNewScale) {
        TiledPage* nextTiledPage = m_glWebViewState->backPage();
        if (nextTiledPage->ready(viewportTileBounds))
            m_glWebViewState->setScaleRequestState(GLWebViewState::kReceivedNewScale);
    }

    // If the page is ready, display it. We do a short transition between
    // the two pages (current one and future one with the new scale factor)
    if (m_glWebViewState->scaleRequestState() == GLWebViewState::kReceivedNewScale) {
        TiledPage* nextTiledPage = m_glWebViewState->backPage();
        double transitionTime = m_glWebViewState->transitionTime(currentTime);
        transparency = m_glWebViewState->transparency(currentTime);

        float newTilesTransparency = 1;
        if (scale < m_glWebViewState->currentScale())
            newTilesTransparency = 1 - transparency;

        nextTiledPage->draw(newTilesTransparency, viewport, viewportTileBounds);

        // The transition between the two pages is finished, swap them
        if (currentTime > transitionTime) {
            m_glWebViewState->setCurrentScale(scale);
            m_glWebViewState->resetTransitionTime();
            doSwap = true;
        }
    }

    // Display the current page
    TiledPage* tiledPage = m_glWebViewState->frontPage();
    tiledPage->setScale(m_glWebViewState->currentScale());
    const SkIRect& preZoomBounds = m_glWebViewState->preZoomBounds();
    tiledPage->prepare(goingDown, goingLeft, preZoomBounds);
    tiledPage->draw(transparency, viewport, preZoomBounds);

    bool ret = false;
    if (m_glWebViewState->scaleRequestState() != GLWebViewState::kNoScaleRequest
        || !tiledPage->ready(preZoomBounds))
      ret = true;

    if (doSwap)
        m_glWebViewState->swapPages();

    return ret;
}
#endif // USE(ACCELERATED_COMPOSITING)

bool BaseLayerAndroid::drawGL(IntRect& viewRect, SkRect& visibleRect,
                              float scale, SkColor color)
{
    bool ret = false;
#if USE(ACCELERATED_COMPOSITING)
    int left = viewRect.x();
    int top = viewRect.y();
    int width = viewRect.width();
    int height = viewRect.height();
    XLOG("drawBasePicture drawGL() viewRect: %d, %d, %d, %d",
         left, top, width, height);

    glEnable(GL_SCISSOR_TEST);

    glScissor(left, top, width, height);
    if (!m_glWebViewState || !m_glWebViewState->hasContent()) {
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return true;
    }
    glClearColor((float)m_color.red() / 255.0,
                 (float)m_color.green() / 255.0,
                 (float)m_color.blue() / 255.0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glViewport(left, top, width, height);
    ShaderProgram* shader = TilesManager::instance()->shader();
    glUseProgram(shader->program());
    glUniform1i(shader->textureSampler(), 0);

    ret = drawBasePictureInGL(visibleRect, scale);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_previousVisible = visibleRect;

#ifdef DEBUG_COUNT
    XLOG("GLWebViewState(%d) DoubleBufferedTexture(%d) BaseTile(%d) TileSet(%d) TiledPage(%d)",
         GLWebViewState::count(), DoubleBufferedTexture::count(),
         BaseTile::count(), TileSet::count(), TiledPage::count());
#endif // DEBUG_COUNT

#endif // USE(ACCELERATED_COMPOSITING)
    return ret;
}

} // namespace WebCore
