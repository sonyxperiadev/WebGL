/*
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
#include "AffineTransform.h"
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
#include "SkShader.h"
#include "SkString.h"

#include <utils/AssetManager.h>

//#define TRACE_SUBSAMPLED_BITMAPS

android::AssetManager* gGlobalAssetMgr;

namespace WebCore {

void FrameData::clear()
{
    if (m_frame) {
        m_frame->unref();
        m_frame = 0;
        m_duration = 0.;
        m_hasAlpha = true;
    }
}

SkBitmapRef* BitmapImage::getBitmap()
{
    return m_bitmapRef;
}

void BitmapImage::initPlatformData()
{
    m_bitmapRef = NULL;
    m_source.clearURL();
}

void BitmapImage::invalidatePlatformData()
{
    if (m_bitmapRef) {
        m_bitmapRef->unref();
        m_bitmapRef = NULL;
    }
}

void BitmapImage::checkForSolidColor()
{
    m_isSolidColor = false;
    if (this->frameCount() > 1) {
        if (!m_bitmapRef) {
            return;
        }
        
        const SkBitmap& bm = m_bitmapRef->bitmap();
        
        if (bm.width() == 1 && bm.height() == 1) {
            SkAutoLockPixels alp(bm);
            if (bm.getPixels() == NULL) {
                return;
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
                default:    // don't check other configs
                    return;
            }
            m_isSolidColor = true;
            m_solidColor = android_SkPMColorToWebCoreColor(color);
        }
    }
}

void BitmapImage::draw(GraphicsContext* ctxt, const FloatRect& dstRect,
                       const FloatRect& srcRect, CompositeOperator compositeOp)
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

    SkIRect srcR;
    SkRect  dstR;    
    float invScaleX = (float)bitmap.width() / image->origWidth();
    float invScaleY = (float)bitmap.height() / image->origHeight();

    android_setrect(&dstR, dstRect);
    android_setrect_scaled(&srcR, srcRect, invScaleX, invScaleY);
    if (srcR.isEmpty() || dstR.isEmpty()) {
        return;
    }
    
    SkCanvas*   canvas = ctxt->platformContext()->mCanvas;
    SkPaint     paint;

    paint.setFilterBitmap(true);
    paint.setPorterDuffXfermode(android_convert_compositeOp(compositeOp));
    canvas->drawBitmapRect(bitmap, &srcR, dstR, &paint);

    startAnimation();

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
                        const AffineTransform& patternTransform,
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
Image* Image::loadPlatformResource(const char *name)
{
    if (NULL == gGlobalAssetMgr) {
        gGlobalAssetMgr = new android::AssetManager();
        gGlobalAssetMgr->addDefaultAssets();
    }

    SkString path("webkit/");
    path.append(name);
    path.append(".png");

    android::Asset* a = gGlobalAssetMgr->open(path.c_str(),
                                              android::Asset::ACCESS_BUFFER);
    if (a == NULL) {
        SkDebugf("---------------- failed to open image asset %s\n", name);
        return NULL;
    }

    Image* image = new BitmapImage;
    RefPtr<SharedBuffer> buffer =
            new SharedBuffer((const char*)a->getBuffer(false), a->getLength());
    image->setData(buffer, true);
    delete a;
    return image;
}

}
