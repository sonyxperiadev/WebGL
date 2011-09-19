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

#include "config.h"
#include "FindCanvas.h"
#include "LayerAndroid.h"
#include "IntRect.h"
#include "SelectText.h"
#include "SkBlurMaskFilter.h"
#include "SkCornerPathEffect.h"
#include "SkRect.h"
#include "SkUtils.h"

#include <utils/Log.h>

#define INTEGER_OUTSET 2

namespace android {

// MatchInfo methods
////////////////////////////////////////////////////////////////////////////////

MatchInfo::MatchInfo() {
    m_picture = 0;
}

MatchInfo::~MatchInfo() {
    SkSafeUnref(m_picture);
}

MatchInfo::MatchInfo(const MatchInfo& src) {
    m_layerId = src.m_layerId;
    m_location = src.m_location;
    m_picture = src.m_picture;
    SkSafeRef(m_picture);
}

void MatchInfo::set(const SkRegion& region, SkPicture* pic, int layerId) {
    SkSafeUnref(m_picture);
    m_layerId = layerId;
    m_location = region;
    m_picture = pic;
    SkASSERT(pic);
    pic->ref();
}

// GlyphSet methods
////////////////////////////////////////////////////////////////////////////////

GlyphSet::GlyphSet(const SkPaint& paint, const UChar* lower, const UChar* upper,
            size_t byteLength) {
    SkPaint clonePaint(paint);
    clonePaint.setTextEncoding(SkPaint::kUTF16_TextEncoding);
    mTypeface = paint.getTypeface();
    mCount = clonePaint.textToGlyphs(lower, byteLength, NULL);
    if (mCount > MAX_STORAGE_COUNT) {
        mLowerGlyphs = new uint16_t[2*mCount];
    } else {
        mLowerGlyphs = &mStorage[0];
    }
    // Use one array, and have mUpperGlyphs point to a portion of it,
    // so that we can reduce the number of new/deletes
    mUpperGlyphs = mLowerGlyphs + mCount;
    int count2 = clonePaint.textToGlyphs(lower, byteLength, mLowerGlyphs);
    SkASSERT(mCount == count2);
    count2 = clonePaint.textToGlyphs(upper, byteLength, mUpperGlyphs);
    SkASSERT(mCount == count2);
}

GlyphSet::~GlyphSet() {
    // Do not need to delete mTypeface, which is not owned by us.
    if (mCount > MAX_STORAGE_COUNT) {
        delete[] mLowerGlyphs;
    }   // Otherwise, we just used local storage space, so no need to delete
    // Also do not need to delete mUpperGlyphs, which simply points to a
    // part of mLowerGlyphs
}

GlyphSet& GlyphSet::operator=(GlyphSet& src) {
    mTypeface = src.mTypeface;
    mCount = src.mCount;
    if (mCount > MAX_STORAGE_COUNT) {
        mLowerGlyphs = new uint16_t[2*mCount];
    } else {
        mLowerGlyphs = &mStorage[0];
    }
    memcpy(mLowerGlyphs, src.mLowerGlyphs, 2*mCount*sizeof(uint16_t));
    mUpperGlyphs = mLowerGlyphs + mCount;
    return *this;
}

bool GlyphSet::characterMatches(uint16_t c, int index) {
    SkASSERT(index < mCount && index >= 0);
    return c == mLowerGlyphs[index] || c == mUpperGlyphs[index];
}

// FindCanvas methods
////////////////////////////////////////////////////////////////////////////////

FindCanvas::FindCanvas(int width, int height, const UChar* lower,
        const UChar* upper, size_t byteLength)
        : mLowerText(lower)
        , mUpperText(upper)
        , mLength(byteLength)
        , mNumFound(0) {
    // the text has been provided in read order. Reverse as needed so the
    // result contains left-to-right characters.
    const uint16_t* start = mLowerText;
    size_t count = byteLength >> 1;
    const uint16_t* end = mLowerText + count;
    while (start < end) {
        SkUnichar ch = SkUTF16_NextUnichar(&start);
        WTF::Unicode::Direction charDirection = WTF::Unicode::direction(ch);
        if (WTF::Unicode::RightToLeftArabic == charDirection
                || WTF::Unicode::RightToLeft == charDirection) {
            mLowerReversed.clear();
            mLowerReversed.append(mLowerText, count);
            WebCore::ReverseBidi(mLowerReversed.begin(), count);
            mLowerText = mLowerReversed.begin();
            mUpperReversed.clear();
            mUpperReversed.append(mUpperText, count);
            WebCore::ReverseBidi(mUpperReversed.begin(), count);
            mUpperText = mUpperReversed.begin();
            break;
        }
    }

    setBounder(&mBounder);
    mOutset = -SkIntToScalar(INTEGER_OUTSET);
    mMatches = new WTF::Vector<MatchInfo>();
    mWorkingIndex = 0;
    mWorkingCanvas = 0;
    mWorkingPicture = 0;
}

FindCanvas::~FindCanvas() {
    setBounder(NULL);
    /* Just in case getAndClear was not called. */
    delete mMatches;
    SkSafeUnref(mWorkingPicture);
}

// Each version of addMatch returns a rectangle for a match.
// Not all of the parameters are used by each version.
SkRect FindCanvas::addMatchNormal(int index,
        const SkPaint& paint, int count, const uint16_t* glyphs,
        const SkScalar pos[], SkScalar y) {
    const uint16_t* lineStart = glyphs - index;
    /* Use the original paint, since "text" is in glyphs */
    SkScalar before = paint.measureText(lineStart, index * sizeof(uint16_t), 0);
    SkRect rect;
    rect.fLeft = pos[0] + before;
    int countInBytes = count * sizeof(uint16_t);
    rect.fRight = paint.measureText(glyphs, countInBytes, 0) + rect.fLeft;
    SkPaint::FontMetrics fontMetrics;
    paint.getFontMetrics(&fontMetrics);
    SkScalar baseline = y;
    rect.fTop = baseline + fontMetrics.fAscent;
    rect.fBottom = baseline + fontMetrics.fDescent;
    const SkMatrix& matrix = getTotalMatrix();
    matrix.mapRect(&rect);
    // Add the text to our picture.
    SkCanvas* canvas = getWorkingCanvas();
    int saveCount = canvas->save();
    canvas->concat(matrix);
    canvas->drawText(glyphs, countInBytes, pos[0] + before, y, paint);
    canvas->restoreToCount(saveCount);
    return rect;
}

SkRect FindCanvas::addMatchPos(int index,
        const SkPaint& paint, int count, const uint16_t* glyphs,
        const SkScalar xPos[], SkScalar /* y */) {
    SkRect r;
    r.setEmpty();
    const SkPoint* temp = reinterpret_cast<const SkPoint*> (xPos);
    const SkPoint* points = &temp[index];
    int countInBytes = count * sizeof(uint16_t);
    SkPaint::FontMetrics fontMetrics;
    paint.getFontMetrics(&fontMetrics);
    // Need to check each character individually, since the heights may be
    // different.
    for (int j = 0; j < count; j++) {
        SkRect bounds;
        bounds.fLeft = points[j].fX;
        bounds.fRight = bounds.fLeft +
                paint.measureText(&glyphs[j], sizeof(uint16_t), 0);
        SkScalar baseline = points[j].fY;
        bounds.fTop = baseline + fontMetrics.fAscent;
        bounds.fBottom = baseline + fontMetrics.fDescent;
        /* Accumulate and then add the resulting rect to mMatches */
        r.join(bounds);
    }
    SkMatrix matrix = getTotalMatrix();
    matrix.mapRect(&r);
    SkCanvas* canvas = getWorkingCanvas();
    int saveCount = canvas->save();
    canvas->concat(matrix);
    canvas->drawPosText(glyphs, countInBytes, points, paint);
    canvas->restoreToCount(saveCount);
    return r;
}

SkRect FindCanvas::addMatchPosH(int index,
        const SkPaint& paint, int count, const uint16_t* glyphs,
        const SkScalar position[], SkScalar constY) {
    SkRect r;
    // We only care about the positions starting at the index of our match
    const SkScalar* xPos = &position[index];
    // This assumes that the position array is monotonic increasing
    // The left bounds will be the position of the left most character
    r.fLeft = xPos[0];
    // The right bounds will be the position of the last character plus its
    // width
    int lastIndex = count - 1;
    r.fRight = paint.measureText(&glyphs[lastIndex], sizeof(uint16_t), 0)
            + xPos[lastIndex];
    // Grab font metrics to determine the top and bottom of the bounds
    SkPaint::FontMetrics fontMetrics;
    paint.getFontMetrics(&fontMetrics);
    r.fTop = constY + fontMetrics.fAscent;
    r.fBottom = constY + fontMetrics.fDescent;
    const SkMatrix& matrix = getTotalMatrix();
    matrix.mapRect(&r);
    SkCanvas* canvas = getWorkingCanvas();
    int saveCount = canvas->save();
    canvas->concat(matrix);
    canvas->drawPosTextH(glyphs, count * sizeof(uint16_t), xPos, constY, paint);
    canvas->restoreToCount(saveCount);
    return r;
}

void FindCanvas::drawLayers(LayerAndroid* layer) {
#if USE(ACCELERATED_COMPOSITING)
    SkPicture* picture = layer->picture();
    if (picture) {
        setLayerId(layer->uniqueId());
        drawPicture(*picture);
    }
    for (int i = 0; i < layer->countChildren(); i++)
        drawLayers(layer->getChild(i));
#endif
}

void FindCanvas::drawText(const void* text, size_t byteLength, SkScalar x,
                          SkScalar y, const SkPaint& paint) {
    findHelper(text, byteLength, paint, &x, y, &FindCanvas::addMatchNormal);
}

void FindCanvas::drawPosText(const void* text, size_t byteLength,
                             const SkPoint pos[], const SkPaint& paint) {
    // Pass in the first y coordinate for y so that we can check to see whether
    // it is lower than the last draw call (to check if we are continuing to
    // another line).
    findHelper(text, byteLength, paint, (const SkScalar*) pos, pos[0].fY,
            &FindCanvas::addMatchPos);
}

void FindCanvas::drawPosTextH(const void* text, size_t byteLength,
                              const SkScalar xpos[], SkScalar constY,
                              const SkPaint& paint) {
    findHelper(text, byteLength, paint, xpos, constY,
            &FindCanvas::addMatchPosH);
}

/* The current behavior is to skip substring matches. This means that in the
 * string
 *      batbatbat
 * a search for
 *      batbat
 * will return 1 match.  If the desired behavior is to return 2 matches, define
 * INCLUDE_SUBSTRING_MATCHES to be 1.
 */
#define INCLUDE_SUBSTRING_MATCHES 0

// Need a quick way to know a maximum distance between drawText calls to know if
// they are part of the same logical phrase when searching.  By crude
// inspection, half the point size seems a good guess at the width of a space
// character.
static inline SkScalar approximateSpaceWidth(const SkPaint& paint) {
    return SkScalarHalf(paint.getTextSize());
}

void FindCanvas::findHelper(const void* text, size_t byteLength,
                          const SkPaint& paint, const SkScalar positions[], 
                          SkScalar y,
                          SkRect (FindCanvas::*addMatch)(int index,
                          const SkPaint& paint, int count,
                          const uint16_t* glyphs,
                          const SkScalar positions[], SkScalar y)) {
    SkASSERT(paint.getTextEncoding() == SkPaint::kGlyphID_TextEncoding);
    SkASSERT(mMatches);
    GlyphSet* glyphSet = getGlyphs(paint);
    const int count = glyphSet->getCount();
    int numCharacters = byteLength >> 1;
    const uint16_t* chars = (const uint16_t*) text;
    // This block will check to see if we are continuing from another line.  If
    // so, the user needs to have added a space, which we do not draw.
    if (mWorkingIndex) {
        SkPoint newY;
        getTotalMatrix().mapXY(0, y, &newY);
        SkIRect workingBounds = mWorkingRegion.getBounds();
        int newYInt = SkScalarRound(newY.fY);
        if (workingBounds.fTop > newYInt) {
            // The new text is above the working region, so we know it's not
            // a continuation.
            resetWorkingCanvas();
            mWorkingIndex = 0;
            mWorkingRegion.setEmpty();
        } else if (workingBounds.fBottom < newYInt) {
            // Now we know that this line is lower than our partial match.
            SkPaint clonePaint(paint);
            clonePaint.setTextEncoding(SkPaint::kUTF8_TextEncoding);
            uint16_t space;
            clonePaint.textToGlyphs(" ", 1, &space);
            if (glyphSet->characterMatches(space, mWorkingIndex)) {
                mWorkingIndex++;
                if (mWorkingIndex == count) {
                    // We already know that it is not clipped out because we
                    // checked for that before saving the working region.
                    insertMatchInfo(mWorkingRegion);

                    resetWorkingCanvas();
                    mWorkingIndex = 0;
                    mWorkingRegion.setEmpty();
                    // We have found a match, so continue on this line from
                    // scratch.
                }
            } else {
                resetWorkingCanvas();
                mWorkingIndex = 0;
                mWorkingRegion.setEmpty();
            }
        }
        // If neither one is true, then we are likely continuing on the same
        // line, but are in a new draw call because the paint has changed.  In
        // this case, we can continue without adding a space.
    }
    // j is the position in the search text
    // Start off with mWorkingIndex in case we are continuing from a prior call
    int j = mWorkingIndex;
    // index is the position in the drawn text
    int index = 0;
    for ( ; index != numCharacters; index++) {
        if (glyphSet->characterMatches(chars[index], j)) {
            // The jth character in the search text matches the indexth position
            // in the drawn text, so increase j.
            j++;
            if (j != count) {
                continue;
            }
            // The last count characters match, so we found the entire
            // search string.
            int remaining = count - mWorkingIndex;
            int matchIndex = index - remaining + 1;
            // Set up a pointer to the matching text in 'chars'.
            const uint16_t* glyphs = chars + matchIndex;
            SkRect rect = (this->*addMatch)(matchIndex, paint,
                    remaining, glyphs, positions, y);
            // We need an SkIRect for SkRegion operations.
            SkIRect iRect;
            rect.roundOut(&iRect);
            // Want to outset the drawn rectangle by the same amount as
            // mOutset
            iRect.inset(-INTEGER_OUTSET, -INTEGER_OUTSET);
            SkRegion regionToAdd(iRect);
            if (!mWorkingRegion.isEmpty()) {
                // If this is on the same line as our working region, make
                // sure that they are close enough together that they are
                // supposed to be part of the same text string.
                // The width of two spaces has arbitrarily been chosen.
                const SkIRect& workingBounds = mWorkingRegion.getBounds();
                if (workingBounds.fTop <= iRect.fBottom &&
                        workingBounds.fBottom >= iRect.fTop &&
                        SkIntToScalar(iRect.fLeft - workingBounds.fRight) >
                        approximateSpaceWidth(paint)) {
                    index = -1;     // Will increase to 0 on next run
                    // In this case, we need to start from the beginning of
                    // the text being searched and our search term.
                    j = 0;
                    mWorkingIndex = 0;
                    mWorkingRegion.setEmpty();
                    continue;
                }
                // Add the mWorkingRegion, which contains rectangles from
                // the previous line(s).
                regionToAdd.op(mWorkingRegion, SkRegion::kUnion_Op);
            }
            insertMatchInfo(regionToAdd);
#if INCLUDE_SUBSTRING_MATCHES
            // Reset index to the location of the match and reset j to the
            // beginning, so that on the next iteration of the loop, index
            // will advance by 1 and we will compare the next character in
            // chars to the first character in the GlyphSet.
            index = matchIndex;
#endif
            // Whether the clip contained it or not, we need to start over
            // with our recording canvas
            resetWorkingCanvas();
        } else {
            // Index needs to be set to index - j + 1.
            // This is a ridiculous case, but imagine the situation where the
            // user is looking for the string "jjog" in the drawText call for
            // "jjjog".  The first two letters match.  However, when the index
            // is 2, and we discover that 'o' and 'j' do not match, we should go
            // back to 1, where we do, in fact, have a match
            // FIXME: This does not work if (as in our example) "jj" is in one
            // draw call and "jog" is in the next.  Doing so would require a
            // stack, keeping track of multiple possible working indeces and
            // regions.  This is likely an uncommon case.
            index = index - j;  // index will be increased by one on the next
                                // iteration
        }
        // We reach here in one of two cases:
        // 1) We just completed a match, so any working rectangle/index is no
        // longer needed, and we will start over from the beginning
        // 2) The glyphs do not match, so we start over at the beginning of
        // the search string.
        j = 0;
        mWorkingIndex = 0;
        mWorkingRegion.setEmpty();
    }
    // At this point, we have searched all of the text in the current drawText
    // call.
    // Keep track of a partial match that may start on this line.
    if (j > 0) {    // if j is greater than 0, we have a partial match
        int relativeCount = j - mWorkingIndex;  // Number of characters in this
                                                // part of the match.
        int partialIndex = index - relativeCount; // Index that starts our
                                                  // partial match.
        const uint16_t* partialGlyphs = chars + partialIndex;
        SkRect partial = (this->*addMatch)(partialIndex, paint, relativeCount,
                partialGlyphs, positions, y);
        partial.inset(mOutset, mOutset);
        SkIRect dest;
        partial.roundOut(&dest);
        mWorkingRegion.op(dest, SkRegion::kUnion_Op);
        mWorkingIndex = j;
        return;
    }
    // This string doesn't go into the next drawText, so reset our working
    // variables
    mWorkingRegion.setEmpty();
    mWorkingIndex = 0;
}

SkCanvas* FindCanvas::getWorkingCanvas() {
    if (!mWorkingPicture) {
        mWorkingPicture = new SkPicture;
        mWorkingCanvas = mWorkingPicture->beginRecording(0,0);
    }
    return mWorkingCanvas;
}

GlyphSet* FindCanvas::getGlyphs(const SkPaint& paint) {
    SkTypeface* typeface = paint.getTypeface();
    GlyphSet* end = mGlyphSets.end();
    for (GlyphSet* ptr = mGlyphSets.begin();ptr != end; ptr++) {
        if (ptr->getTypeface() == typeface) {
            return ptr;
        }
    }

    GlyphSet set(paint, mLowerText, mUpperText, mLength);
    *mGlyphSets.append() = set;
    return &(mGlyphSets.top());
}

void FindCanvas::insertMatchInfo(const SkRegion& region) {
    mNumFound++;
    mWorkingPicture->endRecording();
    MatchInfo matchInfo;
    mMatches->append(matchInfo);
    LOGD("%s region=%p pict=%p layer=%d", __FUNCTION__,
        &region, mWorkingPicture, mLayerId);
    mMatches->last().set(region, mWorkingPicture, mLayerId);
}

void FindCanvas::resetWorkingCanvas() {
    mWorkingPicture->unref();
    mWorkingPicture = 0;
    // Do not need to reset mWorkingCanvas itself because we only access it via
    // getWorkingCanvas.
}

// This function sets up the paints that are used to draw the matches.
void FindOnPage::setUpFindPaint() {
    // Set up the foreground paint
    m_findPaint.setAntiAlias(true);
    const SkScalar roundiness = SkIntToScalar(2);
    SkCornerPathEffect* cornerEffect = new SkCornerPathEffect(roundiness);
    m_findPaint.setPathEffect(cornerEffect);
    m_findPaint.setARGB(255, 132, 190, 0);

    // Set up the background blur paint.
    m_findBlurPaint.setAntiAlias(true);
    m_findBlurPaint.setARGB(204, 0, 0, 0);
    m_findBlurPaint.setPathEffect(cornerEffect);
    cornerEffect->unref();
    SkMaskFilter* blurFilter = SkBlurMaskFilter::Create(SK_Scalar1,
            SkBlurMaskFilter::kNormal_BlurStyle);
    m_findBlurPaint.setMaskFilter(blurFilter)->unref();
    m_isFindPaintSetUp = true;
}

IntRect FindOnPage::currentMatchBounds() const {
    IntRect noBounds = IntRect(0, 0, 0, 0);
    if (!m_matches || !m_matches->size())
        return noBounds;
    MatchInfo& info = (*m_matches)[m_findIndex];
    return info.getLocation().getBounds();
}

bool FindOnPage::currentMatchIsInLayer() const {
    if (!m_matches || !m_matches->size())
        return false;
    MatchInfo& info = (*m_matches)[m_findIndex];
    return info.isInLayer();
}

int FindOnPage::currentMatchLayerId() const {
    return (*m_matches)[m_findIndex].layerId();
}

// This function is only used by findNext and setMatches.  In it, we store
// upper left corner of the match specified by m_findIndex in
// m_currentMatchLocation.
void FindOnPage::storeCurrentMatchLocation() {
    SkASSERT(m_findIndex < m_matches->size());
    const SkIRect& bounds = (*m_matches)[m_findIndex].getLocation().getBounds();
    m_currentMatchLocation.set(bounds.fLeft, bounds.fTop);
    m_hasCurrentLocation = true;
}

// Put a cap on the number of matches to draw.  If the current page has more
// matches than this, only draw the focused match.
#define MAX_NUMBER_OF_MATCHES_TO_DRAW 101

void FindOnPage::draw(SkCanvas* canvas, LayerAndroid* layer, IntRect* inval) {
    if (!m_lastBounds.isEmpty()) {
        inval->unite(m_lastBounds);
        m_lastBounds.setEmpty();
    }
    if (!m_hasCurrentLocation || !m_matches || !m_matches->size())
        return;
    int layerId = layer->uniqueId();
    if (m_findIndex >= m_matches->size())
        m_findIndex = 0;
    const MatchInfo& matchInfo = (*m_matches)[m_findIndex];
    const SkRegion& currentMatchRegion = matchInfo.getLocation();

    // Set up the paints used for drawing the matches
    if (!m_isFindPaintSetUp)
        setUpFindPaint();

    // Draw the current match
    if (matchInfo.layerId() == layerId) {
        drawMatch(currentMatchRegion, canvas, true);
        // Now draw the picture, so that it shows up on top of the rectangle
        int saveCount = canvas->save();
        SkPath matchPath;
        currentMatchRegion.getBoundaryPath(&matchPath);
        canvas->clipPath(matchPath);
        canvas->drawPicture(*matchInfo.getPicture());
        canvas->restoreToCount(saveCount);
        const SkMatrix& matrix = canvas->getTotalMatrix();
        const SkRect& localBounds = matchPath.getBounds();
        SkRect globalBounds;
        matrix.mapRect(&globalBounds, localBounds);
        globalBounds.round(&m_lastBounds);
        inval->unite(m_lastBounds);
    }
    // Draw the rest
    unsigned numberOfMatches = m_matches->size();
    if (numberOfMatches > 1
            && numberOfMatches < MAX_NUMBER_OF_MATCHES_TO_DRAW) {
        for (unsigned i = 0; i < numberOfMatches; i++) {
            // The current match has already been drawn
            if (i == m_findIndex)
                continue;
            if ((*m_matches)[i].layerId() != layerId)
                continue;
            const SkRegion& region = (*m_matches)[i].getLocation();
            // Do not draw matches which intersect the current one, or if it is
            // offscreen
            if (currentMatchRegion.intersects(region))
                continue;
            SkRect bounds;
            bounds.set(region.getBounds());
            if (canvas->quickReject(bounds, SkCanvas::kAA_EdgeType))
                continue;
            drawMatch(region, canvas, false);
        }
    }
}

// Draw the match specified by region to the canvas.
void FindOnPage::drawMatch(const SkRegion& region, SkCanvas* canvas,
        bool focused)
{
    // For the match which has focus, use a filled paint.  For the others, use
    // a stroked paint.
    if (focused) {
        m_findPaint.setStyle(SkPaint::kFill_Style);
        m_findBlurPaint.setStyle(SkPaint::kFill_Style);
    } else {
        m_findPaint.setStyle(SkPaint::kStroke_Style);
        m_findPaint.setStrokeWidth(SK_Scalar1);
        m_findBlurPaint.setStyle(SkPaint::kStroke_Style);
        m_findBlurPaint.setStrokeWidth(SkIntToScalar(2));
    }
    // Find the path for the current match
    SkPath matchPath;
    region.getBoundaryPath(&matchPath);
    // Offset the path for a blurred shadow
    SkPath blurPath;
    matchPath.offset(SK_Scalar1, SkIntToScalar(2), &blurPath);
    int saveCount = 0;
    if (!focused) {
        saveCount = canvas->save();
        canvas->clipPath(matchPath, SkRegion::kDifference_Op);
    }
    // Draw the blurred background
    canvas->drawPath(blurPath, m_findBlurPaint);
    if (!focused)
        canvas->restoreToCount(saveCount);
    // Draw the foreground
    canvas->drawPath(matchPath, m_findPaint);
}

void FindOnPage::findNext(bool forward)
{
    if (!m_matches || !m_matches->size() || !m_hasCurrentLocation)
        return;
    if (forward) {
        m_findIndex++;
        if (m_findIndex == m_matches->size())
            m_findIndex = 0;
    } else {
        if (m_findIndex == 0) {
            m_findIndex = m_matches->size() - 1;
        } else {
            m_findIndex--;
        }
    }
    storeCurrentMatchLocation();
}

// With this call, WebView takes ownership of matches, and is responsible for
// deleting it.
void FindOnPage::setMatches(WTF::Vector<MatchInfo>* matches)
{
    if (m_matches)
        delete m_matches;
    m_matches = matches;
    if (m_matches->size()) {
        if (m_hasCurrentLocation) {
            for (unsigned i = 0; i < m_matches->size(); i++) {
                const SkIRect& rect = (*m_matches)[i].getLocation().getBounds();
                if (rect.fLeft == m_currentMatchLocation.fX
                        && rect.fTop == m_currentMatchLocation.fY) {
                    m_findIndex = i;
                    return;
                }
            }
        }
        // If we did not have a stored location, or if we were unable to restore
        // it, store the new one.
        m_findIndex = 0;
        storeCurrentMatchLocation();
    } else {
        m_hasCurrentLocation = false;
    }
}

}
