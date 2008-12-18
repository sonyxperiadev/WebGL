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

#ifndef CachedFrame_H
#define CachedFrame_H

#include "CachedNode.h"
#include "IntRect.h"
#include "SkFixed.h"
#include "wtf/Vector.h"

namespace WebCore {
    class Frame;
    class Node;
}

namespace android {

class CachedHistory;
class CachedRoot;

    // first node referenced by cache is always document
class CachedFrame {
public:
    enum Direction {
        UNINITIALIZED = -1,
        LEFT,
        RIGHT,
        UP,
        DOWN,
        DIRECTION_COUNT,
        DIRECTION_MASK = DIRECTION_COUNT - 1,
        UP_DOWN = UP & DOWN,  // mask and result
        RIGHT_DOWN = RIGHT & DOWN, // mask and result
    };
    enum Compare {
        UNDECIDED = -1,
        TEST_IS_BEST,
        REJECT_TEST 
    };
    CachedFrame() {}
    void add(CachedNode& node) { mCachedNodes.append(node); }
    void addFrame(CachedFrame& child) { mCachedFrames.append(child); }
    bool checkVisited(const CachedNode* , CachedFrame::Direction ) const;
    size_t childCount() { return mCachedFrames.size(); }
    void clearFocus();
    bool containsFrame(const CachedFrame* ) const;
    const CachedNode* currentFocus() const { return currentFocus(NULL); }
    const CachedNode* currentFocus(const CachedFrame** ) const;
    bool directionChange() const;
    const CachedNode* document() const { return mCachedNodes.begin(); }
    bool empty() const { return mCachedNodes.size() < 2; } // must have 1 past doc
    const CachedNode* findBestAt(const WebCore::IntRect& , int* best,
        const CachedNode** , const CachedFrame** , int* x, int* y) const;
    const CachedFrame* findBestFrameAt(int x, int y) const;
    const CachedNode* findBestHitAt(const WebCore::IntRect& , 
        int* best, const CachedFrame** , int* x, int* y) const;
    bool finishInit();
    CachedFrame* firstChild() { return mCachedFrames.begin(); }
    const CachedFrame* firstChild() const { return mCachedFrames.begin(); }
    int focusIndex() const { return mFocus; }
    void* framePointer() const { return mFrame; }
    CachedNode* getIndex(int index) { return index >= 0 ?
        &mCachedNodes[index] : NULL; }
    const CachedFrame* hasFrame(const CachedNode* node) const;
    int indexInParent() const { return mIndex; }
    void init(const CachedRoot* root, int index, WebCore::Frame* frame);
    const CachedFrame* lastChild() const { return &mCachedFrames.last(); }
    CachedNode* lastNode() { return &mCachedNodes.last(); }
    CachedFrame* lastChild() { return &mCachedFrames.last(); }
    const CachedFrame* parent() const { return mParent; }
    CachedFrame* parent() { return mParent; }
    void removeLast() { mCachedNodes.removeLast(); }
    void resetClippedOut();
    void setContentsSize(int width, int height) { mContents.setWidth(width);
        mContents.setHeight(height); }
    void setData();
    bool setFocus(WebCore::Frame* , WebCore::Node* , int x, int y);
    void setFocusIndex(int focusIndex) const { mFocus = focusIndex; }
    void setLocalViewBounds(const WebCore::IntRect& bounds) { mLocalViewBounds = bounds; }
    int size() { return mCachedNodes.size(); }
    const CachedNode* validDocument() const;
protected:
    struct BestData {
        WebCore::IntRect mNodeBounds;
        WebCore::IntRect mMouseBounds;
        int mDistance;
        int mSideDistance;
        int mMajorDelta; // difference of center of object
            // used only when leading and trailing edges contain another set of edges
        int mMajorDelta2; // difference of leading edge (only used when center is same)
        int mMajorButt; // checks for next cell butting up against or close to previous one
        int mWorkingDelta;
        int mWorkingDelta2;
        int mNavDelta;
        int mNavDelta2;
        const CachedFrame* mFrame;
        const CachedNode* mNode;
        SkFixed mWorkingOverlap;   // this and below are fuzzy answers instead of bools
        SkFixed mNavOverlap;
        SkFixed mPreferred;
        bool mFocusChild;
        bool mInNav;
        bool mNavOutside;
        bool mWorkingOutside;
        int bottom() const { return bounds().bottom(); }
        const WebCore::IntRect& bounds() const { return mNodeBounds; }
        bool canBeReachedByAnotherDirection();
        int height() const { return bounds().height(); }
        bool inOrSubsumesNav() const { return (mNavDelta ^ mNavDelta2) >= 0; }
        bool inOrSubsumesWorking() const { return (mWorkingDelta ^ mWorkingDelta2) >= 0; }
        int isContainer(BestData* );
        static SkFixed Overlap(int span, int left, int right);
        void reset() { mNode = NULL; }
        int right() const { return bounds().right(); }
        void setDistances();
        bool setDownDirection(const CachedHistory* );
        bool setLeftDirection(const CachedHistory* );
        bool setRightDirection(const CachedHistory* );
        bool setUpDirection(const CachedHistory* );
        void setNavInclusion(int left, int right);
        void setNavOverlap(int span, int left, int right);
        void setWorkingInclusion(int left, int right);
        void setWorkingOverlap(int span, int left, int right);
        int width() const { return bounds().width(); }
        int x() const { return bounds().x(); }
        int y() const { return bounds().y(); }
    };
    typedef const CachedNode* (CachedFrame::*MoveInDirection)(
        const CachedNode* test, const CachedNode* limit, BestData* bestData, 
        const CachedNode* focus) const;
    void adjustToTextColumn(int* delta) const;
    static bool CheckBetween(Direction , const WebCore::IntRect& bestRect, 
        const WebCore::IntRect& prior, WebCore::IntRect* result);
    bool checkBetween(BestData* , Direction );
    int compare(BestData& testData, const BestData& bestData, const 
        CachedNode* focus) const;
    void findClosest(BestData* , Direction original, Direction test,
        WebCore::IntRect* clip) const;
    int frameNodeCommon(BestData& testData, const CachedNode* test, 
        BestData* bestData, BestData* originalData, 
        const CachedNode* focus) const;
    int framePartCommon(BestData& testData, const CachedNode* test, 
        BestData* bestData, const CachedNode* focus) const;
    const CachedNode* frameDown(const CachedNode* test, const CachedNode* limit, 
        BestData* , const CachedNode* focus) const;
    const CachedNode* frameLeft(const CachedNode* test, const CachedNode* limit, 
        BestData* , const CachedNode* focus) const;
    const CachedNode* frameRight(const CachedNode* test, const CachedNode* limit, 
        BestData* , const CachedNode* focus) const;
    const CachedNode* frameUp(const CachedNode* test, const CachedNode* limit, 
        BestData* , const CachedNode* focus) const;
    int minWorkingHorizontal() const;
    int minWorkingVertical() const;
    int maxWorkingHorizontal() const;
    int maxWorkingVertical() const;
    bool moveInFrame(MoveInDirection , const CachedNode* test, BestData* best,
        const CachedNode* focus) const;
    const WebCore::IntRect& _navBounds() const;
    WebCore::IntRect mContents;
    WebCore::IntRect mLocalViewBounds;
    WebCore::IntRect mViewBounds;
    WTF::Vector<CachedNode> mCachedNodes;
    WTF::Vector<CachedFrame> mCachedFrames;
    void* mFrame; // WebCore::Frame*, used only to compare pointers
    CachedFrame* mParent;
    int mIndex; // index within parent's array of children, or -1 if root
    const CachedRoot* mRoot;
    mutable int mFocus;
private:
    CachedHistory* history() const;
#ifdef BROWSER_DEBUG
public:
        CachedNode* find(WebCore::Node* ); // !!! probably debugging only
        int mDebugIndex;
        int mDebugLoopbackOffset;
#endif
#if !defined NDEBUG || DUMP_NAV_CACHE 
public:
    class Debug {
public:
        Debug() { 
#if DUMP_NAV_CACHE
            mFrameName[0] = '\0'; 
#endif
#if !defined NDEBUG
            mInUse = true; 
#endif
        }
#if !defined NDEBUG
        ~Debug() { mInUse = false; }
        bool mInUse;
#endif
#if DUMP_NAV_CACHE
        CachedFrame* base() const;
        void print() const;
        bool validate(const CachedNode* ) const;
        char mFrameName[256];
#endif
    } mDebug;
#endif
};

}

#endif
