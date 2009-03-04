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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "IntPoint.h"
#include "SkPicture.h"

class SkRect;

namespace android {

class CachedHistory;
class CachedNode;

class CachedRoot : public CachedFrame {
public:
    bool adjustForScroll(BestData* , Direction , WebCore::IntPoint* scrollPtr,
        bool findClosest);
    int checkForCenter(int x, int y) const;
    void checkForJiggle(int* ) const;
    int documentHeight() { return mContents.height(); }
    int documentWidth() { return mContents.width(); }
    const CachedNode* findAt(const WebCore::IntRect& , const CachedFrame** ,
        int* x, int* y) const;
    const WebCore::IntRect& focusBounds() const { return mFocusBounds; }
    bool focusChild() const { return mFocusChild; }
    WebCore::IntPoint focusLocation() const;
    int generation() const { return mGeneration; }
    SkPicture* getPicture() { return mPicture; }
    int getAndResetSelectionEnd();
    int getAndResetSelectionStart();
//    const WebCore::IntRect& navClipBounds() const { return mClippedBounds; }
    void getSimulatedMousePosition(WebCore::IntPoint* );
//    bool hasNavClipBounds() { return mClippedBounds.isEmpty() == false; }
    void init(WebCore::Frame* , CachedHistory* );
    bool innerDown(const CachedNode* , BestData* ) const;
    bool innerLeft(const CachedNode* , BestData* ) const;
    void innerMove(const CachedNode* ,BestData* bestData, Direction , 
        WebCore::IntPoint* scroll, bool firstCall);
    bool innerRight(const CachedNode* , BestData* ) const;
    bool innerUp(const CachedNode* , BestData* ) const;
    WebCore::String imageURI(int x, int y) const;
    bool maskIfHidden(BestData* ) const;
    const CachedNode* moveFocus(Direction , const CachedFrame** , WebCore::IntPoint* scroll);
    void reset();
//    void resetNavClipBounds() { mClippedBounds = WebCore::IntRect(-1, -1, 0, 0); }
    CachedHistory* rootHistory() const { return mHistory; }
    bool scrollDelta(WebCore::IntRect& focusRingBounds, Direction , int* delta);
    const WebCore::IntRect& scrolledBounds() const { return mScrolledBounds; }
    void setCachedFocus(CachedFrame* , CachedNode* );
    void setFocusBounds(const WebCore::IntRect& r) { mFocusBounds = r; }
    void setGeneration(int generation) { mGeneration = generation; }
    void setTextGeneration(int textGeneration) { mTextGeneration = textGeneration; }
    void setFocusChild(bool state) const { mFocusChild = state; }
    void setMaxScroll(int x, int y) { mMaxXScroll = x; mMaxYScroll = y; }
//    void setNavClipBounds(const WebCore::IntRect& r) { mClippedBounds = r; }
    void setPicture(SkPicture* picture) { mPicture = picture; }
    void setScrollOnly(bool state) { mScrollOnly = state; }
    void setSelection(int start, int end) { mSelectionStart = start; mSelectionEnd = end; }
    void setupScrolledBounds() const;
    void setVisibleRect(const WebCore::IntRect& r) { mViewBounds = r; }
    int textGeneration() const { return mTextGeneration; }
    int width() const { return mPicture ? mPicture->width() : 0; }
private:
    CachedHistory* mHistory;
    SkPicture* mPicture;
    WebCore::IntRect mFocusBounds; // chosen focus ring
    mutable WebCore::IntRect mScrolledBounds; // view bounds + amount visible as result of scroll
    int mGeneration;
    int mTextGeneration;
    int mMaxXScroll;
    int mMaxYScroll;
    // These two are ONLY used when the tree is rebuilt and the focus is a textfield/area
    int mSelectionStart;
    int mSelectionEnd;
    mutable bool mFocusChild; // temporary state set if walked nodes are children of focus
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
