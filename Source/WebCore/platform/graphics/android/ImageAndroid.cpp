/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "TransformationMatrix.h"
#include "BitmapImage.h"
#include "Image.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "PlatformGraphicsContext.h"
#include "PlatformString.h"
#include "SharedBuffer.h"

#include "android_graphics.h"
#include "SkBitmapRef.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkImageDecoder.h"
#include "SkShader.h"
#include "SkString.h"
#include "SkTemplates.h"
#include "SkiaUtils.h"

#include <utils/AssetManager.h>

//#define TRACE_SUBSAMPLED_BITMAPS
//#define TRACE_SKIPPED_BITMAPS

android::AssetManager* globalAssetManager() {
    static android::AssetManager* gGlobalAssetMgr;
    if (!gGlobalAssetMgr) {
        gGlobalAssetMgr = new android::AssetManager();
        gGlobalAssetMgr->addDefaultAssets();
    }
    return gGlobalAssetMgr;
}

namespace WebCore {
    
bool FrameData::clear(bool clearMetadata)
{
    if (clearMetadata)
        m_haveMetadata = false;

    if (m_frame) {
        m_frame->unref();
        m_frame = 0;
        return true;
    }
    return false;
}

BitmapImage::BitmapImage(SkBitmapRef* ref, ImageObserver* observer)
    : Image(observer)
    , m_currentFrame(0)
    , m_frames(0)
    , m_frameTimer(0)
    , m_repetitionCount(0)
    , m_repetitionCountStatus(Unknown)
    , m_repetitionsComplete(0)
    , m_isSolidColor(false)
    , m_animationFinished(true)
    , m_allDataReceived(true)
    , m_haveSize(true)
    , m_sizeAvailable(true)
    , m_decodedSize(0)
    , m_haveFrameCount(true)
    , m_frameCount(1)
{
    initPlatformData();
    
    m_size = IntSize(ref->bitmap().width(), ref->bitmap().height());
    
    m_frames.grow(1);
    m_frames[0].m_frame = ref;
    m_frames[0].m_hasAlpha = !ref->bitmap().isOpaque();
    checkForSolidColor();
    ref->ref();
}


void BitmapImage::initPlatformData()
{
    m_source.clearURL();
}

void BitmapImage::invalidatePlatformData()
{
}

void BitmapImage::checkForSolidColor()
{
    m_checkedForSolidColor = true;
    m_isSolidColor = false;
    if (frameCount() == 1) {
        SkBitmapRef* ref = frameAtIndex(0);
        if (!ref) {
            return; // keep solid == false
        }
        
        const SkBitmap& bm = ref->bitmap();
        if (bm.width() != 1 || bm.height() != 1) {
            return;  // keep solid == false
        }
        
        SkAutoLockPixels alp(bm);
        if (!bm.readyToDraw()) {
            return;  // keep solid == false
        }

        SkPMColor color;
        switch (bm.getConfig()) {
            case SkBitmap::kARGB_8888_Config:
                color = *bm.getAddr32(0, 0);
                break;
            case SkBitmap::kRGB_565_Config:
                color = SkPixel16ToPixel32(*bm.getAddr16(0, 0));
                break;
            case SkBitmap::kIndex8_Config: {
                SkColorTable* ctable = bm.getColorTable();
                if (!ctable) {
                return;
                }
                color = (*ctable)[*bm.getAddr8(0, 0)];
                break;
            }
            default:
                return;  // keep solid == false
        }
        m_isSolidColor = true;
        m_solidColor = SkPMColorToWebCoreColor(color);
    }
}

static void round(SkIRect* dst, const WebCore::FloatRect& src)
{
    dst->set(SkScalarRound(SkFloatToScalar(src.x())),
             SkScalarRound(SkFloatToScalar(src.y())),
             SkScalarRound(SkFloatToScalar((src.x() + src.width()))),
             SkScalarRound(SkFloatToScalar((src.y() + src.height()))));
}

static void round_scaled(SkIRect* dst, const WebCore::FloatRect& src,
                         float sx, float sy)
{
    dst->set(SkScalarRound(SkFloatToScalar(src.x() * sx)),
             SkScalarRound(SkFloatToScalar(src.y() * sy)),
             SkScalarRound(SkFloatToScalar((src.x() + src.width()) * sx)),
             SkScalarRound(SkFloatToScalar((src.y() + src.height()) * sy)));
}

static inline void fixPaintForBitmapsThatMaySeam(SkPaint* paint) {
    /*  Bitmaps may be drawn to seem next to other images. If we are drawn
        zoomed, or at fractional coordinates, we may see cracks/edges if
        we antialias, because that will cause us to draw the same pixels
        more than once (e.g. from the left and right bitmaps that share
        an edge).

        Disabling antialiasing fixes this, and since so far we are never
        rotated at non-multiple-of-90 angles, this seems to do no harm
     */
    paint->setAntiAlias(false);
}

void BitmapImage::draw(GraphicsContext* ctxt, const FloatRect& dstRect,
                   const FloatRect& srcRect, ColorSpace,
                   CompositeOperator compositeOp)
{
    startAnimation();

    SkBitmapRef* image = this->nativeImageForCurrentFrame();
    if (!image) { // If it's too early we won't have an image yet.
        return;
    }

    // in case we get called with an incomplete bitmap
    const SkBitmap& bitmap = image->bitmap();
    if (bitmap.getPixels() == NULL && bitmap.pixelRef() == NULL) {
#ifdef TRACE_SKIPPED_BITMAPS
        SkDebugf("----- skip bitmapimage: [%d %d] pixels %p pixelref %p\n",
                 bitmap.width(), bitmap.height(),
                 bitmap.getPixels(), bitmap.pixelRef());
#endif
        return;
    }

    SkIRect srcR;
    SkRect  dstR(dstRect);
    float invScaleX = (float)bitmap.width() / image->origWidth();
    float invScaleY = (float)bitmap.height() / image->origHeight();

    round_scaled(&srcR, srcRect, invScaleX, invScaleY);
    if (srcR.isEmpty() || dstR.isEmpty()) {
#ifdef TRACE_SKIPPED_BITMAPS
        SkDebugf("----- skip bitmapimage: [%d %d] src-empty %d dst-empty %d\n",
                 bitmap.width(), bitmap.height(),
                 srcR.isEmpty(), dstR.isEmpty());
#endif
        return;
    }

    SkCanvas*   canvas = ctxt->platformContext()->mCanvas;
    SkPaint     paint;

    ctxt->setupBitmapPaint(&paint);   // need global alpha among other things
    paint.setFilterBitmap(true);
    paint.setXfermodeMode(WebCoreCompositeToSkiaComposite(compositeOp));
    fixPaintForBitmapsThatMaySeam(&paint);
    canvas->drawBitmapRect(bitmap, &srcR, dstR, &paint);

#ifdef TRACE_SUBSAMPLED_BITMAPS
    if (bitmap.width() != image->origWidth() ||
        bitmap.height() != image->origHeight()) {
        SkDebugf("--- BitmapImage::draw [%d %d] orig [%d %d]\n",
                 bitmap.width(), bitmap.height(),
                 image->origWidth(), image->origHeight());
    }
#endif
}

void BitmapImage::setURL(const String& str) 
{
    m_source.setURL(str);
}

///////////////////////////////////////////////////////////////////////////////

void Image::drawPattern(GraphicsContext* ctxt, const FloatRect& srcRect,
                        const AffineTransform& patternTransform,
                        const FloatPoint& phase, ColorSpace,
                        CompositeOperator compositeOp, const FloatRect& destRect)
{
    SkBitmapRef* image = this->nativeImageForCurrentFrame();
    if (!image) { // If it's too early we won't have an image yet.
        return;
    }

    // in case we get called with an incomplete bitmap
    const SkBitmap& origBitmap = image->bitmap();
    if (origBitmap.getPixels() == NULL && origBitmap.pixelRef() == NULL) {
        return;
    }

    SkRect  dstR(destRect);
    if (dstR.isEmpty()) {
        return;
    }

    SkIRect srcR;
    // we may have to scale if the image has been subsampled (so save RAM)
    bool imageIsSubSampled = image->origWidth() != origBitmap.width() ||
                             image->origHeight() != origBitmap.height();
    float scaleX = 1;
    float scaleY = 1;
    if (imageIsSubSampled) {
        scaleX = (float)image->origWidth() / origBitmap.width();
        scaleY = (float)image->origHeight() / origBitmap.height();
//        SkDebugf("----- subsampled %g %g\n", scaleX, scaleY);
        round_scaled(&srcR, srcRect, 1 / scaleX, 1 / scaleY);
    } else {
        round(&srcR, srcRect);
    }

    // now extract the proper subset of the src image
    SkBitmap bitmap;
    if (!origBitmap.extractSubset(&bitmap, srcR)) {
        SkDebugf("--- Image::drawPattern calling extractSubset failed\n");
        return;
    }

    SkCanvas*   canvas = ctxt->platformContext()->mCanvas;
    SkPaint     paint;
    ctxt->setupBitmapPaint(&paint);   // need global alpha among other things

    SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                    SkShader::kRepeat_TileMode,
                                                    SkShader::kRepeat_TileMode);
    paint.setShader(shader)->unref();
    // now paint is the only owner of shader
    paint.setXfermodeMode(WebCoreCompositeToSkiaComposite(compositeOp));
    paint.setFilterBitmap(true);
    fixPaintForBitmapsThatMaySeam(&paint);

    SkMatrix matrix(patternTransform);

    if (imageIsSubSampled) {
        matrix.preScale(SkFloatToScalar(scaleX), SkFloatToScalar(scaleY));
    }
    // We also need to translate it such that the origin of the pattern is the
    // origin of the destination rect, which is what WebKit expects. Skia uses
    // the coordinate system origin as the base for the patter. If WebKit wants
    // a shifted image, it will shift it from there using the patternTransform.
    float tx = phase.x() + srcRect.x() * patternTransform.a();
    float ty = phase.y() + srcRect.y() * patternTransform.d();
    matrix.postTranslate(SkFloatToScalar(tx), SkFloatToScalar(ty));
    shader->setLocalMatrix(matrix);
#if 0
    SkDebugf("--- drawPattern: src [%g %g %g %g] dst [%g %g %g %g] transform [%g %g %g %g %g %g] matrix [%g %g %g %g %g %g]\n",
             srcRect.x(), srcRect.y(), srcRect.width(), srcRect.height(),
             destRect.x(), destRect.y(), destRect.width(), destRect.height(),
             patternTransform.a(), patternTransform.b(), patternTransform.c(),
             patternTransform.d(), patternTransform.e(), patternTransform.f(),
             matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5]);
#endif
    canvas->drawRect(dstR, paint);

#ifdef TRACE_SUBSAMPLED_BITMAPS
    if (bitmap.width() != image->origWidth() ||
        bitmap.height() != image->origHeight()) {
        SkDebugf("--- Image::drawPattern [%d %d] orig [%d %d] dst [%g %g]\n",
                 bitmap.width(), bitmap.height(),
                 image->origWidth(), image->origHeight(),
                 SkScalarToFloat(dstR.width()), SkScalarToFloat(dstR.height()));
    }
#endif
}

// missingImage, textAreaResizeCorner
PassRefPtr<Image> Image::loadPlatformResource(const char *name)
{
    android::AssetManager* am = globalAssetManager();
    
    SkString path("webkit/");
    path.append(name);
    path.append(".png");
    
    android::Asset* a = am->open(path.c_str(),
                                 android::Asset::ACCESS_BUFFER);
    if (a == NULL) {
        SkDebugf("---------------- failed to open image asset %s\n", name);
        return NULL;
    }
    
    SkAutoTDelete<android::Asset> ad(a);

    SkBitmap bm;
    if (SkImageDecoder::DecodeMemory(a->getBuffer(false), a->getLength(), &bm)) {
        SkBitmapRef* ref = new SkBitmapRef(bm);
        // create will call ref(), so we need aur() to release ours upon return
        SkAutoUnref aur(ref);
        return BitmapImage::create(ref, 0);
    }
    return Image::nullImage();
}

}   // namespace
