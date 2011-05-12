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

#define LOG_TAG "webviewglue"

#include "CachedPrefix.h"
#include "BidiResolver.h"
#include "CachedRoot.h"
#include "LayerAndroid.h"
#include "SelectText.h"
#include "SkBitmap.h"
#include "SkBounder.h"
#include "SkCanvas.h"
#include "SkGradientShader.h"
#include "SkMatrix.h"
#include "SkPicture.h"
#include "SkPixelXorXfermode.h"
#include "SkPoint.h"
#include "SkRect.h"
#include "SkRegion.h"
#include "SkUtils.h"
#include "TextRun.h"

#ifdef DEBUG_NAV_UI
#include "CString.h"
#endif

#define VERBOSE_LOGGING 0
// #define EXTRA_NOISY_LOGGING 1

// TextRunIterator has been copied verbatim from GraphicsContext.cpp
namespace WebCore {

class TextRunIterator {
public:
    TextRunIterator()
        : m_textRun(0)
        , m_offset(0)
    {
    }

    TextRunIterator(const TextRun* textRun, unsigned offset)
        : m_textRun(textRun)
        , m_offset(offset)
    {
    }

    TextRunIterator(const TextRunIterator& other)
        : m_textRun(other.m_textRun)
        , m_offset(other.m_offset)
    {
    }

    unsigned offset() const { return m_offset; }
    void increment() { m_offset++; }
    bool atEnd() const { return !m_textRun || m_offset >= m_textRun->length(); }
    UChar current() const { return (*m_textRun)[m_offset]; }
    WTF::Unicode::Direction direction() const { return atEnd() ? WTF::Unicode::OtherNeutral : WTF::Unicode::direction(current()); }

    bool operator==(const TextRunIterator& other)
    {
        return m_offset == other.m_offset && m_textRun == other.m_textRun;
    }

    bool operator!=(const TextRunIterator& other) { return !operator==(other); }

private:
    const TextRun* m_textRun;
    int m_offset;
};

// ReverseBidi is a trimmed-down version of GraphicsContext::drawBidiText()
void ReverseBidi(UChar* chars, int len) {
    using namespace WTF::Unicode;
    WTF::Vector<UChar> result;
    result.reserveCapacity(len);
    TextRun run(chars, len);
    BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
    bidiResolver.setStatus(BidiStatus(LeftToRight, LeftToRight, LeftToRight,
        BidiContext::create(0, LeftToRight, false)));
    bidiResolver.setPosition(TextRunIterator(&run, 0));
    bidiResolver.createBidiRunsForLine(TextRunIterator(&run, len));
    if (!bidiResolver.runCount())
        return;
    BidiCharacterRun* bidiRun = bidiResolver.firstRun();
    while (bidiRun) {
        int bidiStart = bidiRun->start();
        int bidiStop = bidiRun->stop();
        int size = result.size();
        int bidiCount = bidiStop - bidiStart;
        result.append(chars + bidiStart, bidiCount);
        if (bidiRun->level() % 2) {
            UChar* start = &result[size];
            UChar* end = start + bidiCount;
            // reverse the order of any RTL substrings
            while (start < end) {
                UChar temp = *start;
                *start++ = *--end;
                *end = temp;
            }
            start = &result[size];
            end = start + bidiCount - 1;
            // if the RTL substring had a surrogate pair, restore its order
            while (start < end) {
                UChar trail = *start++;
                if (!U16_IS_SURROGATE(trail))
                    continue;
                start[-1] = *start; // lead
                *start++ = trail;
            }
        }
        bidiRun = bidiRun->next();
    }
    bidiResolver.deleteRuns();
    memcpy(chars, &result[0], len * sizeof(UChar));
}

}

namespace android {

/* SpaceBounds and SpaceCanvas are used to measure the left and right side
 * bearings of two consecutive glyphs to help determine if the glyphs were
 * originally laid out with a space character between the glyphs.
 */
class SpaceBounds : public SkBounder {
public:
    virtual bool onIRectGlyph(const SkIRect& , const SkBounder::GlyphRec& rec)
    {
        mFirstGlyph = mLastGlyph;
        mLastGlyph = rec;
        return false;
    }

    SkBounder::GlyphRec mFirstGlyph;
    SkBounder::GlyphRec mLastGlyph;
};

class SpaceCanvas : public SkCanvas {
public:
    SpaceCanvas(const SkIRect& area)
    {
        setBounder(&mBounder);
        SkBitmap bitmap;
        bitmap.setConfig(SkBitmap::kARGB_8888_Config, area.width(),
            area.height());
        setBitmapDevice(bitmap);
        translate(SkIntToScalar(-area.fLeft), SkIntToScalar(-area.fTop));
    }

    SpaceBounds mBounder;
};

#define HYPHEN_MINUS 0x2D // ASCII hyphen
#define SOLIDUS 0x2F // ASCII slash
#define REVERSE_SOLIDUS 0x5C // ASCII backslash
#define HYPHEN 0x2010 // unicode hyphen, first in range of dashes
#define HORZ_BAR 0x2015 // unicode horizontal bar, last in range of dashes
#define TOUCH_SLOP 10 // additional distance from character rect when hit

class CommonCheck : public SkBounder {
public:
    CommonCheck(int width, int height)
        : mHeight(height)
        , mLastUni(0)
        , mMatrix(0)
        , mPaint(0)
        , mWidth(width)
    {
        mLastGlyph.fGlyphID = static_cast<uint16_t>(-1);
        reset();
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

#if DEBUG_NAV_UI
    // make current (possibily uncomputed) value visible for debugging
    int bottomDebug() const
    {
        return mBottom;
    }
#endif

    bool addNewLine(const SkBounder::GlyphRec& rec)
    {
        SkFixed lineSpacing = SkFixedAbs(mLastGlyph.fLSB.fY - rec.fLSB.fY);
        SkFixed lineHeight = SkIntToFixed(bottom() - top());
        return lineSpacing >= lineHeight + (lineHeight >> 1); // 1.5
    }

    bool addSpace(const SkBounder::GlyphRec& rec)
    {
        bool newBaseLine = mLastGlyph.fLSB.fY != rec.fLSB.fY;
        if (((mLastUni >= HYPHEN && mLastUni <= HORZ_BAR)
            || mLastUni == HYPHEN_MINUS || mLastUni == SOLIDUS
            || mLastUni == REVERSE_SOLIDUS) && newBaseLine)
        {
            return false;
        }
        return isSpace(rec);
    }

    void finishGlyph()
    {
        mLastGlyph = mLastCandidate;
        mLastUni = mLastUniCandidate;
    }

    SkUnichar getUniChar(const SkBounder::GlyphRec& rec)
    {
        SkUnichar unichar;
        SkPaint utfPaint = *mPaint;
        utfPaint.setTextEncoding(SkPaint::kUTF16_TextEncoding);
        utfPaint.glyphsToUnichars(&rec.fGlyphID, 1, &unichar);
        return unichar;
    }

    bool isSpace(const SkBounder::GlyphRec& rec)
    {
        DBG_NAV_LOGD("mLastGlyph=((%g, %g),(%g, %g), %d)"
            " rec=((%g, %g),(%g, %g), %d)"
            " mMinSpaceWidth=%g mLastUni=0x%04x '%c'",
            SkFixedToScalar(mLastGlyph.fLSB.fX),
            SkFixedToScalar(mLastGlyph.fLSB.fY),
            SkFixedToScalar(mLastGlyph.fRSB.fX),
            SkFixedToScalar(mLastGlyph.fRSB.fY), mLastGlyph.fGlyphID,
            SkFixedToScalar(rec.fLSB.fX), SkFixedToScalar(rec.fLSB.fY),
            SkFixedToScalar(rec.fRSB.fX), SkFixedToScalar(rec.fRSB.fY),
            rec.fGlyphID,
            SkFixedToScalar(mMinSpaceWidth),
            mLastUni, mLastUni && mLastUni < 0x7f ? mLastUni : '?');
        bool newBaseLine = mLastGlyph.fLSB.fY != rec.fLSB.fY;
        if (newBaseLine)
            return true;
        SkFixed gapOne = mLastGlyph.fLSB.fX - rec.fRSB.fX;
        SkFixed gapTwo = rec.fLSB.fX - mLastGlyph.fRSB.fX;
        if (gapOne < 0 && gapTwo < 0)
            return false; // overlaps
        uint16_t test[2];
        test[0] = mLastGlyph.fGlyphID;
        test[1] = rec.fGlyphID;
        SkIRect area;
        area.set(0, 0, mWidth, mHeight);
        SpaceCanvas spaceChecker(area);
        spaceChecker.drawText(test, sizeof(test),
            SkFixedToScalar(mLastGlyph.fLSB.fX),
            SkFixedToScalar(mLastGlyph.fLSB.fY), *mPaint);
        const SkBounder::GlyphRec& g1 = spaceChecker.mBounder.mFirstGlyph;
        const SkBounder::GlyphRec& g2 = spaceChecker.mBounder.mLastGlyph;
        DBG_NAV_LOGD("g1=(%g, %g, %g, %g) g2=(%g, %g, %g, %g)",
            SkFixedToScalar(g1.fLSB.fX), SkFixedToScalar(g1.fLSB.fY),
            SkFixedToScalar(g1.fRSB.fX), SkFixedToScalar(g1.fRSB.fY),
            SkFixedToScalar(g2.fLSB.fX), SkFixedToScalar(g2.fLSB.fY),
            SkFixedToScalar(g2.fRSB.fX), SkFixedToScalar(g2.fRSB.fY));
        gapOne = SkFixedAbs(gapOne);
        gapTwo = SkFixedAbs(gapTwo);
        SkFixed gap = gapOne < gapTwo ? gapOne : gapTwo;
        SkFixed overlap = g2.fLSB.fX - g1.fRSB.fX;
        if (overlap < 0)
            gap -= overlap;
        DBG_NAV_LOGD("gap=%g overlap=%g gapOne=%g gapTwo=%g minSpaceWidth()=%g",
            SkFixedToScalar(gap), SkFixedToScalar(overlap),
            SkFixedToScalar(gapOne), SkFixedToScalar(gapTwo),
            SkFixedToScalar(minSpaceWidth()));
        // FIXME: the -1/8 below takes care of slop beween the computed gap
        // and the actual space width -- it's a rounding error from
        // moving from fixed to float and back and could be much smaller.
        return gap >= minSpaceWidth() - SK_Fixed1 / 8;
    }

    SkFixed minSpaceWidth()
    {
        if (mMinSpaceWidth == SK_FixedMax) {
            SkPaint charPaint = *mPaint;
            charPaint.setTextEncoding(SkPaint::kUTF8_TextEncoding);
            SkScalar width = charPaint.measureText(" ", 1);
            mMinSpaceWidth = SkScalarToFixed(width * mMatrix->getScaleX());
            DBG_NAV_LOGD("width=%g matrix sx/sy=(%g, %g) tx/ty=(%g, %g)"
                " mMinSpaceWidth=%g", width,
                mMatrix->getScaleX(), mMatrix->getScaleY(),
                mMatrix->getTranslateX(), mMatrix->getTranslateY(),
                SkFixedToScalar(mMinSpaceWidth));
        }
        return mMinSpaceWidth;
    }

    void recordGlyph(const SkBounder::GlyphRec& rec)
    {
        mLastCandidate = rec;
        mLastUniCandidate = getUniChar(rec);
    }

    void reset()
    {
        mMinSpaceWidth = SK_FixedMax; // mark as uninitialized
        mBase = mBottom = mTop = INT_MAX; // mark as uninitialized
    }

    void set(CommonCheck& check)
    {
        mLastGlyph = check.mLastGlyph;
        mLastUni = check.mLastUni;
        mMatrix = check.mMatrix;
        mPaint = check.mPaint;
        reset();
    }

    void setUp(const SkPaint& paint, const SkMatrix& matrix, SkScalar y,
            const void* text)
    {
        mMatrix = &matrix;
        mPaint = &paint;
        mText = static_cast<const uint16_t*>(text);
        mY = y;
        reset();
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

#if DEBUG_NAV_UI
    // make current (possibily uncomputed) value visible for debugging
    int topDebug() const
    {
        return mTop;
    }
#endif

protected:
    int mHeight;
    SkBounder::GlyphRec mLastCandidate;
    SkBounder::GlyphRec mLastGlyph;
    SkUnichar mLastUni;
    SkUnichar mLastUniCandidate;
    const SkMatrix* mMatrix;
    const SkPaint* mPaint;
    const uint16_t* mText;
    int mWidth;
    SkScalar mY;
private:
    int mBase;
    int mBottom;
    SkFixed mMinSpaceWidth;
    int mTop;
    friend class EdgeCheck;
};

class FirstCheck : public CommonCheck {
public:
    FirstCheck(int x, int y, const SkIRect& area)
        : INHERITED(area.width(), area.height())
        , mFocusX(x - area.fLeft)
        , mFocusY(y - area.fTop)
        , mRecordGlyph(false)
    {
        reset();
    }

    const SkIRect& adjustedBounds(const SkIRect& area, int* base)
    {
        *base = mBestBase + area.fTop;
        mBestBounds.offset(area.fLeft, area.fTop);
        DBG_NAV_LOGD("FirstCheck mBestBounds:(%d, %d, %d, %d) mTop=%d mBottom=%d",
            mBestBounds.fLeft, mBestBounds.fTop, mBestBounds.fRight, 
            mBestBounds.fBottom, topDebug(), bottomDebug());
        return mBestBounds;
    }

    virtual bool onIRectGlyph(const SkIRect& rect,
        const SkBounder::GlyphRec& rec)
    {
        /* compute distance from rectangle center.
         * centerX = (rect.L + rect.R) / 2
         * multiply centerX and comparison x by 2 to retain better precision
         */
        int dx = rect.fLeft + rect.fRight - (mFocusX << 1);
        int dy = top() + bottom() - (mFocusY << 1);
        int distance = dx * dx + dy * dy;
#ifdef EXTRA_NOISY_LOGGING
        if (distance < 500 || abs(distance - mDistance) < 500)
            DBG_NAV_LOGD("FirstCheck distance=%d mDistance=%d", distance, mDistance);
#endif
        if (mDistance > distance) {
            mBestBase = base();
            mBestBounds.set(rect.fLeft, top(), rect.fRight, bottom());
            if (distance < 100) {
                DBG_NAV_LOGD("FirstCheck mBestBounds={%d,%d,r=%d,b=%d} distance=%d",
                    mBestBounds.fLeft, mBestBounds.fTop,
                    mBestBounds.fRight, mBestBounds.fBottom, distance >> 2);
            }
            mDistance = distance;
            if (mRecordGlyph)
                recordGlyph(rec);
        }
        return false;
    }

    void reset()
    {
        mBestBounds.setEmpty();
        mDistance = INT_MAX;
    }

    void setRecordGlyph()
    {
        mRecordGlyph = true;
    }

protected:
    int mBestBase;
    SkIRect mBestBounds;
    int mDistance;
    int mFocusX;
    int mFocusY;
    bool mRecordGlyph;
private:
    typedef CommonCheck INHERITED;
};

class EdgeCheck : public FirstCheck {
public:
    EdgeCheck(int x, int y, const SkIRect& area, CommonCheck& last, bool left)
        : INHERITED(x, y, area)
        , mLast(area.width(), area.height())
        , mLeft(left)
    {
        mLast.set(last);
        mLastGlyph = last.mLastGlyph;
        mLastUni = last.mLastUni;
    }

    bool adjacent()
    {
        return !mLast.isSpace(mLastGlyph);
    }

    const SkIRect& bestBounds(int* base)
    {
        *base = mBestBase;
        return mBestBounds;
    }

    virtual bool onIRectGlyph(const SkIRect& rect,
        const SkBounder::GlyphRec& rec)
    {
        int dx = mLeft ? mFocusX - rect.fRight : rect.fLeft - mFocusX;
        int dy = ((top() + bottom()) >> 1) - mFocusY;
        if (mLeft ? mFocusX <= rect.fLeft : mFocusX >= rect.fRight) {
            if (abs(dx) <= 10 && abs(dy) <= 10) {
                DBG_NAV_LOGD("EdgeCheck fLeft=%d fRight=%d mFocusX=%d dx=%d dy=%d",
                    rect.fLeft, rect.fRight, mFocusX, dx, dy);
            }
            return false;
        }
        int distance = dx * dx + dy * dy;
        if (mDistance > distance) {
            if (rec.fLSB == mLastGlyph.fLSB && rec.fRSB == mLastGlyph.fRSB) {
                DBG_NAV_LOGD("dup rec.fLSB.fX=%g rec.fRSB.fX=%g",
                SkFixedToScalar(rec.fLSB.fX), SkFixedToScalar(rec.fRSB.fX));
                return false;
            }
            recordGlyph(rec);
            mDistance = distance;
            mBestBase = base();
            mBestBounds.set(rect.fLeft, top(), rect.fRight, bottom());
            if (distance <= 100) {
                DBG_NAV_LOGD("EdgeCheck mBestBounds={%d,%d,r=%d,b=%d} distance=%d",
                    mBestBounds.fLeft, mBestBounds.fTop,
                    mBestBounds.fRight, mBestBounds.fBottom, distance);
            }
        }
        return false;
    }

    void shiftStart(SkIRect bounds)
    {
        DBG_NAV_LOGD("EdgeCheck mFocusX=%d mLeft=%s bounds.fLeft=%d bounds.fRight=%d",
            mFocusX, mLeft ? "true" : "false", bounds.fLeft, bounds.fRight);
        reset();
        mFocusX = mLeft ? bounds.fLeft : bounds.fRight;
        mLast.set(*this);
    }

protected:
    CommonCheck mLast;
    bool mLeft;
private:
    typedef FirstCheck INHERITED;
};

class FindFirst : public CommonCheck {
public:
    FindFirst(int width, int height)
        : INHERITED(width, height)
    {
        mBestBounds.set(width, height, width, height);
    }

    const SkIRect& bestBounds(int* base)
    {
        *base = mBestBase;
        return mBestBounds;
    }

    virtual bool onIRect(const SkIRect& rect)
    {
        if (mBestBounds.isEmpty()) {
            mBestBase = base();
            mBestBounds.set(rect.fLeft, top(), rect.fRight, bottom());
        }
        return false;
    }

protected:
    int mBestBase;
    SkIRect mBestBounds;
private:
    typedef CommonCheck INHERITED;
};

class FindLast : public FindFirst {
public:
    FindLast(int width, int height)
        : INHERITED(width, height)
    {
        mBestBounds.setEmpty();
    }

    virtual bool onIRect(const SkIRect& rect)
    {
        mBestBase = base();
        mBestBounds.set(rect.fLeft, top(), rect.fRight, bottom());
        return false;
    }

private:
    typedef FindFirst INHERITED;
};

static bool baseLinesAgree(const SkIRect& rectA, int baseA,
    const SkIRect& rectB, int baseB)
{
    return (rectA.fTop < baseB && rectA.fBottom >= baseB)
        || (rectB.fTop < baseA && rectB.fBottom >= baseA);
}

class BuilderCheck : public CommonCheck {
protected:
    enum IntersectionType {
        NO_INTERSECTION, // debugging printf expects this to equal zero
        LAST_INTERSECTION, // debugging printf expects this to equal one
        WAIT_FOR_INTERSECTION
    };

    BuilderCheck(const SkIRect& start, int startBase, const SkIRect& end,
        int endBase, const SkIRect& area)
        : INHERITED(area.width(), area.height())
        , mCapture(false)
        , mEnd(end)
        , mEndBase(endBase)
        , mStart(start)
        , mStartBase(startBase)
    {
        mEnd.offset(-area.fLeft, -area.fTop);
        mEndBase -= area.fTop;
        mEndExtra.setEmpty();
        mLast.setEmpty();
        mLastBase = INT_MAX;
        mSelectRect.setEmpty();
        mStart.offset(-area.fLeft, -area.fTop);
        mStartBase -= area.fTop;
        mStartExtra.setEmpty();
        DBG_NAV_LOGD(" mStart=(%d,%d,r=%d,b=%d) mStartBase=%d"
            " mEnd=(%d,%d,r=%d,b=%d) mEndBase=%d",
            mStart.fLeft, mStart.fTop, mStart.fRight, mStart.fBottom, mStartBase,
            mEnd.fLeft, mEnd.fTop, mEnd.fRight, mEnd.fBottom, mEndBase);
    }

    int checkFlipRect(const SkIRect& full, int fullBase) {
        mCollectFull = false;
        // is the text to collect between the selection top and bottom?
        if (fullBase < mStart.fTop || fullBase > mEnd.fBottom) {
            if (VERBOSE_LOGGING && !mLast.isEmpty()) DBG_NAV_LOGD("%s 1"
                " full=(%d,%d,r=%d,b=%d) fullBase=%d"
                " mLast=(%d,%d,r=%d,b=%d) mLastBase=%d",
                mLastIntersects ? "LAST_INTERSECTION" : "NO_INTERSECTION",
                full.fLeft, full.fTop, full.fRight, full.fBottom, fullBase,
                mLast.fLeft, mLast.fTop, mLast.fRight, mLast.fBottom, mLastBase);
            return mLastIntersects;
        }
        // is the text to the left of the selection start?
        if (baseLinesAgree(mStart, mStartBase, full, fullBase)
            && full.fLeft < mStart.fLeft) {
            if (VERBOSE_LOGGING) DBG_NAV_LOGD("%s 2"
                " full=(%d,%d,r=%d,b=%d) fullBase=%d"
                " mLast=(%d,%d,r=%d,b=%d) mLastBase=%d"
                " mStart=(%d,%d,r=%d,b=%d) mStartBase=%d",
                mLastIntersects ? "LAST_INTERSECTION" : "NO_INTERSECTION",
                full.fLeft, full.fTop, full.fRight, full.fBottom, fullBase,
                mLast.fLeft, mLast.fTop, mLast.fRight, mLast.fBottom, mLastBase,
                mStart.fLeft, mStart.fTop, mStart.fRight, mStart.fBottom, mStartBase);
            mStartExtra.join(full);
            return mLastIntersects;
        }
        // is the text to the right of the selection end?
        if (baseLinesAgree(mEnd, mEndBase, full, fullBase)
            && full.fRight > mEnd.fRight) {
            if (VERBOSE_LOGGING) DBG_NAV_LOGD("%s 3"
                " full=(%d,%d,r=%d,b=%d) fullBase=%d"
                " mLast=(%d,%d,r=%d,b=%d) mLastBase=%d"
                " mEnd=(%d,%d,r=%d,b=%d) mEndBase=%d",
                mLastIntersects ? "LAST_INTERSECTION" : "NO_INTERSECTION",
                full.fLeft, full.fTop, full.fRight, full.fBottom, fullBase,
                mLast.fLeft, mLast.fTop, mLast.fRight, mLast.fBottom, mLastBase,
                mEnd.fLeft, mEnd.fTop, mEnd.fRight, mEnd.fBottom, mEndBase);
            mEndExtra.join(full);
            return mLastIntersects;
        }
        int spaceGap = SkFixedRound(minSpaceWidth() * 3);
        // should text to the left of the start be added to the selection bounds?
        if (!mStartExtra.isEmpty()) {
            if (VERBOSE_LOGGING) DBG_NAV_LOGD("mSelectRect=(%d,%d,r=%d,b=%d)"
                " mStartExtra=(%d,%d,r=%d,b=%d)",
                mSelectRect.fLeft, mSelectRect.fTop, mSelectRect.fRight, mSelectRect.fBottom,
                mStartExtra.fLeft, mStartExtra.fTop, mStartExtra.fRight, mStartExtra.fBottom);
            if (mStartExtra.fRight + spaceGap >= mStart.fLeft)
                mSelectRect.join(mStartExtra);
            mStartExtra.setEmpty();
        }
        // should text to the right of the end be added to the selection bounds?
        if (!mEndExtra.isEmpty()) {
            if (VERBOSE_LOGGING) DBG_NAV_LOGD("mSelectRect=(%d,%d,r=%d,b=%d)"
                " mEndExtra=(%d,%d,r=%d,b=%d)",
                mSelectRect.fLeft, mSelectRect.fTop, mSelectRect.fRight, mSelectRect.fBottom,
                mEndExtra.fLeft, mEndExtra.fTop, mEndExtra.fRight, mEndExtra.fBottom);
            if (mEndExtra.fLeft - spaceGap <= mEnd.fRight)
                mSelectRect.join(mEndExtra);
            mEndExtra.setEmpty();
        }
        bool sameBaseLine = baseLinesAgree(mLast, mLastBase, full, fullBase);
        bool adjacent = (full.fLeft - mLast.fRight) < spaceGap;
        // is this the first, or are there more characters on the same line?
        if (mLast.isEmpty() || (sameBaseLine && adjacent)) {
            if (VERBOSE_LOGGING) DBG_NAV_LOGD("WAIT_FOR_INTERSECTION"
                " full=(%d,%d,r=%d,b=%d) fullBase=%d"
                " mLast=(%d,%d,r=%d,b=%d) mLastBase=%d"
                " mSelectRect=(%d,%d,r=%d,b=%d)",
                full.fLeft, full.fTop, full.fRight, full.fBottom, fullBase,
                mLast.fLeft, mLast.fTop, mLast.fRight, mLast.fBottom, mLastBase,
                mSelectRect.fLeft, mSelectRect.fTop, mSelectRect.fRight, mSelectRect.fBottom);
            mLast.join(full);
            mLastIntersects = SkIRect::Intersects(mLast, mSelectRect);
            return WAIT_FOR_INTERSECTION;
        }
        if (VERBOSE_LOGGING) DBG_NAV_LOGD("%s 4"
            " mLast=(%d,%d,r=%d,b=%d) mLastBase=%d"
            " full=(%d,%d,r=%d,b=%d) fullBase=%d"
            " mSelectRect=(%d,%d,r=%d,b=%d)"
            " mStartExtra=(%d,%d,r=%d,b=%d)"
            " mEndExtra=(%d,%d,r=%d,b=%d)",
            mLastIntersects ? "LAST_INTERSECTION" : "NO_INTERSECTION",
            mLast.fLeft, mLast.fTop, mLast.fRight, mLast.fBottom, mLastBase,
            full.fLeft, full.fTop, full.fRight, full.fBottom, fullBase,
            mSelectRect.fLeft, mSelectRect.fTop, mSelectRect.fRight, mSelectRect.fBottom,
            mStartExtra.fLeft, mStartExtra.fTop, mStartExtra.fRight, mStartExtra.fBottom,
            mEndExtra.fLeft, mEndExtra.fTop, mEndExtra.fRight, mEndExtra.fBottom);
        // after the caller determines what to do with the last collection,
        // start the collection over with full and fullBase.
        mCollectFull = true;
        return mLastIntersects;
    }

    bool resetLast(const SkIRect& full, int fullBase)
    {
        if (mCollectFull) {
            mLast = full;
            mLastBase = fullBase;
            mLastIntersects = SkIRect::Intersects(mLast, mSelectRect);
        } else {
            mLast.setEmpty();
            mLastBase = INT_MAX;
            mLastIntersects = false;
        }
        return mCollectFull;
    }

    void setFlippedState()
    {
        mSelectRect = mStart;
        mSelectRect.join(mEnd);
        DBG_NAV_LOGD("mSelectRect=(%d,%d,r=%d,b=%d)",
            mSelectRect.fLeft, mSelectRect.fTop, mSelectRect.fRight, mSelectRect.fBottom);
        mLast.setEmpty();
        mLastBase = INT_MAX;
        mLastIntersects = NO_INTERSECTION;
    }

    bool mCapture;
    bool mCollectFull;
    SkIRect mEnd;
    int mEndBase;
    SkIRect mEndExtra;
    bool mFlipped;
    SkIRect mLast;
    int mLastBase;
    int mLastIntersects;
    SkIRect mSelectRect;
    SkIRect mStart;
    SkIRect mStartExtra;
    int mStartBase;
private:
    typedef CommonCheck INHERITED;

};

class MultilineBuilder : public BuilderCheck {
public:
    MultilineBuilder(const SkIRect& start, int startBase, const SkIRect& end,
            int endBase, const SkIRect& area, SkRegion* region)
        : INHERITED(start, startBase, end, endBase, area)
        , mSelectRegion(region)
    {
        mFlipped = false;
    }

    void addLastToRegion() {
        if (VERBOSE_LOGGING) DBG_NAV_LOGD(" mLast=(%d,%d,r=%d,b=%d)",
            mLast.fLeft, mLast.fTop, mLast.fRight, mLast.fBottom);
        mSelectRegion->op(mLast, SkRegion::kUnion_Op);
    }

    void finish() {
        if (!mFlipped || !mLastIntersects)
            return;
        addLastToRegion();
    }

    // return true if capture end was not found after capture begin
    bool flipped() {
        DBG_NAV_LOGD("flipped=%s", mCapture ? "true" : "false");
        if (!mCapture)
            return false;
        mFlipped = true;
        setFlippedState();
        mSelectRegion->setEmpty();
        return true;
    }

    virtual bool onIRect(const SkIRect& rect) {
        SkIRect full;
        full.set(rect.fLeft, top(), rect.fRight, bottom());
        int fullBase = base();
        if (mFlipped) {
            int intersectType = checkFlipRect(full, fullBase);
            if (intersectType == LAST_INTERSECTION)
                addLastToRegion();
            if (intersectType != WAIT_FOR_INTERSECTION)
                resetLast(full, fullBase);
            return false;
        }
        if (full == mStart) {
            if (VERBOSE_LOGGING) DBG_NAV_LOGD("full == mStart full=(%d,%d,r=%d,b=%d)",
                full.fLeft, full.fTop, full.fRight, full.fBottom);
            mCapture = true;
        }
        if (mCapture) {
            bool sameLines = baseLinesAgree(mLast, mLastBase, full, fullBase);
            if (sameLines)
                mLast.join(full);
            if (!sameLines || full == mEnd) {
                if (VERBOSE_LOGGING) DBG_NAV_LOGD("finish mLast=(%d,%d,r=%d,b=%d)",
                    mLast.fLeft, mLast.fTop, mLast.fRight, mLast.fBottom);
                addLastToRegion();
                mLast = full;
                mLastBase = fullBase;
            }
        }
        if (full == mEnd) {
            if (VERBOSE_LOGGING) DBG_NAV_LOGD("full == mEnd full=(%d,%d,r=%d,b=%d)",
                full.fLeft, full.fTop, full.fRight, full.fBottom);
            mCapture = false;
        }
        return false;
    }

protected:
    SkRegion* mSelectRegion;
private:
    typedef BuilderCheck INHERITED;
};

static inline bool compareBounds(const SkIRect* first, const SkIRect* second)
{
    return first->fTop < second->fTop;
}

class TextExtractor : public BuilderCheck {
public:
    TextExtractor(const SkIRect& start, int startBase, const SkIRect& end,
        int endBase, const SkIRect& area, bool flipped)
        : INHERITED(start, startBase, end, endBase, area)
        , mSelectStartIndex(-1)
        , mSkipFirstSpace(true) // don't start with a space
    {
        mFlipped = flipped;
        if (flipped)
            setFlippedState();
    }

    void addCharacter(const SkBounder::GlyphRec& rec)
    {
        if (mSelectStartIndex < 0)
            mSelectStartIndex = mSelectText.count();
        if (!mSkipFirstSpace) {
            if (addNewLine(rec)) {
                DBG_NAV_LOG("write new line");
                *mSelectText.append() = '\n';
                *mSelectText.append() = '\n';
            } else if (addSpace(rec)) {
                DBG_NAV_LOG("write space");
                *mSelectText.append() = ' ';
            }
        } else
            mSkipFirstSpace = false;
        recordGlyph(rec);
        finishGlyph();
        if (VERBOSE_LOGGING) DBG_NAV_LOGD("glyphID=%d uni=%d '%c'", rec.fGlyphID,
            mLastUni, mLastUni && mLastUni < 0x7f ? mLastUni : '?');
        if (mLastUni) {
            uint16_t chars[2];
            size_t count = SkUTF16_FromUnichar(mLastUni, chars);
            *mSelectText.append() = chars[0];
            if (count == 2)
                *mSelectText.append() = chars[1];
        }
    }

    void addLast()
    {
        *mSelectBounds.append() = mLast;
        *mSelectStart.append() = mSelectStartIndex;
        *mSelectEnd.append() = mSelectText.count();
    }

    /* Text characters are collected before it's been determined that the
       characters are part of the selection. The bounds describe valid parts
       of the selection, but the bounds are out of order.

       This sorts the characters by sorting the bounds, then copying the
       characters that were captured.
     */
    void finish()
    {
        if (mLastIntersects)
            addLast();
        Vector<SkIRect*> sortedBounds;
        SkTDArray<uint16_t> temp;
        int index;
        DBG_NAV_LOGD("mSelectBounds.count=%d text=%d", mSelectBounds.count(),
            mSelectText.count());
        for (index = 0; index < mSelectBounds.count(); index++)
            sortedBounds.append(&mSelectBounds[index]);
        std::sort(sortedBounds.begin(), sortedBounds.end(), compareBounds);
        int lastEnd = -1;
        for (index = 0; index < mSelectBounds.count(); index++) {
            int order = sortedBounds[index] - &mSelectBounds[0];
            int start = mSelectStart[order];
            int end = mSelectEnd[order];
            DBG_NAV_LOGD("order=%d start=%d end=%d top=%d", order, start, end,
                mSelectBounds[order].fTop);
            int count = temp.count();
            if (count > 0 && temp[count - 1] != '\n' && start != lastEnd) {
                // always separate paragraphs when original text is out of order
                DBG_NAV_LOG("write new line");
                *temp.append() = '\n';
                *temp.append() = '\n';
            }
            temp.append(end - start, &mSelectText[start]);
            lastEnd = end;
        }
        mSelectText.swap(temp);
    }

    virtual bool onIRectGlyph(const SkIRect& rect,
        const SkBounder::GlyphRec& rec)
    {
        SkIRect full;
        full.set(rect.fLeft, top(), rect.fRight, bottom());
        int fullBase = base();
        if (mFlipped) {
            int intersectType = checkFlipRect(full, fullBase);
            if (WAIT_FOR_INTERSECTION == intersectType)
                addCharacter(rec); // may not be copied
            else {
                if (LAST_INTERSECTION == intersectType)
                    addLast();
                else
                    mSkipFirstSpace = true;
                mSelectStartIndex = -1;
                if (resetLast(full, fullBase))
                    addCharacter(rec); // may not be copied
            }
            return false;
        }
        if (full == mStart)
            mCapture = true;
        if (mCapture)
            addCharacter(rec);
        else
            mSkipFirstSpace = true;
        if (full == mEnd)
            mCapture = false;
        return false;
    }

    WebCore::String text() {
        if (mFlipped)
            finish();
        // the text has been copied in visual order. Reverse as needed if
        // result contains right-to-left characters.
        const uint16_t* start = mSelectText.begin();
        const uint16_t* end = mSelectText.end();
        while (start < end) {
            SkUnichar ch = SkUTF16_NextUnichar(&start);
            WTF::Unicode::Direction charDirection = WTF::Unicode::direction(ch);
            if (WTF::Unicode::RightToLeftArabic == charDirection
                    || WTF::Unicode::RightToLeft == charDirection) {
                WebCore::ReverseBidi(mSelectText.begin(), mSelectText.count());
                break;
            }
        }
        return WebCore::String(mSelectText.begin(), mSelectText.count());
    }

protected:
    SkIRect mEmpty;
    SkTDArray<SkIRect> mSelectBounds;
    SkTDArray<int> mSelectEnd;
    SkTDArray<int> mSelectStart;
    int mSelectStartIndex;
    SkTDArray<uint16_t> mSelectText;
    bool mSkipFirstSpace;
private:
    typedef BuilderCheck INHERITED;
};

class TextCanvas : public SkCanvas {
public:

    TextCanvas(CommonCheck* bounder, const SkIRect& area)
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

static bool buildSelection(const SkPicture& picture, const SkIRect& area,
        const SkIRect& selStart, int startBase,
        const SkIRect& selEnd, int endBase, SkRegion* region)
{
    DBG_NAV_LOGD("area=(%d, %d, %d, %d) selStart=(%d, %d, %d, %d)"
        " selEnd=(%d, %d, %d, %d)",
        area.fLeft, area.fTop, area.fRight, area.fBottom,
        selStart.fLeft, selStart.fTop, selStart.fRight, selStart.fBottom,
        selEnd.fLeft, selEnd.fTop, selEnd.fRight, selEnd.fBottom);
    MultilineBuilder builder(selStart, startBase, selEnd, endBase, area, region);
    TextCanvas checker(&builder, area);
    checker.drawPicture(const_cast<SkPicture&>(picture));
    bool flipped = builder.flipped();
    if (flipped) {
        TextCanvas checker(&builder, area);
        checker.drawPicture(const_cast<SkPicture&>(picture));
    }
    builder.finish();
    region->translate(area.fLeft, area.fTop);
    return flipped;
}

static SkIRect findClosest(FirstCheck& _check, const SkPicture& picture,
        const SkIRect& area, int* base)
{
    DBG_NAV_LOGD("area=(%d, %d, %d, %d)", area.fLeft, area.fTop,
        area.fRight, area.fBottom);
    TextCanvas checker(&_check, area);
    checker.drawPicture(const_cast<SkPicture&>(picture));
    _check.finishGlyph();
    return _check.adjustedBounds(area, base);
}

static SkIRect findEdge(const SkPicture& picture, const SkIRect& area,
        int x, int y, bool left, int* base)
{
    SkIRect result;
    result.setEmpty();
    FirstCheck center(x, y, area);
    center.setRecordGlyph();
    int closestBase;
    SkIRect closest = findClosest(center, picture, area, &closestBase);
    closest.inset(-TOUCH_SLOP, -TOUCH_SLOP);
    if (!closest.contains(x, y)) {
        DBG_NAV_LOGD("closest=(%d, %d, %d, %d) area=(%d, %d, %d, %d) x/y=%d,%d",
            closest.fLeft, closest.fTop, closest.fRight, closest.fBottom,
            area.fLeft, area.fTop, area.fRight, area.fBottom, x, y);
        return result;
    }
    EdgeCheck edge(x, y, area, center, left);
    do { // detect left or right until there's a gap
        DBG_NAV_LOGD("edge=%p picture=%p area=%d,%d,%d,%d",
            &edge, &picture, area.fLeft, area.fTop, area.fRight, area.fBottom);
        TextCanvas checker(&edge, area);
        checker.drawPicture(const_cast<SkPicture&>(picture));
        edge.finishGlyph();
        if (!edge.adjacent()) {
            DBG_NAV_LOG("adjacent break");
            break;
        }
        int nextBase;
        const SkIRect& next = edge.bestBounds(&nextBase);
        if (next.isEmpty()) {
            DBG_NAV_LOG("empty");
            break;
        }
        if (result == next) {
            DBG_NAV_LOG("result == next");
            break;
        }
        *base = nextBase;
        result = next;
        edge.shiftStart(result);
    } while (true);
    if (!result.isEmpty()) {
        *base += area.fTop;
        result.offset(area.fLeft, area.fTop);
    }
    return result;
}

static SkIRect findFirst(const SkPicture& picture, int* base)
{
    FindFirst finder(picture.width(), picture.height());
    SkIRect area;
    area.set(0, 0, picture.width(), picture.height());
    TextCanvas checker(&finder, area);
    checker.drawPicture(const_cast<SkPicture&>(picture));
    return finder.bestBounds(base);
}

static SkIRect findLast(const SkPicture& picture, int* base)
{
    FindLast finder(picture.width(), picture.height());
    SkIRect area;
    area.set(0, 0, picture.width(), picture.height());
    TextCanvas checker(&finder, area);
    checker.drawPicture(const_cast<SkPicture&>(picture));
    return finder.bestBounds(base);
}

static SkIRect findLeft(const SkPicture& picture, const SkIRect& area,
        int x, int y, int* base)
{
    return findEdge(picture, area, x, y, true, base);
}

static SkIRect findRight(const SkPicture& picture, const SkIRect& area,
        int x, int y, int* base)
{
    return findEdge(picture, area, x, y, false, base);
}

static WebCore::String text(const SkPicture& picture, const SkIRect& area,
        const SkIRect& start, int startBase, const SkIRect& end,
        int endBase, bool flipped)
{
    TextExtractor extractor(start, startBase, end, endBase, area, flipped);
    TextCanvas checker(&extractor, area);
    checker.drawPicture(const_cast<SkPicture&>(picture));
    return extractor.text();
}

#define CONTROL_OFFSET 0
#define CONTROL_NOTCH 19
#define CONTROL_HEIGHT 35
#define CONTROL_WIDTH 21
#define STROKE_WIDTH 2.0f
#define STROKE_OUTSET 1.0f
#define STROKE_NOTCH_R 2.2f
#define STROKE_NOTCH_L 1.7f
#define DROP_HEIGHT 4

#define STROKE_COLOR 0x90000000
#define FILL_GRADIENT_TOP 0xD0F8DFA0
#define FILL_GRADIENT_BOTTOM 0xD0FFEFEF
#define DROP_GRADIENT_TOP 0x50000000
#define DROP_GRADIENT_BOTTOM 0x00000000

#define SLOP 20

SelectText::SelectText()
{
    reset();
    SkPaint paint;

    SkPath startFillPath;
    startFillPath.moveTo(-CONTROL_WIDTH + STROKE_OUTSET, CONTROL_NOTCH);
    startFillPath.lineTo(-CONTROL_WIDTH + STROKE_OUTSET, CONTROL_HEIGHT - STROKE_OUTSET);
    startFillPath.lineTo(-STROKE_OUTSET, CONTROL_HEIGHT - STROKE_OUTSET);
    startFillPath.lineTo(-STROKE_OUTSET, CONTROL_OFFSET + STROKE_NOTCH_R);
    startFillPath.close();
    SkPath startStrokePath;
    startStrokePath.moveTo(-CONTROL_WIDTH, CONTROL_NOTCH);
    startStrokePath.lineTo(-CONTROL_WIDTH, CONTROL_HEIGHT);
    startStrokePath.lineTo(0, CONTROL_HEIGHT);
    startStrokePath.lineTo(0, CONTROL_OFFSET);
    startStrokePath.close();
    SkPoint gradientLine[] = {{0, 0}, {0, CONTROL_HEIGHT}};
    SkColor gradientColors[] = {FILL_GRADIENT_TOP, FILL_GRADIENT_BOTTOM};
    SkShader* fillGradient = SkGradientShader::CreateLinear(gradientLine,
        gradientColors, 0, 2, SkShader::kClamp_TileMode);
    SkPoint dropLine[] = {{0, CONTROL_HEIGHT}, {0, CONTROL_HEIGHT + DROP_HEIGHT}};
    SkColor dropColors[] = {DROP_GRADIENT_TOP, DROP_GRADIENT_BOTTOM};
    SkShader* dropGradient = SkGradientShader::CreateLinear(dropLine,
        dropColors, 0, 2, SkShader::kClamp_TileMode);
    SkRect startDropRect = {-CONTROL_WIDTH - STROKE_OUTSET, CONTROL_HEIGHT,
        STROKE_OUTSET, CONTROL_HEIGHT + DROP_HEIGHT};

    SkCanvas* canvas = m_startControl.beginRecording(CONTROL_WIDTH, CONTROL_HEIGHT + DROP_HEIGHT);
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setShader(fillGradient);
    canvas->drawPath(startFillPath, paint);
    paint.setShader(0);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(STROKE_COLOR);
    paint.setStrokeWidth(STROKE_WIDTH);
    canvas->drawPath(startStrokePath, paint);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(0xff000000);
    paint.setShader(dropGradient);
    canvas->drawRect(startDropRect, paint);
    m_startControl.endRecording();

    SkPath endFillPath;
    endFillPath.moveTo(STROKE_OUTSET, CONTROL_OFFSET + STROKE_NOTCH_R);
    endFillPath.lineTo(STROKE_OUTSET, CONTROL_HEIGHT - STROKE_OUTSET);
    endFillPath.lineTo(CONTROL_WIDTH - STROKE_OUTSET, CONTROL_HEIGHT - STROKE_OUTSET);
    endFillPath.lineTo(CONTROL_WIDTH - STROKE_OUTSET, CONTROL_NOTCH);
    endFillPath.close();
    SkPath endStrokePath;
    endStrokePath.moveTo(0, CONTROL_OFFSET);
    endStrokePath.lineTo(0, CONTROL_HEIGHT);
    endStrokePath.lineTo(CONTROL_WIDTH, CONTROL_HEIGHT);
    endStrokePath.lineTo(CONTROL_WIDTH, CONTROL_NOTCH);
    endStrokePath.close();
    SkRect endDropRect = {-STROKE_OUTSET, CONTROL_HEIGHT,
        CONTROL_WIDTH + STROKE_OUTSET, CONTROL_HEIGHT + DROP_HEIGHT};

    canvas = m_endControl.beginRecording(CONTROL_WIDTH, CONTROL_HEIGHT);
    paint.setColor(0xff000000);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setShader(fillGradient);
    canvas->drawPath(endFillPath, paint);
    paint.setShader(0);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(STROKE_COLOR);
    paint.setStrokeWidth(STROKE_WIDTH);
    canvas->drawPath(endStrokePath, paint);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(0xff000000);
    paint.setShader(dropGradient);
    canvas->drawRect(endDropRect, paint);
    m_endControl.endRecording();
    SkSafeUnref(fillGradient);
    SkSafeUnref(dropGradient);
    m_picture = 0;
}

void SelectText::draw(SkCanvas* canvas, LayerAndroid* layer)
{
    // Gmail makes layers appear dynamically the page scrolls. The picture
    // recorded when the selection begins is confused by the pictures seen
    // in subsequent layers. To work around this, only allow text selection
    // in the main picture.
    if (layer->uniqueId() != -1)
        return;
    // FIXME: layer may not own the original selected picture
    m_picture = layer->picture();
    if (!m_picture)
        return;
    DBG_NAV_LOGD("m_extendSelection=%d m_drawPointer=%d", m_extendSelection, m_drawPointer);
    if (m_extendSelection)
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
    canvas->translate(m_selectX, m_selectY);
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
    ivisBounds.join(m_selStart);
    ivisBounds.join(m_selEnd);
    DBG_NAV_LOGD("m_selStart=(%d, %d, %d, %d) m_selEnd=(%d, %d, %d, %d)",
        m_selStart.fLeft, m_selStart.fTop, m_selStart.fRight, m_selStart.fBottom,
        m_selEnd.fLeft, m_selEnd.fTop, m_selEnd.fRight, m_selEnd.fBottom);
    m_flipped = buildSelection(*m_picture, ivisBounds, m_selStart, m_startBase,
        m_selEnd, m_endBase, &m_selRegion);
    SkPath path;
    m_selRegion.getBoundaryPath(&path);
    path.setFillType(SkPath::kEvenOdd_FillType);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SkColorSetARGB(0x80, 0xFF, 0xA8, 0x00));
    canvas->drawPath(path, paint);
    // experiment to draw touchable controls that resize the selection
    canvas->save();
    canvas->translate(m_selStart.fLeft, m_selStart.fBottom);
    canvas->drawPicture(m_startControl);
    canvas->restore();
    canvas->save();
    canvas->translate(m_selEnd.fRight, m_selEnd.fBottom);
    canvas->drawPicture(m_endControl);
    canvas->restore();
}

void SelectText::extendSelection(const SkPicture* picture, int x, int y)
{
    if (!picture)
        return;
    SkIRect clipRect = m_visibleRect;
    int base;
    if (m_startSelection) {
        if (!clipRect.contains(x, y)
                || !clipRect.contains(m_original.fX, m_original.fY)) {
            clipRect.set(m_original.fX, m_original.fY, x, y);
            clipRect.sort();
            clipRect.inset(-m_visibleRect.width(), -m_visibleRect.height());
        }
        DBG_NAV_LOGD("selStart clip=(%d,%d,%d,%d)", clipRect.fLeft,
            clipRect.fTop, clipRect.fRight, clipRect.fBottom);
        m_picture = picture;
        FirstCheck center(m_original.fX, m_original.fY, clipRect);
        m_selStart = m_selEnd = findClosest(center, *picture, clipRect, &base);
        m_startBase = m_endBase = base;
        m_startSelection = false;
        m_extendSelection = true;
        m_original.fX = m_original.fY = 0;
    } else if (picture != m_picture)
        return;
    x -= m_original.fX;
    y -= m_original.fY;
    if (!clipRect.contains(x, y) || !clipRect.contains(m_selStart)) {
        clipRect.set(m_selStart.fLeft, m_selStart.fTop, x, y);
        clipRect.sort();
        clipRect.inset(-m_visibleRect.width(), -m_visibleRect.height());
    }
    DBG_NAV_LOGD("extend clip=(%d,%d,%d,%d)", clipRect.fLeft,
        clipRect.fTop, clipRect.fRight, clipRect.fBottom);
    FirstCheck extension(x, y, clipRect);
    SkIRect found = findClosest(extension, *picture, clipRect, &base);
    DBG_NAV_LOGD("pic=%p x=%d y=%d m_startSelection=%s %s=(%d, %d, %d, %d)"
        " m_extendSelection=%s",
        picture, x, y, m_startSelection ? "true" : "false",
        m_hitTopLeft ? "m_selStart" : "m_selEnd",
        found.fLeft, found.fTop, found.fRight, found.fBottom,
        m_extendSelection ? "true" : "false");
    if (m_hitTopLeft) {
        m_startBase = base;
        m_selStart = found;
    } else {
        m_endBase = base;
        m_selEnd = found;
    }
    swapAsNeeded();
}

const String SelectText::getSelection()
{
    if (!m_picture)
        return String();
    SkIRect clipRect;
    clipRect.set(0, 0, m_picture->width(), m_picture->height());
    String result = text(*m_picture, clipRect, m_selStart, m_startBase,
        m_selEnd, m_endBase, m_flipped);
    DBG_NAV_LOGD("clip=(%d,%d,%d,%d)"
        " m_selStart=(%d, %d, %d, %d) m_selEnd=(%d, %d, %d, %d)",
        clipRect.fLeft, clipRect.fTop, clipRect.fRight, clipRect.fBottom,
        m_selStart.fLeft, m_selStart.fTop, m_selStart.fRight, m_selStart.fBottom,
        m_selEnd.fLeft, m_selEnd.fTop, m_selEnd.fRight, m_selEnd.fBottom);
    DBG_NAV_LOGD("text=%s", result.latin1().data()); // uses CString
    return result;
}

void SelectText::getSelectionArrow(SkPath* path)
{
    const int arrow[] = {
        0, 14, 3, 11, 5, 15, 9, 15, 7, 11, 11, 11
    };
    for (unsigned index = 0; index < sizeof(arrow)/sizeof(arrow[0]); index += 2)
        path->lineTo(arrow[index], arrow[index + 1]);
    path->close();
}

void SelectText::getSelectionCaret(SkPath* path)
{
    SkScalar height = m_selStart.fBottom - m_selStart.fTop;
    SkScalar dist = height / 4;
    path->moveTo(0, -height / 2);
    path->rLineTo(0, height);
    path->rLineTo(-dist, dist);
    path->rMoveTo(0, -0.5f);
    path->rLineTo(dist * 2, 0);
    path->rMoveTo(0, 0.5f);
    path->rLineTo(-dist, -dist);
}

bool SelectText::hitCorner(int cx, int cy, int x, int y) const
{
    SkIRect test;
    test.set(cx, cy, cx, cy);
    test.inset(-SLOP, -SLOP);
    return test.contains(x, y);
}

bool SelectText::hitSelection(int x, int y) const
{
    int left = m_selStart.fLeft - CONTROL_WIDTH / 2;
    int top = m_selStart.fBottom + CONTROL_HEIGHT / 2;
    if (hitCorner(left, top, x, y))
        return true;
    int right = m_selEnd.fRight + CONTROL_WIDTH / 2;
    int bottom = m_selEnd.fBottom + CONTROL_HEIGHT / 2;
    if (hitCorner(right, bottom, x, y))
        return true;
    SkIRect test;
    test.set(x - CONTROL_WIDTH, y - CONTROL_HEIGHT, x + CONTROL_WIDTH,
        y + CONTROL_HEIGHT);
    return m_selRegion.intersects(test);
}

void SelectText::moveSelection(const SkPicture* picture, int x, int y)
{
    if (!picture)
        return;
    SkIRect clipRect = m_visibleRect;
    clipRect.join(m_selStart);
    clipRect.join(m_selEnd);
    if (!m_extendSelection)
        m_picture = picture;
    FirstCheck center(x, y, clipRect);
    int base;
    SkIRect found = findClosest(center, *picture, clipRect, &base);
    if (m_hitTopLeft || !m_extendSelection) {
        m_startBase = base;
        m_selStart = found;
    }
    if (!m_hitTopLeft || !m_extendSelection) {
        m_endBase = base;
        m_selEnd = found;
    }
    swapAsNeeded();
    DBG_NAV_LOGD("x=%d y=%d extendSelection=%s m_selStart=(%d, %d, %d, %d)"
        " m_selEnd=(%d, %d, %d, %d)", x, y, m_extendSelection ? "true" : "false",
        m_selStart.fLeft, m_selStart.fTop, m_selStart.fRight, m_selStart.fBottom,
        m_selEnd.fLeft, m_selEnd.fTop, m_selEnd.fRight, m_selEnd.fBottom);
}

void SelectText::reset()
{
    DBG_NAV_LOG("m_extendSelection=false");
    m_selStart.setEmpty();
    m_selEnd.setEmpty();
    m_extendSelection = false;
    m_startSelection = false;
}

void SelectText::selectAll(const SkPicture* picture)
{
    m_selStart = findFirst(*picture, &m_startBase);
    m_selEnd = findLast(*picture, &m_endBase);
    m_extendSelection = true;
}

int SelectText::selectionX() const
{
    return m_hitTopLeft ? m_selStart.fLeft : m_selEnd.fRight;
}

int SelectText::selectionY() const
{
    const SkIRect& rect = m_hitTopLeft ? m_selStart : m_selEnd;
    return (rect.fTop + rect.fBottom) >> 1;
}

bool SelectText::startSelection(int x, int y)
{
    m_original.fX = x;
    m_original.fY = y;
    if (m_selStart.isEmpty()) {
        DBG_NAV_LOGD("empty start x=%d y=%d", x, y);
        m_startSelection = true;
        return true;
    }
    int left = m_selStart.fLeft - CONTROL_WIDTH / 2;
    int top = m_selStart.fBottom + CONTROL_HEIGHT / 2;
    m_hitTopLeft = hitCorner(left, top, x, y);
    int right = m_selEnd.fRight + CONTROL_WIDTH / 2;
    int bottom = m_selEnd.fBottom + CONTROL_HEIGHT / 2;
    bool hitBottomRight = hitCorner(right, bottom, x, y);
    DBG_NAV_LOGD("left=%d top=%d right=%d bottom=%d x=%d y=%d", left, top,
        right, bottom, x, y);
    if (m_hitTopLeft && (!hitBottomRight || y - top < bottom - y)) {
        DBG_NAV_LOG("hit top left");
        m_original.fX -= left;
        m_original.fY -= (m_selStart.fTop + m_selStart.fBottom) >> 1;
    } else if (hitBottomRight) {
        DBG_NAV_LOG("hit bottom right");
        m_original.fX -= right;
        m_original.fY -= (m_selEnd.fTop + m_selEnd.fBottom) >> 1;
    }
    return m_hitTopLeft || hitBottomRight;
}

/* selects the word at (x, y)
* a word is normally delimited by spaces
* a string of digits (even with inside spaces) is a word (for phone numbers)
* FIXME: digit find isn't implemented yet
* returns true if a word was selected
*/
bool SelectText::wordSelection(const SkPicture* picture)
{
    int x = m_selStart.fLeft;
    int y = (m_selStart.fTop + m_selStart.fBottom) >> 1;
    SkIRect clipRect = m_visibleRect;
    clipRect.fLeft -= m_visibleRect.width() >> 1;
    int base;
    SkIRect left = findLeft(*picture, clipRect, x, y, &base);
    if (!left.isEmpty()) {
        m_startBase = base;
        m_selStart = left;
    }
    x = m_selEnd.fRight;
    y = (m_selEnd.fTop + m_selEnd.fBottom) >> 1;
    clipRect = m_visibleRect;
    clipRect.fRight += m_visibleRect.width() >> 1;
    SkIRect right = findRight(*picture, clipRect, x, y, &base);
    if (!right.isEmpty()) {
        m_endBase = base;
        m_selEnd = right;
    }
    DBG_NAV_LOGD("m_selStart=(%d, %d, %d, %d) m_selEnd=(%d, %d, %d, %d)",
        m_selStart.fLeft, m_selStart.fTop, m_selStart.fRight, m_selStart.fBottom,
        m_selEnd.fLeft, m_selEnd.fTop, m_selEnd.fRight, m_selEnd.fBottom);
    if (!left.isEmpty() || !right.isEmpty()) {
        m_extendSelection = true;
        return true;
    }
    return false;
}

void SelectText::swapAsNeeded()
{
    if (m_selStart.fTop >= (m_selEnd.fTop + m_selEnd.fBottom) >> 1
            || (m_selEnd.fTop < (m_selStart.fTop + m_selStart.fBottom) >> 1
            && m_selStart.fRight > m_selEnd.fLeft))
    {
        SkTSwap(m_startBase, m_endBase);
        SkTSwap(m_selStart, m_selEnd);
        m_hitTopLeft ^= true;
        DBG_NAV_LOGD("m_hitTopLeft=%s", m_hitTopLeft ? "true" : "false");
    }
}

}
