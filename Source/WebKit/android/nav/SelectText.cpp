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
#include "BidiRunList.h"
#include "CachedRoot.h"
#include "LayerAndroid.h"
#include "ParseCanvas.h"
#include "SelectText.h"
#include "SkBitmap.h"
#include "SkBounder.h"
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
#include <wtf/text/CString.h>
#endif

#define VERBOSE_LOGGING 0
// #define EXTRA_NOISY_LOGGING 1
#define DEBUG_TOUCH_HANDLES 0
#if DEBUG_TOUCH_HANDLES
#define DBG_HANDLE_LOG(format, ...) LOGD("%s " format, __FUNCTION__, __VA_ARGS__)
#else
#define DBG_HANDLE_LOG(...)
#endif

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
    BidiRunList<BidiCharacterRun>& bidiRuns = bidiResolver.runs();
    bidiResolver.setStatus(BidiStatus(LeftToRight, LeftToRight, LeftToRight,
        BidiContext::create(0, LeftToRight, false)));
    bidiResolver.setPosition(TextRunIterator(&run, 0));
    bidiResolver.createBidiRunsForLine(TextRunIterator(&run, len));
    if (!bidiRuns.runCount())
        return;
    BidiCharacterRun* bidiRun = bidiRuns.firstRun();
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
    bidiRuns.deleteRuns();
    memcpy(chars, &result[0], len * sizeof(UChar));
}

}

namespace android {

#define HYPHEN_MINUS 0x2D // ASCII hyphen
#define SOLIDUS 0x2F // ASCII slash
#define REVERSE_SOLIDUS 0x5C // ASCII backslash
#define HYPHEN 0x2010 // unicode hyphen, first in range of dashes
#define HORZ_BAR 0x2015 // unicode horizontal bar, last in range of dashes
#define TOUCH_SLOP 10 // additional distance from character rect when hit

class CommonCheck : public SkBounder {
public:
    CommonCheck(const SkIRect& area)
        : mArea(area)
        , mLastUni(0)
    {
        mLastGlyph.fGlyphID = static_cast<uint16_t>(-1);
        mLastCandidate.fGlyphID = static_cast<uint16_t>(-1);
        mMatrix.reset();
        reset();
    }

    /* called only while the picture is parsed */
    int base() {
        if (mBase == INT_MAX) {
            SkPoint result;
            mMatrix.mapXY(0, mY, &result);
            mBase = SkScalarFloor(result.fY);
        }
        return mBase;
    }

    /* called only while the picture is parsed */
     int bottom() {
        if (mBottom == INT_MAX) {
            SkPoint result;
            SkPaint::FontMetrics metrics;
            mPaint.getFontMetrics(&metrics);
            mMatrix.mapXY(0, metrics.fDescent + mY, &result);
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
        mLastPaint = mLastPaintCandidate;
    }

    const SkIRect& getArea() const {
        return mArea;
    }

    /* called only while the picture is parsed */
    SkUnichar getUniChar(const SkBounder::GlyphRec& rec)
    {
        SkUnichar unichar;
        SkPaint::TextEncoding save = mPaint.getTextEncoding();
        mPaint.setTextEncoding(SkPaint::kUTF16_TextEncoding);
        mPaint.glyphsToUnichars(&rec.fGlyphID, 1, &unichar);
        mPaint.setTextEncoding(save);
        return unichar;
    }

    bool isSpace(const SkBounder::GlyphRec& rec)
    {
        if (mLastGlyph.fGlyphID == static_cast<uint16_t>(-1))
            return true;
        DBG_NAV_LOGD("mLastGlyph=((%g, %g),(%g, %g), %d)"
            " rec=((%g, %g),(%g, %g), %d) mLastUni=0x%04x '%c'",
            SkFixedToScalar(mLastGlyph.fLSB.fX),
            SkFixedToScalar(mLastGlyph.fLSB.fY),
            SkFixedToScalar(mLastGlyph.fRSB.fX),
            SkFixedToScalar(mLastGlyph.fRSB.fY), mLastGlyph.fGlyphID,
            SkFixedToScalar(rec.fLSB.fX), SkFixedToScalar(rec.fLSB.fY),
            SkFixedToScalar(rec.fRSB.fX), SkFixedToScalar(rec.fRSB.fY),
            rec.fGlyphID,
            mLastUni, mLastUni && mLastUni < 0x7f ? mLastUni : '?');
        bool newBaseLine = mLastGlyph.fLSB.fY != rec.fLSB.fY;
        if (newBaseLine)
            return true;
        SkFixed gapOne = mLastGlyph.fLSB.fX - rec.fRSB.fX;
        SkFixed gapTwo = rec.fLSB.fX - mLastGlyph.fRSB.fX;
        if (gapOne < 0 && gapTwo < 0)
            return false; // overlaps
        const SkBounder::GlyphRec& first = mLastGlyph.fLSB.fX < rec.fLSB.fX
            ? mLastGlyph : rec;
        const SkBounder::GlyphRec& second = mLastGlyph.fLSB.fX < rec.fLSB.fX
            ? rec : mLastGlyph;
        uint16_t firstGlyph = first.fGlyphID;
        SkScalar firstWidth = mLastPaint.measureText(&firstGlyph, sizeof(firstGlyph));
        SkFixed ceilWidth = SkIntToFixed(SkScalarCeil(firstWidth));
        SkFixed posNoSpace = first.fLSB.fX + ceilWidth;
        SkFixed ceilSpace = SkIntToFixed(SkFixedCeil(minSpaceWidth(mLastPaint)));
        SkFixed posWithSpace = posNoSpace + ceilSpace;
        SkFixed diffNoSpace = SkFixedAbs(second.fLSB.fX - posNoSpace);
        SkFixed diffWithSpace = SkFixedAbs(second.fLSB.fX - posWithSpace);
        DBG_NAV_LOGD("second=%g width=%g (%g) noSpace=%g (%g) withSpace=%g (%g)"
            " fontSize=%g",
            SkFixedToScalar(second.fLSB.fX),
            firstWidth, SkFixedToScalar(ceilWidth),
            SkFixedToScalar(posNoSpace), SkFixedToScalar(diffNoSpace),
            SkFixedToScalar(posWithSpace), SkFixedToScalar(diffWithSpace),
            mLastPaint.getTextSize());
        return diffWithSpace <= diffNoSpace;
    }

    SkFixed minSpaceWidth(SkPaint& paint)
    {
        if (mMinSpaceWidth == SK_FixedMax) {
            SkPaint::TextEncoding save = paint.getTextEncoding();
            paint.setTextEncoding(SkPaint::kUTF8_TextEncoding);
            SkScalar width = paint.measureText(" ", 1);
            mMinSpaceWidth = SkScalarToFixed(width * mMatrix.getScaleX());
            paint.setTextEncoding(save);
            DBG_NAV_LOGV("width=%g matrix sx/sy=(%g, %g) tx/ty=(%g, %g)"
                " mMinSpaceWidth=%g", width,
                mMatrix.getScaleX(), mMatrix.getScaleY(),
                mMatrix.getTranslateX(), mMatrix.getTranslateY(),
                SkFixedToScalar(mMinSpaceWidth));
        }
        return mMinSpaceWidth;
    }

    void recordGlyph(const SkBounder::GlyphRec& rec)
    {
        mLastCandidate = rec;
        mLastUniCandidate = getUniChar(rec);
        mLastPaintCandidate = mPaint;
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
        mLastPaint = check.mLastPaint;
        reset();
    }

    void setGlyph(CommonCheck& check)
    {
        mLastGlyph = check.mLastGlyph;
        mLastUni = check.mLastUni;
        mLastPaint = check.mLastPaint;
    }

    void setUp(const SkPaint& paint, const SkMatrix& matrix, SkScalar y,
            const void* text)
    {
        mMatrix = matrix;
        mPaint = paint;
        mText = static_cast<const uint16_t*>(text);
        mY = y;
        reset();
    }

    /* called only while the picture is parsed */
    int top() {
        if (mTop == INT_MAX) {
            SkPoint result;
            SkPaint::FontMetrics metrics;
            mPaint.getFontMetrics(&metrics);
            mMatrix.mapXY(0, metrics.fAscent + mY, &result);
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
    SkIRect mArea;
    SkBounder::GlyphRec mLastCandidate;
    SkBounder::GlyphRec mLastGlyph;
    SkPaint mLastPaint; // available after picture has been parsed
    SkPaint mLastPaintCandidate; // associated with candidate glyph
    SkUnichar mLastUni;
    SkUnichar mLastUniCandidate;
    SkMatrix mMatrix;
    SkPaint mPaint; // only set up while the picture is parsed
    const uint16_t* mText;
    SkScalar mY;
private:
    int mBase;
    int mBottom;
    SkFixed mMinSpaceWidth;
    int mTop;
    friend class EdgeCheck;
};

// generate the limit area for the new selection
class LineCheck : public CommonCheck {
public:
    LineCheck(int x, int y, const SkIRect& area)
        : INHERITED(area)
        , mX(x)
        , mY(y)
        , mInBetween(false)
    {
        mLast.setEmpty();
    }

    void finish(const SkRegion& selectedRgn)
    {
        if (!mParagraphs.count() && mLast.isEmpty())
            return;
        processLine();
        bool above = false;
        bool below = false;
        bool selected = false;
        SkRegion localRgn(selectedRgn);
        localRgn.translate(-mArea.fLeft, -mArea.fTop, &localRgn);
        DBG_NAV_LOGD("localRgn=(%d,%d,%d,%d)",
            localRgn.getBounds().fLeft, localRgn.getBounds().fTop,
            localRgn.getBounds().fRight, localRgn.getBounds().fBottom);
        for (int index = 0; index < mParagraphs.count(); index++) {
            const SkIRect& rect = mParagraphs[index];
            bool localSelected = localRgn.intersects(rect);
            DBG_NAV_LOGD("[%d] rect=(%d,%d,%d,%d)", index, rect.fLeft, rect.fTop,
                rect.fRight, rect.fBottom);
            if (localSelected) {
                DBG_NAV_LOGD("[%d] localSelected=true", index);
                *mSelected.append() = rect;
            }
            if (rect.fRight <= mX || rect.fLeft >= mX)
                continue;
            if (mY > rect.fBottom) {
                below = true;
                selected |= localSelected;
                DBG_NAV_LOGD("[%d] below=true localSelected=%s", index,
                    localSelected ? "true" : "false");
           }
            if (mY < rect.fTop) {
                above = true;
                selected |= localSelected;
                DBG_NAV_LOGD("[%d] above=true localSelected=%s", index,
                    localSelected ? "true" : "false");
            }
        }
        DBG_NAV_LOGD("mX=%d mY=%d above=%s below=%s selected=%s",
            mX, mY, above ? "true" : "false", below ? "true" : "false",
            selected ? "true" : "false");
        mInBetween = above && below && selected;
    }

    bool inBetween() const
    {
        return mInBetween;
    }

    bool inColumn(const SkIRect& test) const
    {
        for (int index = 0; index < mSelected.count(); index++) {
            const SkIRect& rect = mSelected[index];
            if (rect.fRight > test.fLeft && rect.fLeft < test.fRight)
                return true;
        }
        return false;
    }

    bool inColumn(int x, int y) const
    {
        for (int index = 0; index < mSelected.count(); index++) {
            const SkIRect& rect = mSelected[index];
            if (rect.contains(x, y))
                return true;
        }
        return false;
    }

    virtual bool onIRect(const SkIRect& rect)
    {
        SkIRect bounds;
        bounds.set(rect.fLeft, top(), rect.fRight, bottom());
        // assume that characters must be consecutive to describe spaces
        // (i.e., don't join rects drawn at different times)
        if (bounds.fTop != mLast.fTop || bounds.fBottom != mLast.fBottom
            || bounds.fLeft > mLast.fRight + minSpaceWidth(mPaint)
            || bounds.fLeft < mLast.fLeft) {
            processLine();
            mLast = bounds;
        } else
            mLast.join(bounds);
        return false;
    }

    void processLine()
    {
        // assume line spacing of 1.5
        int lineHeight = bottom() - top();
        mLast.inset(0, -lineHeight >> 1);
        // collect arrays of rectangles making up glyphs below or above this one
        for (int index = 0; index < mParagraphs.count(); index++) {
            SkIRect& rect = mParagraphs[index];
            if (SkIRect::Intersects(rect, mLast)) {
                rect.join(mLast);
                return;
            }
        }
        *mParagraphs.append() = mLast;
    }

protected:
    int mX;
    int mY;
    SkIRect mLast;
    SkTDArray<SkIRect> mParagraphs;
    SkTDArray<SkIRect> mSelected;
    bool mInBetween;
private:
    typedef CommonCheck INHERITED;
};

class SelectText::FirstCheck : public CommonCheck {
public:
    FirstCheck(int x, int y, const SkIRect& area)
        : INHERITED(area)
        , mLineCheck(0)
        , mFocusX(x - area.fLeft)
        , mFocusY(y - area.fTop)
        , mBestInColumn(false)
        , mRecordGlyph(false)
    {
        reset();
    }

    const SkIRect& adjustedBounds(int* base)
    {
        *base = mBestBase + mArea.fTop;
        mBestBounds.offset(mArea.fLeft, mArea.fTop);
        DBG_NAV_LOGD("FirstCheck mBestBounds:(%d, %d, %d, %d) mTop=%d mBottom=%d",
            mBestBounds.fLeft, mBestBounds.fTop, mBestBounds.fRight, 
            mBestBounds.fBottom, topDebug(), bottomDebug());
        return mBestBounds;
    }

    int focusX() const { return mFocusX; }
    int focusY() const { return mFocusY; }

    virtual bool onIRectGlyph(const SkIRect& rect,
        const SkBounder::GlyphRec& rec)
    {
        /* compute distance from rectangle center.
         * centerX = (rect.L + rect.R) / 2
         * multiply centerX and comparison x by 2 to retain better precision
         */
        SkIRect testBounds = {rect.fLeft, top(), rect.fRight, bottom()};
        // dx and dy are the distances from the tested edge
        // The edge distance is paramount if the test point is far away
        int dx = std::max(0, std::max(testBounds.fLeft - mFocusX,
            mFocusX - testBounds.fRight));
        int dy = std::max(0, std::max(testBounds.fTop - mFocusY,
            mFocusY - testBounds.fBottom));
        bool testInColumn = false;
        bool inBetween = false;
        bool inFocus = false;
        if (mLineCheck) {
            testInColumn = mLineCheck->inColumn(testBounds);
            inBetween = mLineCheck->inBetween();
            inFocus = mLineCheck->inColumn(mFocusX, mFocusY);
        }
#ifdef EXTRA_NOISY_LOGGING
        if (dy < 10) {
            SkUnichar ch = getUniChar(rec);
            DBG_NAV_LOGD("FC dx/y=%d,%d mDx/y=%d,%d test=%d,%d,%d,%d"
                " best=%d,%d,%d,%d bestIn=%s tween=%s testIn=%s focus=%s ch=%c",
                dx, dy, mDx, mDy,
                testBounds.fLeft, testBounds.fTop, testBounds.fRight,
                testBounds.fBottom, mBestBounds.fLeft, mBestBounds.fTop,
                mBestBounds.fRight, mBestBounds.fBottom, 
                mBestInColumn ? "true" : "false", inBetween ? "true" : "false",
                testInColumn ? "true" : "false", inFocus ? "true" : "false",
                ch < 0x7f ? ch : '?');
        }
#endif
        if ((mBestInColumn || inBetween) && !testInColumn) {
#ifdef EXTRA_NOISY_LOGGING
            if (dy < 10) DBG_NAV_LOG("FirstCheck reject column");
#endif
            return false;
        }
        bool ignoreColumn = mBestInColumn == testInColumn || !inFocus;
        if (ignoreColumn && dy > 0 && (mDy < dy
            || (mDy == dy && dx > 0 && mDx <= dx))) {
#ifdef EXTRA_NOISY_LOGGING
            if (dy < 10) DBG_NAV_LOG("FirstCheck reject edge");
#endif
            return false;
        }
        // cx and cy are the distances from the tested center
        // The center distance is used when the test point is over the text
        int cx = std::abs(((testBounds.fLeft + testBounds.fRight) >> 1)
                - mFocusX);
        int cy = std::abs(((testBounds.fTop + testBounds.fBottom) >> 1)
                - mFocusY);
        if (ignoreColumn && dy == 0 && mDy == 0) {
            if (mCy < cy) {
#ifdef EXTRA_NOISY_LOGGING
                DBG_NAV_LOGD("FirstCheck reject cy=%d mCy=%d", cy, mCy);
#endif
                return false;
            }
            if (mCy == cy) {
                if (dx == 0 && mDx == 0) {
                    if (mCx < cx) {
#ifdef EXTRA_NOISY_LOGGING
                        DBG_NAV_LOGD("FirstCheck reject cx=%d mCx=%d", cx, mCx);
#endif
                        return false;
                    }
                } else if (dx > 0 && mDx <= dx) {
#ifdef EXTRA_NOISY_LOGGING
                    DBG_NAV_LOGD("FirstCheck reject dx=%d mDx=%d", dx, mDx);
#endif
                    return false;
                }
            }
        }
#ifdef EXTRA_NOISY_LOGGING
        if (dy < 10) {
            DBG_NAV_LOGD("FirstCheck cx/y=(%d,%d)", cx, cy);
        }
#endif
        mBestBase = base();
        mBestBounds = testBounds;
        mBestInColumn = testInColumn;
#ifndef EXTRA_NOISY_LOGGING
        if (dy < 10 && dx < 10)
#endif
        {
#if DEBUG_NAV_UI
            SkUnichar ch = getUniChar(rec);
#endif
            DBG_NAV_LOGD("FirstCheck dx/y=(%d,%d) mFocus=(%d,%d)"
                " mBestBounds={%d,%d,r=%d,b=%d} inColumn=%s ch=%c",
                dx, dy, mFocusX, mFocusY,
                mBestBounds.fLeft, mBestBounds.fTop,
                mBestBounds.fRight, mBestBounds.fBottom,
                mBestInColumn ? "true" : "false", ch < 0x7f ? ch : '?');
        }
        mCx = cx;
        mCy = cy;
        mDx = dx;
        mDy = dy;
        if (mRecordGlyph)
            recordGlyph(rec);
        return false;
    }

    void reset()
    {
        mBestBounds.setEmpty();
        mDx = mDy = mCx = mCy = INT_MAX;
    }

    void setLines(const LineCheck* lineCheck) { mLineCheck = lineCheck; }
    void setRecordGlyph() { mRecordGlyph = true; }

protected:
    const LineCheck* mLineCheck;
    int mBestBase;
    SkIRect mBestBounds;
    int mCx;
    int mCy;
    int mDx;
    int mDy;
    int mFocusX;
    int mFocusY;
    bool mBestInColumn;
    bool mRecordGlyph;
private:
    typedef CommonCheck INHERITED;
};

class SelectText::EdgeCheck : public SelectText::FirstCheck {
public:
    EdgeCheck(int x, int y, const SkIRect& area, CommonCheck& last, bool left)
        : INHERITED(x, y, area)
        , mLast(area)
        , mLeft(left)
    {
        mLast.set(last); // CommonCheck::set()
        setGlyph(last);
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
        dx = abs(dx);
        dy = abs(dy);
        if (mLeft ? mFocusX <= rect.fLeft : mFocusX >= rect.fRight) {
            if (dx <= 10 && dy <= 10) {
                DBG_NAV_LOGD("EdgeCheck fLeft=%d fRight=%d mFocusX=%d dx=%d dy=%d",
                    rect.fLeft, rect.fRight, mFocusX, dx, dy);
            }
            return false;
        }
        if (mDy > dy || (mDy == dy && mDx > dx)) {
            if (rec.fLSB == mLastGlyph.fLSB && rec.fRSB == mLastGlyph.fRSB) {
                DBG_NAV_LOGD("dup rec.fLSB.fX=%g rec.fRSB.fX=%g",
                SkFixedToScalar(rec.fLSB.fX), SkFixedToScalar(rec.fRSB.fX));
                return false;
            }
            recordGlyph(rec);
            mDx = dx;
            mDy = dy;
            mBestBase = base();
            mBestBounds.set(rect.fLeft, top(), rect.fRight, bottom());
            if (dx <= 10 && dy <= 10) {
                DBG_NAV_LOGD("EdgeCheck mBestBounds={%d,%d,r=%d,b=%d} dx/y=(%d, %d)",
                    mBestBounds.fLeft, mBestBounds.fTop,
                    mBestBounds.fRight, mBestBounds.fBottom, dx, dy);
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
        mLast.set(*this); // CommonCheck::set()
    }

protected:
    CommonCheck mLast;
    bool mLeft;
private:
    typedef SelectText::FirstCheck INHERITED;
};

class FindFirst : public CommonCheck {
public:
    FindFirst(const SkIRect& area)
        : INHERITED(area)
    {
        mBestBounds.set(area.width(), area.height(), area.width(), area.height());
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
    FindLast(const SkIRect& area)
        : INHERITED(area)
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
        : INHERITED(area)
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
        int spaceGap = SkFixedRound(minSpaceWidth(mPaint) * 3);
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
            if (full == mStart)
                addLastToRegion();
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

    WTF::String text() {
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
        return WTF::String(mSelectText.begin(), mSelectText.count());
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

class TextCanvas : public ParseCanvas {
public:

    TextCanvas(CommonCheck* bounder)
            : mBounder(*bounder) {
        setBounder(bounder);
        SkBitmap bitmap;
        const SkIRect& area = bounder->getArea();
        bitmap.setConfig(SkBitmap::kARGB_8888_Config, area.width(), 
            area.height());
        setBitmapDevice(bitmap);
        translate(SkIntToScalar(-area.fLeft), SkIntToScalar(-area.fTop));
#ifdef DEBUG_NAV_UI
        const SkIRect& clip = getTotalClip().getBounds();
        const SkMatrix& matrix = getTotalMatrix();
        DBG_NAV_LOGD("bitmap=(%d,%d) clip=(%d,%d,%d,%d) matrix=(%g,%g)",
            bitmap.width(), bitmap.height(), clip.fLeft, clip.fTop,
            clip.fRight, clip.fBottom, matrix.getTranslateX(), matrix.getTranslateY());
#endif
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

    virtual void commonDrawBitmap(const SkBitmap& bitmap, const SkIRect* rect,
                              const SkMatrix& matrix, const SkPaint& paint) {
    }

    virtual void drawSprite(const SkBitmap& bitmap, int left, int top,
                            const SkPaint* paint = NULL) {
    }

    virtual void drawText(const void* text, size_t byteLength, SkScalar x, 
                          SkScalar y, const SkPaint& paint) {
        mBounder.setUp(paint, getTotalMatrix(), y, text);
        INHERITED::drawText(text, byteLength, x, y, paint);
    }

    virtual void drawPosTextH(const void* text, size_t byteLength,
                              const SkScalar xpos[], SkScalar constY,
                              const SkPaint& paint) {
        mBounder.setUp(paint, getTotalMatrix(), constY, text);
        INHERITED::drawPosTextH(text, byteLength, xpos, constY, paint);
    }

    virtual void drawVertices(VertexMode vmode, int vertexCount,
                              const SkPoint vertices[], const SkPoint texs[],
                              const SkColor colors[], SkXfermode* xmode,
                              const uint16_t indices[], int indexCount,
                              const SkPaint& paint) {
    }

    CommonCheck& mBounder;
private:
    typedef ParseCanvas INHERITED;
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
    TextCanvas checker(&builder);
    checker.drawPicture(const_cast<SkPicture&>(picture));
    bool flipped = builder.flipped();
    if (flipped) {
        TextCanvas checker(&builder);
        checker.drawPicture(const_cast<SkPicture&>(picture));
    }
    builder.finish();
    region->translate(area.fLeft, area.fTop);
    return flipped;
}

static SkIRect findFirst(const SkPicture& picture, int* base)
{
    SkIRect area;
    area.set(0, 0, picture.width(), picture.height());
    FindFirst finder(area);
    TextCanvas checker(&finder);
    checker.drawPicture(const_cast<SkPicture&>(picture));
    return finder.bestBounds(base);
}

static SkIRect findLast(const SkPicture& picture, int* base)
{
    SkIRect area;
    area.set(0, 0, picture.width(), picture.height());
    FindLast finder(area);
    TextCanvas checker(&finder);
    checker.drawPicture(const_cast<SkPicture&>(picture));
    return finder.bestBounds(base);
}

static WTF::String text(const SkPicture& picture, const SkIRect& area,
        const SkIRect& start, int startBase, const SkIRect& end,
        int endBase, bool flipped)
{
    TextExtractor extractor(start, startBase, end, endBase, area, flipped);
    TextCanvas checker(&extractor);
    checker.drawPicture(const_cast<SkPicture&>(picture));
    return extractor.text();
}

#define CONTROL_NOTCH 16
// TODO: Now that java is the one actually drawing these, get the real values
// from the drawable itself
#define CONTROL_HEIGHT 47
#define CONTROL_WIDTH 26
#define CONTROL_SLOP 5
#define STROKE_WIDTH 1.0f
#define STROKE_OUTSET 3.5f
#define STROKE_I_OUTSET 4 // (int) ceil(STROKE_OUTSET)
#define STROKE_COLOR 0x66000000
#define OUTER_COLOR 0x33000000
#define INNER_COLOR 0xe6aae300

SelectText::SelectText()
    : m_controlWidth(CONTROL_WIDTH)
    , m_controlHeight(CONTROL_HEIGHT)
    , m_controlSlop(CONTROL_SLOP)
{
    m_picture = 0;
    reset();
    SkPaint paint;
    SkRect oval;

    SkPath startOuterPath;
    oval.set(-CONTROL_WIDTH - STROKE_OUTSET, CONTROL_NOTCH - STROKE_OUTSET,
        -CONTROL_WIDTH + STROKE_OUTSET, CONTROL_NOTCH + STROKE_OUTSET);
    startOuterPath.arcTo(oval, 180, 45, true);
    oval.set(-STROKE_OUTSET, -STROKE_OUTSET,  STROKE_OUTSET, STROKE_OUTSET);
    startOuterPath.arcTo(oval, 180 + 45, 135, false);
    oval.set(-STROKE_OUTSET, CONTROL_HEIGHT - STROKE_OUTSET,
        STROKE_OUTSET, CONTROL_HEIGHT + STROKE_OUTSET);
    startOuterPath.arcTo(oval, 0, 90, false);
    oval.set(-CONTROL_WIDTH - STROKE_OUTSET, CONTROL_HEIGHT - STROKE_OUTSET,
        -CONTROL_WIDTH + STROKE_OUTSET, CONTROL_HEIGHT + STROKE_OUTSET);
    startOuterPath.arcTo(oval, 90, 90, false);
    startOuterPath.close();
    SkPath startInnerPath;
    startInnerPath.moveTo(-CONTROL_WIDTH, CONTROL_NOTCH);
    startInnerPath.lineTo(-CONTROL_WIDTH, CONTROL_HEIGHT);
    startInnerPath.lineTo(0, CONTROL_HEIGHT);
    startInnerPath.lineTo(0, 0);
    startInnerPath.close();
    startOuterPath.addPath(startInnerPath, 0, 0);

    SkCanvas* canvas = m_startControl.beginRecording(
        CONTROL_WIDTH + STROKE_OUTSET * 2,
        CONTROL_HEIGHT + STROKE_OUTSET * 2);
    paint.setAntiAlias(true);
    paint.setColor(INNER_COLOR);
    paint.setStyle(SkPaint::kFill_Style);
    canvas->drawPath(startInnerPath, paint);
    paint.setColor(OUTER_COLOR);
    canvas->drawPath(startOuterPath, paint);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(STROKE_COLOR);
    paint.setStrokeWidth(STROKE_WIDTH);
    canvas->drawPath(startInnerPath, paint);
    m_startControl.endRecording();

    SkPath endOuterPath;
    oval.set(-STROKE_OUTSET, -STROKE_OUTSET,  STROKE_OUTSET, STROKE_OUTSET);
    endOuterPath.arcTo(oval, 180, 135, true);
    oval.set(CONTROL_WIDTH - STROKE_OUTSET, CONTROL_NOTCH - STROKE_OUTSET,
        CONTROL_WIDTH + STROKE_OUTSET, CONTROL_NOTCH + STROKE_OUTSET);
    endOuterPath.arcTo(oval, 360 - 45, 45, false);
    oval.set(CONTROL_WIDTH - STROKE_OUTSET, CONTROL_HEIGHT - STROKE_OUTSET,
        CONTROL_WIDTH + STROKE_OUTSET, CONTROL_HEIGHT + STROKE_OUTSET);
    endOuterPath.arcTo(oval, 0, 90, false);
    oval.set(-STROKE_OUTSET, CONTROL_HEIGHT - STROKE_OUTSET,
        STROKE_OUTSET, CONTROL_HEIGHT + STROKE_OUTSET);
    endOuterPath.arcTo(oval, 90, 90, false);
    startOuterPath.close();
    SkPath endInnerPath;
    endInnerPath.moveTo(0, 0);
    endInnerPath.lineTo(0, CONTROL_HEIGHT);
    endInnerPath.lineTo(CONTROL_WIDTH, CONTROL_HEIGHT);
    endInnerPath.lineTo(CONTROL_WIDTH, CONTROL_NOTCH);
    endInnerPath.close();
    endOuterPath.addPath(endInnerPath, 0, 0);

    canvas = m_endControl.beginRecording(CONTROL_WIDTH + STROKE_OUTSET * 2,
        CONTROL_HEIGHT + STROKE_OUTSET * 2);
    paint.setColor(INNER_COLOR);
    paint.setStyle(SkPaint::kFill_Style);
    canvas->drawPath(endInnerPath, paint);
    paint.setColor(OUTER_COLOR);
    canvas->drawPath(endOuterPath, paint);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(STROKE_COLOR);
    paint.setStrokeWidth(STROKE_WIDTH);
    canvas->drawPath(endInnerPath, paint);
    m_endControl.endRecording();
}

SelectText::~SelectText()
{
    SkSafeUnref(m_picture);
}

void SelectText::draw(SkCanvas* canvas, LayerAndroid* layer, IntRect* inval)
{
    if (m_layerId != layer->uniqueId())
        return;
    // reset m_picture to match m_layerId
    SkSafeUnref(m_picture);
    m_picture = layer->picture();
    SkSafeRef(m_picture);
    DBG_NAV_LOGD("m_extendSelection=%d m_drawPointer=%d layer [%d]",
        m_extendSelection, m_drawPointer, layer->uniqueId());
    if (m_extendSelection)
        drawSelectionRegion(canvas, inval);
    if (m_drawPointer)
        drawSelectionPointer(canvas, inval);
}

static void addInval(IntRect* inval, const SkCanvas* canvas,
        const SkRect& bounds) {
    const SkMatrix& matrix = canvas->getTotalMatrix();
    SkRect transformed;
    matrix.mapRect(&transformed, bounds);
    SkIRect iTrans;
    transformed.round(&iTrans);
    inval->unite(iTrans);
}

void SelectText::drawSelectionPointer(SkCanvas* canvas, IntRect* inval)
{
    SkPath path;
    if (m_extendSelection)
        getSelectionCaret(&path);
    else
        getSelectionArrow(&path);
    SkPixelXorXfermode xorMode(SK_ColorWHITE);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(SK_ColorBLACK);
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
    SkRect bounds = path.getBounds();
    bounds.inset(-SK_Scalar1 * 2, -SK_Scalar1 * 2); // stroke width
    addInval(inval, canvas, bounds);
    canvas->restoreToCount(sc);
}

static void addStart(SkRegion* diff, const SkIRect& rect)
{
    SkIRect bounds;
    bounds.set(rect.fLeft - CONTROL_WIDTH - STROKE_I_OUTSET,
        rect.fBottom - STROKE_I_OUTSET, rect.fLeft + STROKE_I_OUTSET,
        rect.fBottom + CONTROL_HEIGHT + STROKE_I_OUTSET);
    diff->op(bounds, SkRegion::kUnion_Op);
}

static void addEnd(SkRegion* diff, const SkIRect& rect)
{
    SkIRect bounds;
    bounds.set(rect.fRight - STROKE_I_OUTSET, rect.fBottom - STROKE_I_OUTSET,
        rect.fRight + CONTROL_WIDTH + STROKE_I_OUTSET,
        rect.fBottom + CONTROL_HEIGHT + STROKE_I_OUTSET);
    diff->op(bounds, SkRegion::kUnion_Op);
}

void SelectText::getSelectionRegion(const IntRect& vis, SkRegion *region,
                                    LayerAndroid* root)
{
    SkIRect ivisBounds = vis;
    ivisBounds.join(m_selStart);
    ivisBounds.join(m_selEnd);
    region->setEmpty();
    buildSelection(*m_picture, ivisBounds, m_selStart, m_startBase,
        m_selEnd, m_endBase, region);
    if (root && m_layerId) {
        Layer* layer = root->findById(m_layerId);
        while (layer) {
            const SkPoint& pos = layer->getPosition();
            region->translate(pos.fX, pos.fY);
            layer = layer->getParent();
        }
    }
}

void SelectText::drawSelectionRegion(SkCanvas* canvas, IntRect* inval)
{
    if (!m_picture)
        return;
    SkIRect ivisBounds = m_visibleRect;
    ivisBounds.join(m_selStart);
    ivisBounds.join(m_selEnd);
    DBG_NAV_LOGD("m_selStart=(%d,%d,r=%d,b=%d) m_selEnd=(%d,%d,r=%d,b=%d)"
        " ivisBounds=(%d,%d,r=%d,b=%d)",
        m_selStart.fLeft, m_selStart.fTop, m_selStart.fRight, m_selStart.fBottom,
        m_selEnd.fLeft, m_selEnd.fTop, m_selEnd.fRight, m_selEnd.fBottom,
        ivisBounds.fLeft, ivisBounds.fTop, ivisBounds.fRight, ivisBounds.fBottom);
    if (m_lastSelRegion != m_selRegion)
        m_lastSelRegion.set(m_selRegion);
    SkRegion diff(m_lastSelRegion);
    m_selRegion.setEmpty();
    m_flipped = buildSelection(*m_picture, ivisBounds, m_selStart, m_startBase,
        m_selEnd, m_endBase, &m_selRegion);
    SkPath path;
    m_selRegion.getBoundaryPath(&path);
    path.setFillType(SkPath::kEvenOdd_FillType);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SkColorSetARGB(0x80, 0x83, 0xCC, 0x39));
    canvas->drawPath(path, paint);
    // experiment to draw touchable controls that resize the selection
    float scale = m_controlHeight / (float)CONTROL_HEIGHT;
    canvas->save();
    canvas->translate(m_selStart.fLeft, m_selStart.fBottom);
    canvas->scale(scale, scale);
    canvas->drawPicture(m_startControl);
    canvas->restore();
    canvas->save();
    canvas->translate(m_selEnd.fRight, m_selEnd.fBottom);
    canvas->scale(scale, scale);
    canvas->drawPicture(m_endControl);
    canvas->restore();

#if DEBUG_TOUCH_HANDLES
    SkRect touchHandleRect;
    paint.setColor(SkColorSetARGB(0x60, 0xFF, 0x00, 0x00));
    touchHandleRect.set(0, m_selStart.fBottom, m_selStart.fLeft, 0);
    touchHandleRect.fBottom = touchHandleRect.fTop + m_controlHeight;
    touchHandleRect.fLeft = touchHandleRect.fRight - m_controlWidth;
    canvas->drawRect(touchHandleRect, paint);
    touchHandleRect.inset(-m_controlSlop, -m_controlSlop);
    canvas->drawRect(touchHandleRect, paint);
    touchHandleRect.set(m_selEnd.fRight, m_selEnd.fBottom, 0, 0);
    touchHandleRect.fBottom = touchHandleRect.fTop + m_controlHeight;
    touchHandleRect.fRight = touchHandleRect.fLeft + m_controlWidth;
    canvas->drawRect(touchHandleRect, paint);
    touchHandleRect.inset(-m_controlSlop, -m_controlSlop);
    canvas->drawRect(touchHandleRect, paint);
#endif

    SkIRect a = diff.getBounds();
    SkIRect b = m_selRegion.getBounds();
    diff.op(m_selRegion, SkRegion::kXOR_Op);
    SkIRect c = diff.getBounds();
    DBG_NAV_LOGD("old=(%d,%d,r=%d,b=%d) new=(%d,%d,r=%d,b=%d) diff=(%d,%d,r=%d,b=%d)",
        a.fLeft, a.fTop, a.fRight, a.fBottom, b.fLeft, b.fTop, b.fRight, b.fBottom,
        c.fLeft, c.fTop, c.fRight, c.fBottom);
    DBG_NAV_LOGD("lastStart=(%d,%d,r=%d,b=%d) m_lastEnd=(%d,%d,r=%d,b=%d)",
        m_lastStart.fLeft, m_lastStart.fTop, m_lastStart.fRight, m_lastStart.fBottom,
        m_lastEnd.fLeft, m_lastEnd.fTop, m_lastEnd.fRight, m_lastEnd.fBottom);
    if (!m_lastDrawnStart.isEmpty())
        addStart(&diff, m_lastDrawnStart);
    if (m_lastStart != m_selStart) {
        m_lastDrawnStart = m_lastStart;
        m_lastStart = m_selStart;
    }
    addStart(&diff, m_selStart);
    if (!m_lastDrawnEnd.isEmpty())
        addEnd(&diff, m_lastDrawnEnd);
    if (m_lastEnd != m_selEnd) {
        m_lastDrawnEnd = m_lastEnd;
        m_lastEnd = m_selEnd;
    }
    addEnd(&diff, m_selEnd);
    SkIRect iBounds = diff.getBounds();
    DBG_NAV_LOGD("diff=(%d,%d,r=%d,b=%d)",
        iBounds.fLeft, iBounds.fTop, iBounds.fRight, iBounds.fBottom);
    SkRect bounds;
    bounds.set(iBounds);
    addInval(inval, canvas, bounds);
}

void SelectText::extendSelection(const IntRect& vis, int x, int y)
{
    if (!m_picture)
        return;
    setVisibleRect(vis);
    SkIRect clipRect = m_visibleRect;
    int base;
    DBG_NAV_LOGD("extend x/y=%d,%d m_startOffset=%d,%d", x, y,
        m_startOffset.fX, m_startOffset.fY);
    x -= m_startOffset.fX;
    y -= m_startOffset.fY;
    if (m_startSelection) {
        if (!clipRect.contains(x, y)
                || !clipRect.contains(m_original.fX, m_original.fY)) {
            clipRect.set(m_original.fX, m_original.fY, x, y);
            clipRect.sort();
            clipRect.inset(-m_visibleRect.width(), -m_visibleRect.height());
        }
        FirstCheck center(m_original.fX, m_original.fY, clipRect);
        m_selStart = m_selEnd = findClosest(center, *m_picture, &base);
        if (m_selStart.isEmpty())
            return;
        DBG_NAV_LOGD("selStart clip=(%d,%d,%d,%d) m_original=%d,%d"
            " m_selStart=(%d,%d,%d,%d)", clipRect.fLeft, clipRect.fTop,
            clipRect.fRight, clipRect.fBottom, m_original.fX, m_original.fY,
            m_selStart.fLeft, m_selStart.fTop, m_selStart.fRight, m_selStart.fBottom);
        m_startBase = m_endBase = base;
        m_startSelection = false;
        m_extendSelection = true;
        m_original.fX = m_original.fY = 0;
    }
    DBG_NAV_LOGD("extend x/y=%d,%d m_original=%d,%d", x, y,
        m_original.fX, m_original.fY);
    x -= m_original.fX;
    y -= m_original.fY;
    if (!clipRect.contains(x, y) || !clipRect.contains(m_selStart)) {
        clipRect.set(m_selStart.fLeft, m_selStart.fTop, x, y);
        clipRect.sort();
        clipRect.inset(-m_visibleRect.width(), -m_visibleRect.height());
    }
    DBG_NAV_LOGD("extend clip=(%d,%d,%d,%d) x/y=%d,%d wordSel=%s outsideWord=%s",
        clipRect.fLeft, clipRect.fTop, clipRect.fRight, clipRect.fBottom, x, y,
        m_wordSelection ? "true" : "false", m_outsideWord ? "true" : "false");
    FirstCheck extension(x, y, clipRect);
    SkIRect found = findClosest(extension, *m_picture, &base);
    if (m_wordSelection) {
        SkIRect wordBounds = m_wordBounds;
        if (!m_outsideWord)
            wordBounds.inset(-TOUCH_SLOP, -TOUCH_SLOP);
        DBG_NAV_LOGD("x=%d y=%d wordBounds=(%d,%d,r=%d,b=%d)"
            " found=(%d,%d,r=%d,b=%d)", x, y, wordBounds.fLeft, wordBounds.fTop,
            wordBounds.fRight, wordBounds.fBottom, found.fLeft, found.fTop,
            found.fRight, found.fBottom);
        if (wordBounds.contains(x, y)) {
            DBG_NAV_LOG("wordBounds.contains=true");
            m_outsideWord = false;
            return;
        }
        m_outsideWord = true;
        if (found.fBottom <= wordBounds.fTop)
            m_hitTopLeft = true;
        else if (found.fTop >= wordBounds.fBottom)
            m_hitTopLeft = false;
        else
            m_hitTopLeft = (found.fLeft + found.fRight)
                < (wordBounds.fLeft + wordBounds.fRight);
    }
    DBG_NAV_LOGD("x=%d y=%d m_startSelection=%s %s=(%d, %d, %d, %d)"
        " m_extendSelection=%s",
        x, y, m_startSelection ? "true" : "false",
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

SkIRect SelectText::findClosest(FirstCheck& check, const SkPicture& picture,
        int* base)
{
    LineCheck lineCheck(check.focusX(), check.focusY(), check.getArea());
    TextCanvas lineChecker(&lineCheck);
    lineChecker.drawPicture(const_cast<SkPicture&>(picture));
    lineCheck.finish(m_selRegion);
    check.setLines(&lineCheck);
    TextCanvas checker(&check);
    checker.drawPicture(const_cast<SkPicture&>(picture));
    check.finishGlyph();
    return check.adjustedBounds(base);
}

SkIRect SelectText::findEdge(const SkPicture& picture, const SkIRect& area,
        int x, int y, bool left, int* base)
{
    SkIRect result;
    result.setEmpty();
    FirstCheck center(x, y, area);
    center.setRecordGlyph();
    int closestBase;
    SkIRect closest = findClosest(center, picture, &closestBase);
    SkIRect sloppy = closest;
    sloppy.inset(-TOUCH_SLOP, -TOUCH_SLOP);
    if (!sloppy.contains(x, y)) {
        DBG_NAV_LOGD("sloppy=(%d, %d, %d, %d) area=(%d, %d, %d, %d) x/y=%d,%d",
            sloppy.fLeft, sloppy.fTop, sloppy.fRight, sloppy.fBottom,
            area.fLeft, area.fTop, area.fRight, area.fBottom, x, y);
        return result;
    }
    EdgeCheck edge(x, y, area, center, left);
    do { // detect left or right until there's a gap
        DBG_NAV_LOGD("edge=%p picture=%p area=%d,%d,%d,%d",
            &edge, &picture, area.fLeft, area.fTop, area.fRight, area.fBottom);
        TextCanvas checker(&edge);
        checker.drawPicture(const_cast<SkPicture&>(picture));
        edge.finishGlyph();
        if (!edge.adjacent()) {
            if (result.isEmpty()) {
                *base = closestBase;
                DBG_NAV_LOGD("closest=%d,%d,%d,%d", closest.fLeft,
                    closest.fTop, closest.fRight, closest.fBottom);
                return closest;
            }
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

SkIRect SelectText::findLeft(const SkPicture& picture, const SkIRect& area,
        int x, int y, int* base)
{
    return findEdge(picture, area, x, y, true, base);
}

SkIRect SelectText::findRight(const SkPicture& picture, const SkIRect& area,
        int x, int y, int* base)
{
    return findEdge(picture, area, x, y, false, base);
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
    test.set(cx, cy, cx + m_controlWidth, cy + m_controlHeight);
    test.inset(-m_controlSlop, -m_controlSlop);
    DBG_HANDLE_LOG("checking if %dx%d,%d-%d contains %dx%d",
                   cx, cy, m_controlWidth, m_controlHeight, x,  y);
    return test.contains(x, y);
}

bool SelectText::hitStartHandle(int x, int y) const
{
    int left = m_selStart.fLeft - m_controlWidth;
    return hitCorner(left, m_selStart.fBottom, x, y);
}

bool SelectText::hitEndHandle(int x, int y) const
{
    int left = m_selEnd.fRight;
    return hitCorner(left, m_selEnd.fBottom, x, y);
}

bool SelectText::hitSelection(int x, int y) const
{
    x -= m_startOffset.fX;
    y -= m_startOffset.fY;
    if (hitStartHandle(x, y))
        return true;
    if (hitEndHandle(x, y))
        return true;
    return m_selRegion.contains(x, y);
}

void SelectText::getSelectionHandles(int* handles, LayerAndroid* root)
{
    handles[0] = m_selStart.fLeft;
    handles[1] = m_selStart.fBottom;
    handles[2] = m_selEnd.fRight;
    handles[3] = m_selEnd.fBottom;
    if (root && m_layerId) {
        Layer* layer = root->findById(m_layerId);
        while (layer) {
            const SkPoint& pos = layer->getPosition();
            handles[0] += pos.fX;
            handles[2] += pos.fX;
            handles[1] += pos.fY;
            handles[3] += pos.fY;
            layer = layer->getParent();
        }
    }
}

void SelectText::moveSelection(const IntRect& vis, int x, int y)
{
    if (!m_picture)
        return;
    x -= m_startOffset.fX;
    y -= m_startOffset.fY;
    setVisibleRect(vis);
    SkIRect clipRect = m_visibleRect;
    clipRect.join(m_selStart);
    clipRect.join(m_selEnd);
    FirstCheck center(x, y, clipRect);
    int base;
    SkIRect found = findClosest(center, *m_picture, &base);
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
    m_lastStart.setEmpty();
    m_lastDrawnStart.setEmpty();
    m_selEnd.setEmpty();
    m_lastEnd.setEmpty();
    m_lastDrawnEnd.setEmpty();
    m_extendSelection = false;
    m_startSelection = false;
    SkSafeUnref(m_picture);
    m_picture = 0;
    m_layerId = 0;
}

IntPoint SelectText::selectableText(const CachedRoot* root)
{
    int x = 0;
    int y = 0;
    SkPicture* picture = root->pictureAt(&x, &y, &m_layerId);
    if (!picture) {
        DBG_NAV_LOG("picture==0");
        return IntPoint(0, 0);
    }
    int width = picture->width();
    int height = picture->height();
    IntRect vis(0, 0, width, height);
    FirstCheck center(width >> 1, height >> 1, vis);
    int base;
    const SkIRect& closest = findClosest(center, *picture, &base);
    return IntPoint((closest.fLeft + closest.fRight) >> 1,
        (closest.fTop + closest.fBottom) >> 1);
}

void SelectText::selectAll()
{
    if (!m_picture)
        return;
    m_selStart = findFirst(*m_picture, &m_startBase);
    m_selEnd = findLast(*m_picture, &m_endBase);
    m_extendSelection = true;
}

int SelectText::selectionX() const
{
    return (m_hitTopLeft ? m_selStart.fLeft : m_selEnd.fRight) + m_startOffset.fX;
}

int SelectText::selectionY() const
{
    const SkIRect& rect = m_hitTopLeft ? m_selStart : m_selEnd;
    return ((rect.fTop + rect.fBottom) >> 1) + m_startOffset.fY;
}

void SelectText::setVisibleRect(const IntRect& vis)
{
    DBG_NAV_LOGD("vis=(%d,%d,w=%d,h=%d) offset=(%d,%d)",
        vis.x(), vis.y(), vis.width(), vis.height(), m_startOffset.fX,
        m_startOffset.fY);
    m_visibleRect = vis;
    m_visibleRect.offset(-m_startOffset.fX, -m_startOffset.fY);
}

bool SelectText::startSelection(const CachedRoot* root, const IntRect& vis,
    int x, int y)
{
    m_wordSelection = false;
    m_startOffset.set(x, y);
    DBG_NAV_LOGD("x/y=(%d,%d)", x, y);
    SkSafeUnref(m_picture);
    m_picture = root->pictureAt(&x, &y, &m_layerId);
    DBG_NAV_LOGD("m_picture=%p m_layerId=%d x/y=(%d,%d)", m_picture, m_layerId,
        x, y);
    if (!m_picture) {
        DBG_NAV_LOG("picture==0");
        return false;
    }
    m_picture->ref();
    m_startOffset.fX -= x;
    m_startOffset.fY -= y;
    m_original.fX = x;
    m_original.fY = y;
    setVisibleRect(vis);
    if (m_selStart.isEmpty()) {
        DBG_NAV_LOGD("empty start picture=(%d,%d) x=%d y=%d", 
             m_picture->width(), m_picture->height(), x, y);
        m_startSelection = true;
        return true;
    }
    m_hitTopLeft = hitStartHandle(x, y);
    bool hitBottomRight = hitEndHandle(x, y);
    DBG_NAV_LOGD("picture=(%d,%d) left=%d top=%d right=%d bottom=%d x=%d y=%d",
        m_picture->width(), m_picture->height(),left, top, right, bottom, x, y);
    if (m_hitTopLeft) {
        DBG_NAV_LOG("hit top left");
        m_original.fX -= m_selStart.fLeft;
        m_original.fY -= (m_selStart.fTop + m_selStart.fBottom) >> 1;
    } else if (hitBottomRight) {
        DBG_NAV_LOG("hit bottom right");
        m_original.fX -= m_selEnd.fRight;
        m_original.fY -= (m_selEnd.fTop + m_selEnd.fBottom) >> 1;
    }
    return m_hitTopLeft || hitBottomRight;
}

void SelectText::updateHandleScale(float handleScale)
{
    m_controlHeight = CONTROL_HEIGHT * handleScale;
    m_controlWidth = CONTROL_WIDTH * handleScale;
    m_controlSlop = CONTROL_SLOP * handleScale;
}

/* selects the word at (x, y)
* a word is normally delimited by spaces
* a string of digits (even with inside spaces) is a word (for phone numbers)
* FIXME: digit find isn't implemented yet
* returns true if a word was selected
*/
bool SelectText::wordSelection(const CachedRoot* root, const IntRect& vis,
    int x, int y)
{
    IntRect tapArea = IntRect(x - TOUCH_SLOP, y - TOUCH_SLOP, TOUCH_SLOP * 2,
        TOUCH_SLOP * 2);
    if (!startSelection(root, tapArea, x, y))
        return false;
    extendSelection(tapArea, x, y);
    if (m_selStart.isEmpty())
        return false;
    setDrawPointer(false);
    setVisibleRect(vis);
    SkIRect ivisBounds = m_visibleRect;
    ivisBounds.join(m_selStart);
    ivisBounds.join(m_selEnd);
    DBG_NAV_LOGD("m_selStart=(%d,%d,r=%d,b=%d) m_selEnd=(%d,%d,r=%d,b=%d)"
        " ivisBounds=(%d,%d,r=%d,b=%d)",
        m_selStart.fLeft, m_selStart.fTop, m_selStart.fRight, m_selStart.fBottom,
        m_selEnd.fLeft, m_selEnd.fTop, m_selEnd.fRight, m_selEnd.fBottom,
        ivisBounds.fLeft, ivisBounds.fTop, ivisBounds.fRight, ivisBounds.fBottom);
    m_selRegion.setEmpty();
    buildSelection(*m_picture, ivisBounds, m_selStart, m_startBase,
        m_selEnd, m_endBase, &m_selRegion);
    x = m_selStart.fLeft;
    y = (m_selStart.fTop + m_selStart.fBottom) >> 1;
    SkIRect clipRect = m_visibleRect;
    clipRect.fLeft -= m_visibleRect.width() >> 1;
    clipRect.fLeft = std::max(clipRect.fLeft, 0);
    int base;
    SkIRect left = findLeft(*m_picture, clipRect, x, y, &base);
    if (!left.isEmpty()) {
        m_startBase = base;
        m_selStart = left;
    }
    x = m_selEnd.fRight;
    y = (m_selEnd.fTop + m_selEnd.fBottom) >> 1;
    clipRect = m_visibleRect;
    clipRect.fRight += m_visibleRect.width() >> 1;
    SkIRect right = findRight(*m_picture, clipRect, x, y, &base);
    if (!right.isEmpty()) {
        m_endBase = base;
        m_selEnd = right;
    }
    DBG_NAV_LOGD("m_selStart=(%d, %d, %d, %d) m_selEnd=(%d, %d, %d, %d)",
        m_selStart.fLeft, m_selStart.fTop, m_selStart.fRight, m_selStart.fBottom,
        m_selEnd.fLeft, m_selEnd.fTop, m_selEnd.fRight, m_selEnd.fBottom);
    if (!left.isEmpty() || !right.isEmpty()) {
        m_wordBounds = m_selStart;
        m_wordBounds.join(m_selEnd);
        m_extendSelection = m_wordSelection = true;
        m_outsideWord = false;
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
