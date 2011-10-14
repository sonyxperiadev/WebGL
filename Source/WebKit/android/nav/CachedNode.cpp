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
#include "android_graphics.h"
#include "CachedFrame.h"
#include "CachedHistory.h"
#include "Node.h"
#include "PlatformString.h"

#include "CachedNode.h"

namespace android {

WebCore::IntRect CachedNode::bounds(const CachedFrame* frame) const
{
    return mIsInLayer ? frame->adjustBounds(this, mBounds) : mBounds;
}

void CachedNode::clearCursor(CachedFrame* parent)
{
    if (isFrame()) {
        CachedFrame* child = const_cast<CachedFrame*>(parent->hasFrame(this));
        child->clearCursor();
    }
    mIsCursor = false;
}

bool CachedNode::Clip(const WebCore::IntRect& outer, WebCore::IntRect* inner,
    WTF::Vector<WebCore::IntRect>* rings)
{
    if (outer.contains(*inner))
        return true;
//    DBG_NAV_LOGD("outer:{%d,%d,%d,%d} does not contain inner:{%d,%d,%d,%d}",
//        outer.x(), outer.y(), outer.width(), outer.height(),
//        inner->x(), inner->y(), inner->width(), inner->height());
    bool intersects = outer.intersects(*inner);
    size_t size = intersects ? rings->size() : 0;
    *inner = WebCore::IntRect(0, 0, 0, 0);
    if (intersects) {
        WebCore::IntRect * const start = rings->begin();
        WebCore::IntRect* ring = start + size - 1;
        do {
            ring->intersect(outer);
            if (ring->isEmpty()) {
                if ((size_t) (ring - start) != --size)
                    *ring = start[size];
            } else
                inner->unite(*ring);
        } while (ring-- != start);
    }
    rings->shrink(size);
//    DBG_NAV_LOGD("size:%d", size);
    return size != 0;
}

bool CachedNode::clip(const WebCore::IntRect& bounds)
{
    return Clip(bounds, &mBounds, &mCursorRing);
}


void CachedNode::cursorRings(const CachedFrame* frame,
    WTF::Vector<WebCore::IntRect>* rings) const
{
    rings->clear();
    for (unsigned index = 0; index < mCursorRing.size(); index++)
        rings->append(ring(frame, index));
}

WebCore::IntRect CachedNode::cursorRingBounds(const CachedFrame* frame) const
{
    int partMax = navableRects();
    WebCore::IntRect bounds;
    for (int partIndex = 0; partIndex < partMax; partIndex++)
        bounds.unite(mCursorRing[partIndex]);
    bounds.inflate(CURSOR_RING_HIT_TEST_RADIUS);
    return mIsInLayer ? frame->adjustBounds(this, bounds) : bounds;
}

#define OVERLAP 3

void CachedNode::fixUpCursorRects(const CachedFrame* frame)
{
    if (mFixedUpCursorRects)
        return;
    mFixedUpCursorRects = true;
    // if the hit-test rect doesn't intersect any other rect, use it
    if (mHitBounds != mBounds && mHitBounds.contains(mBounds) &&
            frame->checkRings(this, mHitBounds)) {
        DBG_NAV_LOGD("use mHitBounds (%d,%d,%d,%d)", mHitBounds.x(),
            mHitBounds.y(), mHitBounds.width(), mHitBounds.height());
        mUseHitBounds = true;
        return;
    }
    if (navableRects() <= 1)
        return;
    // if there is more than 1 rect, and the bounds doesn't intersect
    // any other cursor ring bounds, use it
    IntRect sloppyBounds = mBounds;
    sloppyBounds.inflate(2); // give it a couple of extra pixels
    if (frame->checkRings(this, sloppyBounds)) {
        DBG_NAV_LOGD("use mBounds (%d,%d,%d,%d)", mBounds.x(),
            mBounds.y(), mBounds.width(), mBounds.height());
        mUseBounds = true;
        return;
    }
#if DEBUG_NAV_UI
    {
        WebCore::IntRect* boundsPtr = mCursorRing.begin() - 1;
        const WebCore::IntRect* const boundsEnd = mCursorRing.begin() + mCursorRing.size();
        while (++boundsPtr < boundsEnd)
            LOGD("%s %d:(%d, %d, %d, %d)\n", __FUNCTION__, boundsPtr - mCursorRing.begin(),
                boundsPtr->x(), boundsPtr->y(), boundsPtr->width(), boundsPtr->height());
    }
#endif
    // q: need to know when rects are for drawing and hit-testing, but not mouse down calcs?
    bool again;
    do {
        again = false;
        size_t size = mCursorRing.size();
        WebCore::IntRect* unitBoundsPtr = mCursorRing.begin() - 1;
        const WebCore::IntRect* const unitBoundsEnd = mCursorRing.begin() + size;
        while (++unitBoundsPtr < unitBoundsEnd) {
            // any other unitBounds to the left or right of this one?
            int unitTop = unitBoundsPtr->y();
            int unitBottom = unitBoundsPtr->maxY();
            int unitLeft = unitBoundsPtr->x();
            int unitRight = unitBoundsPtr->maxX();
            WebCore::IntRect* testBoundsPtr = mCursorRing.begin() - 1;
            while (++testBoundsPtr < unitBoundsEnd) {
                if (unitBoundsPtr == testBoundsPtr)
                    continue;
                int testTop = testBoundsPtr->y();
                int testBottom = testBoundsPtr->maxY();
                int testLeft = testBoundsPtr->x();
                int testRight = testBoundsPtr->maxX();
                int candidateTop = unitTop > testTop ? unitTop : testTop;
                int candidateBottom = unitBottom < testBottom ? unitBottom : testBottom;
                int candidateLeft = unitRight < testLeft ? unitRight : testRight;
                int candidateRight = unitRight > testLeft ? unitLeft : testLeft;
                bool leftRight = true;
                if (candidateTop + OVERLAP >= candidateBottom ||
                        candidateLeft + OVERLAP >= candidateRight) {
                    candidateTop = unitBottom < testTop ? unitBottom : testBottom;
                    candidateBottom = unitBottom > testTop ? unitTop : testTop;
                    candidateLeft = unitLeft > testLeft ? unitLeft : testLeft;
                    candidateRight = unitRight < testRight ? unitRight : testRight;
                    if (candidateTop + OVERLAP >= candidateBottom ||
                            candidateLeft + OVERLAP >= candidateRight)
                        continue;
                    leftRight = false;
                }
                // construct candidate to add
                WebCore::IntRect candidate = WebCore::IntRect(candidateLeft, candidateTop, 
                    candidateRight - candidateLeft, candidateBottom - candidateTop);
                // does a different unit bounds intersect the candidate? if so, don't add
                WebCore::IntRect* checkBoundsPtr = mCursorRing.begin() - 1;
                while (++checkBoundsPtr < unitBoundsEnd) {
                    if (checkBoundsPtr->intersects(candidate) == false)
                        continue;
                    if (leftRight) {
                        if (candidateTop >= checkBoundsPtr->y() &&
                                candidateBottom > checkBoundsPtr->maxY())
                            candidateTop = checkBoundsPtr->maxY();
                        else if (candidateTop < checkBoundsPtr->y() &&
                                candidateBottom <= checkBoundsPtr->maxY())
                            candidateBottom = checkBoundsPtr->y();
                        else
                            goto nextCheck;
                    } else {
                        if (candidateLeft >= checkBoundsPtr->x() &&
                                candidateRight > checkBoundsPtr->maxX())
                            candidateLeft = checkBoundsPtr->maxX();
                        else if (candidateLeft < checkBoundsPtr->x() &&
                                candidateRight <= checkBoundsPtr->maxX())
                            candidateRight = checkBoundsPtr->x();
                        else
                            goto nextCheck;
                    }
                 } 
                 candidate = WebCore::IntRect(candidateLeft, candidateTop, 
                    candidateRight - candidateLeft, candidateBottom - candidateTop);
                 ASSERT(candidate.isEmpty() == false);
#if DEBUG_NAV_UI
                LOGD("%s %d:(%d, %d, %d, %d)\n", __FUNCTION__, mCursorRing.size(),
                    candidate.x(), candidate.y(), candidate.width(), candidate.height());
#endif
                mCursorRing.append(candidate);
                again = true;
                goto tryAgain;
        nextCheck:
                continue;
            }
        }
tryAgain:
        ;
    } while (again);
}


void CachedNode::hideCursor(CachedFrame* parent)
{
    if (isFrame()) {
        CachedFrame* child = const_cast<CachedFrame*>(parent->hasFrame(this));
        child->hideCursor();
    }
    mIsHidden = true;
}

WebCore::IntRect CachedNode::hitBounds(const CachedFrame* frame) const
{
    return mIsInLayer ? frame->adjustBounds(this, mHitBounds) : mHitBounds;
}

void CachedNode::init(WebCore::Node* node)
{
    bzero(this, sizeof(CachedNode));
    mExport = WTF::String();
    mNode = node;
    mParentIndex = mDataIndex = -1;
    mType = android::NORMAL_CACHEDNODETYPE;
}

bool CachedNode::isTextField(const CachedFrame* frame) const
{
    const CachedInput* input = frame->textInput(this);
    return input ? input->isTextField() : false;
}

void CachedNode::localCursorRings(const CachedFrame* frame,
    WTF::Vector<WebCore::IntRect>* rings) const
{
    rings->clear();
    for (unsigned index = 0; index < mCursorRing.size(); index++)
        rings->append(localRing(frame, index));
}

WebCore::IntRect CachedNode::localBounds(const CachedFrame* frame) const
{
    return mIsInLayer ? frame->localBounds(this, mBounds) : mBounds;
}

WebCore::IntRect CachedNode::localHitBounds(const CachedFrame* frame) const
{
    return mIsInLayer ? frame->localBounds(this, mHitBounds) : mHitBounds;
}

WebCore::IntRect CachedNode::localRing(const CachedFrame* frame,
    size_t part) const
{
    const WebCore::IntRect& rect = mCursorRing.at(part);
    return mIsInLayer ? frame->localBounds(this, rect) : rect;
}

void CachedNode::move(int x, int y)
{
    mBounds.move(x, y);
    // mHitTestBounds will be moved by caller
    WebCore::IntRect* first = mCursorRing.begin();
    WebCore::IntRect* last = first + mCursorRing.size();
    --first;
    while (++first != last)
        first->move(x, y);
}

bool CachedNode::partRectsContains(const CachedNode* other) const
{    
    int outerIndex = 0;
    int outerMax = navableRects();
    int innerMax = other->navableRects();
    do {
        const WebCore::IntRect& outerBounds = mCursorRing[outerIndex];
        int innerIndex = 0;
        do {
            const WebCore::IntRect& innerBounds = other->mCursorRing[innerIndex];
            if (innerBounds.contains(outerBounds))
                return true;
        } while (++innerIndex < innerMax);
    } while (++outerIndex < outerMax);
    return false;
}

WebCore::IntRect CachedNode::ring(const CachedFrame* frame, size_t part) const
{
    const WebCore::IntRect& rect = mCursorRing.at(part);
    return mIsInLayer ? frame->adjustBounds(this, rect) : rect;
}

#if DUMP_NAV_CACHE

#define DEBUG_PRINT_BOOL(field) \
    DUMP_NAV_LOGD("// bool " #field "=%s;\n", b->field ? "true" : "false")

#define DEBUG_PRINT_RECT(field) \
    { const WebCore::IntRect& r = b->field; \
    DUMP_NAV_LOGD("// IntRect " #field "={%d, %d, %d, %d};\n", \
        r.x(), r.y(), r.width(), r.height()); }

CachedNode* CachedNode::Debug::base() const {
    CachedNode* nav = (CachedNode*) ((char*) this - OFFSETOF(CachedNode, mDebug));
    return nav; 
}

const char* CachedNode::Debug::condition(Condition t) const
{
    switch (t) {
        case NOT_REJECTED: return "NOT_REJECTED"; break;
        case BUTTED_UP: return "BUTTED_UP"; break;
        case CENTER_FURTHER: return "CENTER_FURTHER"; break;
        case CLOSER: return "CLOSER"; break;
        case CLOSER_IN_CURSOR: return "CLOSER_IN_CURSOR"; break;
        case CLOSER_OVERLAP: return "CLOSER_OVERLAP"; break;
        case CLOSER_TOP: return "CLOSER_TOP"; break;
        case NAVABLE: return "NAVABLE"; break;
        case FURTHER: return "FURTHER"; break;
        case IN_UMBRA: return "IN_UMBRA"; break;
        case IN_WORKING: return "IN_WORKING"; break;
        case LEFTMOST: return "LEFTMOST"; break;
        case OVERLAP_OR_EDGE_FURTHER: return "OVERLAP_OR_EDGE_FURTHER"; break;
        case PREFERRED: return "PREFERRED"; break;
        case ANCHOR_IN_ANCHOR: return "ANCHOR_IN_ANCHOR"; break;
        case BEST_DIRECTION: return "BEST_DIRECTION"; break;
        case CHILD: return "CHILD"; break;
        case DISABLED: return "DISABLED"; break;
        case HIGHER_TAB_INDEX: return "HIGHER_TAB_INDEX"; break;
        case IN_CURSOR: return "IN_CURSOR"; break;
        case IN_CURSOR_CHILDREN: return "IN_CURSOR_CHILDREN"; break;
        case NOT_ENCLOSING_CURSOR: return "NOT_ENCLOSING_CURSOR"; break;
        case NOT_CURSOR_NODE: return "NOT_CURSOR_NODE"; break;
        case OUTSIDE_OF_BEST: return "OUTSIDE_OF_BEST"; break;
        case OUTSIDE_OF_ORIGINAL: return "OUTSIDE_OF_ORIGINAL"; break;
        default: return "???";
    }
}

const char* CachedNode::Debug::type(android::CachedNodeType t) const
{
    switch (t) {
        case NORMAL_CACHEDNODETYPE: return "NORMAL"; break;
        case ADDRESS_CACHEDNODETYPE: return "ADDRESS"; break;
        case EMAIL_CACHEDNODETYPE: return "EMAIL"; break;
        case PHONE_CACHEDNODETYPE: return "PHONE"; break;
        case ANCHOR_CACHEDNODETYPE: return "ANCHOR"; break;
        case AREA_CACHEDNODETYPE: return "AREA"; break;
        case FRAME_CACHEDNODETYPE: return "FRAME"; break;
        case PLUGIN_CACHEDNODETYPE: return "PLUGIN"; break;
        case TEXT_INPUT_CACHEDNODETYPE: return "INPUT"; break;
        case SELECT_CACHEDNODETYPE: return "SELECT"; break;
        case CONTENT_EDITABLE_CACHEDNODETYPE: return "CONTENT_EDITABLE"; break;
        default: return "???";
    }
}

void CachedNode::Debug::print() const
{
    CachedNode* b = base();
    char scratch[256];
    size_t index = snprintf(scratch, sizeof(scratch), "// char* mExport=\"");
    const UChar* ch = b->mExport.characters();
    while (ch && *ch && index < sizeof(scratch)) {
        UChar c = *ch++;
        if (c < ' ' || c >= 0x7f) c = ' ';
        scratch[index++] = c;
    }
    DUMP_NAV_LOGD("%.*s\"\n", index, scratch);
    DEBUG_PRINT_RECT(mBounds);
    DEBUG_PRINT_RECT(mHitBounds);
    DEBUG_PRINT_RECT(mOriginalAbsoluteBounds);
    const WTF::Vector<WebCore::IntRect>* rects = &b->mCursorRing;
    size_t size = rects->size();
    DUMP_NAV_LOGD("// IntRect cursorRings={ // size=%d\n", size);
    for (size_t i = 0; i < size; i++) {
        const WebCore::IntRect& rect = (*rects)[i];
        DUMP_NAV_LOGD("    // {%d, %d, %d, %d}, // %d\n", rect.x(), rect.y(),
            rect.width(), rect.height(), i);
    }
    DUMP_NAV_LOGD("// };\n");
    DUMP_NAV_LOGD("// void* mNode=%p; // (%d) \n", b->mNode, mNodeIndex);
    DUMP_NAV_LOGD("// void* mParentGroup=%p; // (%d) \n", b->mParentGroup, mParentGroupIndex);
    DUMP_NAV_LOGD("// int mDataIndex=%d;\n", b->mDataIndex);
    DUMP_NAV_LOGD("// int mIndex=%d;\n", b->mIndex);
    DUMP_NAV_LOGD("// int navableRects()=%d;\n", b->navableRects());
    DUMP_NAV_LOGD("// int mParentIndex=%d;\n", b->mParentIndex);
    DUMP_NAV_LOGD("// int mTabIndex=%d;\n", b->mTabIndex);
    DUMP_NAV_LOGD("// int mColorIndex=%d;\n", b->mColorIndex);
    DUMP_NAV_LOGD("// Condition mCondition=%s;\n", condition(b->mCondition));
    DUMP_NAV_LOGD("// Type mType=%s;\n", type(b->mType));
    DEBUG_PRINT_BOOL(mClippedOut);
    DEBUG_PRINT_BOOL(mDisabled);
    DEBUG_PRINT_BOOL(mFixedUpCursorRects);
    DEBUG_PRINT_BOOL(mHasCursorRing);
    DEBUG_PRINT_BOOL(mHasMouseOver);
    DEBUG_PRINT_BOOL(mIsCursor);
    DEBUG_PRINT_BOOL(mIsFocus);
    DEBUG_PRINT_BOOL(mIsHidden);
    DEBUG_PRINT_BOOL(mIsInLayer);
    DEBUG_PRINT_BOOL(mIsParentAnchor);
    DEBUG_PRINT_BOOL(mIsTransparent);
    DEBUG_PRINT_BOOL(mIsUnclipped);
    DEBUG_PRINT_BOOL(mLast);
    DEBUG_PRINT_BOOL(mUseBounds);
    DEBUG_PRINT_BOOL(mUseHitBounds);
    DEBUG_PRINT_BOOL(mSingleImage);
}

#endif

}
