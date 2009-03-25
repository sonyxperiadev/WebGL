/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
        m_solidColor = android_SkPMColorToWebCoreColor(color);
    }
}

void BitmapImage::draw(GraphicsContext* ctxt, const FloatRect& dstRect,
                       const FloatRect& srcRect, CompositeOperator compositeOp)
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
    SkRect  dstR;    
    float invScaleX = (float)bitmap.width() / image->origWidth();
    float invScaleY = (float)bitmap.height() / image->origHeight();

    android_setrect(&dstR, dstRect);
    android_setrect_scaled(&srcR, srcRect, invScaleX, invScaleY);
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

    paint.setFilterBitmap(true);
    paint.setPorterDuffXfermode(android_convert_compositeOp(compositeOp));
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

void Image::drawPattern(GraphicsContext* ctxt, const FloatRect& tileRect,
                        const TransformationMatrix& patternTransform,
                        const FloatPoint& phase, CompositeOperator compositeOp,
                        const FloatRect& destRect)
{
    SkBitmapRef* image = this->nativeImageForCurrentFrame();
    if (!image) { // If it's too early we won't have an image yet.
        return;
    }
    
    // in case we get called with an incomplete bitmap
    const SkBitmap& bitmap = image->bitmap();
    if (bitmap.getPixels() == NULL && bitmap.pixelRef() == NULL) {
        return;
    }
    
    SkRect  dstR;    
    android_setrect(&dstR, destRect);
    if (dstR.isEmpty()) {
        return;
    }
    
    SkCanvas*   canvas = ctxt->platformContext()->mCanvas;
    SkPaint     paint;
    
    SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                    SkShader::kRepeat_TileMode,
                                                    SkShader::kRepeat_TileMode);
    paint.setShader(shader)->unref();
    // now paint is the only owner of shader
    paint.setPorterDuffXfermode(android_convert_compositeOp(compositeOp));
    paint.setFilterBitmap(true);
    
    SkMatrix matrix(patternTransform);
    
    float scaleX = (float)image->origWidth() / bitmap.width();
    float scaleY = (float)image->origHeight() / bitmap.height();
    matrix.preScale(SkFloatToScalar(scaleX), SkFloatToScalar(scaleY));
    
    matrix.postTranslate(SkFloatToScalar(phase.x()),
                         SkFloatToScalar(phase.y()));
    shader->setLocalMatrix(matrix);
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

