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
#include "CachedNode.h"
#include "FindCanvas.h"
#include "FloatRect.h"
#include "LayerAndroid.h"
#include "SkBitmap.h"
#include "SkBounder.h"
#include "SkPixelRef.h"
#include "SkRegion.h"

#include "CachedRoot.h"

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
        kDrawTextOnPath_Type
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
        "kDrawTextOnPath_Type"
    };
#endif

#define kMargin 16
#define kSlop 2

class BoundsCheck : public CommonCheck {
public:
    BoundsCheck() {
        mAllDrawnIn.setEmpty();
        mLastAll.setEmpty();
        mLastOver.setEmpty();
    }

    static int Area(SkIRect test) {
        return test.width() * test.height();
    }

   void checkLast() {
        if (mAllDrawnIn.isEmpty())
            return;
        if (mLastAll.isEmpty() || Area(mLastAll) < Area(mAllDrawnIn)) {
            mLastAll = mAllDrawnIn;
            mDrawnOver.setEmpty();
        }
        mAllDrawnIn.setEmpty();
    }

    bool hidden() {
        return (mLastAll.isEmpty() && mLastOver.isEmpty()) ||
            mDrawnOver.contains(mBounds);
    }

    virtual bool onIRect(const SkIRect& rect) {
        if (joinGlyphs(rect))
            return false;
        bool interestingType = mType == kDrawBitmap_Type ||
            mType == kDrawRect_Type || isTextType(mType);
        if (SkIRect::Intersects(mBounds, rect) == false) {
            DBG_NAV_LOGD("BoundsCheck (no intersect) rect={%d,%d,%d,%d}"
                " mType=%s", rect.fLeft, rect.fTop, rect.fRight, rect.fBottom,
                TypeNames[mType]);
            if (interestingType)
                checkLast();
            return false;
        }
        if (interestingType == false)
            return false;
        if (!mDrawnOver.contains(rect) && (mBoundsSlop.contains(rect) ||
                (mBounds.fLeft == rect.fLeft && mBounds.fRight == rect.fRight &&
                mBounds.fTop >= rect.fTop && mBounds.fBottom <= rect.fBottom) ||
                (mBounds.fTop == rect.fTop && mBounds.fBottom == rect.fBottom &&
                mBounds.fLeft >= rect.fLeft && mBounds.fRight <= rect.fRight))) {
            mDrawnOver.setEmpty();
            mAllDrawnIn.join(rect);
            DBG_NAV_LOGD("BoundsCheck (contains) rect={%d,%d,%d,%d}"
                " mAllDrawnIn={%d,%d,%d,%d} mType=%s",
                rect.fLeft, rect.fTop, rect.fRight, rect.fBottom,
                mAllDrawnIn.fLeft, mAllDrawnIn.fTop, mAllDrawnIn.fRight,
                mAllDrawnIn.fBottom, TypeNames[mType]);
       } else {
            checkLast();
            if (!isTextType(mType)) {
                if (
#if 0
// should the opaqueness of the bitmap disallow its ability to draw over?
// not sure that this test is needed
                (mType != kDrawBitmap_Type ||
                        (mIsOpaque && mAllOpaque)) &&
#endif
                        mLastAll.isEmpty() == false)
                    mDrawnOver.op(rect, SkRegion::kUnion_Op);
            } else {
// FIXME
// sometimes the text is not drawn entirely inside the cursor area, even though
// it is the correct text. Until I figure out why, I allow text drawn at the
// end that is not covered up by something else to represent the link
// example that triggers this that should be figured out:
// http://cdn.labpixies.com/campaigns/blackjack/blackjack.html?lang=en&country=US&libs=assets/feature/core
// ( http://tinyurl.com/ywsyzb )
                mLastOver = rect;
            }
#if DEBUG_NAV_UI
        const SkIRect& drawnOver = mDrawnOver.getBounds();
        DBG_NAV_LOGD("(overlaps) rect={%d,%d,%d,%d}"
            " mDrawnOver={%d,%d,%d,%d} mType=%s mIsOpaque=%s mAllOpaque=%s",
            rect.fLeft, rect.fTop, rect.fRight, rect.fBottom,
            drawnOver.fLeft, drawnOver.fTop, drawnOver.fRight, drawnOver.fBottom,
            TypeNames[mType], mIsOpaque ? "true" : "false",
            mAllOpaque ? "true" : "false");
#endif
        }
        return false;
    }

    SkIRect mBounds;
    SkIRect mBoundsSlop;
    SkRegion mDrawnOver;
    SkIRect mLastOver;
    SkIRect mAllDrawnIn;
    SkIRect mLastAll;
};

class BoundsCanvas : public SkCanvas {
public:

    BoundsCanvas(CommonCheck* bounder) : mBounder(*bounder) {
        mTransparentLayer = 0;
        setBounder(bounder);
    }

    virtual ~BoundsCanvas() {
        setBounder(NULL);
    }

    virtual void drawPaint(const SkPaint& paint) {
        mBounder.setType(CommonCheck::kDrawPaint_Type);
        SkCanvas::drawPaint(paint);
    }

    virtual void drawPoints(PointMode mode, size_t count, const SkPoint pts[],
                            const SkPaint& paint) {
        mBounder.setType(CommonCheck::kDrawPoints_Type);
        SkCanvas::drawPoints(mode, count, pts, paint);
    }

    virtual void drawRect(const SkRect& rect, const SkPaint& paint) {
        mBounder.setType(CommonCheck::kDrawRect_Type);
        SkCanvas::drawRect(rect, paint);
    }

    virtual void drawPath(const SkPath& path, const SkPaint& paint) {
        mBounder.setType(CommonCheck::kDrawPath_Type);
        SkCanvas::drawPath(path, paint);
    }

    virtual void drawSprite(const SkBitmap& bitmap, int left, int top,
                            const SkPaint* paint = NULL) {
        mBounder.setType(CommonCheck::kDrawSprite_Type);
        mBounder.setIsOpaque(bitmap.isOpaque());
        SkCanvas::drawSprite(bitmap, left, top, paint);
    }

    virtual void drawText(const void* text, size_t byteLength, SkScalar x,
                          SkScalar y, const SkPaint& paint) {
        mBounder.setEmpty();
        mBounder.setType(CommonCheck::kDrawGlyph_Type);
        SkCanvas::drawText(text, byteLength, x, y, paint);
        mBounder.doRect(CommonCheck::kDrawText_Type);
    }

    virtual void drawPosText(const void* text, size_t byteLength,
                             const SkPoint pos[], const SkPaint& paint) {
        mBounder.setEmpty();
        mBounder.setType(CommonCheck::kDrawGlyph_Type);
        SkCanvas::drawPosText(text, byteLength, pos, paint);
        mBounder.doRect(CommonCheck::kDrawPosText_Type);
    }

    virtual void drawPosTextH(const void* text, size_t byteLength,
                              const SkScalar xpos[], SkScalar constY,
                              const SkPaint& paint) {
        mBounder.setEmpty();
        mBounder.setType(CommonCheck::kDrawGlyph_Type);
        SkCanvas::drawPosTextH(text, byteLength, xpos, constY, paint);
        if (mBounder.mUnion.isEmpty())
            return;
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
        SkCanvas::drawTextOnPath(text, byteLength, path, matrix, paint);
        mBounder.doRect(CommonCheck::kDrawTextOnPath_Type);
    }

    virtual void drawPicture(SkPicture& picture) {
        mBounder.setType(CommonCheck::kDrawPicture_Type);
        SkCanvas::drawPicture(picture);
    }

    virtual int saveLayer(const SkRect* bounds, const SkPaint* paint,
                          SaveFlags flags) {
        int depth = SkCanvas::saveLayer(bounds, paint, flags);
        if (mTransparentLayer == 0 && paint && paint->getAlpha() < 255) {
            mTransparentLayer = depth;
            mBounder.setAllOpaque(false);
        }
        return depth;
    }

    virtual void restore() {
        int depth = getSaveCount();
        if (depth == mTransparentLayer) {
            mTransparentLayer = 0;
            mBounder.setAllOpaque(true);
        }
        SkCanvas::restore();
    }

    int mTransparentLayer;
    CommonCheck& mBounder;
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
                && mPartial.fRight + SLOP >= rect.fLeft
                && (mPartialType != kDrawBitmap_Type
                || mPartial.height() <= rect.height() + HIT_SLOP)) {
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
        if (!mBounds.isEmpty() && (mPartial.fTop > mBounds.fBottom + SLOP
                || mPartial.fBottom < mBounds.fTop - SLOP)) {
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

    static const int HIT_SLOP = 5; // space between text parts and lines
    static const int SLOP = 30; // space between text parts and lines
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

class ImageCanvas : public SkCanvas {
public:
    ImageCanvas(SkBounder* bounder) : mURI(NULL) {
        setBounder(bounder);
    }

// Currently webkit's bitmap draws always seem to be cull'd before this entry
// point is called, so we assume that any bitmap that gets here is inside our
// tiny clip (may not be true in the future)
    virtual void commonDrawBitmap(const SkBitmap& bitmap,
                              const SkMatrix& , const SkPaint& ) {
        SkPixelRef* pixelRef = bitmap.pixelRef();
        if (pixelRef != NULL) {
            mURI = pixelRef->getURI();
        }
    }

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
            const WebCore::IntPoint& location) : mSuccess(true) {
        const WebCore::IntRect* r;
        for (r = rings.begin(); r != rings.end(); r++) {
            SkIRect fatter = {r->x(), r->y(), r->right(), r->bottom()};
            fatter.inset(-CURSOR_RING_HIT_TEST_RADIUS, -CURSOR_RING_HIT_TEST_RADIUS);
            DBG_NAV_LOGD("fat=(%d,%d,r=%d,b=%d)", fatter.fLeft, fatter.fTop,
                fatter.fRight, fatter.fBottom);
            mRings.op(fatter, SkRegion::kUnion_Op);
        }
        DBG_NAV_LOGD("translate=(%d,%d)", -location.x(), -location.y());
        mRings.translate(-location.x(), -location.y());
    }

    virtual bool onIRect(const SkIRect& rect) {
        if (mSuccess && mType == kDrawGlyph_Type) {
            DBG_NAV_LOGD("contains (%d,%d,r=%d,b=%d) == %s",
                rect.fLeft, rect.fTop, rect.fRight, rect.fBottom,
                mRings.contains(rect) ? "true" : "false");
            mSuccess &= mRings.contains(rect);
        }
        return false;
    }

    bool success() { return mSuccess; }
    SkRegion mRings;
    bool mSuccess;
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


int CachedRoot::checkForCenter(int x, int y) const
{
    int width = mViewBounds.width();
    CenterCheck centerCheck(x + width - mViewBounds.x(), y - mViewBounds.y(),
        width);
    BoundsCanvas checker(&centerCheck);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width * 3,
        mViewBounds.height());
    checker.setBitmapDevice(bitmap);
    checker.translate(SkIntToScalar(width - mViewBounds.x()),
        SkIntToScalar(-mViewBounds.y()));
    checker.drawPicture(*pictureAt(x, y));
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
    checker.translate(SkIntToScalar(x), SkIntToScalar(y));
    checker.drawPicture(*pictureAt(x, y));
    *xDeltaPtr = jiggleCheck.jiggle();
}

bool CachedRoot::checkRings(SkPicture* picture,
        const WTF::Vector<WebCore::IntRect>& rings,
        const WebCore::IntRect& bounds) const
{
    if (!picture)
        return false;
    RingCheck ringCheck(rings, bounds.location());
    BoundsCanvas checker(&ringCheck);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, bounds.width(),
        bounds.height());
    checker.setBitmapDevice(bitmap);
    checker.translate(SkIntToScalar(-bounds.x()), SkIntToScalar(-bounds.y()));
    checker.drawPicture(*picture);
    DBG_NAV_LOGD("bounds=(%d,%d,r=%d,b=%d) success=%s",
        bounds.x(), bounds.y(), bounds.right(), bounds.bottom(),
        ringCheck.success() ? "true" : "false");
    return ringCheck.success();
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
    int best = INT_MAX;
    bool inside = false;
    (const_cast<CachedRoot*>(this))->resetClippedOut();
    const CachedFrame* directHitFramePtr;
    const CachedNode* directHit = NULL;
    const CachedNode* node = findBestAt(rect, &best, &inside, &directHit,
        &directHitFramePtr, framePtr, x, y, checkForHidden);
    DBG_NAV_LOGD("node=%d (%p)", node == NULL ? 0 : node->index(),
        node == NULL ? NULL : node->nodePointer());
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
    SkPicture* picture = node ? frame->picture(node) : pictureAt(x, y);
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
    ASSERT(maxWorkingVertical() <= mViewBounds.right());
    setupScrolledBounds();
    // (line up)
    mScrolledBounds.setHeight(mScrolledBounds.height() + mMaxYScroll);
    int testTop = mScrolledBounds.y();
    int viewBottom = mViewBounds.bottom();
    const WebCore::IntRect& navBounds = mHistory->mNavBounds;
    if (navBounds.isEmpty() == false &&
            navBounds.bottom() > viewBottom && viewBottom < mContents.height())
        return false;
    if (navBounds.isEmpty() == false) {
        int navTop = navBounds.y();
        int scrollBottom;
        if (testTop < navTop && navTop < (scrollBottom = mScrolledBounds.bottom())) {
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
    ASSERT(maxWorkingHorizontal() <= mViewBounds.bottom());
    setupScrolledBounds();
    mScrolledBounds.setX(mScrolledBounds.x() - mMaxXScroll);
    mScrolledBounds.setWidth(mScrolledBounds.width() + mMaxXScroll);
    int testRight = mScrolledBounds.right();
    int viewLeft = mViewBounds.x();
    const WebCore::IntRect& navBounds = mHistory->mNavBounds;
    if (navBounds.isEmpty() == false &&
            navBounds.x() < viewLeft && viewLeft > mContents.x())
        return false;
    if (navBounds.isEmpty() == false) {
        int navRight = navBounds.right();
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
                    mHistory->mNavBounds = WebCore::IntRect(mViewBounds.right(),
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
                        mViewBounds.bottom(), mViewBounds.width(), 1);
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
        else if (mViewBounds.right() < newBounds.right())
            offsetX = mViewBounds.right() - mHistory->mNavBounds.right();
        if (mViewBounds.y() > newBounds.y())
            offsetY = mViewBounds.y() - mHistory->mNavBounds.y();
        else if (mViewBounds.bottom() < newBounds.bottom())
            offsetY = mViewBounds.bottom() - mHistory->mNavBounds.bottom();
        mHistory->mNavBounds.move(offsetX, offsetY);
    }
    mHistory->setDidFirstLayout(false);
}

bool CachedRoot::innerRight(const CachedNode* test, BestData* bestData) const
{
    ASSERT(minWorkingHorizontal() >= mViewBounds.y());
    ASSERT(maxWorkingHorizontal() <= mViewBounds.bottom());
    setupScrolledBounds();
    // (align)
    mScrolledBounds.setWidth(mScrolledBounds.width() + mMaxXScroll);
    int testLeft = mScrolledBounds.x();
    int viewRight = mViewBounds.right();
    const WebCore::IntRect& navBounds = mHistory->mNavBounds;
    if (navBounds.isEmpty() == false &&
            navBounds.right() > viewRight && viewRight < mContents.width())
        return false;
    if (navBounds.isEmpty() == false) {
        int navLeft = navBounds.x();
        int scrollRight;
        if (testLeft < navLeft && navLeft < (scrollRight = mScrolledBounds.right())) {
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
    ASSERT(maxWorkingVertical() <= mViewBounds.right());
    setupScrolledBounds();
    mScrolledBounds.setY(mScrolledBounds.y() - mMaxYScroll);
    mScrolledBounds.setHeight(mScrolledBounds.height() + mMaxYScroll);
    int testBottom = mScrolledBounds.bottom();
    int viewTop = mViewBounds.y();
    const WebCore::IntRect& navBounds = mHistory->mNavBounds;
    if (navBounds.isEmpty() == false &&
            navBounds.y() < viewTop && viewTop > mContents.y())
        return false;
    if (navBounds.isEmpty() == false) {
        int navBottom = navBounds.bottom();
        int scrollTop;
        if (testBottom > navBottom && navBottom > (scrollTop = mScrolledBounds.y()))
            mScrolledBounds.setHeight(navBottom - scrollTop);
    }
    setCursorCache(0, -mMaxYScroll);
    frameUp(test, NULL, bestData);
    return true;
}

WebCore::String CachedRoot::imageURI(int x, int y) const
{
    ImageCheck imageCheck;
    ImageCanvas checker(&imageCheck);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
    checker.setBitmapDevice(bitmap);
    checker.translate(SkIntToScalar(-x), SkIntToScalar(-y));
    checker.drawPicture(*pictureAt(x, y));
    return WebCore::String(checker.mURI);
}

bool CachedRoot::maskIfHidden(BestData* best) const
{
    const CachedNode* bestNode = best->mNode;
    if (bestNode->isUnclipped() || bestNode->isTransparent())
        return false;
    const CachedFrame* frame = best->mFrame;
    SkPicture* picture = frame->picture(bestNode);
    if (picture == NULL) {
        DBG_NAV_LOG("missing picture");
        return false;
    }
    // given the picture matching this nav cache
        // create an SkBitmap with dimensions of the cursor intersected w/ extended view
    const WebCore::IntRect& nodeBounds = bestNode->bounds(frame);
    WebCore::IntRect bounds = nodeBounds;
    bounds.intersect(mScrolledBounds);
    int leftMargin = bounds.x() == nodeBounds.x() ? kMargin : 0;
    int topMargin = bounds.y() == nodeBounds.y() ? kMargin : 0;
    int rightMargin = bounds.right() == nodeBounds.right() ? kMargin : 0;
    int bottomMargin = bounds.bottom() == nodeBounds.bottom() ? kMargin : 0;
    bool unclipped = (leftMargin & topMargin & rightMargin & bottomMargin) != 0;
    WebCore::IntRect marginBounds = nodeBounds;
    marginBounds.inflate(kMargin);
    marginBounds.intersect(mScrolledBounds);
    SkScalar offsetX = SkIntToScalar(leftMargin - bounds.x());
    SkScalar offsetY = SkIntToScalar(topMargin - bounds.y());
#if USE(ACCELERATED_COMPOSITING)
    // When cached nodes are constructed in CacheBuilder.cpp, their
    // unclipped attribute is set so this condition won't be reached.
    // In the future, layers may contain nodes that can be clipped.
    // So to be safe, adjust the layer picture by its offset.
    if (bestNode->isInLayer()) {
        const CachedLayer* cachedLayer = frame->layer(bestNode);
        const LayerAndroid* layer = cachedLayer->layer(mRootLayer);
        SkMatrix pictMatrix;
        layer->localToGlobal(&pictMatrix);
        // FIXME: ignore scale, rotation for now
        offsetX += pictMatrix.getTranslateX();
        offsetY += pictMatrix.getTranslateY();
        DBG_NAV_LOGD("layer picture=%p (%g,%g)", picture,
            pictMatrix.getTranslateX(), pictMatrix.getTranslateY());
    }
#endif
    BoundsCheck boundsCheck;
    BoundsCanvas checker(&boundsCheck);
    boundsCheck.mBounds.set(leftMargin, topMargin,
        leftMargin + bounds.width(), topMargin + bounds.height());
    boundsCheck.mBoundsSlop = boundsCheck.mBounds;
    boundsCheck.mBoundsSlop.inset(-kSlop, -kSlop);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, marginBounds.width(),
        marginBounds.height());
    checker.setBitmapDevice(bitmap);
    // insert probes to be called when the data corresponding to this ring is drawn
        // need to know if ring was generated by text, image, or parent (like div)
        // ? need to know (like imdb menu bar) to give up sometimes (when?)
    checker.translate(offsetX, offsetY);
    checker.drawPicture(*picture);
    boundsCheck.checkLast();
    // was it not drawn or clipped out?
    CachedNode* node = const_cast<CachedNode*>(best->mNode);
    if (boundsCheck.hidden()) { // if hidden, return false so that nav can try again
#if DEBUG_NAV_UI
        const SkIRect& m = boundsCheck.mBounds;
        const SkIRect& s = boundsCheck.mBoundsSlop;
        DBG_NAV_LOGD("hidden node:%p (%d) mBounds={%d,%d,%d,%d} mBoundsSlop="
            "{%d,%d,%d,%d}", node, node->index(),
            m.fLeft, m.fTop, m.fRight, m.fBottom,
            s.fLeft, s.fTop, s.fRight, s.fBottom);
        const SkIRect& o = boundsCheck.mDrawnOver.getBounds();
        const SkIRect& l = boundsCheck.mLastAll;
        const SkIRect& u = boundsCheck.mUnion;
        DBG_NAV_LOGD("hidden mDrawnOver={%d,%d,%d,%d} mLastAll={%d,%d,%d,%d}"
            " mUnion={%d,%d,%d,%d}",
            o.fLeft, o.fTop, o.fRight, o.fBottom,
            l.fLeft, l.fTop, l.fRight, l.fBottom,
            u.fLeft, u.fTop, u.fRight, u.fBottom);
        const SkIRect& a = boundsCheck.mAllDrawnIn;
        const WebCore::IntRect& c = mScrolledBounds;
        const WebCore::IntRect& b = nodeBounds;
        DBG_NAV_LOGD("hidden mAllDrawnIn={%d,%d,%d,%d}"
            " mScrolledBounds={%d,%d,%d,%d} nodeBounds={%d,%d,%d,%d}",
            a.fLeft, a.fTop, a.fRight, a.fBottom,
            c.x(), c.y(), c.right(), c.bottom(),
            b.x(), b.y(), b.right(), b.bottom());
        DBG_NAV_LOGD("bits.mWidth=%d bits.mHeight=%d transX=%d transY=%d",
            marginBounds.width(),marginBounds.height(),
            kMargin - bounds.x(), kMargin - bounds.y());
#endif
        node->setDisabled(true);
        node->setClippedOut(unclipped == false);
        return true;
    }
    // was it partially occluded by later drawing?
    // if partially occluded, modify the bounds so that the mouse click has a better x,y
       const SkIRect& over = boundsCheck.mDrawnOver.getBounds();
    if (over.isEmpty() == false) {
#if DEBUG_NAV_UI
        SkIRect orig = boundsCheck.mBounds;
#endif
        SkIRect& base = boundsCheck.mBounds;
        if (base.fLeft < over.fRight && base.fRight > over.fRight)
            base.fLeft = over.fRight;
        else if (base.fRight > over.fLeft && base.fLeft < over.fLeft)
            base.fRight = over.fLeft;
        if (base.fTop < over.fBottom && base.fBottom > over.fBottom)
            base.fTop = over.fBottom;
        else if (base.fBottom > over.fTop && base.fTop < over.fTop)
            base.fBottom = over.fTop;
#if DEBUG_NAV_UI
        const SkIRect& modded = boundsCheck.mBounds;
        DBG_NAV_LOGD("partially occluded node:%p (%d) old:{%d,%d,%d,%d}"
            " new:{%d,%d,%d,%d}", node, node->index(),
            orig.fLeft, orig.fTop, orig.fRight, orig.fBottom,
            base.fLeft, base.fTop, base.fRight, base.fBottom);
#endif
        best->setMouseBounds(WebCore::IntRect(bounds.x() + base.fLeft - kMargin,
            bounds.y() + base.fTop - kMargin, base.width(), base.height()));
        node->clip(best->mouseBounds());
    }
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

SkPicture* CachedRoot::pictureAt(int x, int y) const
{
#if USE(ACCELERATED_COMPOSITING)
    if (mRootLayer) {
        const LayerAndroid* layer = mRootLayer->find(x, y);
        if (layer) {
            SkPicture* picture = layer->picture();
            if (picture)
                return picture;
        }
    }
#endif
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
            return newOutset.right() <= mViewBounds.right();
        case UP:
            *delta = -mMaxYScroll;
            return newOutset.y() >= mViewBounds.y();
        case DOWN:
            *delta = mMaxYScroll;
            return newOutset.bottom() <= mViewBounds.bottom();
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
#ifdef DUMP_NAV_CACHE_USING_PRINTF
    if (gNavCacheLogFile)
        fclose(gNavCacheLogFile);
    gNavCacheLogFile = NULL;
    gWriteLogMutex.unlock();
#endif
}

#endif

}
