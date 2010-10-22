/*
 * Copyright 2007, The Android Open Source Project
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
#include "ImageBuffer.h"

#include "Base64.h"
#include "BitmapImage.h"
#include "ColorSpace.h"
#include "ImageData.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "PlatformGraphicsContext.h"
#include "SkBitmapRef.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkDevice.h"
#include "SkImageEncoder.h"
#include "SkStream.h"
#include "SkUnPreMultiply.h"
#include "android_graphics.h"

using namespace std;

namespace WebCore {

ImageBufferData::ImageBufferData(const IntSize&)
{
}

ImageBuffer::ImageBuffer(const IntSize& size, ColorSpace, bool& success)
    : m_data(size)
    , m_size(size)
{
    m_context.set(GraphicsContext::createOffscreenContext(size.width(), size.height()));
    success = true;
}

ImageBuffer::~ImageBuffer()
{
}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

bool ImageBuffer::drawsUsingCopy() const
{
    return true;
}

PassRefPtr<Image> ImageBuffer::copyImage() const
{
    ASSERT(context());
    SkCanvas* canvas = context()->platformContext()->mCanvas;
    SkDevice* device = canvas->getDevice();
    const SkBitmap& orig = device->accessBitmap(false);

    SkBitmap copy;
    orig.copyTo(&copy, orig.config());

    SkBitmapRef* ref = new SkBitmapRef(copy);
    RefPtr<Image> image = BitmapImage::create(ref, 0);
    ref->unref();
    return image;
}

void ImageBuffer::clip(GraphicsContext* context, const FloatRect& rect) const
{
    SkDebugf("xxxxxxxxxxxxxxxxxx clip not implemented\n");
}

void ImageBuffer::draw(GraphicsContext* context, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, bool useLowQualityScale)
{
    RefPtr<Image> imageCopy = copyImage();
    context->drawImage(imageCopy.get(), styleColorSpace, destRect, srcRect, op, useLowQualityScale);
}

void ImageBuffer::drawPattern(GraphicsContext* context, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator op, const FloatRect& destRect)
{
    RefPtr<Image> imageCopy = copyImage();
    imageCopy->drawPattern(context, srcRect, patternTransform, phase, styleColorSpace, op, destRect);
}

PassRefPtr<ImageData> ImageBuffer::getUnmultipliedImageData(const IntRect& rect) const
{
    GraphicsContext* gc = this->context();
    if (!gc) {
        return 0;
    }

    const SkBitmap& src = android_gc2canvas(gc)->getDevice()->accessBitmap(false);
    SkAutoLockPixels alp(src);
    if (!src.getPixels()) {
        return 0;
    }

    // ! Can't use PassRefPtr<>, otherwise the second access will cause crash.
    RefPtr<ImageData> result = ImageData::create(rect.width(), rect.height());
    unsigned char* data = result->data()->data()->data();

    if (rect.x() < 0 || rect.y() < 0 || (rect.x() + rect.width()) > m_size.width() || (rect.y() + rect.height()) > m_size.height())
        memset(data, 0, result->data()->length());

    int originx = rect.x();
    int destx = 0;
    if (originx < 0) {
        destx = -originx;
        originx = 0;
    }
    int endx = rect.x() + rect.width();
    if (endx > m_size.width())
        endx = m_size.width();
    int numColumns = endx - originx;

    int originy = rect.y();
    int desty = 0;
    if (originy < 0) {
        desty = -originy;
        originy = 0;
    }
    int endy = rect.y() + rect.height();
    if (endy > m_size.height())
        endy = m_size.height();
    int numRows = endy - originy;

    unsigned srcPixelsPerRow = src.rowBytesAsPixels();
    unsigned destBytesPerRow = 4 * rect.width();

    const SkPMColor* srcRows = src.getAddr32(originx, originy);
    unsigned char* destRows = data + desty * destBytesPerRow + destx * 4;
    for (int y = 0; y < numRows; ++y) {
        for (int x = 0; x < numColumns; x++) {
            // ugh, it appears they want unpremultiplied pixels
            SkColor c = SkUnPreMultiply::PMColorToColor(srcRows[x]);
            int basex = x * 4;
            destRows[basex + 0] = SkColorGetR(c);
            destRows[basex + 1] = SkColorGetG(c);
            destRows[basex + 2] = SkColorGetB(c);
            destRows[basex + 3] = SkColorGetA(c);
        }
        srcRows += srcPixelsPerRow;
        destRows += destBytesPerRow;
    }
    return result;
}

void ImageBuffer::putUnmultipliedImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint)
{
    GraphicsContext* gc = this->context();
    if (!gc) {
        return;
    }

    const SkBitmap& dst = android_gc2canvas(gc)->getDevice()->accessBitmap(true);
    SkAutoLockPixels alp(dst);
    if (!dst.getPixels()) {
        return;
    }

    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originx = sourceRect.x();
    int destx = destPoint.x() + sourceRect.x();
    ASSERT(destx >= 0);
    ASSERT(destx < m_size.width());
    ASSERT(originx >= 0);
    ASSERT(originx <= sourceRect.right());

    int endx = destPoint.x() + sourceRect.right();
    ASSERT(endx <= m_size.width());

    int numColumns = endx - destx;

    int originy = sourceRect.y();
    int desty = destPoint.y() + sourceRect.y();
    ASSERT(desty >= 0);
    ASSERT(desty < m_size.height());
    ASSERT(originy >= 0);
    ASSERT(originy <= sourceRect.bottom());

    int endy = destPoint.y() + sourceRect.bottom();
    ASSERT(endy <= m_size.height());
    int numRows = endy - desty;

    unsigned srcBytesPerRow = 4 * source->width();
    unsigned dstPixelsPerRow = dst.rowBytesAsPixels();

    unsigned char* srcRows = source->data()->data()->data() + originy * srcBytesPerRow + originx * 4;
    SkPMColor* dstRows = dst.getAddr32(destx, desty);
    for (int y = 0; y < numRows; ++y) {
        for (int x = 0; x < numColumns; x++) {
            int basex = x * 4;
            dstRows[x] = SkPackARGB32(srcRows[basex + 3],
                                      srcRows[basex + 0],
                                      srcRows[basex + 1],
                                      srcRows[basex + 2]);
        }
        dstRows += dstPixelsPerRow;
        srcRows += srcBytesPerRow;
    }
}


String ImageBuffer::toDataURL(const String&, const double*) const
{
    // Encode the image into a vector.
    SkDynamicMemoryWStream pngStream;
    const SkBitmap& dst = android_gc2canvas(context())->getDevice()->accessBitmap(true);
    SkImageEncoder::EncodeStream(&pngStream, dst, SkImageEncoder::kPNG_Type, 100);

    // Convert it into base64.
    Vector<char> pngEncodedData;
    pngEncodedData.append(pngStream.getStream(), pngStream.getOffset());
    Vector<char> base64EncodedData;
    base64Encode(pngEncodedData, base64EncodedData);
    // Append with a \0 so that it's a valid string.
    base64EncodedData.append('\0');

    // And the resulting string.
    return String::format("data:image/png;base64,%s", base64EncodedData.data());
}

void ImageBuffer::platformTransformColorSpace(const Vector<int>& lookupTable)
{
    notImplemented();
}

}
