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

#ifndef Find_Canvas_h
#define Find_Canvas_h

#include "DrawExtra.h"
#include "IntRect.h"
#include "SkBounder.h"
#include "SkCanvas.h"
#include "SkPicture.h"
#include "SkRect.h"
#include "SkRegion.h"
#include "SkTDArray.h"

#include <unicode/umachine.h>
#include <wtf/Vector.h>

namespace android {

// Stores both region information and an SkPicture of the match, so that the
// region can be drawn, followed by drawing the matching text on top of it.
// This class owns its SkPicture
class MatchInfo {
public:
    MatchInfo();
    ~MatchInfo();
    MatchInfo(const MatchInfo& src);
    const SkRegion& getLocation() const { return m_location; }
    // Return a pointer to our picture, representing the matching text.  Does
    // not transfer ownership of the picture.
    SkPicture* getPicture() const { return m_picture; }
    // This will make a copy of the region, and increase the ref count on the
    // SkPicture.  If this MatchInfo already had one, unref it.
    bool isInLayer() const { return m_layerId >= 0; }
    int layerId() const { return m_layerId; }
    void set(const SkRegion& region, SkPicture* pic, int layerId);
private:
    MatchInfo& operator=(MatchInfo& src);
    SkRegion    m_location;
    SkPicture*  m_picture;
    int         m_layerId;
};

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

    void drawLayers(LayerAndroid* );
    int found() const { return mNumFound; }
    void setLayerId(int layerId) { mLayerId = layerId; }

    // This method detaches our array of matches and passes ownership to
    // the caller, who is then responsible for deleting them.
    WTF::Vector<MatchInfo>* detachMatches() {
        WTF::Vector<MatchInfo>* array = mMatches;
        mMatches = NULL;
        return array;
    }

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

    // If we already have a working canvas, grab it.  Otherwise, create a new
    // one.
    SkCanvas* getWorkingCanvas();

    // Return the set of glyphs and its count for the text being searched for
    // and the parameter paint.  If one has already been created and cached
    // for this paint, use it.  If not, create a new one and cache it.
    GlyphSet* getGlyphs(const SkPaint& paint);

    // Store all the accumulated info about a match in our vector.
    void insertMatchInfo(const SkRegion& region);

    // Throw away our cumulative information about our working SkCanvas.  After
    // this call, next call to getWorkingCanvas will create a new one.
    void resetWorkingCanvas();

    // Since we may transfer ownership of this array (see detachRects()), we
    // hold a pointer to the array instead of just the array itself.
    WTF::Vector<MatchInfo>* mMatches;
    const UChar*            mLowerText;
    const UChar*            mUpperText;
    Vector<UChar>           mLowerReversed;
    Vector<UChar>           mUpperReversed;
    size_t                  mLength;
    FindBounder             mBounder;
    int                     mNumFound;
    SkScalar                mOutset;
    SkTDArray<GlyphSet>     mGlyphSets;

    SkPicture*              mWorkingPicture;
    SkCanvas*               mWorkingCanvas;
    SkRegion                mWorkingRegion;
    int                     mWorkingIndex;
    int                     mLayerId;
};

class FindOnPage : public DrawExtra {
public:
    FindOnPage() {
        m_matches = 0;
        m_hasCurrentLocation = false;
        m_isFindPaintSetUp = false;
        m_lastBounds.setEmpty();
    }
    virtual ~FindOnPage() { delete m_matches; }
    void clearCurrentLocation() { m_hasCurrentLocation = false; }
    IntRect currentMatchBounds() const;
    int currentMatchIndex() const { return m_findIndex; }
    bool currentMatchIsInLayer() const;
    virtual void draw(SkCanvas* , LayerAndroid* , IntRect* );
    void findNext(bool forward);
    bool isCurrentLocationValid() { return m_hasCurrentLocation; }
    void setMatches(WTF::Vector<MatchInfo>* matches);
private:
    void drawMatch(const SkRegion& region, SkCanvas* canvas, bool focused);
    void setUpFindPaint();
    void storeCurrentMatchLocation();
    WTF::Vector<MatchInfo>* m_matches;
    // Stores the location of the current match.
    SkIPoint m_currentMatchLocation;
    // Tells whether the value in m_currentMatchLocation is valid.
    bool m_hasCurrentLocation;
    // Tells whether we have done the setup to draw the Find matches.
    bool m_isFindPaintSetUp;
    // Paint used to draw our Find matches.
    SkPaint m_findPaint;
    // Paint used for the background of our Find matches.
    SkPaint m_findBlurPaint;
    unsigned m_findIndex;
    SkIRect m_lastBounds;
};

}

#endif  // Find_Canvas_h
