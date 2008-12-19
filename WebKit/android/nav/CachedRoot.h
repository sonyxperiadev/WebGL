/* 
** Copyright 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
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
 //   WebCore::IntRect mClippedBounds;
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
