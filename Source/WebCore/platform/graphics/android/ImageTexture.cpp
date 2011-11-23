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
#include "LayerAndroid.h"
#include "SkDevice.h"
#include "SkPicture.h"
#include "TilesManager.h"
#include "TiledTexture.h"

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

// CRC computation adapted from Tools/DumpRenderTree/CyclicRedundancyCheck.cpp
static void makeCrcTable(unsigned crcTable[256])
{
    for (unsigned i = 0; i < 256; i++) {
        unsigned c = i;
        for (int k = 0; k < 8; k++) {
            if (c & 1)
                c = -306674912 ^ ((c >> 1) & 0x7fffffff);
            else
                c = c >> 1;
        }
        crcTable[i] = c;
    }
}

unsigned computeCrc(uint8_t* buffer, size_t size)
{
    static unsigned crcTable[256];
    static bool crcTableComputed = false;
    if (!crcTableComputed) {
        makeCrcTable(crcTable);
        crcTableComputed = true;
    }

    unsigned crc = 0xffffffffL;
    for (size_t i = 0; i < size; ++i)
        crc = crcTable[(crc ^ buffer[i]) & 0xff] ^ ((crc >> 8) & 0x00ffffffL);
    return crc ^ 0xffffffffL;
}

ImageTexture::ImageTexture(SkBitmap* bmp, unsigned crc)
    : m_image(bmp)
    , m_texture(0)
    , m_layer(0)
    , m_picture(0)
    , m_crc(crc)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("ImageTexture");
#endif
    if (!m_image)
        return;

    // NOTE: This constructor is called on the webcore thread

    // Create a picture containing the image (needed for TiledTexture)
    m_picture = new SkPicture();
    SkCanvas* pcanvas = m_picture->beginRecording(m_image->width(), m_image->height());
    pcanvas->clear(SkColorSetARGBInline(0, 0, 0, 0));
    pcanvas->drawBitmap(*m_image, 0, 0);
    m_picture->endRecording();
}

ImageTexture::~ImageTexture()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("ImageTexture");
#endif
    delete m_image;
    delete m_texture;
    SkSafeUnref(m_picture);
}

SkBitmap* ImageTexture::convertBitmap(SkBitmap* bitmap)
{
    SkBitmap* img = new SkBitmap();
    int w = bitmap->width();
    int h = bitmap->height();

    // Create a copy of the image
    img->setConfig(SkBitmap::kARGB_8888_Config, w, h);
    img->allocPixels();
    SkDevice* device = new SkDevice(NULL, *img, false);
    SkCanvas canvas;
    canvas.setDevice(device);
    device->unref();
    SkRect dest;
    dest.set(0, 0, w, h);
    img->setIsOpaque(false);
    img->eraseARGB(0, 0, 0, 0);
    canvas.drawBitmapRect(*bitmap, 0, dest);

    return img;
}

unsigned ImageTexture::computeCRC(const SkBitmap* bitmap)
{
    if (!bitmap)
        return 0;
    bitmap->lockPixels();
    uint8_t* img = static_cast<uint8_t*>(bitmap->getPixels());
    unsigned crc = computeCrc(img, bitmap->getSize());
    bitmap->unlockPixels();
    return crc;
}

bool ImageTexture::equalsCRC(unsigned crc)
{
    return m_crc == crc;
}

int ImageTexture::nbTextures()
{
    if (!hasContentToShow())
        return 0;
    if (!m_texture)
        return 0;

    // TODO: take in account the visible clip (need to maintain
    // a list of the clients layer, etc.)
    IntRect visibleArea(0, 0, m_image->width(), m_image->height());
    int nbTextures = m_texture->nbTextures(visibleArea, 1.0);
    XLOG("ImageTexture %p, %d x %d needs %d textures",
          this, m_image->width(), m_image->height(),
          nbTextures);
    return nbTextures;
}

bool ImageTexture::hasContentToShow()
{
    // Don't display 1x1 image -- no need to allocate a full texture for this
    if (!m_image)
        return false;
    if (m_image->width() == 1 && m_image->height() == 1)
        return false;
    return true;
}

bool ImageTexture::prepareGL(GLWebViewState* state)
{
    if (!hasContentToShow())
        return false;

    if (!m_texture && m_picture) {
        m_texture = new TiledTexture(this);
        SkRegion region;
        region.setRect(0, 0, m_image->width(), m_image->height());
        m_texture->update(region, m_picture);
    }

    if (!m_texture)
        return false;

    IntRect visibleArea(0, 0, m_image->width(), m_image->height());
    m_texture->prepare(state, 1.0, true, true, visibleArea);
    if (m_texture->ready()) {
        m_texture->swapTiles();
        return false;
    }
    return true;
}

const TransformationMatrix* ImageTexture::transform()
{
    if (!m_layer)
        return 0;

    FloatPoint p(0, 0);
    p = m_layer->drawTransform()->mapPoint(p);
    IntRect layerArea = m_layer->unclippedArea();
    float scaleW = static_cast<float>(layerArea.width()) / static_cast<float>(m_image->width());
    float scaleH = static_cast<float>(layerArea.height()) / static_cast<float>(m_image->height());
    TransformationMatrix d = *(m_layer->drawTransform());
    TransformationMatrix m;
    m.scaleNonUniform(scaleW, scaleH);
    m_layerMatrix = d.multiply(m);
    return &m_layerMatrix;
}

float ImageTexture::opacity()
{
    if (!m_layer)
        return 1.0;
    return m_layer->drawOpacity();
}

void ImageTexture::drawGL(LayerAndroid* layer)
{
    if (!layer)
        return;
    if (!hasContentToShow())
        return;

    // TiledTexture::draw() will call us back to know the
    // transform and opacity, so we need to set m_layer
    m_layer = layer;
    if (m_texture)
        m_texture->draw();
    m_layer = 0;
}

void ImageTexture::drawCanvas(SkCanvas* canvas, SkRect& rect)
{
    if (canvas && m_image)
        canvas->drawBitmapRect(*m_image, 0, rect);
}

} // namespace WebCore
