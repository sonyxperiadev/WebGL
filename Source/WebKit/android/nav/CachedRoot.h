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

#ifndef CachedRoot_H
#define CachedRoot_H

#include "CachedFrame.h"
#include "IntRect.h"
#include "SkPicture.h"
#include "SkRegion.h"
#include "wtf/Vector.h"

class SkRect;

namespace WebCore {
    class LayerAndroid;
}

namespace android {

class CachedHistory;
class CachedNode;
class FindCanvas;

class CachedRoot : public CachedFrame {
public:
    bool adjustForScroll(BestData* , Direction , WebCore::IntPoint* scrollPtr,
        bool findClosest);
    const SkRegion& baseUncovered() const { return mBaseUncovered; }
    void calcBitBounds(const IntRect& , IntRect* ) const;
    int checkForCenter(int x, int y) const;
    void checkForJiggle(int* ) const;
    bool checkRings(SkPicture* , const CachedNode* ,
        const WebCore::IntRect& testBounds) const;
    WebCore::IntPoint cursorLocation() const;
    int documentHeight() { return mContents.height(); }
    int documentWidth() { return mContents.width(); }
    void draw(FindCanvas& ) const;
    const CachedNode* findAt(const WebCore::IntRect& , const CachedFrame** ,
        int* x, int* y, bool checkForHidden) const;
    const WebCore::IntRect& focusBounds() const { return mFocusBounds; }
    WebCore::IntPoint focusLocation() const;
    int getAndResetSelectionEnd();
    int getAndResetSelectionStart();
    int getBlockLeftEdge(int x, int y, float scale) const;
    void getSimulatedMousePosition(WebCore::IntPoint* ) const;
    void init(WebCore::Frame* , CachedHistory* );
    bool innerDown(const CachedNode* , BestData* ) const;
    bool innerLeft(const CachedNode* , BestData* ) const;
    void innerMove(const CachedNode* ,BestData* bestData, Direction , 
        WebCore::IntPoint* scroll, bool firstCall);
    bool innerRight(const CachedNode* , BestData* ) const;
    bool innerUp(const CachedNode* , BestData* ) const;
    WTF::String imageURI(int x, int y) const;
    bool maskIfHidden(BestData* ) const;
    const CachedNode* moveCursor(Direction , const CachedFrame** , WebCore::IntPoint* scroll);
    /**
     * Find the next textfield/textarea
     * @param start         The textfield/textarea to search from.
     * @param framePtr      If non-zero, returns CachedFrame* containing result.
     * @return CachedNode*  Next textfield/textarea or null (0) if none.
     */
    const CachedNode* nextTextField(const CachedNode* start,
        const CachedFrame** framePtr) const;
    SkPicture* pictureAt(int* xPtr, int* yPtr, int* id) const;
    SkPicture* pictureAt(int* xPtr, int* yPtr) const {
        return pictureAt(xPtr, yPtr, 0); }
    void reset();
    CachedHistory* rootHistory() const { return mHistory; }
    const WebCore::LayerAndroid* rootLayer() const { return mRootLayer; }
    bool scrollDelta(WebCore::IntRect& cursorRingBounds, Direction , int* delta);
    const WebCore::IntRect& scrolledBounds() const { return mScrolledBounds; }
    void setCursor(CachedFrame* , CachedNode* );
    void setCursorCache(int scrollX, int scrollY) const; // compute cached state used to find next cursor
    void setCachedFocus(CachedFrame* , CachedNode* );
    void setFocusBounds(const WebCore::IntRect& r) { mFocusBounds = r; }
    void setTextGeneration(int textGeneration) { mTextGeneration = textGeneration; }
    void setMaxScroll(int x, int y) { mMaxXScroll = x; mMaxYScroll = y; }
    void setPicture(SkPicture* picture) { mPicture = picture; }
    void setRootLayer(WebCore::LayerAndroid* layer) {
        mRootLayer = layer;
        resetLayers();
    }
    void setScrollOnly(bool state) { mScrollOnly = state; }
    void setSelection(int start, int end) { mSelectionStart = start; mSelectionEnd = end; }
    void setupScrolledBounds() const { mScrolledBounds = mViewBounds; }
    void setVisibleRect(const WebCore::IntRect& r) { mViewBounds = r; }
    int textGeneration() const { return mTextGeneration; }
    int width() const { return mPicture ? mPicture->width() : 0; }
private:
    friend class CachedFrame;
    CachedHistory* mHistory;
    SkPicture* mPicture;
    WebCore::LayerAndroid* mRootLayer;
    WebCore::IntRect mFocusBounds; // dom text input focus node bounds
    mutable WebCore::IntRect mScrolledBounds; // view bounds + amount visible as result of scroll
    int mTextGeneration;
    int mMaxXScroll;
    int mMaxYScroll;
    // These two are ONLY used when the tree is rebuilt and the focus is a textfield/area
    int mSelectionStart;
    int mSelectionEnd;
    // these four set up as cache for use by frameDown/Up/Left/Right etc
    mutable WebCore::IntRect mCursorBounds;
    mutable const CachedNode* mCursor;
    mutable SkRegion mBaseUncovered;
    bool mScrollOnly;
#if DUMP_NAV_CACHE
public:
    class Debug {
public:
        CachedRoot* base() const;
        void print() const;
    } mDebug;
#endif
};

}

#endif
