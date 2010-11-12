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
}

void BaseLayerAndroid::drawCanvas(SkCanvas* canvas)
{
#if USE(ACCELERATED_COMPOSITING)
    android::Mutex::Autolock lock(m_drawLock);
#endif
    if (!m_content.isEmpty())
        m_content.draw(canvas);
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

    int firstTileX = m_glWebViewState->firstTileX();
    int firstTileY = m_glWebViewState->firstTileY();

    XLOG("drawBasePicture, TX: %d, TY: %d scale %.2f", firstTileX, firstTileY, scale);
    if (scale == m_glWebViewState->currentScale()) {
        m_glWebViewState->setOriginalTilesPosX(firstTileX);
        m_glWebViewState->setOriginalTilesPosY(firstTileY);
    }

    // If we have a different scale than the current one, we have to
    // decide what to do. The current behaviour is to delay an update,
    // so that we do not slow down zooming unnecessarily.
    if (m_glWebViewState->currentScale() != scale
        && (m_glWebViewState->scaleRequestState() == GLWebViewState::kNoScaleRequest
            || m_glWebViewState->scaleRequestState() == GLWebViewState::kWillScheduleRequest
            || m_glWebViewState->futureScale() != scale)) {
        m_glWebViewState->scheduleUpdate(currentTime, scale);
    }

    float transparency = 1;
    bool doSwap = false;

    // Here we have to schedule the painting of the tiles corresponding
    // to the new tiles for the new scale factor (see above)
    if (m_glWebViewState->scaleRequestState() == GLWebViewState::kRequestNewScale
        || m_glWebViewState->scaleRequestState() == GLWebViewState::kReceivedNewScale) {
        TiledPage* nextTiledPage = m_glWebViewState->backPage();
        nextTiledPage->setScale(scale);

        // Check if the page is ready...
        if (m_glWebViewState->scaleRequestState() == GLWebViewState::kRequestNewScale
            && nextTiledPage->ready(firstTileX, firstTileY)) {
            m_glWebViewState->setScaleRequestState(GLWebViewState::kReceivedNewScale);
        }

        // If the page is ready, display it. We do a short transition between
        // the two pages (current one and future one with the new scale factor)
        if (m_glWebViewState->scaleRequestState() == GLWebViewState::kReceivedNewScale) {
            double transitionTime = m_glWebViewState->transitionTime(currentTime);
            transparency = m_glWebViewState->transparency(currentTime);

            float newTilesTransparency = 1;
            if (scale < m_glWebViewState->currentScale())
                newTilesTransparency = 1 - transparency;

            nextTiledPage->draw(newTilesTransparency, viewport,
                                firstTileX, firstTileY);

            // The transition between the two pages is finished, swap them
            if (currentTime > transitionTime) {
                m_glWebViewState->setCurrentScale(scale);
                m_glWebViewState->resetTransitionTime();
                doSwap = true;
            }
        } else {
            // If the page is not ready, schedule it if needed.
            nextTiledPage->prepare(goingDown, goingLeft, firstTileX, firstTileY);
        }
    }

    // Display the current page
    TiledPage* tiledPage = m_glWebViewState->frontPage();
    tiledPage->setScale(m_glWebViewState->currentScale());
    int originalTX = m_glWebViewState->originalTilesPosX();
    int originalTY = m_glWebViewState->originalTilesPosY();
    tiledPage->prepare(goingDown, goingLeft, originalTX, originalTY);
    tiledPage->draw(transparency, viewport, originalTX, originalTY);

    bool ret = false;
    if (m_glWebViewState->scaleRequestState() != GLWebViewState::kNoScaleRequest
        || !tiledPage->ready(originalTX, originalTY))
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
