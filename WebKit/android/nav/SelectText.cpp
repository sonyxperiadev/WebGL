/*
 * Copyright 2008, The Android Open Source Project
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

#define LOG_TAG "webcoreglue"

#include "CachedPrefix.h"
#include "CachedRoot.h"
#include "LayerAndroid.h"
#include "SelectText.h"
#include "SkBitmap.h"
#include "SkBounder.h"
#include "SkCanvas.h"
#include "SkMatrix.h"
#include "SkPicture.h"
#include "SkPixelXorXfermode.h"
#include "SkPoint.h"
#include "SkRect.h"
#include "SkRegion.h"
#include "SkUtils.h"

#ifdef DEBUG_NAV_UI
#include "CString.h"
#endif

namespace android {

class CommonCheck : public SkBounder {
public:
    CommonCheck() : mMatrix(NULL), mPaint(NULL) {}
    
    virtual void setUp(const SkPaint& paint, const SkMatrix& matrix, SkScalar y,
            const void* text) {
        mMatrix = &matrix;
        mPaint = &paint;
        mText = static_cast<const uint16_t*>(text);
        mY = y;
        mBase = mBottom = mTop = INT_MAX;
    }
    
    int base() {
        if (mBase == INT_MAX) {
            SkPoint result;
            mMatrix->mapXY(0, mY, &result);
            mBase = SkScalarFloor(result.fY);
        }
        return mBase;
    }
    
     int bottom() {
        if (mBottom == INT_MAX) {
            SkPoint result;
            SkPaint::FontMetrics metrics;
            mPaint->getFontMetrics(&metrics);
            mMatrix->mapXY(0, metrics.fDescent + mY, &result);
            mBottom = SkScalarCeil(result.fY);
        }
        return mBottom;
    }
    
    int top() {
        if (mTop == INT_MAX) {
            SkPoint result;
            SkPaint::FontMetrics metrics;
            mPaint->getFontMetrics(&metrics);
            mMatrix->mapXY(0, metrics.fAscent + mY, &result);
            mTop = SkScalarFloor(result.fY);
        }
        return mTop;
    }
    
protected:   
    const SkMatrix* mMatrix;
    const SkPaint* mPaint;
    const uint16_t* mText;
    SkScalar mY;
    int mBase;
    int mBottom;
    int mTop;
};

class FirstCheck : public CommonCheck {
public:
    FirstCheck(int x, int y) 
            : mDistance(INT_MAX), mFocusX(x), mFocusY(y) {
        mBestBounds.setEmpty(); 
    }

    const SkIRect& bestBounds() { 
        DBG_NAV_LOGD("mBestBounds:(%d, %d, %d, %d) mTop=%d mBottom=%d", 
            mBestBounds.fLeft, mBestBounds.fTop, mBestBounds.fRight, 
            mBestBounds.fBottom, mTop, mBottom);
        return mBestBounds; 
    }
    
    void offsetBounds(int dx, int dy) {
        mBestBounds.offset(dx, dy);
    }
    
    virtual bool onIRect(const SkIRect& rect) {
        int dx = ((rect.fLeft + rect.fRight) >> 1) - mFocusX;
        int dy = ((top() + bottom()) >> 1) - mFocusY;
        int distance = dx * dx + dy * dy;
#ifdef EXTRA_NOISY_LOGGING
        if (distance < 500 || abs(distance - mDistance) < 500)
            DBG_NAV_LOGD("distance=%d mDistance=%d", distance, mDistance);
#endif
        if (mDistance > distance) {
            mDistance = distance;
            mBestBounds.set(rect.fLeft, top(), rect.fRight, bottom());
#ifdef EXTRA_NOISY_LOGGING
            DBG_NAV_LOGD("mBestBounds={%d,%d,r=%d,b=%d}", 
                mBestBounds.fLeft, mBestBounds.fTop,
                mBestBounds.fRight, mBestBounds.fBottom);
#endif
        }
        return false;
    }
protected:
    SkIRect mBestBounds;
    int mDistance;
    int mFocusX;
    int mFocusY;
};

class MultilineBuilder : public CommonCheck {
public:
    MultilineBuilder(const SkIRect& start, const SkIRect& end, int dx, int dy,
            SkRegion* region)
            : mStart(start), mEnd(end), mSelectRegion(region), mCapture(false) {
        mLast.setEmpty();
        mLastBase = INT_MAX;
        mStart.offset(-dx, -dy);
        mEnd.offset(-dx, -dy);
    }

    virtual bool onIRect(const SkIRect& rect) {
        bool captureLast = false;
        if ((rect.fLeft == mStart.fLeft && rect.fRight == mStart.fRight &&
                top() == mStart.fTop && bottom() == mStart.fBottom) ||
                (rect.fLeft == mEnd.fLeft && rect.fRight == mEnd.fRight &&
                top() == mEnd.fTop && bottom() == mEnd.fBottom)) {
            captureLast = mCapture;
            mCapture ^= true;
        }
        if (mCapture || captureLast) {
            SkIRect full;
            full.set(rect.fLeft, top(), rect.fRight, bottom());
            if ((mLast.fTop < base() && mLast.fBottom >= base())
                    || (mLastBase <= full.fBottom && mLastBase > full.fTop)) {
                if (full.fLeft > mLast.fRight)
                    full.fLeft = mLast.fRight;
                else if (full.fRight < mLast.fLeft)
                    full.fRight = mLast.fLeft;
            }
            mSelectRegion->op(full, SkRegion::kUnion_Op);
            DBG_NAV_LOGD("MultilineBuilder full=(%d,%d,r=%d,b=%d)",
                full.fLeft, full.fTop, full.fRight, full.fBottom);
            mLast = full;
            mLastBase = base();
            if (mStart == mEnd)
                mCapture = false;
        }
        return false;
    }
protected:    
    SkIRect mStart;
    SkIRect mEnd;
    SkIRect mLast;
    int mLastBase;
    SkRegion* mSelectRegion;
    bool mCapture;
};

#define HYPHEN_MINUS 0x2D // ASCII hyphen
#define HYPHEN 0x2010 // unicode hyphen, first in range of dashes
#define HORZ_BAR 0x2015 // unicode horizontal bar, last in range of dashes

class TextExtractor : public CommonCheck {
public:
    TextExtractor(const SkRegion& region) : mSelectRegion(region),
        mSkipFirstSpace(true) { // don't start with a space
    }

    virtual void setUp(const SkPaint& paint, const SkMatrix& matrix, SkScalar y,
            const void* text) {
        INHERITED::setUp(paint, matrix, y, text);
        SkPaint charPaint = paint;
        charPaint.setTextEncoding(SkPaint::kUTF8_TextEncoding);
        mMinSpaceWidth = std::max(0, SkScalarToFixed(
            charPaint.measureText(" ", 1)) - SK_Fixed1);
    }

    virtual bool onIRectGlyph(const SkIRect& rect,
        const SkBounder::GlyphRec& rec)
    {
        SkIRect full;
        full.set(rect.fLeft, top(), rect.fRight, bottom());
        if (mSelectRegion.contains(full)) {
            if (!mSkipFirstSpace && (mLastUni < HYPHEN || mLastUni > HORZ_BAR)
                    && mLastUni != HYPHEN_MINUS
                    && (mLastGlyph.fLSB.fY != rec.fLSB.fY // new baseline
                    || mLastGlyph.fLSB.fX > rec.fLSB.fX // glyphs are LTR
                    || mLastGlyph.fRSB.fX + mMinSpaceWidth < rec.fLSB.fX)) {
                DBG_NAV_LOGD("TextExtractor append space"
                    " mLast=(%d,%d,r=%d,b=%d) mLastGlyph=((%g,%g),(%g,%g),%d)"
                    " full=(%d,%d,r=%d,b=%d) rec=((%g,%g),(%g,%g),%d)"
                    " mMinSpaceWidth=%g",
                    mLast.fLeft, mLast.fTop, mLast.fRight, mLast.fBottom,
                    SkFixedToScalar(mLastGlyph.fLSB.fX),
                    SkFixedToScalar(mLastGlyph.fLSB.fY),
                    SkFixedToScalar(mLastGlyph.fRSB.fX),
                    SkFixedToScalar(mLastGlyph.fRSB.fY), mLastGlyph.fGlyphID,
                    full.fLeft, full.fTop, full.fRight, full.fBottom,
                    SkFixedToScalar(rec.fLSB.fX),
                    SkFixedToScalar(rec.fLSB.fY),
                    SkFixedToScalar(rec.fRSB.fX),
                    SkFixedToScalar(rec.fRSB.fY), rec.fGlyphID,
                    SkFixedToScalar(mMinSpaceWidth));
                *mSelectText.append() = ' ';
            } else
                mSkipFirstSpace = false;
            DBG_NAV_LOGD("TextExtractor [%02x] append full=(%d,%d,r=%d,b=%d)",
                rec.fGlyphID, full.fLeft, full.fTop, full.fRight, full.fBottom);
            SkPaint utfPaint = *mPaint;
            utfPaint.setTextEncoding(SkPaint::kUTF16_TextEncoding);
            utfPaint.glyphsToUnichars(&rec.fGlyphID, 1, &mLastUni);
            if (mLastUni) {
                uint16_t chars[2];
                size_t count = SkUTF16_FromUnichar(mLastUni, chars);
                *mSelectText.append() = chars[0];
                if (count == 2)
                    *mSelectText.append() = chars[1];
            }
            mLast = full;
            mLastGlyph = rec;
        } else {
            mSkipFirstSpace = true;
            DBG_NAV_LOGD("TextExtractor [%02x] skip full=(%d,%d,r=%d,b=%d)",
                rec.fGlyphID, full.fLeft, full.fTop, full.fRight, full.fBottom);
        }
        return false;
    }

    WebCore::String text() {
        return WebCore::String(mSelectText.begin(), mSelectText.count());
    }

protected:
    const SkRegion& mSelectRegion;
    SkTDArray<uint16_t> mSelectText;
    SkIRect mLast;
    SkBounder::GlyphRec mLastGlyph;
    SkUnichar mLastUni;
    SkFixed mMinSpaceWidth;
    bool mSkipFirstSpace;
private:
    typedef CommonCheck INHERITED;
};

class TextCanvas : public SkCanvas {
public:

    TextCanvas(CommonCheck* bounder, const SkPicture& picture, const SkIRect& area) 
            : mBounder(*bounder) {
        setBounder(bounder);
        SkBitmap bitmap;
        bitmap.setConfig(SkBitmap::kARGB_8888_Config, area.width(), 
            area.height());
        setBitmapDevice(bitmap);
        translate(SkIntToScalar(-area.fLeft), SkIntToScalar(-area.fTop));
    }

    virtual ~TextCanvas() {
        setBounder(NULL);
    }

    virtual void drawPaint(const SkPaint& paint) {
    }

    virtual void drawPoints(PointMode mode, size_t count, const SkPoint pts[],
                            const SkPaint& paint) {
    }

    virtual void drawRect(const SkRect& rect, const SkPaint& paint) {
    }

    virtual void drawPath(const SkPath& path, const SkPaint& paint) {
    }

    virtual void commonDrawBitmap(const SkBitmap& bitmap,
                              const SkMatrix& matrix, const SkPaint& paint) {
    }

    virtual void drawSprite(const SkBitmap& bitmap, int left, int top,
                            const SkPaint* paint = NULL) {
    }

    virtual void drawText(const void* text, size_t byteLength, SkScalar x, 
                          SkScalar y, const SkPaint& paint) {
        mBounder.setUp(paint, getTotalMatrix(), y, text);
        SkCanvas::drawText(text, byteLength, x, y, paint);
    }

    virtual void drawPosTextH(const void* text, size_t byteLength,
                              const SkScalar xpos[], SkScalar constY,
                              const SkPaint& paint) {
        mBounder.setUp(paint, getTotalMatrix(), constY, text);
        SkCanvas::drawPosTextH(text, byteLength, xpos, constY, paint);
    }

    virtual void drawVertices(VertexMode vmode, int vertexCount,
                              const SkPoint vertices[], const SkPoint texs[],
                              const SkColor colors[], SkXfermode* xmode,
                              const uint16_t indices[], int indexCount,
                              const SkPaint& paint) {
    }

    CommonCheck& mBounder;
};

void CopyPaste::buildSelection(const SkPicture& picture, const SkIRect& area,
        const SkIRect& selStart, const SkIRect& selEnd, SkRegion* region) {
    DBG_NAV_LOGD("area=(%d, %d, %d, %d) selStart=(%d, %d, %d, %d)"
        " selEnd=(%d, %d, %d, %d)", 
        area.fLeft, area.fTop, area.fRight, area.fBottom,
        selStart.fLeft, selStart.fTop, selStart.fRight, selStart.fBottom,
        selEnd.fLeft, selEnd.fTop, selEnd.fRight, selEnd.fBottom);
    MultilineBuilder builder(selStart, selEnd, area.fLeft, area.fTop, region);
    TextCanvas checker(&builder, picture, area);
    checker.drawPicture(const_cast<SkPicture&>(picture));
    region->translate(area.fLeft, area.fTop);
}

SkIRect CopyPaste::findClosest(const SkPicture& picture, const SkIRect& area,
        int x, int y) {
    FirstCheck _check(x - area.fLeft, y - area.fTop);
    DBG_NAV_LOGD("area=(%d, %d, %d, %d) x=%d y=%d", area.fLeft, area.fTop,
        area.fRight, area.fBottom, x, y);
    TextCanvas checker(&_check, picture, area);
    checker.drawPicture(const_cast<SkPicture&>(picture));
    _check.offsetBounds(area.fLeft, area.fTop);
    return _check.bestBounds();
}

WebCore::String CopyPaste::text(const SkPicture& picture, const SkIRect& area,
        const SkRegion& region) {
    SkRegion copy = region;
    copy.translate(-area.fLeft, -area.fTop);
    const SkIRect& bounds = copy.getBounds();
    DBG_NAV_LOGD("area=(%d, %d, %d, %d) region=(%d, %d, %d, %d)",
        area.fLeft, area.fTop, area.fRight, area.fBottom,
        bounds.fLeft, bounds.fTop, bounds.fRight, bounds.fBottom);
    TextExtractor extractor(copy);
    TextCanvas checker(&extractor, picture, area);
    checker.drawPicture(const_cast<SkPicture&>(picture));
    return extractor.text();
}

void SelectText::draw(SkCanvas* canvas, LayerAndroid* layer)
{
    if (layer->picture() != m_picture)
        return;
    if (m_drawRegion)
        drawSelectionRegion(canvas);
    if (m_drawPointer)
        drawSelectionPointer(canvas);
}

void SelectText::drawSelectionPointer(SkCanvas* canvas)
{
    SkPath path;
    if (m_extendSelection)
        getSelectionCaret(&path);
    else
        getSelectionArrow(&path);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(SK_ColorBLACK);
    SkPixelXorXfermode xorMode(SK_ColorWHITE);
    if (m_extendSelection)
        paint.setXfermode(&xorMode);
    else
        paint.setStrokeWidth(SK_Scalar1 * 2);
    int sc = canvas->save();
    canvas->scale(m_inverseScale, m_inverseScale);
    canvas->translate(SkIntToScalar(m_selectX), SkIntToScalar(m_selectY));
    canvas->drawPath(path, paint);
    if (!m_extendSelection) {
        paint.setStyle(SkPaint::kFill_Style);
        paint.setColor(SK_ColorWHITE);
        canvas->drawPath(path, paint);
    }
    canvas->restoreToCount(sc);
}

void SelectText::drawSelectionRegion(SkCanvas* canvas)
{
    m_selRegion.setEmpty();
    SkRect visBounds;
    if (!canvas->getClipBounds(&visBounds, SkCanvas::kAA_EdgeType))
        return;
    SkIRect ivisBounds;
    visBounds.round(&ivisBounds);
    CopyPaste::buildSelection(*m_picture, ivisBounds, m_selStart, m_selEnd,
        &m_selRegion);
    SkPath path;
    m_selRegion.getBoundaryPath(&path);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SkColorSetARGB(0x40, 255, 51, 204));
    canvas->drawPath(path, paint);
}

const String SelectText::getSelection()
{
    String result = CopyPaste::text(*m_picture, m_visibleRect, m_selRegion);
    DBG_NAV_LOGD("text=%s", result.latin1().data()); // uses CString
    return result;
}

void SelectText::getSelectionArrow(SkPath* path)
{
    const int arrow[] = {
        0, 14, 3, 11, 5, 15, 9, 15, 7, 11, 11, 11
    };
    for (unsigned index = 0; index < sizeof(arrow)/sizeof(arrow[0]); index += 2)
        path->lineTo(SkIntToScalar(arrow[index]), SkIntToScalar(arrow[index + 1]));
    path->close();
}

void SelectText::getSelectionCaret(SkPath* path)
{
    SkScalar height = SkIntToScalar(m_selStart.fBottom - m_selStart.fTop);
    SkScalar dist = height / 4;
    path->moveTo(0, -height / 2);
    path->rLineTo(0, height);
    path->rLineTo(-dist, dist);
    path->rMoveTo(0, -SK_Scalar1/2);
    path->rLineTo(dist * 2, 0);
    path->rMoveTo(0, SK_Scalar1/2);
    path->rLineTo(-dist, -dist);
}

void SelectText::moveSelection(const SkPicture* picture, int x, int y,
    bool extendSelection)
{
    if (!extendSelection)
        m_picture = picture;
    m_selEnd = CopyPaste::findClosest(*picture, m_visibleRect, x, y);
    if (!extendSelection)
        m_selStart = m_selEnd;
    DBG_NAV_LOGD("x=%d y=%d extendSelection=%s m_selStart=(%d, %d, %d, %d)"
        " m_selEnd=(%d, %d, %d, %d)", x, y, extendSelection ? "true" : "false",
        m_selStart.fLeft, m_selStart.fTop, m_selStart.fRight, m_selStart.fBottom,
        m_selEnd.fLeft, m_selEnd.fTop, m_selEnd.fRight, m_selEnd.fBottom);
}

}
