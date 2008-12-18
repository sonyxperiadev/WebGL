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

#include "config.h"
#include "FindCanvas.h"

#include "SkRect.h"

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

GlyphSet::GlyphSet& GlyphSet::operator=(GlyphSet& src) {
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

    setBounder(&mBounder);
    mOutset = -SkIntToScalar(2);
    mRegions = new WTF::Vector<SkRegion>();
#if RECORD_MATCHES
    mPicture = new SkPicture();
    mRecordingCanvas = mPicture->beginRecording(width, height);
#endif
    mWorkingIndex = 0;
}

FindCanvas::~FindCanvas() {
    setBounder(NULL);
    /* Just in case getAndClear was not called. */
    delete mRegions;
#if RECORD_MATCHES
    mPicture->unref();
#endif
}

// SkPaint.measureText is used to get a rectangle surrounding the specified
// text.  It is a tighter bounds than we want.  We want the height to account
// for the ascent and descent of the font, so that the rectangles will line up,
// regardless of the characters contained in them.
static void correctSize(const SkPaint& paint, SkRect& rect)
{
    SkPaint::FontMetrics fontMetrics;
    paint.getFontMetrics(&fontMetrics);
    rect.fTop = fontMetrics.fAscent;
    rect.fBottom = fontMetrics.fDescent;    
}

// Each version of addMatch returns a rectangle for a match.
// Not all of the parameters are used by each version.
SkRect FindCanvas::addMatchNormal(int index,
        const SkPaint& paint, int count, const uint16_t* glyphs,
        const SkScalar pos[], SkScalar y) {
    const uint16_t* lineStart = glyphs - index;
    /* Use the original paint, since "text" is in glyphs */
    SkScalar before = paint.measureText(lineStart, index * sizeof(uint16_t),
            NULL);
    SkRect rect;
    int countInBytes = count << 1;
    paint.measureText(glyphs, countInBytes, &rect);
    correctSize(paint, rect);
    rect.offset(pos[0] + before, y);
    getTotalMatrix().mapRect(&rect);
    // Add the text to our picture.
#if RECORD_MATCHES
    mRecordingCanvas->setMatrix(getTotalMatrix());
    SkPoint point;
    matrix.mapXY(pos[0] + before, y, &point);
    mRecordingCanvas->drawText(glyphs, countInBytes, point.fX, point.fY, paint);
#endif
    return rect;
}

SkRect FindCanvas::addMatchPos(int index,
        const SkPaint& paint, int count, const uint16_t* glyphs,
        const SkScalar xPos[], SkScalar /* y */) {
    SkRect r;
    r.setEmpty();
    const SkScalar* pos = &xPos[index << 1];
    int countInBytes = count << 1;
    SkPaint::FontMetrics fontMetrics;
    paint.getFontMetrics(&fontMetrics);
    for (int j = 0; j < countInBytes; j += 2) {
        SkRect bounds;
        paint.getTextWidths(&(glyphs[j >> 1]), 2, NULL, &bounds);
        bounds.fTop = fontMetrics.fAscent;
        bounds.fBottom = fontMetrics.fDescent;
        bounds.offset(pos[j], pos[j+1]);
        /* Accumulate and then add the resulting rect to mRegions */
        r.join(bounds);
    }
    getTotalMatrix().mapRect(&r);
    // Add the text to our picture.
#if RECORD_MATCHES
    mRecordingCanvas->setMatrix(getTotalMatrix());
    // FIXME: Need to do more work to get xPos and constY in the proper
    // coordinates.
    //mRecordingCanvas->drawPosText(glyphs, countInBytes, positions, paint);
#endif
    return r;
}

SkRect FindCanvas::addMatchPosH(int index,
        const SkPaint& paint, int count, const uint16_t* glyphs,
        const SkScalar position[], SkScalar constY) {
    SkRect r;
    r.setEmpty();
    const SkScalar* xPos = &position[index];
    for (int j = 0; j < count; j++) {
        SkRect bounds;
        paint.getTextWidths(&glyphs[j], 1, NULL, &bounds);
        bounds.offset(xPos[j], constY);
        /* Accumulate and then add the resulting rect to mRegions */
        r.join(bounds);
    }
    correctSize(paint, r);
    getTotalMatrix().mapRect(&r);
    // Add the text to our picture.
#if RECORD_MATCHES
    mRecordingCanvas->setMatrix(getTotalMatrix());
    // FIXME: Need to do more work to get xPos and constY in the proper
    // coordinates.
    //mRecordingCanvas->drawPosTextH(glyphs, count << 1, xPos, constY, paint);
#endif
    return r;
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
    SkASSERT(mRegions);
#if RECORD_MATCHES
    SkASSERT(mRecordingCanvas);
#endif
    GlyphSet* glyphSet = getGlyphs(paint);
    const int count = glyphSet->getCount();
    int numCharacters = byteLength >> 1;
    const uint16_t* chars = (const uint16_t*) text;
    // This block will check to see if we are continuing from another line.  If
    // so, the user needs to have added a space, which we do not draw.
    if (mWorkingIndex) {
        SkPoint newY;
        getTotalMatrix().mapXY(0, y, &newY);
        if (mWorkingRegion.getBounds().fBottom < SkScalarRound(newY.fY)) {
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
                    mNumFound++;
                    mRegions->append(mWorkingRegion);
                    mWorkingIndex = 0;
                    mWorkingRegion.setEmpty();
                    // We have found a match, so continue on this line from
                    // scratch.
                }
            } else {
                mWorkingIndex = 0;
                mWorkingRegion.setEmpty();
            }
        }
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
            mNumFound++;
            int remaining = count - mWorkingIndex;
            int matchIndex = index - remaining + 1;
            // Set up a pointer to the matching text in 'chars'.
            const uint16_t* glyphs = chars + matchIndex;
            SkRect rect = (this->*addMatch)(matchIndex, paint,
                    remaining, glyphs, positions, y);
            rect.inset(mOutset, mOutset);
            // We need an SkIRect for SkRegion operations.
            SkIRect iRect;
            rect.roundOut(&iRect);
            // If the rectangle is partially clipped, assume that the text is
            // not visible, so skip this match.
            if (getTotalClip().contains(iRect)) {
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
                mRegions->append(regionToAdd);
#if INCLUDE_SUBSTRING_MATCHES
                // Reset index to the location of the match and reset j to the
                // beginning, so that on the next iteration of the loop, index
                // will advance by 1 and we will compare the next character in
                // chars to the first character in the GlyphSet.
                index = matchIndex;
#endif
            } else {
                // This match was clipped out, so begin looking at the next
                // character from our hidden match
                index = matchIndex;
            }
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
        int partialIndex = index - j + mWorkingIndex;
        const uint16_t* partialGlyphs = chars + partialIndex;
        SkRect partial = (this->*addMatch)(partialIndex, paint, j,
                partialGlyphs, positions, y);
        partial.inset(mOutset, mOutset);
        getTotalMatrix().mapRect(&partial);
        SkIRect dest;
        partial.roundOut(&dest);
        // Only save a partial if it is in the current clip.
        if (getTotalClip().contains(dest)) {
            mWorkingRegion.op(dest, SkRegion::kUnion_Op);
            mWorkingIndex = j;
            // From one perspective, it seems like we would want to draw here,
            // since we have all the information (paint, matrix, etc)
            // However, we only want to draw if we find the rest
            return;
        }
    }
    // This string doesn't go into the next drawText, so reset our working
    // variables
    mWorkingRegion.setEmpty();
    mWorkingIndex = 0;
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
