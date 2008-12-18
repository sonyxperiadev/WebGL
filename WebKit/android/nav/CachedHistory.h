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

#ifndef CachedHistory_H
#define CachedHistory_H

#include "CachedFrame.h"

#define NAVIGATION_VISIT_DEPTH 8    // the number of nodes last visited -- used to detect ping-ponging (number should be tuned)

namespace android {

class CachedRoot;

// CachedHistory is maintained even if DOM is rebuilt by running script.
// It uses blind pointers for comparison in the previously visited nodes.
class CachedHistory {
public:
    CachedHistory();
    void addToVisited(const CachedNode* , CachedFrame::Direction );
    bool checkVisited(const CachedNode* , CachedFrame::Direction ) const;
    bool didFirstLayout() const { return mDidFirstLayout; }
    bool directionChange() const { return mDirectionChange; }
    const WebCore::IntRect& focusBounds() const { return mFocusBounds; }
    int minWorkingHorizontal() const { return mMinWorkingHorizontal; }
    int minWorkingVertical() const { return mMinWorkingVertical; }
    int maxWorkingHorizontal() const { return mMaxWorkingHorizontal; }
    int maxWorkingVertical() const { return mMaxWorkingVertical; }
    const WebCore::IntRect& navBounds() const { return mNavBounds; }
    const WebCore::IntRect& priorBounds() const { return mPriorBounds; }
    void setDidFirstLayout(bool did) { mDidFirstLayout = did; }
    void setNavBounds(const WebCore::IntRect& loc) { mNavBounds = loc; }
    void setWorking(CachedFrame::Direction , const CachedNode* focus,
        const WebCore::IntRect& viewBounds);
    void reset();
private:
    void pinMaxMin(const WebCore::IntRect& viewBounds);
    struct Visited {
        const CachedNode* mNode;
        CachedFrame::Direction mDirection;
    } mVisited[NAVIGATION_VISIT_DEPTH];
    WebCore::IntRect mFocusBounds; // chosen focus ring
    WebCore::IntRect mMouseBounds; // constricted bounds, if focus ring is partially visible
    WebCore::IntRect mNavBounds; // focus ring bounds plus optional keystroke movement
    WebCore::IntRect mPriorBounds; // prior chosen focus ring (for reversing narrowing)
    bool mDirectionChange;
    bool mFocusIsInput; // defer max/min to non-focus node if focus is too broad
    bool mPriorIsInput; // defer max/min to non-focus node if focus is too broad
    bool mDidFirstLayout; // set true when page is newly laid out
    CachedFrame::Direction mLastMove;
    CachedFrame::Direction mPriorMove;
    int mMinWorkingHorizontal;
    int mMaxWorkingHorizontal;
    int mMinWorkingVertical;
    int mMaxWorkingVertical;
    friend class CachedRoot;
#if DUMP_NAV_CACHE
public:
    class Debug {
public:
        CachedHistory* base() const;
        const char* direction(CachedFrame::Direction d) const;
        void print(CachedRoot* ) const;
    } mDebug;
#endif
};

}

#endif
