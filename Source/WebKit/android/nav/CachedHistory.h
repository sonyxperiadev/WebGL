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

#ifndef CachedHistory_h
#define CachedHistory_h

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
    int minWorkingHorizontal() const { return mMinWorkingHorizontal; }
    int minWorkingVertical() const { return mMinWorkingVertical; }
    int maxWorkingHorizontal() const { return mMaxWorkingHorizontal; }
    int maxWorkingVertical() const { return mMaxWorkingVertical; }
    const WebCore::IntRect& navBounds() const { return mNavBounds; }
    const WebCore::IntRect& priorBounds() const { return mPriorBounds; }
    void setDidFirstLayout(bool did) { mDidFirstLayout = did; }
    void setMouseBounds(const WebCore::IntRect& loc) { mMouseBounds = loc; }
    void setNavBounds(const WebCore::IntRect& loc) { mNavBounds = loc; }
    void setWorking(CachedFrame::Direction , const CachedFrame* ,
        const CachedNode* , const WebCore::IntRect& viewBounds);
    void reset();
private:
    void pinMaxMin(const WebCore::IntRect& viewBounds);
    struct Visited {
        const CachedNode* mNode;
        CachedFrame::Direction mDirection;
    } mVisited[NAVIGATION_VISIT_DEPTH];
    WebCore::IntRect mMouseBounds; // constricted bounds, if cursor ring is partially visible
    WebCore::IntRect mNavBounds; // cursor ring bounds plus optional keystroke movement
    WebCore::IntRect mPriorBounds; // prior chosen cursor ring (for reversing narrowing)
    bool mDirectionChange;
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
