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
#include "GaneshRenderer.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GaneshContext.h"
#include "SkCanvas.h"
#include "SkGpuDevice.h"
#include "TilesManager.h"

#include <wtf/text/CString.h>

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/CurrentTime.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "GaneshRenderer", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

static const String TAG_CREATE_FBO = "create_fbo";
static const String TAG_DRAW_PICTURE = "draw_picture";
static const String TAG_UPDATE_TEXTURE = "update_texture";
#define TAG_COUNT 3
static const String TAGS[] = {
    TAG_CREATE_FBO,
    TAG_DRAW_PICTURE,
    TAG_UPDATE_TEXTURE,
};

GaneshRenderer::GaneshRenderer() : BaseRenderer(BaseRenderer::Raster)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("GaneshRenderer");
#endif
}

GaneshRenderer::~GaneshRenderer()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("GaneshRenderer");
#endif
}

void GaneshRenderer::setupCanvas(const TileRenderInfo& renderInfo, SkCanvas* canvas)
{
    if (renderInfo.measurePerf)
        m_perfMon.start(TAG_CREATE_FBO);

    const float tileWidth = renderInfo.tileSize.width();
    const float tileHeight = renderInfo.tileSize.height();

    glBindTexture(GL_TEXTURE_2D, renderInfo.textureInfo->m_textureId);

    // setup the texture if needed
    if (renderInfo.textureInfo->m_width != tileWidth
            || renderInfo.textureInfo->m_height != tileHeight) {

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tileWidth, tileHeight,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        renderInfo.textureInfo->m_width = tileWidth;
        renderInfo.textureInfo->m_height = tileHeight;

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    GaneshContext* ganesh = GaneshContext::instance();

    // TODO only need to reset if others are sharing our context
    ganesh->getGrContext()->resetContext();

    SkDevice* device = NULL;
    if (tileWidth == TilesManager::tileWidth() && tileHeight == TilesManager::tileHeight()) {
        device = ganesh->getDeviceForBaseTile(renderInfo.textureInfo->m_textureId);
    } else {
        // TODO support arbitrary sizes for layers
        XLOG("ERROR: expected (%d,%d) actual (%d,%d)",
                TilesManager::tileWidth(), TilesManager::tileHeight(),
                tileWidth, tileHeight);
    }

    if (renderInfo.measurePerf) {
        m_perfMon.stop(TAG_CREATE_FBO);
        m_perfMon.start(TAG_DRAW_PICTURE);
    }

    // set the GPU device to the canvas
    canvas->setDevice(device);

    // invert canvas contents
    canvas->scale(SK_Scalar1, -SK_Scalar1);
    canvas->translate(0, -SkIntToScalar(renderInfo.tileSize.height()));
}

void GaneshRenderer::setupPartialInval(const TileRenderInfo& renderInfo, SkCanvas* canvas)
{
    // set the clip to our invalRect
    SkRect clipRect = SkRect::MakeLTRB(renderInfo.invalRect->fLeft,
                                       renderInfo.invalRect->fTop,
                                       renderInfo.invalRect->fRight,
                                       renderInfo.invalRect->fBottom);
    canvas->clipRect(clipRect);
}

void GaneshRenderer::renderingComplete(const TileRenderInfo& renderInfo, SkCanvas* canvas)
{
    if (renderInfo.measurePerf) {
        m_perfMon.stop(TAG_DRAW_PICTURE);
        m_perfMon.start(TAG_UPDATE_TEXTURE);
    }

    GaneshContext::instance()->getGrContext()->flush();

    //TODO if texture is surfaceTexture then...

    if (renderInfo.measurePerf)
        m_perfMon.stop(TAG_UPDATE_TEXTURE);
}

const String* GaneshRenderer::getPerformanceTags(int& tagCount)
{
    tagCount = TAG_COUNT;
    return TAGS;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
