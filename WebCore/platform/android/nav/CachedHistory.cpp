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
    mPriorBounds = mFocusBounds = WebCore::IntRect(0, 0, 0, 0);
    mDirectionChange = false;
    mFocusIsInput = false;
    mPriorIsInput = false;
    mDidFirstLayout = false;
    mPriorMove = mLastMove = CachedFrame::UNINITIALIZED;
    mMinWorkingHorizontal = mMinWorkingVertical = INT_MIN;
    mMaxWorkingHorizontal = mMaxWorkingVertical = INT_MAX;
}

void CachedHistory::setWorking(CachedFrame::Direction newMove, 
    const CachedNode* focus, const WebCore::IntRect& viewBounds)
{
    CachedFrame::Direction lastAxis = (CachedFrame::Direction) (mLastMove & ~CachedFrame::RIGHT_DOWN); // up, left or uninitialized
    CachedFrame::Direction newAxis = (CachedFrame::Direction) (newMove & ~CachedFrame::RIGHT_DOWN); 
    bool change = newAxis != lastAxis;
    mDirectionChange = change && mLastMove != CachedFrame::UNINITIALIZED;
    if (focus != NULL || mLastMove != CachedFrame::UNINITIALIZED) {
        mPriorMove = mLastMove;
        mLastMove = newMove;
    }
    const WebCore::IntRect* navBounds = &mNavBounds;
    if (focus != NULL) {
        WebCore::IntRect focusBounds;
        focus->getBounds(&focusBounds);
        if (focusBounds.isEmpty() == false && focusBounds != mFocusBounds)
            mNavBounds = mFocusBounds = focusBounds;
        mPriorIsInput = mFocusIsInput;
        mFocusIsInput = focus->isInput(); // focus->localName() == "input";
    } else
        mFocusBounds = WebCore::IntRect(0, 0, 0, 0);
    if (change) {   // uninitialized or change in direction
        if (lastAxis != CachedFrame::LEFT && navBounds->height() > 0) {
            mMinWorkingHorizontal = navBounds->y();
            mMaxWorkingHorizontal = navBounds->bottom();
        }
        if (lastAxis != CachedFrame::UP && navBounds->width() > 0) {
            mMinWorkingVertical = navBounds->x();
            mMaxWorkingVertical = navBounds->right();
        }
    } else if (mPriorIsInput) {
        if (newAxis == CachedFrame::UP_DOWN) {
            if (mPriorBounds.x() == mMinWorkingVertical && mPriorBounds.right() == mMaxWorkingVertical) {
                mMinWorkingVertical = navBounds->x();
                mMaxWorkingVertical = navBounds->right();
            }
        } else {
            if (mPriorBounds.y() == mMinWorkingHorizontal && mPriorBounds.bottom() == mMaxWorkingHorizontal) {
                mMinWorkingHorizontal = navBounds->y();
                mMaxWorkingHorizontal = navBounds->bottom();
            }
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
    DEBUG_PRINT_RECT(mFocusBounds);
    DEBUG_PRINT_RECT(mNavBounds);
    DEBUG_PRINT_RECT(mPriorBounds);
    DEBUG_PRINT_BOOL(mDirectionChange);
    DEBUG_PRINT_BOOL(mFocusIsInput);
    DEBUG_PRINT_BOOL(mPriorIsInput);
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
