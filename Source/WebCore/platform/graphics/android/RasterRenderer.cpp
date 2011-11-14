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
#include "RasterRenderer.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GLUtils.h"
#include "SkBitmap.h"
#include "SkBitmapRef.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "TilesManager.h"

#include <wtf/text/CString.h>

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/CurrentTime.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "RasterRenderer", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

static const String TAG_CREATE_BITMAP = "create_bitmap";
static const String TAG_DRAW_PICTURE = "draw_picture";
static const String TAG_UPDATE_TEXTURE = "update_texture";
#define TAG_COUNT 3
static const String TAGS[] = {
    TAG_CREATE_BITMAP,
    TAG_DRAW_PICTURE,
    TAG_UPDATE_TEXTURE,
};

SkBitmap* RasterRenderer::g_bitmap = 0;

RasterRenderer::RasterRenderer() : BaseRenderer(BaseRenderer::Raster)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("RasterRenderer");
#endif
    if (!g_bitmap) {
        g_bitmap = new SkBitmap();
        g_bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                           TilesManager::instance()->tileWidth(),
                           TilesManager::instance()->tileHeight());
        g_bitmap->allocPixels();
    }
}

RasterRenderer::~RasterRenderer()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("RasterRenderer");
#endif
}

void RasterRenderer::setupCanvas(const TileRenderInfo& renderInfo, SkCanvas* canvas)
{
    if (renderInfo.measurePerf)
        m_perfMon.start(TAG_CREATE_BITMAP);

    if (renderInfo.baseTile->isLayerTile()) {
        g_bitmap->setIsOpaque(false);
        g_bitmap->eraseARGB(0, 0, 0, 0);
    } else {
        g_bitmap->setIsOpaque(true);
        g_bitmap->eraseARGB(255, 255, 255, 255);
    }

    SkDevice* device = new SkDevice(NULL, *g_bitmap, false);

    if (renderInfo.measurePerf) {
        m_perfMon.stop(TAG_CREATE_BITMAP);
        m_perfMon.start(TAG_DRAW_PICTURE);
    }

    canvas->setDevice(device);

    device->unref();

    // ensure the canvas origin is translated to the coordinates of our inval rect
    canvas->translate(-renderInfo.invalRect->fLeft, -renderInfo.invalRect->fTop);
}

void RasterRenderer::renderingComplete(const TileRenderInfo& renderInfo, SkCanvas* canvas)
{
    if (renderInfo.measurePerf) {
        m_perfMon.stop(TAG_DRAW_PICTURE);
        m_perfMon.start(TAG_UPDATE_TEXTURE);
    }

    const SkBitmap& bitmap = canvas->getDevice()->accessBitmap(false);

    GLUtils::paintTextureWithBitmap(&renderInfo, bitmap);

    if (renderInfo.measurePerf)
        m_perfMon.stop(TAG_UPDATE_TEXTURE);
}

const String* RasterRenderer::getPerformanceTags(int& tagCount)
{
    tagCount = TAG_COUNT;
    return TAGS;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
