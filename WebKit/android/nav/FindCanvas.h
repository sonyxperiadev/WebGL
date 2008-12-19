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

#ifndef Find_Canvas_h
#define Find_Canvas_h

// The code marked with this is used to record the draw calls into an SkPicture,
// which is passed to the caller to draw the matches on top of the opaque green
// rectangles.  The code is a checkpoint.
#define RECORD_MATCHES 0

#include "SkBounder.h"
#include "SkCanvas.h"
#include "SkRegion.h"
#include "SkTDArray.h"
#include "icu/unicode/umachine.h"

#if RECORD_MATCHES
class SkPicture;
#endif
class SkRect;
class SkTypeface;

// A class containing a typeface for reference, the length in glyphs, and
// the upper and lower case representations of the search string.
class GlyphSet {
public:
    GlyphSet(const SkPaint& paint, const UChar* lower, const UChar* upper,
            size_t byteLength);
    ~GlyphSet();
    GlyphSet& operator=(GlyphSet& src);

    // Return true iff c matches one of our glyph arrays at index
    bool characterMatches(uint16_t c, int index);
    
    int getCount() const { return mCount; }

    const SkTypeface* getTypeface() const { return mTypeface; }

private:
    // Disallow copy constructor
    GlyphSet(GlyphSet& src) { }

    // mTypeface is used for comparison only
    const SkTypeface* mTypeface;
    // mLowerGlyphs points to all of our storage space: the lower set followed
    // by the upper set.  mUpperGlyphs is purely a convenience pointer to the
    // start of the upper case glyphs.
    uint16_t*   mLowerGlyphs;
    uint16_t*   mUpperGlyphs;
    // mCount is the number of glyphs of the search string.  Must be the same
    // for both the lower case set and the upper case set.
    int         mCount;

    // Arbitrarily chose the maximum storage to use in the GlyphSet.  This is
    // based on the length of the word being searched.  If users are always
    // searching for 3 letter words (for example), an ideal number would be 3.
    // Each time the user searches for a word longer than (in this case, 3) that
    // will result in calling new/delete.
    enum Storage {
        MAX_STORAGE_COUNT = 16
    };
    // In order to eliminate new/deletes, create storage that will be enough
    // most of the time
    uint16_t    mStorage[2*MAX_STORAGE_COUNT];
};

class FindBounder : public SkBounder {
public:
    FindBounder() {}
    ~FindBounder() {}
protected:
    virtual bool onIRect(const SkIRect&) { return false; }
};

class FindCanvas : public SkCanvas {
public:
    FindCanvas(int width, int height, const UChar* , const UChar*,
            size_t byteLength);

    virtual ~FindCanvas();

    virtual void drawText(const void* text, size_t byteLength, SkScalar x,
                          SkScalar y, const SkPaint& paint);

    /* FIXME: This path has not been tested. */
    virtual void drawPosText(const void* text, size_t byteLength,
                             const SkPoint pos[], const SkPaint& paint);

    /* Also untested */
    virtual void drawPosTextH(const void* text, size_t byteLength,
                              const SkScalar xpos[], SkScalar constY,
                              const SkPaint& paint);

    /* Not sure what to do here or for drawTextOnPathHV */
    virtual void drawTextOnPath(const void* text, size_t byteLength,
                                const SkPath& path, const SkMatrix* matrix,
                                const SkPaint& paint) {
    }

    int found() const { return mNumFound; }

    // This method detaches our array of regions and passes ownership to
    // the caller, who is then responsible for deleting them.
    WTF::Vector<SkRegion>* detachRegions() {
        WTF::Vector<SkRegion>* array = mRegions;
        mRegions = NULL;
        return array;
    }

#if RECORD_MATCHES
    // This SkPicture contains only draw calls for the drawn text.  This is
    // used to draw over the highlight rectangle so that it can be seen.
    SkPicture* getDrawnMatches() const { return mPicture; }
#endif

private:
    // These calls are made by findHelper to store information about each match
    // that is found.  They return a rectangle which is used to highlight the
    // match.  They also add to our SkPicture (which can be accessed with
    // getDrawnMatches) a draw of each match.  This way it can be drawn after
    // the rectangle.  The rect that is returned is in device coordinates.
    SkRect addMatchNormal(int index,
        const SkPaint& paint, int count, const uint16_t* glyphs,
        const SkScalar pos[], SkScalar y);

    SkRect addMatchPos(int index,
        const SkPaint& paint, int count, const uint16_t* glyphs,
        const SkScalar xPos[], SkScalar /* y */);

    SkRect addMatchPosH(int index,
        const SkPaint& paint, int count, const uint16_t* glyphs,
        const SkScalar position[], SkScalar constY);
    
    // Helper for each of our draw calls
    void findHelper(const void* text, size_t byteLength, const SkPaint& paint,
                    const SkScalar xPos[], SkScalar y,
                    SkRect (FindCanvas::*addMatch)(int index,
                    const SkPaint& paint, int count, const uint16_t* glyphs,
                    const SkScalar pos[], SkScalar y));

    // Return the set of glyphs and its count for the text being searched for
    // and the parameter paint.  If one has already been created and cached
    // for this paint, use it.  If not, create a new one and cache it.
    GlyphSet* getGlyphs(const SkPaint& paint);

    // Since we may transfer ownership of this array (see detachRects()), we
    // hold a pointer to the array instead of just the array itself.
    WTF::Vector<SkRegion>*  mRegions;
    const UChar*            mLowerText;
    const UChar*            mUpperText;
    size_t                  mLength;
    FindBounder             mBounder;
    int                     mNumFound;
    SkScalar                mOutset;
    SkTDArray<GlyphSet>     mGlyphSets;
#if RECORD_MATCHES
    SkPicture*              mPicture;
    SkCanvas*               mRecordingCanvas;
#endif

    SkRegion                mWorkingRegion;
    int                     mWorkingIndex;
};

#endif  // Find_Canvas_h

