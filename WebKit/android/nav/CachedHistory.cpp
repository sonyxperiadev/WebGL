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

#include "CachedPrefix.h"
#include "CachedFrame.h"
#include "CachedNode.h"
#if DUMP_NAV_CACHE
#include "CachedRoot.h"
#endif

#include "CachedHistory.h"

namespace android {

CachedHistory::CachedHistory() {
    memset(this, 0, sizeof(CachedHistory)); // this assume the class has no virtuals
    mLastMove = CachedFrame::UNINITIALIZED;
    mPriorMove = CachedFrame::UNINITIALIZED;
}


void CachedHistory::addToVisited(const CachedNode* node, CachedFrame::Direction direction)
{
    memmove(&mVisited[1], &mVisited[0], sizeof(mVisited) - sizeof(mVisited[0]));
    mVisited[0].mNode = node;
    mVisited[0].mDirection = direction;
}

bool CachedHistory::checkVisited(const CachedNode* node, CachedFrame::Direction direction) const
{
    // if the direction is unchanged and we've already visited this node, don't visit it again
    int index = 0;
    while (index < NAVIGATION_VISIT_DEPTH - 1) {
        if (direction != mVisited[index].mDirection)
            break;
        index++; // compare with last direction, previous to last node (where the arrow took us from)
        if (node == mVisited[index].mNode)
            return false;
    }
    return true;
}

void CachedHistory::pinMaxMin(const WebCore::IntRect& viewBounds)
{
    if (mMinWorkingHorizontal < viewBounds.y() || 
            mMinWorkingHorizontal >= viewBounds.bottom())
        mMinWorkingHorizontal = viewBounds.y();
    if (mMaxWorkingHorizontal > viewBounds.bottom() || 
            mMaxWorkingHorizontal <= viewBounds.y())
        mMaxWorkingHorizontal = viewBounds.bottom();
    if (mMinWorkingVertical < viewBounds.x() || 
            mMinWorkingVertical >= viewBounds.right())
        mMinWorkingVertical = viewBounds.x();
    if (mMaxWorkingVertical > viewBounds.right() || 
            mMaxWorkingVertical <= viewBounds.x())
        mMaxWorkingVertical = viewBounds.right();
}

void CachedHistory::reset()
{
    memset(mVisited, 0, sizeof(mVisited));
//    mLastScroll = 0;
    mPriorBounds = WebCore::IntRect(0, 0, 0, 0);
    mDirectionChange = false;
    mDidFirstLayout = false;
    mPriorMove = mLastMove = CachedFrame::UNINITIALIZED;
    mMinWorkingHorizontal = mMinWorkingVertical = INT_MIN;
    mMaxWorkingHorizontal = mMaxWorkingVertical = INT_MAX;
}

void CachedHistory::setWorking(CachedFrame::Direction newMove, 
    const CachedFrame* cursorFrame, const CachedNode* cursor,
    const WebCore::IntRect& viewBounds)
{
    CachedFrame::Direction lastAxis = (CachedFrame::Direction) (mLastMove & ~CachedFrame::RIGHT_DOWN); // up, left or uninitialized
    CachedFrame::Direction newAxis = (CachedFrame::Direction) (newMove & ~CachedFrame::RIGHT_DOWN); 
    bool change = newAxis != lastAxis;
    mDirectionChange = change && mLastMove != CachedFrame::UNINITIALIZED;
    if (cursor != NULL || mLastMove != CachedFrame::UNINITIALIZED) {
        mPriorMove = mLastMove;
        mLastMove = newMove;
    }
    const WebCore::IntRect* navBounds = &mNavBounds;
    if (cursor != NULL) {
        WebCore::IntRect cursorBounds = cursor->bounds(cursorFrame);
        if (cursorBounds.isEmpty() == false)
            mNavBounds = cursorBounds;
    }
    if (change) {   // uninitialized or change in direction
        if (lastAxis != CachedFrame::LEFT && navBounds->height() > 0) {
            mMinWorkingHorizontal = navBounds->y();
            mMaxWorkingHorizontal = navBounds->bottom();
        }
        if (lastAxis != CachedFrame::UP && navBounds->width() > 0) {
            mMinWorkingVertical = navBounds->x();
            mMaxWorkingVertical = navBounds->right();
        }
    }
    pinMaxMin(viewBounds);
}

#if DUMP_NAV_CACHE

#define DEBUG_PRINT_BOOL(field) \
    DUMP_NAV_LOGD("// bool " #field "=%s;\n", b->field ? "true" : "false")

#define DEBUG_PRINT_RECT(field) \
    { const WebCore::IntRect& r = b->field; \
    DUMP_NAV_LOGD("// IntRect " #field "={%d, %d, %d, %d};\n", \
        r.x(), r.y(), r.width(), r.height()); }

CachedHistory* CachedHistory::Debug::base() const {
    CachedHistory* nav = (CachedHistory*) ((char*) this - OFFSETOF(CachedHistory, mDebug));
    return nav; 
}

const char* CachedHistory::Debug::direction(CachedFrame::Direction d) const
{
    switch (d) {
        case CachedFrame::LEFT: return "LEFT"; break;
        case CachedFrame::RIGHT: return "RIGHT"; break;
        case CachedFrame::UP: return "UP"; break;
        case CachedFrame::DOWN: return "DOWN"; break;
        default: return "UNINITIALIZED";
    }
}

void CachedHistory::Debug::print(CachedRoot* root) const
{
    CachedHistory* b = base();
    DUMP_NAV_LOGD("// Visited mVisited[]={\n");
    for (size_t i = 0; i < NAVIGATION_VISIT_DEPTH; i++) {
        const Visited& visit = b->mVisited[i];
        const CachedNode* node = visit.mNode;
        int index = root != NULL && root->CachedFrame::mDebug.validate(node) ?
            node->index() : -1;
        DUMP_NAV_LOGD("    // { 0x%p (%d), %s },\n", node, index, direction(visit.mDirection));
    }
    DUMP_NAV_LOGD("// };\n");
//    DUMP_NAV_LOGD("// int mLastScroll=%d;\n", b->mLastScroll);
    DEBUG_PRINT_RECT(mMouseBounds);
    DEBUG_PRINT_RECT(mNavBounds);
    DEBUG_PRINT_RECT(mPriorBounds);
    DEBUG_PRINT_BOOL(mDirectionChange);
    DEBUG_PRINT_BOOL(mDidFirstLayout);
    DUMP_NAV_LOGD("// CachedFrame::Direction mLastMove=%s, mPriorMove=%s;\n", 
        direction(b->mLastMove), direction(b->mPriorMove));
    int max = b->mMaxWorkingHorizontal;
    DUMP_NAV_LOGD("static int TEST_MAX_H = %d;\n",  max);
    int min = b->mMinWorkingHorizontal;
    if (min == INT_MIN)
        min++;
    DUMP_NAV_LOGD("static int TEST_MIN_H = %d;\n",  min);
    max = b->mMaxWorkingVertical;
    DUMP_NAV_LOGD("static int TEST_MAX_V = %d;\n",  max);
    min = b->mMinWorkingVertical;
    if (min == INT_MIN)
        min++;
    DUMP_NAV_LOGD("static int TEST_MIN_V = %d;\n",  min);
    DUMP_NAV_LOGD("\n");
}

#endif

}
