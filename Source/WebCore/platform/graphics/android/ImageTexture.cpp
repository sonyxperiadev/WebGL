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
#include "ImageTexture.h"

#include "ImagesManager.h"
#include "SkDevice.h"
#include "TilesManager.h"

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "ImageTexture", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "ImageTexture", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

ImageTexture::ImageTexture(SkBitmapRef* img)
    : m_imageRef(img)
    , m_image(0)
    , m_textureId(0)
    , m_refCount(0)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("ImageTexture");
#endif
    if (!m_imageRef)
        return;

    SkBitmap* bitmap = &m_imageRef->bitmap();
    m_image = new SkBitmap();
    int w = bitmap->width();
    int h = bitmap->height();
    m_image->setConfig(SkBitmap::kARGB_8888_Config, w, h);
    m_image->allocPixels();
    SkDevice* device = new SkDevice(NULL, *m_image, false);
    SkCanvas canvas;
    canvas.setDevice(device);
    device->unref();
    SkRect dest;
    dest.set(0, 0, w, h);
    m_image->setIsOpaque(false);
    m_image->eraseARGB(0, 0, 0, 0);
    canvas.drawBitmapRect(*bitmap, 0, dest);
}

ImageTexture::~ImageTexture()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("ImageTexture");
#endif
    delete m_image;
}

void ImageTexture::prepareGL()
{
    if (m_textureId)
        return;

    ImagesManager::instance()->scheduleTextureUpload(this);
}

void ImageTexture::uploadGLTexture()
{
    if (m_textureId)
        return;

    glGenTextures(1, &m_textureId);
    GLUtils::createTextureWithBitmap(m_textureId, *m_image);
}

void ImageTexture::drawGL(LayerAndroid* layer)
{
    if (!layer)
        return;
    if (!m_textureId)
        return;
    if (!m_image)
        return;

    SkRect rect;
    rect.fLeft = 0;
    rect.fTop = 0;
    rect.fRight = layer->getSize().width();
    rect.fBottom = layer->getSize().height();
    TilesManager::instance()->shader()->drawLayerQuad(*layer->drawTransform(),
                                                      rect, m_textureId,
                                                      layer->drawOpacity(), true);
}

void ImageTexture::drawCanvas(SkCanvas* canvas, SkRect& rect)
{
    canvas->drawBitmapRect(*m_image, 0, rect);
}

void ImageTexture::release()
{
    if (m_refCount >= 1)
        m_refCount--;
    if (!m_refCount)
        deleteTexture();
}

void ImageTexture::deleteTexture()
{
   if (m_textureId)
       glDeleteTextures(1, &m_textureId);
}

} // namespace WebCore
