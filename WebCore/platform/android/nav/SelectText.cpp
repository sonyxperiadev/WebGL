/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *
 *     http://www.apache.org/licenses/LICENSE-2.0 
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License.
 */

#define LOG_TAG "webcoreglue"

#include <config.h>
#include "CachedDebug.h"
#include "SelectText.h"
#include "SkBitmap.h"
#include "SkBounder.h"
#include "SkCanvas.h"
#include "SkMatrix.h"
#include "SkPicture.h"
#include "SkPoint.h"
#include "SkRect.h"
#include "SkRegion.h"

#ifdef LOG
#undef LOG
#endif

#include <utils/Log.h>

class CommonCheck : public SkBounder {
public:
    CommonCheck() : mMatrix(NULL), mPaint(NULL) {}
    
    virtual void setUp(const SkPaint& paint, const SkMatrix& matrix, SkScalar y) {
        mMatrix = &matrix;
        mPaint = &paint;
        mY = y;
        mTop = INT_MAX;
        mBottom = INT_MAX;
    }
    
    int top() {
        if (mTop == INT_MAX) {
            SkPoint result;
            SkPaint::FontMetrics metrics;
            mPaint->getFontMetrics(&metrics);
            mMatrix->mapXY(0, metrics.fAscent + mY, &result);
            mTop = SkScalarFloor(result.fY);
            DBG_NAV_LOGD("mMatrix:(%d, %d) mTop=%g", 
                mMatrix->getTranslateX(), mMatrix->getTranslateY(), mTop);
        }
        return mTop;
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
    
protected:   
    const SkMatrix* mMatrix;
    const SkPaint* mPaint;
    int mBottom;
    int mTop;
    SkScalar mY;
};

class FirstCheck : public CommonCheck {
public:
    FirstCheck(int x, int y) 
            : mDistance(INT_MAX), mFocusX(x), mFocusY(y) {
        mBestBounds.setEmpty(); 
    }

    const SkIRect& bestBounds() { 
        DBG_NAV_LOGD("mBestBounds:(%d, %d, %d, %d) mTop=%g mBottom=%g", 
            mBestBounds.fLeft, mBestBounds.fTop, mBestBounds.fRight, 
            mBestBounds.fBottom, SkScalarToFloat(mTop), SkScalarToFloat(mBottom));
        return mBestBounds; 
    }
    
    void offsetBounds(int dx, int dy) {
        mBestBounds.offset(dx, dy);
    }
    
    virtual bool onIRect(const SkIRect& rect) {
        if (mBestBounds.isEmpty()) {
            mBestBounds.set(rect.fLeft, top(), rect.fRight, bottom());
        } else {
            int dx = ((rect.fLeft + rect.fRight) >> 1) - mFocusX;
            int dy = SkScalarRound(((top() + bottom()) / 2)) - mFocusY;
            int distance = dx * dx + dy * dy;
            if (mDistance > distance) {
                mDistance = distance;
                mBestBounds.set(rect.fLeft, top(), rect.fRight, bottom());
            }
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
            if (mLast.fTop < full.fBottom && mLast.fBottom > full.fTop) {
                if (full.fLeft > mLast.fRight)
                    full.fLeft = mLast.fRight;
                else if (full.fRight < mLast.fLeft)
                    full.fRight = mLast.fLeft;
            }
            mSelectRegion->op(full, SkRegion::kUnion_Op);
            mLast = full;
            if (mStart == mEnd)
                mCapture = false;
        }
        return false;
    }
protected:    
    SkIRect mStart;
    SkIRect mEnd;
    SkIRect mLast;
    SkRegion* mSelectRegion;
    bool mCapture;
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
        mBounder.setUp(paint, getTotalMatrix(), y);
        SkCanvas::drawText(text, byteLength, x, y, paint);
    }

    virtual void drawPosTextH(const void* text, size_t byteLength,
                              const SkScalar xpos[], SkScalar constY,
                              const SkPaint& paint) {
        mBounder.setUp(paint, getTotalMatrix(), constY);
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
