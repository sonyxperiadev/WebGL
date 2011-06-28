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
#include "SkPicture.h"
#include "TilesManager.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

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
static const String TAG_RESET_BITMAP = "reset_bitmap";
#define TAG_COUNT 4
static const String TAGS[] = {
    TAG_CREATE_BITMAP,
    TAG_DRAW_PICTURE,
    TAG_UPDATE_TEXTURE,
    TAG_RESET_BITMAP
};

RasterRenderer::RasterRenderer()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("RasterRenderer");
#endif
}

RasterRenderer::~RasterRenderer()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("RasterRenderer");
#endif
}

void RasterRenderer::drawTileInfo(SkCanvas* canvas,
                            BaseTileTexture* texture,
                            TiledPage* tiledPage,
                            int x, int y, float scale,
                            int pictureCount)
{
    SkPaint paint;
    char str[256];
    snprintf(str, 256, "(%d,%d) %.2f, tl%x tx%x p%x c%d",
             x, y, scale, this, texture, tiledPage, pictureCount);
    paint.setARGB(255, 0, 0, 0);
    canvas->drawText(str, strlen(str), 0, 10, paint);
    paint.setARGB(255, 255, 0, 0);
    canvas->drawText(str, strlen(str), 0, 11, paint);
    float total = 0;
    for (int i = 0; i < TAG_COUNT; i++) {
        float tagDuration = m_perfMon.getAverageDuration(TAGS[i]);
        total += tagDuration;
        snprintf(str, 256, "%s: %.2f", TAGS[i].utf8().data(), tagDuration);
        paint.setARGB(255, 0, 0, 0);
        int textY = (i * 12) + 25;
        canvas->drawText(str, strlen(str), 0, textY, paint);
        paint.setARGB(255, 255, 0, 0);
        canvas->drawText(str, strlen(str), 0, textY + 1, paint);
    }
    snprintf(str, 256, "total: %.2f", total);
    paint.setARGB(255, 0, 0, 0);
    int textY = (TAG_COUNT * 12) + 30;
    canvas->drawText(str, strlen(str), 0, textY, paint);
    paint.setARGB(255, 255, 0, 0);
    canvas->drawText(str, strlen(str), 0, textY + 1, paint);
}

int RasterRenderer::renderContent(int x, int y, SkIRect rect, float tx, float ty,
                            float scale, BaseTileTexture* texture,
                            TextureInfo* textureInfo,
                            TiledPage* tiledPage, bool fullRepaint)
{
    bool visualIndicator = TilesManager::instance()->getShowVisualIndicator();
    bool measurePerf = fullRepaint && visualIndicator;

#ifdef DEBUG
    visualIndicator = true;
    measurePerf = true;
#endif

    if (measurePerf)
        m_perfMon.start(TAG_CREATE_BITMAP);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height());
    bitmap.allocPixels();
    bitmap.eraseColor(0);

    SkCanvas canvas(bitmap);
    canvas.drawARGB(255, 255, 255, 255);

    if (measurePerf) {
        m_perfMon.stop(TAG_CREATE_BITMAP);
        m_perfMon.start(TAG_DRAW_PICTURE);
    }
    if (visualIndicator)
        canvas.save();

    canvas.scale(scale, scale);
    canvas.translate(-tx, -ty);
    int pictureCount = tiledPage->paintBaseLayerContent(&canvas);

    if (measurePerf) {
        m_perfMon.stop(TAG_DRAW_PICTURE);
    }
    if (visualIndicator) {
        canvas.restore();

        int color = 20 + pictureCount % 100;
        canvas.drawARGB(color, 0, 255, 0);

        SkPaint paint;
        paint.setARGB(128, 255, 0, 0);
        paint.setStrokeWidth(3);
        canvas.drawLine(0, 0, rect.width(), rect.height(), paint);
        paint.setARGB(128, 0, 255, 0);
        canvas.drawLine(0, rect.height(), rect.width(), 0, paint);
        paint.setARGB(128, 0, 0, 255);
        canvas.drawLine(0, 0, rect.width(), 0, paint);
        canvas.drawLine(rect.width(), 0, rect.width(), rect.height(), paint);

        drawTileInfo(&canvas, texture, tiledPage, x, y, scale, pictureCount);
    }

    if (measurePerf)
        m_perfMon.start(TAG_UPDATE_TEXTURE);
    GLUtils::paintTextureWithBitmap(textureInfo, texture->getSize(), bitmap, rect.fLeft, rect.fTop);
    if (measurePerf)
        m_perfMon.stop(TAG_UPDATE_TEXTURE);

    if (measurePerf)
        m_perfMon.start(TAG_RESET_BITMAP);

    bitmap.reset();

    if (measurePerf)
        m_perfMon.stop(TAG_RESET_BITMAP);

    return pictureCount;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
