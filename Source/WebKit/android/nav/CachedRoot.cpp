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

#include "CachedPrefix.h"
#include "android_graphics.h"
#include "CachedHistory.h"
#include "CachedInput.h"
#include "CachedLayer.h"
#include "CachedNode.h"
#include "FindCanvas.h"
#include "FloatRect.h"
#include "LayerAndroid.h"
#include "ParseCanvas.h"
#include "SkBitmap.h"
#include "SkBounder.h"
#include "SkPixelRef.h"
#include "SkRegion.h"

#include "CachedRoot.h"

#if DEBUG_NAV_UI
#include "wtf/text/CString.h"
#endif

#define DONT_CENTER_IF_ALREADY_VISIBLE

using std::min;
using std::max;

#ifdef DUMP_NAV_CACHE_USING_PRINTF
    extern android::Mutex gWriteLogMutex;
#endif

namespace android {

class CommonCheck : public SkBounder {
public:
    enum Type {
        kNo_Type,
        kDrawBitmap_Type,
        kDrawGlyph_Type,
        kDrawPaint_Type,
        kDrawPath_Type,
        kDrawPicture_Type,
        kDrawPoints_Type,
        kDrawPosText_Type,
        kDrawPosTextH_Type,
        kDrawRect_Type,
        kDrawSprite_Type,
        kDrawText_Type,
        kDrawTextOnPath_Type,
        kPopLayer_Type,
        kPushLayer_Type,
        kPushSave_Type
    };

    static bool isTextType(Type t) {
        return t == kDrawPosTextH_Type || t == kDrawText_Type;
    }

    CommonCheck() : mType(kNo_Type), mAllOpaque(true), mIsOpaque(true) {
        setEmpty();
    }

    bool doRect(Type type) {
        mType = type;
        return doIRect(mUnion);
    }

    bool isEmpty() { return mUnion.isEmpty(); }

    bool joinGlyphs(const SkIRect& rect) {
        bool isGlyph = mType == kDrawGlyph_Type;
        if (isGlyph)
            mUnion.join(rect);
        return isGlyph;
    }

    void setAllOpaque(bool opaque) { mAllOpaque = opaque; }
    void setEmpty() { mUnion.setEmpty(); }
    void setIsOpaque(bool opaque) { mIsOpaque = opaque; }
    void setType(Type type) { mType = type; }

    Type mType;
    SkIRect mUnion;
    bool mAllOpaque;
    bool mIsOpaque;
};

#if DEBUG_NAV_UI
    static const char* TypeNames[] = {
        "kNo_Type",
        "kDrawBitmap_Type",
        "kDrawGlyph_Type",
        "kDrawPaint_Type",
        "kDrawPath_Type",
        "kDrawPicture_Type",
        "kDrawPoints_Type",
        "kDrawPosText_Type",
        "kDrawPosTextH_Type",
        "kDrawRect_Type",
        "kDrawSprite_Type",
        "kDrawText_Type",
        "kDrawTextOnPath_Type",
        "kPopLayer_Type",
        "kPushLayer_Type",
        "kPushSave_Type"
    };
#endif

#define kMargin 16
#define kSlop 2

class BoundsCanvas : public ParseCanvas {
public:

    BoundsCanvas(CommonCheck* bounder) : mBounder(*bounder) {
        mTransparentLayer = 0;
        setBounder(bounder);
    }

    virtual void drawPaint(const SkPaint& paint) {
        mBounder.setType(CommonCheck::kDrawPaint_Type);
        INHERITED::drawPaint(paint);
    }

    virtual void drawPoints(PointMode mode, size_t count, const SkPoint pts[],
                            const SkPaint& paint) {
        mBounder.setType(CommonCheck::kDrawPoints_Type);
        INHERITED::drawPoints(mode, count, pts, paint);
    }

    virtual void drawRect(const SkRect& rect, const SkPaint& paint) {
        mBounder.setType(CommonCheck::kDrawRect_Type);
        INHERITED::drawRect(rect, paint);
    }

    virtual void drawPath(const SkPath& path, const SkPaint& paint) {
        mBounder.setType(CommonCheck::kDrawPath_Type);
        INHERITED::drawPath(path, paint);
    }

    virtual void commonDrawBitmap(const SkBitmap& bitmap, const SkIRect* rect,
                              const SkMatrix& matrix, const SkPaint& paint) {
        mBounder.setType(CommonCheck::kDrawBitmap_Type);
        mBounder.setIsOpaque(bitmap.isOpaque());
        INHERITED::commonDrawBitmap(bitmap, rect, matrix, paint);
    }

    virtual void drawSprite(const SkBitmap& bitmap, int left, int top,
                            const SkPaint* paint) {
        mBounder.setType(CommonCheck::kDrawSprite_Type);
        mBounder.setIsOpaque(bitmap.isOpaque() &&
            (!paint || paint->getAlpha() == 255));
        INHERITED::drawSprite(bitmap, left, top, paint);
    }

    virtual void drawText(const void* text, size_t byteLength, SkScalar x,
                          SkScalar y, const SkPaint& paint) {
        mBounder.setEmpty();
        mBounder.setType(CommonCheck::kDrawGlyph_Type);
        INHERITED::drawText(text, byteLength, x, y, paint);
        mBounder.doRect(CommonCheck::kDrawText_Type);
    }

    virtual void drawPosText(const void* text, size_t byteLength,
                             const SkPoint pos[], const SkPaint& paint) {
        mBounder.setEmpty();
        mBounder.setType(CommonCheck::kDrawGlyph_Type);
        INHERITED::drawPosText(text, byteLength, pos, paint);
        if (!mBounder.isEmpty())
            mBounder.doRect(CommonCheck::kDrawPosText_Type);
    }

    virtual void drawPosTextH(const void* text, size_t byteLength,
                              const SkScalar xpos[], SkScalar constY,
                              const SkPaint& paint) {
        mBounder.setEmpty();
        mBounder.setType(CommonCheck::kDrawGlyph_Type);
        INHERITED::drawPosTextH(text, byteLength, xpos, constY, paint);
        if (mBounder.mUnion.isEmpty()) {
            DBG_NAV_LOGD("empty constY=%g", SkScalarToFloat(constY));
            return;
        }
        SkPaint::FontMetrics metrics;
        paint.getFontMetrics(&metrics);
        SkPoint upDown[2] = { {xpos[0], constY + metrics.fAscent},
            {xpos[0], constY + metrics.fDescent} };
        const SkMatrix& matrix = getTotalMatrix();
        matrix.mapPoints(upDown, 2);
        if (upDown[0].fX == upDown[1].fX) {
            mBounder.mUnion.fTop = SkScalarFloor(upDown[0].fY);
            mBounder.mUnion.fBottom = SkScalarFloor(upDown[1].fY);
        }
        mBounder.doRect(CommonCheck::kDrawPosTextH_Type);
    }

    virtual void drawTextOnPath(const void* text, size_t byteLength,
                                const SkPath& path, const SkMatrix* matrix,
                                const SkPaint& paint) {
        mBounder.setEmpty();
        mBounder.setType(CommonCheck::kDrawGlyph_Type);
        INHERITED::drawTextOnPath(text, byteLength, path, matrix, paint);
        mBounder.doRect(CommonCheck::kDrawTextOnPath_Type);
    }

    virtual void drawPicture(SkPicture& picture) {
        mBounder.setType(CommonCheck::kDrawPicture_Type);
        INHERITED::drawPicture(picture);
    }

    virtual int saveLayer(const SkRect* bounds, const SkPaint* paint,
                          SaveFlags flags) {
        int depth = INHERITED::saveLayer(bounds, paint, flags);
        if (mTransparentLayer == 0 && paint && paint->getAlpha() < 255) {
            mTransparentLayer = depth;
            mBounder.setAllOpaque(false);
        }
        return depth;
    }

    virtual void restore() {
        mBounder.setType(CommonCheck::kDrawSprite_Type); // for layer draws
        int depth = getSaveCount();
        if (depth == mTransparentLayer) {
            mTransparentLayer = 0;
            mBounder.setAllOpaque(true);
        }
        INHERITED::restore();
    }

    int mTransparentLayer;
    CommonCheck& mBounder;
private:
    typedef ParseCanvas INHERITED;
};

/*
LeftCheck examines the text in a picture, within a viewable rectangle,
and returns via left() the position of the left edge of the paragraph.
It first looks at the left edge of the test point, then looks above and below
it for more lines of text to determine the div's left edge.
*/
class LeftCheck : public CommonCheck {
public:
    LeftCheck(int x, int y) : mX(x), mY(y), mHitLeft(INT_MAX),
            mMostLeft(INT_MAX) {
        mHit.set(x - (HIT_SLOP << 1), y - HIT_SLOP, x, y + HIT_SLOP);
        mPartial.setEmpty();
        mBounds.setEmpty();
        mPartialType = kNo_Type;
    }

    int left() {
        if (isTextType(mType))
            doRect(); // process the final line of text
        return mMostLeft != INT_MAX ? mMostLeft : mX >> 1;
    }

    // FIXME: this is identical to CenterCheck::onIRect()
    // refactor so that LeftCheck and CenterCheck inherit common functions
    virtual bool onIRect(const SkIRect& rect) {
        bool opaqueBitmap = mType == kDrawBitmap_Type && mIsOpaque;
        if (opaqueBitmap && rect.contains(mX, mY)) {
            mMostLeft = rect.fLeft;
            return false;
        }
        if (joinGlyphs(rect)) // assembles glyphs into a text string
            return false;
        if (!isTextType(mType) && !opaqueBitmap)
            return false;
        /* Text on one line may be broken into several parts. Reassemble
           the text into a rectangle before considering it. */
        if (rect.fTop < mPartial.fBottom
                && rect.fBottom > mPartial.fTop
                && mPartial.fRight + JOIN_SLOP_X >= rect.fLeft
                && (mPartialType != kDrawBitmap_Type
                || mPartial.height() <= rect.height() + JOIN_SLOP_Y)) {
            DBG_NAV_LOGD("LeftCheck join mPartial=(%d, %d, %d, %d)"
                " rect=(%d, %d, %d, %d)",
                mPartial.fLeft, mPartial.fTop, mPartial.fRight, mPartial.fBottom,
                rect.fLeft, rect.fTop, rect.fRight, rect.fBottom);
            mPartial.join(rect);
            return false;
        }
        if (mPartial.isEmpty() == false) {
            doRect(); // process the previous line of text
#if DEBUG_NAV_UI
            if (mHitLeft == INT_MAX)
                DBG_NAV_LOGD("LeftCheck disabled rect=(%d, %d, %d, %d)",
                    rect.fLeft, rect.fTop, rect.fRight, rect.fBottom);
#endif
        }
        mPartial = rect;
        mPartialType = mType;
        return false;
    }

    void doRect()
    {
        /* Record the outer bounds of the lines of text that intersect the
           touch coordinates, given some slop */
        if (SkIRect::Intersects(mPartial, mHit)) {
            if (mHitLeft > mPartial.fLeft)
                mHitLeft = mPartial.fLeft;
            DBG_NAV_LOGD("LeftCheck mHitLeft=%d", mHitLeft);
        } else if (mHitLeft == INT_MAX)
            return; // wait for intersect success
        /* If text is too far away vertically, don't consider it */
        if (!mBounds.isEmpty() && (mPartial.fTop > mBounds.fBottom + HIT_SLOP
                || mPartial.fBottom < mBounds.fTop - HIT_SLOP)) {
            DBG_NAV_LOGD("LeftCheck stop mPartial=(%d, %d, %d, %d)"
                " mBounds=(%d, %d, %d, %d)",
                mPartial.fLeft, mPartial.fTop, mPartial.fRight, mPartial.fBottom,
                mBounds.fLeft, mBounds.fTop, mBounds.fRight, mBounds.fBottom);
            mHitLeft = INT_MAX; // and disable future comparisons
            return;
        }
        /* If the considered text is completely to the left or right of the
           touch coordinates, skip it, turn off further detection */
        if (mPartial.fLeft > mX || mPartial.fRight < mX) {
            DBG_NAV_LOGD("LeftCheck stop mX=%d mPartial=(%d, %d, %d, %d)", mX,
                mPartial.fLeft, mPartial.fTop, mPartial.fRight, mPartial.fBottom);
            mHitLeft = INT_MAX;
            return;
        }
        /* record the smallest margins on the left and right */
        if (mMostLeft > mPartial.fLeft) {
            DBG_NAV_LOGD("LeftCheck new mMostLeft=%d (old=%d)", mPartial.fLeft,
                mMostLeft);
            mMostLeft = mPartial.fLeft;
        }
        if (mBounds.isEmpty())
            mBounds = mPartial;
        else if (mPartial.fBottom > mBounds.fBottom) {
            DBG_NAV_LOGD("LeftCheck new bottom=%d (old=%d)", mPartial.fBottom,
                mBounds.fBottom);
            mBounds.fBottom = mPartial.fBottom;
        }
    }

    static const int JOIN_SLOP_X = 30; // horizontal space between text parts
    static const int JOIN_SLOP_Y = 5; // vertical space between text lines
    static const int HIT_SLOP = 30; // diameter allowing for tap size
    /* const */ SkIRect mHit; // sloppy hit rectangle
    SkIRect mBounds; // reference bounds
    SkIRect mPartial; // accumulated text bounds, per line
    const int mX; // touch location
    const int mY;
    int mHitLeft; // touched text extremes
    int mMostLeft; // paragraph extremes
    Type mPartialType;
};

/*
CenterCheck examines the text in a picture, within a viewable rectangle,
and returns via center() the optimal amount to scroll in x to display the
paragraph of text.

The caller of CenterCheck has configured (but not allocated) a bitmap
the height and three times the width of the view. The picture is drawn centered
in the bitmap, so text that would be revealed, if the view was scrolled up to
a view-width to the left or right, is considered.
*/
class CenterCheck : public CommonCheck {
public:
    CenterCheck(int x, int y, int width) : mX(x), mY(y),
            mHitLeft(x), mHitRight(x), mMostLeft(INT_MAX), mMostRight(-INT_MAX),
            mViewLeft(width), mViewRight(width << 1) {
        mHit.set(x - CENTER_SLOP, y - CENTER_SLOP,
            x + CENTER_SLOP, y + CENTER_SLOP);
        mPartial.setEmpty();
    }

    int center() {
        doRect(); // process the final line of text
        /* If the touch coordinates aren't near any text, return 0 */
        if (mHitLeft == mHitRight) {
            DBG_NAV_LOGD("abort: mHitLeft=%d ==mHitRight", mHitLeft);
            return 0;
        }
        int leftOver = mHitLeft - mViewLeft;
        int rightOver = mHitRight - mViewRight;
        int center;
        /* If the touched text is too large to entirely fit on the screen,
           center it. */
        if (leftOver < 0 && rightOver > 0) {
            center = (leftOver + rightOver) >> 1;
            DBG_NAV_LOGD("overlap: leftOver=%d rightOver=%d center=%d",
                leftOver, rightOver, center);
            return center;
        }
        center = (mMostLeft + mMostRight) >> 1; // the paragraph center
        if (leftOver > 0 && rightOver >= 0) { // off to the right
            if (center > mMostLeft) // move to center loses left-most text?
                center = mMostLeft;
        } else if (rightOver < 0 && leftOver <= 0) { // off to the left
            if (center < mMostRight) // move to center loses right-most text?
                center = mMostRight;
        } else {
#ifdef DONT_CENTER_IF_ALREADY_VISIBLE
            center = 0; // paragraph is already fully visible
#endif
        }
        DBG_NAV_LOGD("scroll: leftOver=%d rightOver=%d center=%d",
            leftOver, rightOver, center);
        return center;
    }

protected:
    virtual bool onIRect(const SkIRect& rect) {
        if (joinGlyphs(rect)) // assembles glyphs into a text string
            return false;
        if (!isTextType(mType))
            return false;
        /* Text on one line may be broken into several parts. Reassemble
           the text into a rectangle before considering it. */
        if (rect.fTop < mPartial.fBottom && rect.fBottom >
                mPartial.fTop && mPartial.fRight + CENTER_SLOP >= rect.fLeft) {
            DBG_NAV_LOGD("join mPartial=(%d, %d, %d, %d) rect=(%d, %d, %d, %d)",
                mPartial.fLeft, mPartial.fTop, mPartial.fRight, mPartial.fBottom,
                rect.fLeft, rect.fTop, rect.fRight, rect.fBottom);
            mPartial.join(rect);
            return false;
        }
        if (mPartial.isEmpty() == false)
            doRect(); // process the previous line of text
        mPartial = rect;
        return false;
    }

    void doRect()
    {
        /* Record the outer bounds of the lines of text that was 'hit' by the
           touch coordinates, given some slop */
        if (SkIRect::Intersects(mPartial, mHit)) {
            if (mHitLeft > mPartial.fLeft)
                mHitLeft = mPartial.fLeft;
            if (mHitRight < mPartial.fRight)
                mHitRight = mPartial.fRight;
            DBG_NAV_LOGD("mHitLeft=%d mHitRight=%d", mHitLeft, mHitRight);
        }
        /* If the considered text is completely to the left or right of the
           touch coordinates, skip it */
        if (mPartial.fLeft > mX || mPartial.fRight < mX)
            return;
        int leftOver = mPartial.fLeft - mViewLeft;
        int rightOver = mPartial.fRight - mViewRight;
        /* If leftOver <= 0, the text starts off the screen.
           If rightOver >= 0, the text ends off the screen.
        */
        if (leftOver <= 0 && rightOver >= 0) // discard wider than screen
            return;
#ifdef DONT_CENTER_IF_ALREADY_VISIBLE
        if (leftOver > 0 && rightOver < 0)   // discard already visible
            return;
#endif
        /* record the smallest margins on the left and right */
        if (mMostLeft > leftOver)
            mMostLeft = leftOver;
        if (mMostRight < rightOver)
            mMostRight = rightOver;
        DBG_NAV_LOGD("leftOver=%d rightOver=%d mMostLeft=%d mMostRight=%d",
            leftOver, rightOver, mMostLeft, mMostRight);
    }

    static const int CENTER_SLOP = 10; // space between text parts and lines
    /* const */ SkIRect mHit; // sloppy hit rectangle
    SkIRect mPartial; // accumulated text bounds, per line
    const int mX; // touch location
    const int mY;
    int mHitLeft; // touched text extremes
    int mHitRight;
    int mMostLeft; // paragraph extremes
    int mMostRight;
    const int mViewLeft; // middle third of 3x-wide view
    const int mViewRight;
};

class ImageCanvas : public ParseCanvas {
public:
    ImageCanvas(SkBounder* bounder) : mURI(NULL) {
        setBounder(bounder);
    }

    const char* getURI() { return mURI; }

protected:
// Currently webkit's bitmap draws always seem to be cull'd before this entry
// point is called, so we assume that any bitmap that gets here is inside our
// tiny clip (may not be true in the future)
    virtual void commonDrawBitmap(const SkBitmap& bitmap, const SkIRect* rect,
                              const SkMatrix& , const SkPaint& ) {
        SkPixelRef* pixelRef = bitmap.pixelRef();
        if (pixelRef != NULL) {
            mURI = pixelRef->getURI();
        }
    }

private:
    const char* mURI;
};

class ImageCheck : public SkBounder {
public:
    virtual bool onIRect(const SkIRect& rect) {
        return false;
    }
};

class JiggleCheck : public CommonCheck {
public:
    JiggleCheck(int delta, int width) : mDelta(delta), mMaxX(width) {
        mMaxJiggle = 0;
        mMinX = mMinJiggle = abs(delta);
        mMaxWidth = width + mMinX;
    }

    int jiggle() {
        if (mMinJiggle > mMaxJiggle)
            return mDelta;
        int avg = (mMinJiggle + mMaxJiggle + 1) >> 1;
        return mDelta < 0 ? -avg : avg;
    }

    virtual bool onIRect(const SkIRect& rect) {
        if (joinGlyphs(rect))
            return false;
        if (mType != kDrawBitmap_Type && !isTextType(mType))
            return false;
        int min, max;
        if (mDelta < 0) {
            min = mMinX - rect.fLeft;
            max = mMaxWidth - rect.fRight;
        } else {
            min = rect.fRight - mMaxX;
            max = rect.fLeft;
        }
        if (min <= 0)
            return false;
        if (max >= mMinX)
            return false;
        if (mMinJiggle > min)
            mMinJiggle = min;
        if (mMaxJiggle < max)
            mMaxJiggle = max;
        return false;
    }

    int mDelta;
    int mMaxJiggle;
    int mMaxX;
    int mMinJiggle;
    int mMinX;
    int mMaxWidth;
};

class RingCheck : public CommonCheck {
public:
    RingCheck(const WTF::Vector<WebCore::IntRect>& rings,
        const WebCore::IntRect& bitBounds, const WebCore::IntRect& testBounds,
        bool singleImage)
        : mTestBounds(testBounds)
        , mBitBounds(bitBounds)
        , mPushPop(false)
        , mSingleImage(singleImage)
    {
        const WebCore::IntRect* r;
        for (r = rings.begin(); r != rings.end(); r++) {
            SkIRect fatter = {r->x(), r->y(), r->maxX(), r->maxY()};
            fatter.inset(-CURSOR_RING_HIT_TEST_RADIUS, -CURSOR_RING_HIT_TEST_RADIUS);
            DBG_NAV_LOGD("RingCheck fat=(%d,%d,r=%d,b=%d)", fatter.fLeft, fatter.fTop,
                fatter.fRight, fatter.fBottom);
            mTextSlop.op(fatter, SkRegion::kUnion_Op);
            mTextTest.op(*r, SkRegion::kUnion_Op);
        }
        int dx = -bitBounds.x();
        int dy = -bitBounds.y();
        DBG_NAV_LOGD("RingCheck translate=(%d,%d)", dx, dy);
        mTextSlop.translate(dx, dy);
        mTextTest.translate(dx, dy);
        mTestBounds.translate(dx, dy);
        mEmpty.setEmpty();
    }

    bool hiddenRings(SkRegion* clipped)
    {
        findBestLayer();
        if (!mBestLayer) {
            DBG_NAV_LOG("RingCheck empty");
            clipped->setEmpty();
            return true;
        }
        const SkRegion* layersEnd = mLayers.end();
        const Type* layerTypes = &mLayerTypes[mBestLayer - mLayers.begin()];
        bool collectGlyphs = true;
        bool collectOvers = false;
        SkRegion over;
        for (const SkRegion* layers = mBestLayer; layers != layersEnd; layers++) {
            Type layerType = *layerTypes++;
            DBG_NAV_LOGD("RingCheck #%d %s (%d,%d,r=%d,b=%d)",
                layers - mLayers.begin(), TypeNames[layerType],
                layers->getBounds().fLeft, layers->getBounds().fTop,
                layers->getBounds().fRight, layers->getBounds().fBottom);
            if (collectGlyphs && (layerType == kDrawGlyph_Type
                || ((layerType == kDrawRect_Type && mTextTest.contains(*layers))
                || (layerType == kDrawBitmap_Type && mTextSlop.contains(*layers))))) {
                DBG_NAV_LOGD("RingCheck #%d collectOvers", layers - mLayers.begin());
                collectOvers = true;
                clipped->op(*layers, SkRegion::kUnion_Op);
                continue;
            }
            collectGlyphs &= layerType != kPushLayer_Type;
            if (collectOvers && (layerType == kDrawRect_Type
                || layerType == kDrawBitmap_Type
                || (!collectGlyphs && layerType == kDrawSprite_Type))) {
                DBG_NAV_LOGD("RingCheck #%d over.op", layers - mLayers.begin());
                over.op(*layers, SkRegion::kUnion_Op);
            }
        }
        bool result = !collectOvers || clipped->intersects(over);
        const SkIRect t = clipped->getBounds();
        const SkIRect o = over.getBounds();
        clipped->op(over, SkRegion::kDifference_Op);
        clipped->translate(mBitBounds.x(), mBitBounds.y());
        const SkIRect c = clipped->getBounds();
        DBG_NAV_LOGD("RingCheck intersects=%s text=(%d,%d,r=%d,b=%d)"
            " over=(%d,%d,r=%d,b=%d) clipped=(%d,%d,r=%d,b=%d)",
            result ? "true" : "false",
            t.fLeft, t.fTop, t.fRight, t.fBottom,
            o.fLeft, o.fTop, o.fRight, o.fBottom,
            c.fLeft, c.fTop, c.fRight, c.fBottom);
        return result;
    }

    void push(Type type, const SkIRect& bounds)
    {
#if DEBUG_NAV_UI
        // this caches the push string and subquently ignores if pushSave
        // is immediately followed by popLayer. Push/pop pairs happen
        // frequently and just add noise to the log.
        static String lastLog;
        String currentLog = String("RingCheck append #")
            + String::number(mLayers.size())
            + " type=" + TypeNames[type] + " bounds=("
            + String::number(bounds.fLeft)
            + "," + String::number(bounds.fTop) + ","
            + String::number(bounds.fRight) + ","
            + String::number(bounds.fBottom) + ")";
        if (lastLog.length() == 0 || type != kPopLayer_Type) {
            if (lastLog.length() != 0)
                DBG_NAV_LOGD("%s", lastLog.latin1().data());
            if (type == kPushSave_Type)
                lastLog = currentLog;
            else
                DBG_NAV_LOGD("%s", currentLog.latin1().data());
        } else
            lastLog = "";
#endif
        popEmpty();
        mPushPop |= type >= kPopLayer_Type;
        if (type == kPopLayer_Type) {
            Type last = mLayerTypes.last();
            // remove empty brackets
            if (last == kPushLayer_Type || last == kPushSave_Type) {
                mLayers.removeLast();
                mLayerTypes.removeLast();
                return;
            }
            // remove push/pop from push/bitmap/pop
            size_t pushIndex = mLayerTypes.size() - 2;
            if (last == kDrawBitmap_Type
                && mLayerTypes.at(pushIndex) == kPushLayer_Type) {
                mLayers.at(pushIndex) = mLayers.last();
                mLayerTypes.at(pushIndex) = kDrawBitmap_Type;
                mLayers.removeLast();
                mLayerTypes.removeLast();
                return;
            }
            // remove non-layer brackets
            int stack = 0;
            Type* types = mLayerTypes.end();
            while (types != mLayerTypes.begin()) {
                Type type = *--types;
                if (type == kPopLayer_Type) {
                    stack++;
                    continue;
                }
                if (type != kPushLayer_Type && type != kPushSave_Type)
                    continue;
                if (--stack >= 0)
                    continue;
                if (type == kPushLayer_Type)
                    break;
                int remove = types - mLayerTypes.begin();
                DBG_NAV_LOGD("RingCheck remove=%d mLayers.size=%d"
                    " mLayerTypes.size=%d", remove, mLayers.size(),
                    mLayerTypes.size());
                mLayers.remove(remove);
                mLayerTypes.remove(remove);
                mAppendLikeTypes = false;
                return;
            }
        }
        mLayers.append(bounds);
        mLayerTypes.append(type);
    }

    void startText(const SkPaint& paint)
    {
        mPaint = &paint;
        if (!mLayerTypes.isEmpty() && mLayerTypes.last() == kDrawGlyph_Type
            && !mLayers.last().isEmpty()) {
            push(kDrawGlyph_Type, mEmpty);
        }
    }

    bool textOutsideRings()
    {
        findBestLayer();
        if (!mBestLayer) {
            DBG_NAV_LOG("RingCheck empty");
            return false;
        }
        const SkRegion* layers = mBestLayer;
        const Type* layerTypes = &mLayerTypes[layers - mLayers.begin()];
        // back up to include text drawn before the best layer
        SkRegion active = SkRegion(mBitBounds);
        active.translate(-mBitBounds.x(), -mBitBounds.y());
        while (layers != mLayers.begin()) {
            --layers;
            Type layerType = *--layerTypes;
            DBG_NAV_LOGD("RingCheck #%d %s"
                " mTestBounds=(%d,%d,r=%d,b=%d) layers=(%d,%d,r=%d,b=%d)"
                " active=(%d,%d,r=%d,b=%d)",
                layers - mLayers.begin(), TypeNames[layerType],
                mTestBounds.getBounds().fLeft, mTestBounds.getBounds().fTop,
                mTestBounds.getBounds().fRight, mTestBounds.getBounds().fBottom,
                layers->getBounds().fLeft, layers->getBounds().fTop,
                layers->getBounds().fRight, layers->getBounds().fBottom,
                active.getBounds().fLeft, active.getBounds().fTop,
                active.getBounds().fRight, active.getBounds().fBottom);
            if (layerType == kDrawRect_Type || layerType == kDrawBitmap_Type) {
                SkRegion temp = *layers;
                temp.op(mTestBounds, SkRegion::kIntersect_Op);
                active.op(temp, SkRegion::kDifference_Op);
                if (active.isEmpty()) {
                    DBG_NAV_LOGD("RingCheck #%d empty", layers - mLayers.begin());
                    break;
                }
            } else if (layerType == kDrawGlyph_Type) {
                SkRegion temp = *layers;
                temp.op(active, SkRegion::kIntersect_Op);
                if (!mTestBounds.intersects(temp))
                    continue;
                if (!mTestBounds.contains(temp))
                    return false;
            } else
                break;
        }
        layers = mBestLayer;
        layerTypes = &mLayerTypes[layers - mLayers.begin()];
        bool foundGlyph = false;
        bool collectGlyphs = true;
        do {
            Type layerType = *layerTypes++;
            DBG_NAV_LOGD("RingCheck #%d %s mTestBounds=(%d,%d,r=%d,b=%d)"
            " layers=(%d,%d,r=%d,b=%d) collects=%s intersects=%s contains=%s",
                layers - mLayers.begin(), TypeNames[layerType],
                mTestBounds.getBounds().fLeft, mTestBounds.getBounds().fTop,
                mTestBounds.getBounds().fRight, mTestBounds.getBounds().fBottom,
                layers->getBounds().fLeft, layers->getBounds().fTop,
                layers->getBounds().fRight, layers->getBounds().fBottom,
                collectGlyphs ? "true" : "false",
                mTestBounds.intersects(*layers) ? "true" : "false",
                mTextSlop.contains(*layers) ? "true" : "false");
            if (collectGlyphs && layerType == kDrawGlyph_Type) {
                if (!mTestBounds.intersects(*layers))
                    continue;
                if (!mTextSlop.contains(*layers))
                    return false;
                foundGlyph = true;
            }
            collectGlyphs &= layerType != kPushLayer_Type;
        } while (++layers != mLayers.end());
        DBG_NAV_LOGD("RingCheck foundGlyph=%s", foundGlyph ? "true" : "false");
        return foundGlyph;
    }

protected:
    virtual bool onIRect(const SkIRect& rect)
    {
        joinGlyphs(rect);
        if (mType != kDrawGlyph_Type && mType != kDrawRect_Type
                && mType != kDrawSprite_Type && mType != kDrawBitmap_Type)
            return false;
        if (mLayerTypes.isEmpty() || mLayerTypes.last() != mType
            || !mAppendLikeTypes || mPushPop || mSingleImage
            // if the last and current were not glyphs,
            // and the two bounds have a gap between, don't join them -- push
            // an empty between them
            || (mType != kDrawGlyph_Type && !joinable(rect))) {
            push(mType, mEmpty);
        }
        DBG_NAV_LOGD("RingCheck join %s (%d,%d,r=%d,b=%d) '%c'",
            TypeNames[mType], rect.fLeft, rect.fTop, rect.fRight, rect.fBottom,
            mCh);
        mLayers.last().op(rect, SkRegion::kUnion_Op);
        mAppendLikeTypes = true;
        mPushPop = false;
        return false;
    }

    virtual bool onIRectGlyph(const SkIRect& rect,
        const SkBounder::GlyphRec& rec)
    {
        mCh = ' ';
        if (mPaint) {
            SkUnichar unichar;
            SkPaint utfPaint = *mPaint;
            utfPaint.setTextEncoding(SkPaint::kUTF16_TextEncoding);
            utfPaint.glyphsToUnichars(&rec.fGlyphID, 1, &unichar);
            mCh = unichar < 0x7f ? unichar : '?';
        }
        return onIRect(rect);
    }

private:
    int calcOverlap(SkRegion& testRegion)
    {
        if (testRegion.isEmpty())
            return INT_MAX;
        testRegion.op(mTextTest, SkRegion::kXOR_Op);
        SkRegion::Iterator iter(testRegion);
        int area = 0;
        while (!iter.done()) {
            const SkIRect& cr = iter.rect();
            area += cr.width() * cr.height();
            iter.next();
        }
        DBG_NAV_LOGD("RingCheck area=%d", area);
        return area;
    }

    void findBestLayer()
    {
        popEmpty();
        mBestLayer = 0;
        const SkRegion* layers = mLayers.begin();
        const SkRegion* layersEnd = mLayers.end();
        if (layers == layersEnd) {
            DBG_NAV_LOG("RingCheck empty");
            return;
        }
        // find text most like focus rings by xoring found with original
        int bestArea = INT_MAX;
        const SkRegion* testLayer = 0;
        SkRegion testRegion;
        const Type* layerTypes = &mLayerTypes[layers - mLayers.begin()];
        for (; layers != mLayers.end(); layers++) {
            Type layerType = *layerTypes++;
#if DEBUG_NAV_UI
            const SkIRect& gb = layers->getBounds();
            const SkIRect& tb = mTextSlop.getBounds();
            DBG_NAV_LOGD("RingCheck #%d %s mTextSlop=(%d,%d,%d,%d)"
                " contains=%s bounds=(%d,%d,%d,%d)",
                layers - mLayers.begin(), TypeNames[layerType],
                tb.fLeft, tb.fTop, tb.fRight, tb.fBottom,
                mTextSlop.contains(*layers) ? "true" : "false",
                gb.fLeft, gb.fTop, gb.fRight, gb.fBottom);
#endif
            if (((layerType == kDrawGlyph_Type || layerType == kDrawBitmap_Type)
                && mTextSlop.contains(*layers))
                || (layerType == kDrawRect_Type
                && mTextTest.contains(*layers))) {
                if (!testLayer)
                    testLayer = layers;
                testRegion.op(*layers, SkRegion::kUnion_Op);
                continue;
            }
            if (testLayer) {
                int area = calcOverlap(testRegion);
                if (bestArea > area) {
                    bestArea = area;
                    mBestLayer = testLayer;
                }
                DBG_NAV_LOGD("RingCheck #%d push test=%d best=%d",
                    layers - mLayers.begin(), testLayer - mLayers.begin(),
                    mBestLayer ? mBestLayer - mLayers.begin() : -1);
                testRegion.setEmpty();
                testLayer = 0;
            }
        }
        if (testLayer && bestArea > calcOverlap(testRegion)) {
            DBG_NAV_LOGD("RingCheck last best=%d", testLayer - mLayers.begin());
            mBestLayer = testLayer;
        }
    }

    bool joinable(const SkIRect& rect)
    {
        SkRegion region = mLayers.last();
        if (!region.isRect())
            return false;
        const SkIRect& bounds1 = region.getBounds();
        int area1 = bounds1.width() * bounds1.height();
        area1 += rect.width() * rect.height();
        region.op(rect, SkRegion::kUnion_Op);
        const SkIRect& bounds2 = region.getBounds();
        int area2 = bounds2.width() * bounds2.height();
        return area2 <= area1;
    }

    void popEmpty()
    {
        if (mLayerTypes.size() == 0)
            return;
        Type last = mLayerTypes.last();
        if (last >= kPopLayer_Type)
            return;
        const SkRegion& area = mLayers.last();
        if (!area.isEmpty())
            return;
        DBG_NAV_LOGD("RingCheck #%d %s", mLayers.size() - 1, TypeNames[last]);
        mLayers.removeLast();
        mLayerTypes.removeLast();
    }

    SkRegion mTestBounds;
    IntRect mBitBounds;
    SkIRect mEmpty;
    const SkRegion* mBestLayer;
    SkRegion mTextSlop; // outset rects for inclusion test
    SkRegion mTextTest; // exact rects for xor area test
    Type mLastType;
    Vector<SkRegion> mLayers;
    Vector<Type> mLayerTypes;
    const SkPaint* mPaint;
    char mCh;
    bool mAppendLikeTypes;
    bool mPushPop;
    bool mSingleImage;
};

class RingCanvas : public BoundsCanvas {
public:
    RingCanvas(RingCheck* bounder)
        : INHERITED(bounder)
    {
    }

protected:
    virtual void drawText(const void* text, size_t byteLength, SkScalar x,
                          SkScalar y, const SkPaint& paint) {
        static_cast<RingCheck&>(mBounder).startText(paint);
        INHERITED::drawText(text, byteLength, x, y, paint);
    }

    virtual void drawPosText(const void* text, size_t byteLength,
                             const SkPoint pos[], const SkPaint& paint) {
        static_cast<RingCheck&>(mBounder).startText(paint);
        INHERITED::drawPosText(text, byteLength, pos, paint);
    }

    virtual void drawTextOnPath(const void* text, size_t byteLength,
                                const SkPath& path, const SkMatrix* matrix,
                                const SkPaint& paint) {
        static_cast<RingCheck&>(mBounder).startText(paint);
        INHERITED::drawTextOnPath(text, byteLength, path, matrix, paint);
    }

    virtual void drawPosTextH(const void* text, size_t byteLength,
                              const SkScalar xpos[], SkScalar constY,
                              const SkPaint& paint) {
        static_cast<RingCheck&>(mBounder).startText(paint);
        INHERITED::drawPosTextH(text, byteLength, xpos, constY, paint);
    }

    virtual int save(SaveFlags flags)
    {
        RingCheck& bounder = static_cast<RingCheck&>(mBounder);
        bounder.push(CommonCheck::kPushSave_Type, getTotalClip().getBounds());
        return INHERITED::save(flags);
    }

    virtual int saveLayer(const SkRect* bounds, const SkPaint* paint,
        SaveFlags flags)
    {
        RingCheck& bounder = static_cast<RingCheck&>(mBounder);
        bounder.push(CommonCheck::kPushLayer_Type, getTotalClip().getBounds());
        return INHERITED::save(flags);
    }

    virtual void restore()
    {
        RingCheck& bounder = static_cast<RingCheck&>(mBounder);
        bounder.push(CommonCheck::kPopLayer_Type, getTotalClip().getBounds());
        INHERITED::restore();
    }

private:
    typedef BoundsCanvas INHERITED;
};

bool CachedRoot::adjustForScroll(BestData* best, CachedFrame::Direction direction,
    WebCore::IntPoint* scrollPtr, bool findClosest)
{
    WebCore::IntRect newOutset;
    const CachedNode* newNode = best->mNode;
    // see if there's a middle node
        // if the middle node is in the visited list,
        // or if none was computed and the newNode is in the visited list,
        // treat result as NULL
    if (newNode != NULL && findClosest) {
        if (best->bounds().intersects(mHistory->mPriorBounds) == false &&
                checkBetween(best, direction))
            newNode = best->mNode;
        if (findClosest && maskIfHidden(best)) {
            innerMove(document(), best, direction, scrollPtr, false);
            return true;
        }
        newOutset = newNode->cursorRingBounds(best->mFrame);
    }
    int delta;
    bool newNodeInView = scrollDelta(newOutset, direction, &delta);
    if (delta && scrollPtr && (newNode == NULL || newNodeInView == false ||
            (best->mNavOutside && best->mWorkingOutside)))
        *scrollPtr = WebCore::IntPoint(direction & UP_DOWN ? 0 : delta,
            direction & UP_DOWN ? delta : 0);
    return false;
}

void CachedRoot::calcBitBounds(const IntRect& nodeBounds, IntRect* bitBounds) const
{
    IntRect contentBounds = IntRect(0, 0, mPicture->width(), mPicture->height());
    IntRect overBounds = nodeBounds;
    overBounds.inflate(kMargin);
    IntRect viewableBounds = mScrolledBounds;
    viewableBounds.unite(mViewBounds);
    *bitBounds = contentBounds;
    bitBounds->intersect(overBounds);
    if (!bitBounds->intersects(viewableBounds))
        *bitBounds = IntRect(0, 0, 0, 0);
    DBG_NAV_LOGD("contentBounds=(%d,%d,r=%d,b=%d) overBounds=(%d,%d,r=%d,b=%d)"
        " mScrolledBounds=(%d,%d,r=%d,b=%d) mViewBounds=(%d,%d,r=%d,b=%d)"
        " bitBounds=(%d,%d,r=%d,b=%d)",
        contentBounds.x(), contentBounds.y(), contentBounds.maxX(),
        contentBounds.maxY(),
        overBounds.x(), overBounds.y(), overBounds.maxX(), overBounds.maxY(),
        mScrolledBounds.x(), mScrolledBounds.y(), mScrolledBounds.maxX(),
        mScrolledBounds.maxY(),
        mViewBounds.x(), mViewBounds.y(), mViewBounds.maxX(),
        mViewBounds.maxY(),
        bitBounds->x(), bitBounds->y(), bitBounds->maxX(),
        bitBounds->maxY());
}


int CachedRoot::checkForCenter(int x, int y) const
{
    int width = mViewBounds.width();
    SkPicture* picture = pictureAt(&x, &y);
    CenterCheck centerCheck(x + width - mViewBounds.x(), y - mViewBounds.y(),
        width);
    BoundsCanvas checker(&centerCheck);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width * 3,
        mViewBounds.height());
    checker.setBitmapDevice(bitmap);
    checker.translate(SkIntToScalar(width - mViewBounds.x()),
        SkIntToScalar(-mViewBounds.y()));
    checker.drawPicture(*picture);
    return centerCheck.center();
}

void CachedRoot::checkForJiggle(int* xDeltaPtr) const
{
    int xDelta = *xDeltaPtr;
    JiggleCheck jiggleCheck(xDelta, mViewBounds.width());
    BoundsCanvas checker(&jiggleCheck);
    SkBitmap bitmap;
    int absDelta = abs(xDelta);
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, mViewBounds.width() +
        absDelta, mViewBounds.height());
    checker.setBitmapDevice(bitmap);
    int x = -mViewBounds.x() - (xDelta < 0 ? xDelta : 0);
    int y = -mViewBounds.y();
    SkPicture* picture = pictureAt(&x, &y);
    checker.translate(SkIntToScalar(x), SkIntToScalar(y));
    checker.drawPicture(*picture);
    *xDeltaPtr = jiggleCheck.jiggle();
}

bool CachedRoot::checkRings(SkPicture* picture, const CachedNode* node,
        const WebCore::IntRect& testBounds) const
{
    if (!picture)
        return false;
    const WTF::Vector<WebCore::IntRect>& rings = node->rings();
    const WebCore::IntRect& nodeBounds = node->rawBounds();
    IntRect bitBounds;
    calcBitBounds(nodeBounds, &bitBounds);
    RingCheck ringCheck(rings, bitBounds, testBounds, node->singleImage());
    RingCanvas checker(&ringCheck);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, bitBounds.width(),
        bitBounds.height());
    checker.setBitmapDevice(bitmap);
    checker.translate(SkIntToScalar(-bitBounds.x()),
        SkIntToScalar(-bitBounds.y()));
    checker.drawPicture(*picture);
    bool result = ringCheck.textOutsideRings();
    DBG_NAV_LOGD("bitBounds=(%d,%d,r=%d,b=%d) nodeBounds=(%d,%d,r=%d,b=%d)"
        " testBounds=(%d,%d,r=%d,b=%d) success=%s",
        bitBounds.x(), bitBounds.y(), bitBounds.maxX(), bitBounds.maxY(),
        nodeBounds.x(), nodeBounds.y(), nodeBounds.maxX(), nodeBounds.maxY(),
        testBounds.x(), testBounds.y(), testBounds.maxX(), testBounds.maxY(),
        result ? "true" : "false");
    return result;
}

void CachedRoot::draw(FindCanvas& canvas) const
{
    canvas.setLayerId(-1); // overlays change the ID as their pictures draw
    canvas.drawPicture(*mPicture);
#if USE(ACCELERATED_COMPOSITING)
    if (!mRootLayer)
        return;
    canvas.drawLayers(mRootLayer);
#endif
}

const CachedNode* CachedRoot::findAt(const WebCore::IntRect& rect,
    const CachedFrame** framePtr, int* x, int* y, bool checkForHidden) const
{
#if DEBUG_NAV_UI
    DBG_NAV_LOGD("rect=(%d,%d,w=%d,h=%d) xy=(%d,%d)", rect.x(), rect.y(),
        rect.width(), rect.height(), *x, *y);
#if DUMP_NAV_CACHE
    if (mRootLayer) CachedLayer::Debug::printRootLayerAndroid(mRootLayer);
#endif
#endif
    int best = INT_MAX;
    bool inside = false;
    (const_cast<CachedRoot*>(this))->resetClippedOut();
    const CachedFrame* directHitFramePtr;
    const CachedNode* directHit = NULL;
    const CachedNode* node = findBestAt(rect, &best, &inside, &directHit,
        &directHitFramePtr, framePtr, x, y, checkForHidden);
    DBG_NAV_LOGD("node=%d (%p) xy=(%d,%d)", node == NULL ? 0 : node->index(),
        node == NULL ? NULL : node->nodePointer(), *x, *y);
    if (node == NULL) {
        node = findBestHitAt(rect, framePtr, x, y);
        DBG_NAV_LOGD("node=%d (%p)", node == NULL ? 0 : node->index(),
            node == NULL ? NULL : node->nodePointer());
    }
    if (node == NULL) {
        *framePtr = findBestFrameAt(rect.x() + (rect.width() >> 1),
            rect.y() + (rect.height() >> 1));
    }
    return node;
}

WebCore::IntPoint CachedRoot::cursorLocation() const
{
    const WebCore::IntRect& bounds = mHistory->mNavBounds;
    return WebCore::IntPoint(bounds.x() + (bounds.width() >> 1),
        bounds.y() + (bounds.height() >> 1));
}

WebCore::IntPoint CachedRoot::focusLocation() const
{
    return WebCore::IntPoint(mFocusBounds.x() + (mFocusBounds.width() >> 1),
        mFocusBounds.y() + (mFocusBounds.height() >> 1));
}

// These reset the values because we only want to get the selection the first time.
// After that, the selection is no longer accurate.
int CachedRoot::getAndResetSelectionEnd()
{
    int end = mSelectionEnd;
    mSelectionEnd = -1;
    return end;
}

int CachedRoot::getAndResetSelectionStart()
{
    int start = mSelectionStart;
    mSelectionStart = -1;
    return start;
}

int CachedRoot::getBlockLeftEdge(int x, int y, float scale) const
{
    DBG_NAV_LOGD("x=%d y=%d scale=%g mViewBounds=(%d,%d,%d,%d)", x, y, scale,
        mViewBounds.x(), mViewBounds.y(), mViewBounds.width(),
        mViewBounds.height());
    // if (x, y) is in a textArea or textField, return that
    const int slop = 1;
    WebCore::IntRect rect = WebCore::IntRect(x - slop, y - slop,
        slop * 2, slop * 2);
    const CachedFrame* frame;
    int fx, fy;
    const CachedNode* node = findAt(rect, &frame, &fx, &fy, true);
    if (node && node->wantsKeyEvents()) {
        DBG_NAV_LOGD("x=%d (%s)", node->bounds(frame).x(),
            node->isTextInput() ? "text" : "plugin");
        return node->bounds(frame).x();
    }
    SkPicture* picture = node ? frame->picture(node, &x, &y) : pictureAt(&x, &y);
    if (!picture)
        return x;
    int halfW = (int) (mViewBounds.width() * scale * 0.5f);
    int fullW = halfW << 1;
    int halfH = (int) (mViewBounds.height() * scale * 0.5f);
    int fullH = halfH << 1;
    LeftCheck leftCheck(fullW, halfH);
    BoundsCanvas checker(&leftCheck);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, fullW, fullH);
    checker.setBitmapDevice(bitmap);
    checker.translate(SkIntToScalar(fullW - x), SkIntToScalar(halfH - y));
    checker.drawPicture(*picture);
    int result = x + leftCheck.left() - fullW;
    DBG_NAV_LOGD("halfW=%d halfH=%d mMostLeft=%d x=%d",
        halfW, halfH, leftCheck.mMostLeft, result);
    return result;
}

void CachedRoot::getSimulatedMousePosition(WebCore::IntPoint* point) const
{
#ifndef NDEBUG
    ASSERT(CachedFrame::mDebug.mInUse);
#endif
    const WebCore::IntRect& mouseBounds = mHistory->mMouseBounds;
    int x = mouseBounds.x();
    int y = mouseBounds.y();
    int width = mouseBounds.width();
    int height = mouseBounds.height();
    point->setX(x + (width >> 1)); // default to box center
    point->setY(y + (height >> 1));
#if DEBUG_NAV_UI
    const WebCore::IntRect& navBounds = mHistory->mNavBounds;
    DBG_NAV_LOGD("mHistory->mNavBounds={%d,%d,%d,%d} "
        "mHistory->mMouseBounds={%d,%d,%d,%d} point={%d,%d}",
        navBounds.x(), navBounds.y(), navBounds.width(), navBounds.height(),
        mouseBounds.x(), mouseBounds.y(), mouseBounds.width(),
        mouseBounds.height(), point->x(), point->y());
#endif
}

void CachedRoot::init(WebCore::Frame* frame, CachedHistory* history)
{
    CachedFrame::init(this, -1, frame);
    reset();
    mHistory = history;
    mPicture = NULL;
}

bool CachedRoot::innerDown(const CachedNode* test, BestData* bestData) const
{
    ASSERT(minWorkingVertical() >= mViewBounds.x());
    ASSERT(maxWorkingVertical() <= mViewBounds.maxX());
    setupScrolledBounds();
    // (line up)
    mScrolledBounds.setHeight(mScrolledBounds.height() + mMaxYScroll);
    int testTop = mScrolledBounds.y();
    int viewBottom = mViewBounds.maxY();
    const WebCore::IntRect& navBounds = mHistory->mNavBounds;
    if (navBounds.isEmpty() == false &&
            navBounds.maxY() > viewBottom && viewBottom < mContents.height())
        return false;
    if (navBounds.isEmpty() == false) {
        int navTop = navBounds.y();
        int scrollBottom;
        if (testTop < navTop && navTop < (scrollBottom = mScrolledBounds.maxY())) {
            mScrolledBounds.setHeight(scrollBottom - navTop);
            mScrolledBounds.setY(navTop);
        }
    }
    setCursorCache(0, mMaxYScroll);
    frameDown(test, NULL, bestData);
    return true;
}

bool CachedRoot::innerLeft(const CachedNode* test, BestData* bestData) const
{
    ASSERT(minWorkingHorizontal() >= mViewBounds.y());
    ASSERT(maxWorkingHorizontal() <= mViewBounds.maxY());
    setupScrolledBounds();
    mScrolledBounds.setX(mScrolledBounds.x() - mMaxXScroll);
    mScrolledBounds.setWidth(mScrolledBounds.width() + mMaxXScroll);
    int testRight = mScrolledBounds.maxX();
    int viewLeft = mViewBounds.x();
    const WebCore::IntRect& navBounds = mHistory->mNavBounds;
    if (navBounds.isEmpty() == false &&
            navBounds.x() < viewLeft && viewLeft > mContents.x())
        return false;
    if (navBounds.isEmpty() == false) {
        int navRight = navBounds.maxX();
        int scrollLeft;
        if (testRight > navRight && navRight > (scrollLeft = mScrolledBounds.x()))
            mScrolledBounds.setWidth(navRight - scrollLeft);
    }
    setCursorCache(-mMaxXScroll, 0);
    frameLeft(test, NULL, bestData);
    return true;
}


void CachedRoot::innerMove(const CachedNode* node, BestData* bestData,
    Direction direction, WebCore::IntPoint* scroll, bool firstCall)
{
    bestData->reset();
    bool outOfCursor = mCursorIndex == CURSOR_CLEARED;
    DBG_NAV_LOGD("mHistory->didFirstLayout()=%s && mCursorIndex=%d",
        mHistory->didFirstLayout() ? "true" : "false", mCursorIndex);
    if (mHistory->didFirstLayout() && mCursorIndex < CURSOR_SET) {
        mHistory->reset();
        outOfCursor = true;
    }
    const CachedFrame* cursorFrame;
    const CachedNode* cursor = currentCursor(&cursorFrame);
    mHistory->setWorking(direction, cursorFrame, cursor, mViewBounds);
    bool findClosest = false;
    if (mScrollOnly == false) {
        switch (direction) {
            case LEFT:
                if (outOfCursor)
                    mHistory->mNavBounds = WebCore::IntRect(mViewBounds.maxX(),
                        mViewBounds.y(), 1, mViewBounds.height());
                findClosest = innerLeft(node, bestData);
                break;
            case RIGHT:
                if (outOfCursor)
                    mHistory->mNavBounds = WebCore::IntRect(mViewBounds.x() - 1,
                        mViewBounds.y(), 1, mViewBounds.height());
                findClosest = innerRight(node, bestData);
                break;
            case UP:
                if (outOfCursor)
                    mHistory->mNavBounds = WebCore::IntRect(mViewBounds.x(),
                        mViewBounds.maxY(), mViewBounds.width(), 1);
                findClosest = innerUp(node, bestData);
                break;
            case DOWN:
                if (outOfCursor)
                    mHistory->mNavBounds = WebCore::IntRect(mViewBounds.x(),
                        mViewBounds.y() - 1, mViewBounds.width(), 1);
                findClosest = innerDown(node, bestData);
                break;
            case UNINITIALIZED:
            default:
                ASSERT(0);
        }
    }
    if (firstCall)
        mHistory->mPriorBounds = mHistory->mNavBounds; // bounds always advances, even if new node is ultimately NULL
    bestData->setMouseBounds(bestData->bounds());
    if (adjustForScroll(bestData, direction, scroll, findClosest))
        return;
    if (bestData->mNode != NULL) {
        mHistory->addToVisited(bestData->mNode, direction);
        mHistory->mNavBounds = bestData->bounds();
        mHistory->mMouseBounds = bestData->mouseBounds();
    } else if (scroll->x() != 0 || scroll->y() != 0) {
        WebCore::IntRect newBounds = mHistory->mNavBounds;
        int offsetX = scroll->x();
        int offsetY = scroll->y();
        newBounds.move(offsetX, offsetY);
        if (mViewBounds.x() > newBounds.x())
            offsetX = mViewBounds.x() - mHistory->mNavBounds.x();
        else if (mViewBounds.maxX() < newBounds.maxX())
            offsetX = mViewBounds.maxX() - mHistory->mNavBounds.maxX();
        if (mViewBounds.y() > newBounds.y())
            offsetY = mViewBounds.y() - mHistory->mNavBounds.y();
        else if (mViewBounds.maxY() < newBounds.maxY())
            offsetY = mViewBounds.maxY() - mHistory->mNavBounds.maxY();
        mHistory->mNavBounds.move(offsetX, offsetY);
    }
    mHistory->setDidFirstLayout(false);
}

bool CachedRoot::innerRight(const CachedNode* test, BestData* bestData) const
{
    ASSERT(minWorkingHorizontal() >= mViewBounds.y());
    ASSERT(maxWorkingHorizontal() <= mViewBounds.maxY());
    setupScrolledBounds();
    // (align)
    mScrolledBounds.setWidth(mScrolledBounds.width() + mMaxXScroll);
    int testLeft = mScrolledBounds.x();
    int viewRight = mViewBounds.maxX();
    const WebCore::IntRect& navBounds = mHistory->mNavBounds;
    if (navBounds.isEmpty() == false &&
            navBounds.maxX() > viewRight && viewRight < mContents.width())
        return false;
    if (navBounds.isEmpty() == false) {
        int navLeft = navBounds.x();
        int scrollRight;
        if (testLeft < navLeft && navLeft < (scrollRight = mScrolledBounds.maxX())) {
            mScrolledBounds.setWidth(scrollRight - navLeft);
            mScrolledBounds.setX(navLeft);
        }
    }
    setCursorCache(mMaxXScroll, 0);
    frameRight(test, NULL, bestData);
    return true;
}

bool CachedRoot::innerUp(const CachedNode* test, BestData* bestData) const
{
    ASSERT(minWorkingVertical() >= mViewBounds.x());
    ASSERT(maxWorkingVertical() <= mViewBounds.maxX());
    setupScrolledBounds();
    mScrolledBounds.setY(mScrolledBounds.y() - mMaxYScroll);
    mScrolledBounds.setHeight(mScrolledBounds.height() + mMaxYScroll);
    int testBottom = mScrolledBounds.maxY();
    int viewTop = mViewBounds.y();
    const WebCore::IntRect& navBounds = mHistory->mNavBounds;
    if (navBounds.isEmpty() == false &&
            navBounds.y() < viewTop && viewTop > mContents.y())
        return false;
    if (navBounds.isEmpty() == false) {
        int navBottom = navBounds.maxY();
        int scrollTop;
        if (testBottom > navBottom && navBottom > (scrollTop = mScrolledBounds.y()))
            mScrolledBounds.setHeight(navBottom - scrollTop);
    }
    setCursorCache(0, -mMaxYScroll);
    frameUp(test, NULL, bestData);
    return true;
}

WTF::String CachedRoot::imageURI(int x, int y) const
{
    DBG_NAV_LOGD("x/y=(%d,%d)", x, y);
    ImageCheck imageCheck;
    ImageCanvas checker(&imageCheck);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
    checker.setBitmapDevice(bitmap);
    SkPicture* picture = pictureAt(&x, &y);
    checker.translate(SkIntToScalar(-x), SkIntToScalar(-y));
    checker.drawPicture(*picture);
    DBG_NAV_LOGD("uri=%s", checker.getURI());
    return WTF::String(checker.getURI());
}

bool CachedRoot::maskIfHidden(BestData* best) const
{
    const CachedNode* bestNode = best->mNode;
    if (bestNode->isUnclipped())
        return false;
    const CachedFrame* frame = best->mFrame;
    SkPicture* picture = frame->picture(bestNode);
    if (picture == NULL) {
        DBG_NAV_LOG("missing picture");
        return false;
    }
    Vector<IntRect> rings;
    bestNode->cursorRings(frame, &rings);
    const WebCore::IntRect& bounds = bestNode->bounds(frame);
    IntRect bitBounds;
    calcBitBounds(bounds, &bitBounds);
    RingCheck ringCheck(rings, bitBounds, bounds, bestNode->singleImage());
    RingCanvas checker(&ringCheck);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, bitBounds.width(),
        bitBounds.height());
    checker.setBitmapDevice(bitmap);
    checker.translate(SkIntToScalar(-bitBounds.x()),
        SkIntToScalar(-bitBounds.y()));
    checker.drawPicture(*picture);
    SkRegion clipRgn;
    bool clipped = ringCheck.hiddenRings(&clipRgn);
    CachedNode* node = const_cast<CachedNode*>(best->mNode);
    DBG_NAV_LOGD("clipped=%s clipRgn.isEmpty=%s", clipped ? "true" : "false",
        clipRgn.isEmpty() ? "true" : "false");
    if (clipped && clipRgn.isEmpty()) {
        node->setDisabled(true);
        IntRect clippedBounds = bounds;
        clippedBounds.intersect(bitBounds);
        node->setClippedOut(clippedBounds != bounds);
        return true;
    }
    // was it partially occluded by later drawing?
    // if partially occluded, modify the bounds so that the mouse click has a better x,y
    if (clipped) {
        DBG_NAV_LOGD("clipped clipRgn={%d,%d,r=%d,b=%d}",
            clipRgn.getBounds().fLeft, clipRgn.getBounds().fTop,
            clipRgn.getBounds().fRight, clipRgn.getBounds().fBottom);
        best->setMouseBounds(clipRgn.getBounds());
        if (!node->clip(best->mouseBounds())) {
            node->setDisabled(true);
            node->setClippedOut(true);
            return true;
        }
    } else
        node->fixUpCursorRects(frame);
    return false;
}

const CachedNode* CachedRoot::moveCursor(Direction direction, const CachedFrame** framePtr,
    WebCore::IntPoint* scroll)
{
#ifndef NDEBUG
    ASSERT(CachedFrame::mDebug.mInUse);
#endif
    CachedRoot* frame = this;
    const CachedNode* node = frame->document();
    if (node == NULL)
        return NULL;
    if (mViewBounds.isEmpty())
        return NULL;
    resetClippedOut();
    setData();
    BestData bestData;
    innerMove(node, &bestData, direction, scroll, true);
    // if node is partially or fully concealed by layer, scroll it into view
    if (mRootLayer && bestData.mNode && !bestData.mNode->isInLayer()) {
#if USE(ACCELERATED_COMPOSITING)
#if DUMP_NAV_CACHE
        CachedLayer::Debug::printRootLayerAndroid(mRootLayer);
#endif
        SkIRect original = bestData.mNode->cursorRingBounds(bestData.mFrame);
        DBG_NAV_LOGD("original=(%d,%d,w=%d,h=%d) scroll=(%d,%d)",
            original.fLeft, original.fTop, original.width(), original.height(),
            scroll->x(), scroll->y());
        original.offset(-scroll->x(), -scroll->y());
        SkRegion rings(original);
        SkTDArray<SkRect> region;
        mRootLayer->clipArea(&region);
        SkRegion layers;
        for (int index = 0; index < region.count(); index++) {
            SkIRect enclosing;
            region[index].round(&enclosing);
            rings.op(enclosing, SkRegion::kDifference_Op);
            layers.op(enclosing, SkRegion::kUnion_Op);
        }
        SkIRect layerBounds(layers.getBounds());
        SkIRect ringBounds(rings.getBounds());
        int scrollX = scroll->x();
        int scrollY = scroll->y();
        if (rings.getBounds() != original) {
            int topOverlap = layerBounds.fBottom - original.fTop;
            int bottomOverlap = original.fBottom - layerBounds.fTop;
            int leftOverlap = layerBounds.fRight - original.fLeft;
            int rightOverlap = original.fRight - layerBounds.fLeft;
            if (direction & UP_DOWN) {
                if (layerBounds.fLeft < original.fLeft && leftOverlap < 0)
                    scroll->setX(leftOverlap);
                if (original.fRight < layerBounds.fRight && rightOverlap > 0
                        && -leftOverlap > rightOverlap)
                    scroll->setX(rightOverlap);
                bool topSet = scrollY > topOverlap && (direction == UP
                    || !scrollY);
                if (topSet)
                    scroll->setY(topOverlap);
                if (scrollY < bottomOverlap && (direction == DOWN || (!scrollY
                        && (!topSet || -topOverlap > bottomOverlap))))
                    scroll->setY(bottomOverlap);
            } else {
                if (layerBounds.fTop < original.fTop && topOverlap < 0)
                    scroll->setY(topOverlap);
                if (original.fBottom < layerBounds.fBottom && bottomOverlap > 0
                        && -topOverlap > bottomOverlap)
                    scroll->setY(bottomOverlap);
                bool leftSet = scrollX > leftOverlap && (direction == LEFT
                    || !scrollX);
                if (leftSet)
                    scroll->setX(leftOverlap);
                if (scrollX < rightOverlap && (direction == RIGHT || (!scrollX
                        && (!leftSet || -leftOverlap > rightOverlap))))
                    scroll->setX(rightOverlap);
           }
            DBG_NAV_LOGD("rings=(%d,%d,w=%d,h=%d) layers=(%d,%d,w=%d,h=%d)"
                " scroll=(%d,%d)",
                ringBounds.fLeft, ringBounds.fTop, ringBounds.width(), ringBounds.height(),
                layerBounds.fLeft, layerBounds.fTop, layerBounds.width(), layerBounds.height(),
                scroll->x(), scroll->y());
        }
#endif
    }
    *framePtr = bestData.mFrame;
    return const_cast<CachedNode*>(bestData.mNode);
}

const CachedNode* CachedRoot::nextTextField(const CachedNode* start,
        const CachedFrame** framePtr) const
{
    bool startFound = false;
    return CachedFrame::nextTextField(start, framePtr, &startFound);
}

SkPicture* CachedRoot::pictureAt(int* xPtr, int* yPtr, int* id) const
{
#if USE(ACCELERATED_COMPOSITING)
    if (mRootLayer) {
        const LayerAndroid* layer = mRootLayer->find(xPtr, yPtr, mPicture);
        if (layer) {
            SkPicture* picture = layer->picture();
            DBG_NAV_LOGD("layer %d picture=%p (%d,%d)", layer->uniqueId(),
                picture, picture ? picture->width() : 0,
                picture ? picture->height() : 0);
            if (picture) {
                if (id)
                    *id = layer->uniqueId();
                return picture;
            }
        }
    }
#endif
    DBG_NAV_LOGD("root mPicture=%p (%d,%d)", mPicture, mPicture ?
        mPicture->width() : 0, mPicture ? mPicture->height() : 0);
    if (id)
        *id = -1;
    return mPicture;
}

void CachedRoot::reset()
{
#ifndef NDEBUG
    ASSERT(CachedFrame::mDebug.mInUse);
#endif
    mContents = mViewBounds = WebCore::IntRect(0, 0, 0, 0);
    mMaxXScroll = mMaxYScroll = 0;
    mRootLayer = 0;
    mSelectionStart = mSelectionEnd = -1;
    mScrollOnly = false;
}

bool CachedRoot::scrollDelta(WebCore::IntRect& newOutset, Direction direction, int* delta)
{
    switch (direction) {
        case LEFT:
            *delta = -mMaxXScroll;
            return newOutset.x() >= mViewBounds.x();
        case RIGHT:
            *delta = mMaxXScroll;
            return newOutset.maxX() <= mViewBounds.maxX();
        case UP:
            *delta = -mMaxYScroll;
            return newOutset.y() >= mViewBounds.y();
        case DOWN:
            *delta = mMaxYScroll;
            return newOutset.maxY() <= mViewBounds.maxY();
        default:
            *delta = 0;
            ASSERT(0);
    }
    return false;
}

void CachedRoot::setCachedFocus(CachedFrame* frame, CachedNode* node)
{
    mFocusBounds = WebCore::IntRect(0, 0, 0, 0);
    if (node == NULL)
        return;
    node->setIsFocus(true);
    mFocusBounds = node->bounds(frame);
    frame->setFocusIndex(node - frame->document());
    CachedFrame* parent;
    while ((parent = frame->parent()) != NULL) {
        parent->setFocusIndex(frame->indexInParent());
        frame = parent;
    }
#if DEBUG_NAV_UI
    const CachedFrame* focusFrame;
    const CachedNode* focus = currentFocus(&focusFrame);
    WebCore::IntRect bounds = WebCore::IntRect(0, 0, 0, 0);
    if (focus)
        bounds = focus->bounds(focusFrame);
    DBG_NAV_LOGD("new focus %d (nodePointer=%p) bounds={%d,%d,%d,%d}",
        focus ? focus->index() : 0,
        focus ? focus->nodePointer() : NULL, bounds.x(), bounds.y(),
        bounds.width(), bounds.height());
#endif
}

void CachedRoot::setCursor(CachedFrame* frame, CachedNode* node)
{
#if DEBUG_NAV_UI
    const CachedFrame* cursorFrame;
    const CachedNode* cursor = currentCursor(&cursorFrame);
    WebCore::IntRect bounds;
    if (cursor)
        bounds = cursor->bounds(cursorFrame);
    DBG_NAV_LOGD("old cursor %d (nodePointer=%p) bounds={%d,%d,%d,%d}",
        cursor ? cursor->index() : 0,
        cursor ? cursor->nodePointer() : NULL, bounds.x(), bounds.y(),
        bounds.width(), bounds.height());
#endif
    clearCursor();
    if (node == NULL)
        return;
    node->setIsCursor(true);
    node->show();
    frame->setCursorIndex(node - frame->document());
    CachedFrame* parent;
    while ((parent = frame->parent()) != NULL) {
        parent->setCursorIndex(frame->indexInParent());
        frame = parent;
    }
#if DEBUG_NAV_UI
    cursor = currentCursor(&cursorFrame);
    bounds = WebCore::IntRect(0, 0, 0, 0);
    if (cursor)
        bounds = cursor->bounds(cursorFrame);
    DBG_NAV_LOGD("new cursor %d (nodePointer=%p) bounds={%d,%d,%d,%d}",
        cursor ? cursor->index() : 0,
        cursor ? cursor->nodePointer() : NULL, bounds.x(), bounds.y(),
        bounds.width(), bounds.height());
#endif
}

void CachedRoot::setCursorCache(int scrollX, int scrollY) const
{
    mCursor = currentCursor();
    if (mCursor)
        mCursorBounds = mCursor->bounds(this);
    if (!mRootLayer)
        return;
    SkRegion baseScrolled(mScrolledBounds);
    mBaseUncovered = SkRegion(mScrolledBounds);
#if USE(ACCELERATED_COMPOSITING)
#if DUMP_NAV_CACHE
    CachedLayer::Debug::printRootLayerAndroid(mRootLayer);
#endif
    SkTDArray<SkRect> region;
    mRootLayer->clipArea(&region);
    WebCore::IntSize offset(
        copysign(min(max(0, mContents.width() - mScrolledBounds.width()),
        abs(scrollX)), scrollX),
        copysign(min(max(0, mContents.height() - mScrolledBounds.height()),
        abs(scrollY)), scrollY));
    bool hasOffset = offset.width() || offset.height();
    // restrict scrollBounds to that which is not under layer
    for (int index = 0; index < region.count(); index++) {
        SkIRect less;
        region[index].round(&less);
        DBG_NAV_LOGD("less=(%d,%d,w=%d,h=%d)", less.fLeft, less.fTop,
            less.width(), less.height());
        mBaseUncovered.op(less, SkRegion::kDifference_Op);
        if (!hasOffset)
            continue;
        less.offset(offset.width(), offset.height());
        baseScrolled.op(less, SkRegion::kDifference_Op);
    }
    if (hasOffset)
        mBaseUncovered.op(baseScrolled, SkRegion::kUnion_Op);
#endif
}

#if DUMP_NAV_CACHE

#define DEBUG_PRINT_BOOL(field) \
    DUMP_NAV_LOGD("// bool " #field "=%s;\n", b->field ? "true" : "false")

#define DEBUG_PRINT_RECT(field) \
    { const WebCore::IntRect& r = b->field; \
    DUMP_NAV_LOGD("// IntRect " #field "={%d, %d, %d, %d};\n", \
        r.x(), r.y(), r.width(), r.height()); }

CachedRoot* CachedRoot::Debug::base() const {
    CachedRoot* nav = (CachedRoot*) ((char*) this - OFFSETOF(CachedRoot, mDebug));
    return nav;
}

void CachedRoot::Debug::print() const
{
#ifdef DUMP_NAV_CACHE_USING_PRINTF
    gWriteLogMutex.lock();
    ASSERT(gNavCacheLogFile == NULL);
    gNavCacheLogFile = fopen(NAV_CACHE_LOG_FILE, "a");
#endif
    CachedRoot* b = base();
    b->CachedFrame::mDebug.print();
    b->mHistory->mDebug.print(b);
    DUMP_NAV_LOGD("// int mMaxXScroll=%d, mMaxYScroll=%d;\n",
        b->mMaxXScroll, b->mMaxYScroll);
    if (b->mRootLayer)
        CachedLayer::Debug::printRootLayerAndroid(b->mRootLayer);
#ifdef DUMP_NAV_CACHE_USING_PRINTF
    if (gNavCacheLogFile)
        fclose(gNavCacheLogFile);
    gNavCacheLogFile = NULL;
    gWriteLogMutex.unlock();
#endif
}

#endif

}
