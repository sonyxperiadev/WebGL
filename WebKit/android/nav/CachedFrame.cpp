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
#include "CachedHistory.h"
#include "CachedNode.h"
#include "CachedRoot.h"
#include "LayerAndroid.h"

#include "CachedFrame.h"

#define OFFSETOF(type, field) ((char*)&(((type*)1)->field) - (char*)1) // avoids gnu warning

#define MIN_OVERLAP 3 // if rects overlap by 2 pixels or fewer, treat them as non-intersecting

namespace android {

WebCore::IntRect CachedFrame::adjustBounds(const CachedNode* node,
    const WebCore::IntRect& rect) const
{
    DBG_NAV_LOGV("node=%p [%d] rect=(%d,%d,w=%d,h=%d) view=(%d,%d,w=%d,h=%d)"
        " local=(%d,%d,w=%d,h=%d) root=(%d,%d,w=%d,h=%d)",
        node, node->index(), rect.x(), rect.y(), rect.width(), rect.height(),
        mViewBounds.x(), mViewBounds.y(),
        mViewBounds.width(), mViewBounds.height(),
        mLocalViewBounds.x(), mLocalViewBounds.y(),
        mLocalViewBounds.width(), mLocalViewBounds.height(),
        mRoot->mViewBounds.x(), mRoot->mViewBounds.y(),
        mRoot->mViewBounds.width(), mRoot->mViewBounds.height());
#if USE(ACCELERATED_COMPOSITING)
    if (!mRoot)
        return rect;

    const CachedLayer* cachedLayer = layer(node);
    if (!cachedLayer)
        return rect;

    const WebCore::LayerAndroid* rootLayer = mRoot->rootLayer();
    const LayerAndroid* aLayer = cachedLayer->layer(rootLayer);
    if (aLayer)
        return cachedLayer->adjustBounds(rootLayer, rect);
#endif
    return rect;
}

bool CachedFrame::CheckBetween(Direction direction, const WebCore::IntRect& bestRect,
        const WebCore::IntRect& prior, WebCore::IntRect* result)
{
    int left, top, width, height;
    if (direction & UP_DOWN) {
        top = direction == UP ? bestRect.bottom() : prior.bottom();
        int bottom = direction == UP ? prior.y() : bestRect.y();
        height = bottom - top;
        if (height < 0)
            return false;
        left = prior.x();
        int testLeft = bestRect.x();
        if (left > testLeft)
            left = testLeft;
        int right = prior.right();
        int testRight = bestRect.right();
        if (right < testRight)
            right = testRight;
        width = right - left;
    } else {
        left = direction == LEFT ? bestRect.right() : prior.right();
        int right = direction == LEFT ? prior.x() : bestRect.x();
        width = right - left;
        if (width < 0)
            return false;
        top = prior.y();
        int testTop = bestRect.y();
        if (top > testTop)
            top = testTop;
        int bottom = prior.bottom();
        int testBottom = bestRect.bottom();
        if (bottom < testBottom)
            bottom = testBottom;
        height = bottom - top;
    }
    *result = WebCore::IntRect(left, top, width, height);
    return true;
}

bool CachedFrame::checkBetween(BestData* best, Direction direction)
{
    const WebCore::IntRect& bestRect = best->bounds();
    BestData test;
    test.mDistance = INT_MAX;
    test.mNode = NULL;
    int index = direction;
    int limit = index + DIRECTION_COUNT;
    do {
        WebCore::IntRect edges;
        Direction check = (Direction) (index & DIRECTION_MASK);
        if (CheckBetween(check, bestRect,
                history()->priorBounds(), &edges) == false)
            continue;
        WebCore::IntRect clip = mRoot->scrolledBounds();
        clip.intersect(edges);
        if (clip.isEmpty())
            continue;
        findClosest(&test, direction, check, &clip);
        if (test.mNode == NULL)
            continue;
        if (direction == check)
            break;
    } while (++index < limit);
    if (test.mNode == NULL)
        return false;
    *best = test;
    return true;
}

bool CachedFrame::checkRings(const CachedNode* node,
        const WebCore::IntRect& testBounds) const
{
    return mRoot->checkRings(picture(node), node, testBounds);
}

bool CachedFrame::checkVisited(const CachedNode* node, Direction direction) const
{
    return history()->checkVisited(node, direction);
}

void CachedFrame::clearCursor()
{
    DBG_NAV_LOGD("mCursorIndex=%d", mCursorIndex);
    if (mCursorIndex < CURSOR_SET)
        return;
    CachedNode& cursor = mCachedNodes[mCursorIndex];
    cursor.clearCursor(this);
    mCursorIndex = CURSOR_CLEARED; // initialized and explicitly cleared
}

// returns 0 if test is preferable to best, 1 if not preferable, or -1 if unknown
int CachedFrame::compare(BestData& testData, const BestData& bestData) const
{
    if (testData.mNode->tabIndex() != bestData.mNode->tabIndex()) {
        if (testData.mNode->tabIndex() < bestData.mNode->tabIndex()
                || (mRoot->mCursor && mRoot->mCursor->tabIndex() < bestData.mNode->tabIndex())) {
            testData.mNode->setCondition(CachedNode::HIGHER_TAB_INDEX);
            return REJECT_TEST;
        }
        return TEST_IS_BEST;
    }
    // if the test minor axis line intersects the line segment between cursor
    // center and best center, choose it
    // give more weight to exact major axis alignment (rows, columns)
    if (testData.mInNav != bestData.mInNav) {
        if (bestData.mInNav) {
            testData.mNode->setCondition(CachedNode::IN_CURSOR);
            return REJECT_TEST;
        }
        return TEST_IS_BEST;
    }
    if (testData.mInNav) {
        if (bestData.mMajorDelta < testData.mMajorDelta) {
            testData.mNode->setCondition(CachedNode::CLOSER_IN_CURSOR);
            return REJECT_TEST;
        }
        if (testData.mMajorDelta < bestData.mMajorDelta)
            return TEST_IS_BEST;
    }
    if (testData.mMajorDelta < 0 && bestData.mMajorDelta >= 0) {
        testData.mNode->setCondition(CachedNode::FURTHER);
        return REJECT_TEST;
    }
    if ((testData.mMajorDelta ^ bestData.mMajorDelta) < 0) // one above, one below (or one left, one right)
        return TEST_IS_BEST;
    bool bestInWorking = bestData.inOrSubsumesWorking();
    bool testInWorking = testData.inOrSubsumesWorking();
    if (bestInWorking && testData.mWorkingOutside && testData.mNavOutside) {
        testData.mNode->setCondition(CachedNode::IN_WORKING);
        return REJECT_TEST;
    }
    if (testInWorking && bestData.mWorkingOutside && bestData.mNavOutside)
        return TEST_IS_BEST;
    bool bestInNav = directionChange() && bestData.inOrSubsumesNav();
    bool testInNav = directionChange() && testData.inOrSubsumesNav();
    if (bestInWorking == false && testInWorking == false) {
        if (bestInNav && testData.mNavOutside) {
            testData.mNode->setCondition(CachedNode::IN_UMBRA);
            return REJECT_TEST;
        }
        if (testInNav && bestData.mNavOutside)
            return TEST_IS_BEST;
    }
#if 01 // hopefully butt test will remove need for this
    if (testData.mCursorChild != bestData.mCursorChild) {
        if (bestData.mCursorChild) {
            testData.mNode->setCondition(CachedNode::IN_CURSOR_CHILDREN);
            return REJECT_TEST;
        }
        return TEST_IS_BEST;
    }
#endif
    bool bestTestIn = (bestInWorking || bestInNav) && (testInWorking || testInNav);
    bool testOverlap = bestTestIn || (testData.mWorkingOverlap != 0 && bestData.mWorkingOverlap == 0);
    bool bestOverlap = bestTestIn || (testData.mWorkingOverlap == 0 && bestData.mWorkingOverlap != 0);
#if 01 // this isn't working?
    if (testOverlap == bestOverlap) {
        if (bestData.mMajorButt < 10 && testData.mMajorButt >= 40) {
            testData.mNode->setCondition(CachedNode::BUTTED_UP);
            return REJECT_TEST;
        }
        if (testData.mMajorButt < 10 && bestData.mMajorButt >= 40)
            return TEST_IS_BEST;
    }
#endif
    if (bestOverlap && bestData.mMajorDelta < testData.mMajorDelta) { // choose closest major axis center
        testData.mNode->setCondition(CachedNode::CLOSER);
        return REJECT_TEST;
    }
    if (testOverlap && testData.mMajorDelta < bestData.mMajorDelta)
        return TEST_IS_BEST;
    if (bestOverlap && bestData.mMajorDelta2 < testData.mMajorDelta2) {
        testData.mNode->setCondition(CachedNode::CLOSER_TOP);
        return REJECT_TEST;
    }
    if (testOverlap && testData.mMajorDelta2 < bestData.mMajorDelta2)
        return TEST_IS_BEST;
#if 01
    if (bestOverlap && ((bestData.mSideDistance <= 0 && testData.mSideDistance > 0) ||
            abs(bestData.mSideDistance) < abs(testData.mSideDistance))) {
        testData.mNode->setCondition(CachedNode::LEFTMOST);
        return REJECT_TEST;
    }
    if (testOverlap && ((testData.mSideDistance <= 0 && bestData.mSideDistance > 0) ||
            abs(testData.mSideDistance) < abs(bestData.mSideDistance)))
        return TEST_IS_BEST;
// fix me : the following ASSERT fires -- not sure if this case should be handled or not
//    ASSERT(bestOverlap == false && testOverlap == false);
#endif
    SkFixed testMultiplier = testData.mWorkingOverlap > testData.mNavOverlap ?
        testData.mWorkingOverlap : testData.mNavOverlap;
    SkFixed bestMultiplier = bestData.mWorkingOverlap > bestData.mNavOverlap ?
        bestData.mWorkingOverlap : bestData.mNavOverlap;
    int testDistance = testData.mDistance;
    int bestDistance = bestData.mDistance;
//    start here;
    // this fails if they're off by 1
    // try once again to implement sliding scale so that off by 1 is nearly like zero,
    // and off by a lot causes sideDistance to have little or no effect
        // try elliptical distance -- lengthen side contribution
        // these ASSERTs should not fire, but do fire on mail.google.com
        // can't debug yet, won't reproduce
    ASSERT(testDistance >= 0);
    ASSERT(bestDistance >= 0);
    testDistance += testDistance; // multiply by 2
    testDistance *= testDistance;
    bestDistance += bestDistance; // multiply by 2
    bestDistance *= bestDistance;
    int side = testData.mSideDistance;
    int negative = side < 0 && bestData.mSideDistance > 0;
    side *= side;
    if (negative)
        side = -side;
    testDistance += side;
    side = bestData.mSideDistance;
    negative = side < 0 && testData.mSideDistance > 0;
    side *= side;
    if (negative)
        side = -side;
    bestDistance += side;
    if (testMultiplier > (SK_Fixed1 >> 1) || bestMultiplier > (SK_Fixed1 >> 1)) { // considerable working overlap?
       testDistance = SkFixedMul(testDistance, bestMultiplier);
       bestDistance = SkFixedMul(bestDistance, testMultiplier);
    }
    if (bestDistance < testDistance) {
        testData.mNode->setCondition(CachedNode::CLOSER_OVERLAP);
        return REJECT_TEST;
    }
    if (testDistance < bestDistance)
        return TEST_IS_BEST;
#if 0
    int distance = testData.mDistance + testData.mSideDistance;
    int best = bestData.mDistance + bestData.mSideDistance;
    if (distance > best) {
        testData.mNode->setCondition(CachedNode::CLOSER_RAW_DISTANCE);
        return REJECT_TEST;
    }
    else if (distance < best)
        return TEST_IS_BEST;
    best = bestData.mSideDistance;
    if (testData.mSideDistance > best) {
        testData.mNode->setCondition(CachedNode::SIDE_DISTANCE);
        return REJECT_TEST;
    }
    if (testData.mSideDistance < best)
        return TEST_IS_BEST;
#endif
    if (testData.mPreferred < bestData.mPreferred) {
        testData.mNode->setCondition(CachedNode::PREFERRED);
        return REJECT_TEST;
    }
    if (testData.mPreferred > bestData.mPreferred)
        return TEST_IS_BEST;
    return UNDECIDED;
}

const CachedNode* CachedFrame::currentCursor(const CachedFrame** framePtr) const
{
    if (framePtr)
        *framePtr = this;
    if (mCursorIndex < CURSOR_SET)
        return NULL;
    const CachedNode* result = &mCachedNodes[mCursorIndex];
    const CachedFrame* frame = hasFrame(result);
    if (frame != NULL)
        return frame->currentCursor(framePtr);
    (const_cast<CachedNode*>(result))->fixUpCursorRects(this);
    return result;
}

const CachedNode* CachedFrame::currentFocus(const CachedFrame** framePtr) const
{
    if (framePtr)
        *framePtr = this;
    if (mFocusIndex < 0)
        return NULL;
    const CachedNode* result = &mCachedNodes[mFocusIndex];
    const CachedFrame* frame = hasFrame(result);
    if (frame != NULL)
        return frame->currentFocus(framePtr);
    return result;
}

bool CachedFrame::directionChange() const
{
    return history()->directionChange();
}

#ifdef BROWSER_DEBUG
CachedNode* CachedFrame::find(WebCore::Node* node) // !!! probably debugging only
{
    for (CachedNode* test = mCachedNodes.begin(); test != mCachedNodes.end(); test++)
        if (node == test->webCoreNode())
            return test;
    for (CachedFrame* frame = mCachedFrames.begin(); frame != mCachedFrames.end();
            frame++) {
        CachedNode* result = frame->find(node);
        if (result != NULL)
            return result;
    }
    return NULL;
}
#endif

const CachedNode* CachedFrame::findBestAt(const WebCore::IntRect& rect,
    int* best, bool* inside, const CachedNode** directHit,
    const CachedFrame** directHitFramePtr,
    const CachedFrame** framePtr, int* x, int* y,
    bool checkForHiddenStart) const
{
    const CachedNode* result = NULL;
    int rectWidth = rect.width();
    WebCore::IntPoint center = WebCore::IntPoint(rect.x() + (rectWidth >> 1),
        rect.y() + (rect.height() >> 1));
    mRoot->setupScrolledBounds();
    for (const CachedNode* test = mCachedNodes.begin(); test != mCachedNodes.end(); test++) {
        if (test->disabled())
            continue;
        size_t parts = test->navableRects();
        BestData testData;
        testData.mNode = test;
        testData.mFrame = this;
        WebCore::IntRect bounds = test->bounds(this);
        testData.setMouseBounds(bounds);
        testData.setNodeBounds(bounds);
        bool checkForHidden = checkForHiddenStart;
        for (size_t part = 0; part < parts; part++) {
            WebCore::IntRect testRect = test->ring(this, part);
            if (testRect.intersects(rect)) {
#if DEBUG_NAV_UI
                if (test->isInLayer()) {
                    DBG_NAV_LOGD("[%d] intersects=%s testRect=(%d,%d,w=%d,h=%d)"
                        " rect=(%d,%d,w=%d,h=%d)", test->index(),
                        testRect.intersects(rect) ? "true" : "false",
                        testRect.x(), testRect.y(),
                        testRect.width(), testRect.height(),
                        rect.x(), rect.y(), rect.width(), rect.height());
                }
#endif
                if (checkForHidden && mRoot->maskIfHidden(&testData) == true) {
                    DBG_NAV_LOGD("hidden [%d]", test->index());
                    break;
                }
                checkForHidden = false;
                testRect.intersect(testData.mouseBounds());
                if (testRect.contains(center)) {
                    // We have a direct hit.
                    if (*directHit == NULL) {
                        DBG_NAV_LOGD("direct hit 1 [%d]", test->index());
                        *directHit = test;
                        *directHitFramePtr = this;
                        IntRect r(center, IntSize(0, 0));
                        *x = r.x();
                        *y = r.y();
                    } else {
                        DBG_NAV_LOGD("direct hit 2 [%d]", test->index());
                        // We have hit another one before
                        const CachedNode* d = *directHit;
                        if (d->bounds(this).contains(testRect)) {
                            // This rectangle is inside the other one, so it is
                            // the best one.
                            *directHit = test;
                            *directHitFramePtr = this;
                        }
                    }
                }
                if (NULL != *directHit) {
                    // If we have a direct hit already, there is no need to
                    // calculate the distances, or check the other parts
                    break;
                }
                DBG_NAV_LOGD("indirect hit [%d]", test->index());
                WebCore::IntRect both = rect;
                int smaller = testRect.width() < testRect.height() ?
                    testRect.width() : testRect.height();
                smaller -= rectWidth;
                int inset = smaller < rectWidth ? smaller : rectWidth;
                inset >>= 1; // inflate doubles the width decrease
                if (inset > 1)
                    both.inflate(1 - inset);
                both.intersect(testRect);
                if (both.isEmpty())
                    continue;
                bool testInside = testRect.contains(center);
                if (*inside && !testInside)
                    continue;
                WebCore::IntPoint testCenter = WebCore::IntPoint(testRect.x() +
                    (testRect.width() >> 1), testRect.y() + (testRect.height() >> 1));
                int dx = testCenter.x() - center.x();
                int dy = testCenter.y() - center.y();
                int distance = dx * dx + dy * dy;
                if ((!*inside && testInside) || *best >= distance) {
                    *best = distance;
                    *inside = testInside;
                    result = test;
                    *framePtr = this;
                    *x = both.x() + (both.width() >> 1);
                    *y = both.y() + (both.height() >> 1);
                }
            }
        }
    }
    for (const CachedFrame* frame = mCachedFrames.begin();
            frame != mCachedFrames.end(); frame++) {
        const CachedNode* frameResult = frame->findBestAt(rect, best, inside,
            directHit, directHitFramePtr, framePtr, x, y, checkForHiddenStart);
        if (NULL != frameResult)
            result = frameResult;
    }
    if (NULL != *directHit) {
        result = *directHit;
        *framePtr = *directHitFramePtr;
    }
    return result;
}

const CachedFrame* CachedFrame::findBestFrameAt(int x, int y) const
{
    if (mLocalViewBounds.contains(x, y) == false)
        return NULL;
    const CachedFrame* result = this;
    for (const CachedFrame* frame = mCachedFrames.begin();
            frame != mCachedFrames.end(); frame++) {
        const CachedFrame* frameResult = frame->findBestFrameAt(x, y);
        if (NULL != frameResult)
            result = frameResult;
    }
    return result;
}

const CachedNode* CachedFrame::findBestHitAt(const WebCore::IntRect& rect,
    const CachedFrame** framePtr, int* x, int* y) const
{
    mRoot->setupScrolledBounds();
    for (const CachedFrame* frame = mCachedFrames.end() - 1;
            frame != mCachedFrames.begin() - 1; frame--) {
        const CachedNode* frameResult = frame->findBestHitAt(rect,
            framePtr, x, y);
        if (NULL != frameResult)
            return frameResult;
    }
    for (const CachedNode* test = mCachedNodes.end() - 1;
            test != mCachedNodes.begin() - 1; test--) {
        if (test->disabled())
            continue;
        WebCore::IntRect testRect = test->hitBounds(this);
        if (testRect.intersects(rect) == false)
            continue;
        BestData testData;
        testData.mNode = test;
        testData.mFrame = this;
        testData.setMouseBounds(testRect);
        testData.setNodeBounds(testRect);
        if (mRoot->maskIfHidden(&testData) == true)
            continue;
        DBG_NAV_LOGD("candidate %d rect=(%d,%d,r=%d,b=%d)"
            " testRect=(%d,%d,r=%d,b=%d)",
            test->index(), rect.x(), rect.y(), rect.right(), rect.bottom(),
            testRect.x(), testRect.y(), testRect.right(), testRect.bottom());
        for (int i = 0; i < test->navableRects(); i++) {
            WebCore::IntRect cursorRect = test->ring(this, i);
            DBG_NAV_LOGD("candidate %d cursorRect=(%d,%d,r=%d,b=%d)",
                i, cursorRect.x(), cursorRect.y(), cursorRect.right(),
                cursorRect.bottom());
            if (cursorRect.intersects(rect)) {
                WebCore::IntRect intersection(cursorRect);
                intersection.intersect(rect);
                *x = intersection.x() + (intersection.width() >> 1);
                *y = intersection.y() + (intersection.height() >> 1);
                *framePtr = this;
                return test;
            }
        }
        testRect.intersect(rect);
        *x = testRect.x() + (testRect.width() >> 1);
        *y = testRect.y() + (testRect.height() >> 1);
        *framePtr = this;
        return test;
    }
    return NULL;
}

void CachedFrame::findClosest(BestData* bestData, Direction originalDirection,
    Direction direction, WebCore::IntRect* clip) const
{
    const CachedNode* test = mCachedNodes.begin();
    while ((test = test->traverseNextNode()) != NULL) {
        const CachedFrame* child = hasFrame(test);
        if (child != NULL) {
            const CachedNode* childDoc = child->validDocument();
            if (childDoc == NULL)
                continue;
            child->findClosest(bestData, originalDirection, direction, clip);
        }
        if (test->noSecondChance())
            continue;
        if (test->isNavable(this, *clip) == false)
            continue;
        if (checkVisited(test, originalDirection) == false)
            continue;
        size_t partMax = test->navableRects();
        for (size_t part = 0; part < partMax; part++) {
            WebCore::IntRect testBounds = test->ring(this, part);
            if (clip->intersects(testBounds) == false)
                continue;
            if (clip->contains(testBounds) == false) {
                if (direction & UP_DOWN) {
//                    if (testBounds.x() > clip->x() || testBounds.right() < clip->right())
//                        continue;
                    testBounds.setX(clip->x());
                    testBounds.setWidth(clip->width());
                } else {
//                    if (testBounds.y() > clip->y() || testBounds.bottom() < clip->bottom())
//                        continue;
                    testBounds.setY(clip->y());
                    testBounds.setHeight(clip->height());
                }
                if (clip->contains(testBounds) == false)
                    continue;
            }
            int distance;
            // seems like distance for UP for instance needs to be 'test top closest to
            // clip bottom' -- keep the old code but try this instead
            switch (direction) {
#if 0
                case LEFT: distance = testBounds.x() - clip->x(); break;
                case RIGHT: distance = clip->right() - testBounds.right(); break;
                case UP: distance = testBounds.y() - clip->y(); break;
                case DOWN: distance = clip->bottom() - testBounds.bottom(); break;
#else
                case LEFT: distance = clip->right() - testBounds.x(); break;
                case RIGHT: distance = testBounds.right()  - clip->x(); break;
                case UP: distance = clip->bottom() - testBounds.y(); break;
                case DOWN: distance = testBounds.bottom() - clip->y(); break;
#endif
                default:
                    distance = 0; ASSERT(0);
            }
            if (distance < bestData->mDistance) {
                bestData->mNode = test;
                bestData->mFrame = this;
                bestData->mDistance = distance;
                WebCore::IntRect rect = test->ring(this, part);
                bestData->setMouseBounds(rect);
                bestData->setNodeBounds(rect);
                CachedHistory* cachedHistory = history();
                switch (direction) {
                    case LEFT:
                        bestData->setLeftDirection(cachedHistory);
                    break;
                    case RIGHT:
                        bestData->setRightDirection(cachedHistory);
                    break;
                    case UP:
                        bestData->setUpDirection(cachedHistory);
                    break;
                    case DOWN:
                        bestData->setDownDirection(cachedHistory);
                    break;
                    default:
                        ASSERT(0);
                }
            }
        }
    }
}

void CachedFrame::finishInit()
{
    CachedNode* lastCached = lastNode();
    lastCached->setLast();
    CachedFrame* child = mCachedFrames.begin();
    while (child != mCachedFrames.end()) {
        child->mParent = this;
        child->finishInit();
        child++;
    }
    CachedFrame* frameParent;
    if (mFocusIndex >= 0 && (frameParent = parent()))
        frameParent->setFocusIndex(indexInParent());
}

const CachedNode* CachedFrame::frameDown(const CachedNode* test,
    const CachedNode* limit, BestData* bestData) const
{
    BestData originalData = *bestData;
    do {
        if (moveInFrame(&CachedFrame::frameDown, test, bestData))
            continue;
        BestData testData;
        if (frameNodeCommon(testData, test, bestData, &originalData) == REJECT_TEST)
            continue;
        if (checkVisited(test, DOWN) == false)
            continue;
        size_t parts = test->navableRects();
        for (size_t part = 0; part < parts; part++) {
            testData.setNodeBounds(test->ring(this, part));
            if (testData.setDownDirection(history()))
                continue;
            int result = framePartCommon(testData, test, bestData);
            if (result == REJECT_TEST)
                continue;
            if (result == 0 && limit == NULL) { // retry all data up to this point, since smaller may have replaced node preferable to larger
                BestData innerData = testData;
                frameDown(document(), test, &innerData);
                if (checkVisited(innerData.mNode, DOWN)) {
                    *bestData = innerData;
                    continue;
                }
            }
            if (checkVisited(test, DOWN))
                *bestData = testData;
        }
    } while ((test = test->traverseNextNode()) != limit);
    ASSERT(mRoot->mCursor == NULL || bestData->mNode != mRoot->mCursor);
    // does the best contain something (or, is it contained by an area which is not the cursor?)
        // if so, is the conainer/containee should have been chosen, but wasn't -- so there's a better choice
        // in the doc list prior to this choice
    //
    return bestData->mNode;
}

const CachedNode* CachedFrame::frameLeft(const CachedNode* test,
    const CachedNode* limit, BestData* bestData) const
{
    BestData originalData = *bestData;
    do {
        if (moveInFrame(&CachedFrame::frameLeft, test, bestData))
            continue;
        BestData testData;
        if (frameNodeCommon(testData, test, bestData, &originalData) == REJECT_TEST)
            continue;
        if (checkVisited(test, LEFT) == false)
            continue;
        size_t parts = test->navableRects();
        for (size_t part = 0; part < parts; part++) {
            testData.setNodeBounds(test->ring(this, part));
            if (testData.setLeftDirection(history()))
                continue;
            int result = framePartCommon(testData, test, bestData);
            if (result == REJECT_TEST)
                continue;
            if (result == 0 && limit == NULL) { // retry all data up to this point, since smaller may have replaced node preferable to larger
                BestData innerData = testData;
                frameLeft(document(), test, &innerData);
                if (checkVisited(innerData.mNode, LEFT)) {
                    *bestData = innerData;
                    continue;
                }
            }
            if (checkVisited(test, LEFT))
                *bestData = testData;
        }
    } while ((test = test->traverseNextNode()) != limit);  // FIXME ??? left and up should use traversePreviousNode to choose reverse document order
    ASSERT(mRoot->mCursor == NULL || bestData->mNode != mRoot->mCursor);
    return bestData->mNode;
}

int CachedFrame::frameNodeCommon(BestData& testData, const CachedNode* test,
    BestData* bestData, BestData* originalData) const
{
    testData.mFrame = this;
    testData.mNode = test;
    test->clearCondition();
    if (test->disabled()) {
        testData.mNode->setCondition(CachedNode::DISABLED);
        return REJECT_TEST;
    }
    WebCore::IntRect bounds = test->bounds(this);
    if (bounds.isEmpty()) {
        testData.mNode->setCondition(CachedNode::NAVABLE);
        return REJECT_TEST;
    }
    if (mRoot->scrolledBounds().intersects(bounds) == false) {
        testData.mNode->setCondition(CachedNode::NAVABLE);
        return REJECT_TEST;
    }
    if (mRoot->rootLayer() && !test->isInLayer()
            && !mRoot->baseUncovered().intersects(bounds)) {
        testData.mNode->setCondition(CachedNode::UNDER_LAYER);
        return REJECT_TEST;
    }
//    if (isNavable(test, &testData.mNodeBounds, walk) == false) {
//        testData.mNode->setCondition(CachedNode::NAVABLE);
//        return REJECT_TEST;
//    }
//
    if (test == mRoot->mCursor) {
        testData.mNode->setCondition(CachedNode::NOT_CURSOR_NODE);
        return REJECT_TEST;
    }
//    if (test->bounds().contains(mRoot->mCursorBounds)) {
//        testData.mNode->setCondition(CachedNode::NOT_ENCLOSING_CURSOR);
//        return REJECT_TEST;
//    }
    void* par = mRoot->mCursor ? mRoot->mCursor->parentGroup() : NULL;
    testData.mCursorChild = par ? test->parentGroup() == par : false;
    if (bestData->mNode == NULL)
        return TEST_IS_BEST;
    if (mRoot->mCursor && testData.mNode->parentIndex() != bestData->mNode->parentIndex()) {
        int cursorParentIndex = mRoot->mCursor->parentIndex();
        if (cursorParentIndex >= 0) {
            if (bestData->mNode->parentIndex() == cursorParentIndex)
                return REJECT_TEST;
            if (testData.mNode->parentIndex() == cursorParentIndex)
                return TEST_IS_BEST;
        }
    }
    if (testData.mNode->parent() == bestData->mNode) {
        testData.mNode->setCondition(CachedNode::CHILD);
        return REJECT_TEST;
    }
    if (testData.mNode == bestData->mNode->parent())
        return TEST_IS_BEST;
    int testInBest = testData.isContainer(bestData); /* -1 pick best over test, 0 no containership, 1 pick test over best */
    if (testInBest == 1) {
        if (test->isArea() || bestData->mNode->isArea())
            return UNDECIDED;
        bestData->mNode = NULL;    // force part tests to be ignored, yet still set up remaining test data for later comparisons
        return TEST_IS_BEST;
    }
    if (testInBest == -1) {
        testData.mNode->setCondition(CachedNode::OUTSIDE_OF_BEST);
        return REJECT_TEST;
    }
    if (originalData->mNode != NULL) { // test is best case
        testInBest = testData.isContainer(originalData);
        if (testInBest == -1) { /* test is inside best */
            testData.mNode->setCondition(CachedNode::OUTSIDE_OF_ORIGINAL);
            return REJECT_TEST;
        }
    }
    return UNDECIDED;
}

int CachedFrame::framePartCommon(BestData& testData,
    const CachedNode* test, BestData* bestData) const
{
    if (mRoot->mCursor
            && testData.bounds().contains(mRoot->mCursorBounds)
            && !test->wantsKeyEvents()) {
        testData.mNode->setCondition(CachedNode::NOT_ENCLOSING_CURSOR);
        return REJECT_TEST;
    }
    testData.setDistances();
    if (bestData->mNode != NULL) {
        int compared = compare(testData, *bestData);
        if (compared == 0 && test->isArea() == false && bestData->mNode->isArea() == false)
            goto pickTest;
        if (compared >= 0)
            return compared;
    }
pickTest:
    return -1; // pick test
}

const CachedNode* CachedFrame::frameRight(const CachedNode* test,
    const CachedNode* limit, BestData* bestData) const
{
    BestData originalData = *bestData;
    do {
        if (moveInFrame(&CachedFrame::frameRight, test, bestData))
            continue;
        BestData testData;
        if (frameNodeCommon(testData, test, bestData, &originalData) == REJECT_TEST)
            continue;
        if (checkVisited(test, RIGHT) == false)
            continue;
        size_t parts = test->navableRects();
        for (size_t part = 0; part < parts; part++) {
            testData.setNodeBounds(test->ring(this, part));
            if (testData.setRightDirection(history()))
                continue;
            int result = framePartCommon(testData, test, bestData);
            if (result == REJECT_TEST)
                continue;
            if (result == 0 && limit == NULL) { // retry all data up to this point, since smaller may have replaced node preferable to larger
                BestData innerData = testData;
                frameRight(document(), test, &innerData);
                if (checkVisited(innerData.mNode, RIGHT)) {
                    *bestData = innerData;
                    continue;
                }
            }
            if (checkVisited(test, RIGHT))
                *bestData = testData;
        }
    } while ((test = test->traverseNextNode()) != limit);
    ASSERT(mRoot->mCursor == NULL || bestData->mNode != mRoot->mCursor);
    return bestData->mNode;
}

const CachedNode* CachedFrame::frameUp(const CachedNode* test,
    const CachedNode* limit, BestData* bestData) const
{
    BestData originalData = *bestData;
    do {
        if (moveInFrame(&CachedFrame::frameUp, test, bestData))
            continue;
        BestData testData;
        if (frameNodeCommon(testData, test, bestData, &originalData) == REJECT_TEST)
            continue;
        if (checkVisited(test, UP) == false)
            continue;
        size_t parts = test->navableRects();
        for (size_t part = 0; part < parts; part++) {
            testData.setNodeBounds(test->ring(this, part));
            if (testData.setUpDirection(history()))
                continue;
            int result = framePartCommon(testData, test, bestData);
            if (result == REJECT_TEST)
                continue;
            if (result == 0 && limit == NULL) { // retry all data up to this point, since smaller may have replaced node preferable to larger
                BestData innerData = testData;
                frameUp(document(), test, &innerData);
                if (checkVisited(innerData.mNode, UP)) {
                    *bestData = innerData;
                    continue;
                }
            }
            if (checkVisited(test, UP))
                *bestData = testData;
        }
    } while ((test = test->traverseNextNode()) != limit);  // FIXME ??? left and up should use traversePreviousNode to choose reverse document order
    ASSERT(mRoot->mCursor == NULL || bestData->mNode != mRoot->mCursor);
    return bestData->mNode;
}

CachedFrame* CachedFrame::hasFrame(const CachedNode* node)
{
    return node->isFrame() ? &mCachedFrames[node->childFrameIndex()] : NULL;
}

void CachedFrame::hideCursor()
{
    DBG_NAV_LOGD("mCursorIndex=%d", mCursorIndex);
    if (mCursorIndex < CURSOR_SET)
        return;
    CachedNode& cursor = mCachedNodes[mCursorIndex];
    cursor.hideCursor(this);
}

CachedHistory* CachedFrame::history() const
{
    return mRoot->rootHistory();
}

void CachedFrame::init(const CachedRoot* root, int childFrameIndex,
    WebCore::Frame* frame)
{
    mContents = WebCore::IntRect(0, 0, 0, 0); // fixed up for real in setData()
    mLocalViewBounds = WebCore::IntRect(0, 0, 0, 0);
    mViewBounds = WebCore::IntRect(0, 0, 0, 0);
    mRoot = root;
    mCursorIndex = CURSOR_UNINITIALIZED; // not explicitly cleared
    mFocusIndex = -1;
    mFrame = frame;
    mParent = NULL; // set up parents after stretchy arrays are set up
    mIndexInParent = childFrameIndex;
}

#if USE(ACCELERATED_COMPOSITING)
const CachedLayer* CachedFrame::layer(const CachedNode* node) const
{
    if (!node->isInLayer())
        return 0;
    CachedLayer test;
    test.setCachedNodeIndex(node->index());
    return std::lower_bound(mCachedLayers.begin(), mCachedLayers.end(), test);
}
#endif

WebCore::IntRect CachedFrame::localBounds(const CachedNode* node,
    const WebCore::IntRect& rect) const
{
    DBG_NAV_LOGD("node=%p [%d] rect=(%d,%d,w=%d,h=%d)",
        node, node->index(), rect.x(), rect.y(), rect.width(), rect.height());
#if USE(ACCELERATED_COMPOSITING)
    return layer(node)->localBounds(mRoot->rootLayer(), rect);
#else
    return rect;
#endif
}

int CachedFrame::minWorkingHorizontal() const
{
    return history()->minWorkingHorizontal();
}

int CachedFrame::minWorkingVertical() const
{
    return history()->minWorkingVertical();
}

int CachedFrame::maxWorkingHorizontal() const
{
    return history()->maxWorkingHorizontal();
}

int CachedFrame::maxWorkingVertical() const
{
    return history()->maxWorkingVertical();
}

const CachedNode* CachedFrame::nextTextField(const CachedNode* start,
        const CachedFrame** framePtr, bool* startFound) const
{
    const CachedNode* test = mCachedNodes.begin();
    while ((test = test->traverseNextNode())) {
        const CachedFrame* frame = hasFrame(test);
        if (frame) {
            if (!frame->validDocument())
                continue;
            const CachedNode* node
                    = frame->nextTextField(start, framePtr, startFound);
            if (node)
                return node;
        } else if (test->isTextInput()) {
            if (test == start)
                *startFound = true;
            else if (*startFound) {
                if (framePtr)
                    *framePtr = this;
                return test;
            }
        }
    }
    return 0;
}

bool CachedFrame::moveInFrame(MoveInDirection moveInDirection,
    const CachedNode* test, BestData* bestData) const
{
    const CachedFrame* frame = hasFrame(test);
    if (frame == NULL)
        return false; // if it's not a frame, let the caller have another swing at it
    const CachedNode* childDoc = frame->validDocument();
    if (childDoc == NULL)
        return true;
    (frame->*moveInDirection)(childDoc, NULL, bestData);
    return true;
}

const WebCore::IntRect& CachedFrame::_navBounds() const
{
    return history()->navBounds();
}

SkPicture* CachedFrame::picture(const CachedNode* node) const
{
#if USE(ACCELERATED_COMPOSITING)
    if (node->isInLayer())
        return layer(node)->picture(mRoot->rootLayer());
#endif
    return mRoot->mPicture;
}

SkPicture* CachedFrame::picture(const CachedNode* node, int* xPtr, int* yPtr) const
{
#if USE(ACCELERATED_COMPOSITING)
    if (node->isInLayer()) {
        const CachedLayer* cachedLayer = layer(node);
        const LayerAndroid* rootLayer = mRoot->rootLayer();
        cachedLayer->toLocal(rootLayer, xPtr, yPtr);
        return cachedLayer->picture(rootLayer);
    }
#endif
    return mRoot->mPicture;
}

void CachedFrame::resetClippedOut()
{
    for (CachedNode* test = mCachedNodes.begin(); test != mCachedNodes.end(); test++)
    {
        if (test->clippedOut()) {
            test->setDisabled(false);
            test->setClippedOut(false);
        }
    }
    for (CachedFrame* frame = mCachedFrames.begin(); frame != mCachedFrames.end();
            frame++) {
        frame->resetClippedOut();
    }
}

void CachedFrame::resetLayers()
{
#if USE(ACCELERATED_COMPOSITING)
    for (CachedFrame* frame = mCachedFrames.begin(); frame != mCachedFrames.end();
            frame++) {
        frame->resetLayers();
    }
#endif
}

bool CachedFrame::sameFrame(const CachedFrame* test) const
{
    ASSERT(test);
    if (mIndexInParent != test->mIndexInParent)
        return false;
    if (mIndexInParent == -1) // index within parent's array of children, or -1 if root
        return true;
    return mParent->sameFrame(test->mParent);
}

void CachedFrame::setData()
{
    if (this != mRoot) {
        mViewBounds = mLocalViewBounds;
        mViewBounds.intersect(mRoot->mViewBounds);
    }
    int x, y;
    if (parent() == NULL)
        x = y = 0;
    else {
        x = mLocalViewBounds.x();
        y = mLocalViewBounds.y();
    }
    mContents.setX(x);
    mContents.setY(y);
    CachedFrame* child = mCachedFrames.begin();
    while (child != mCachedFrames.end()) {
        child->setData();
        child++;
    }
}

bool CachedFrame::setCursor(WebCore::Frame* frame, WebCore::Node* node,
    int x, int y)
{
    if (NULL == node) {
        const_cast<CachedRoot*>(mRoot)->setCursor(NULL, NULL);
        return true;
    }
    if (mFrame != frame) {
        for (CachedFrame* testF = mCachedFrames.begin(); testF != mCachedFrames.end();
                testF++) {
            if (testF->setCursor(frame, node, x, y))
                return true;
        }
        DBG_NAV_LOGD("no frame frame=%p node=%p", frame, node);
        return false;
    }
    bool first = true;
    CachedNode const * const end = mCachedNodes.end();
    do {
        for (CachedNode* test = mCachedNodes.begin(); test != end; test++) {
            if (test->nodePointer() != node && first)
                continue;
            size_t partMax = test->navableRects();
            for (size_t part = 0; part < partMax; part++) {
                WebCore::IntRect testBounds = test->ring(this, part);
                if (testBounds.contains(x, y) == false)
                    continue;
                if (test->isCursor()) {
                    DBG_NAV_LOGD("already set? test=%d frame=%p node=%p x=%d y=%d",
                        test->index(), frame, node, x, y);
                    return false;
                }
                const_cast<CachedRoot*>(mRoot)->setCursor(this, test);
                return true;
            }
        }
        DBG_NAV_LOGD("moved? frame=%p node=%p x=%d y=%d", frame, node, x, y);
    } while ((first ^= true) == false);
failed:
    DBG_NAV_LOGD("no match frame=%p node=%p", frame, node);
    return false;
}

const CachedNode* CachedFrame::validDocument() const
{
    const CachedNode* doc = document();
    return doc != NULL && mViewBounds.isEmpty() == false ? doc : NULL;
}

bool CachedFrame::BestData::canBeReachedByAnotherDirection()
{
    if (mMajorButt > -MIN_OVERLAP)
        return false;
    mMajorButt = -mMajorButt;
    return mNavOutside;
}

int CachedFrame::BestData::isContainer(CachedFrame::BestData* other)
{
    int _x = x();
    int otherRight = other->right();
    if (_x >= otherRight)
        return 0; // does not intersect
    int _y = y();
    int otherBottom = other->bottom();
    if (_y >= otherBottom)
        return 0; // does not intersect
    int _right = right();
    int otherX = other->x();
    if (otherX >= _right)
        return 0; // does not intersect
    int _bottom = bottom();
    int otherY = other->y();
    if (otherY >= _bottom)
        return 0; // does not intersect
    int intoX = otherX - _x;
    int intoY = otherY - _y;
    int intoRight = otherRight - _right;
    int intoBottom = otherBottom - _bottom;
    bool contains = intoX >= 0 && intoY >= 0 && intoRight <= 0 && intoBottom <= 0;
    if (contains && mNode->partRectsContains(other->mNode)) {
//        if (mIsArea == false && hasMouseOver())
//            other->mMouseOver = mNode;
        return mNode->isArea() ? 1  : -1;
    }
    bool containedBy = intoX <= 0 && intoY <= 0 && intoRight >= 0 && intoBottom >= 0;
    if (containedBy && other->mNode->partRectsContains(mNode)) {
//        if (other->mIsArea == false && other->hasMouseOver())
//            mMouseOver = other->mNode;
        return other->mNode->isArea() ? -1  : 1;
    }
    return 0;
}

// distance scale factor factor as a 16.16 scalar
SkFixed CachedFrame::BestData::Overlap(int span, int left, int right)
{
    unsigned result;
    if (left > 0 && left < span && right > span)
        result = (unsigned) left;
    else if (right > 0 && right < span && left > span)
        result = (unsigned) right;
    else if (left > 0 && right > 0)
        return SK_Fixed1;
    else
        return 0;
    result = (result << 16) / (unsigned) span; // linear proportion, always less than fixed 1
    return (SkFixed) result;
// !!! experiment with weight -- enable if overlaps are preferred too much
// or reverse weighting if overlaps are preferred to little
//    return (SkFixed) (result * result >> 16); // but fall off with square
}

void CachedFrame::BestData::setDistances()
{
    mDistance = abs(mMajorDelta);
    int sideDistance = mWorkingDelta;
    if (mWorkingOverlap < SK_Fixed1) {
        if (mPreferred > 0)
            sideDistance = mWorkingDelta2;
    } else if (sideDistance >= 0 && mWorkingDelta2 >=- 0)
        sideDistance = 0;
    else {
        ASSERT(sideDistance <= 0 && mWorkingDelta2 <= 0);
        if (sideDistance < mWorkingDelta2)
            sideDistance = mWorkingDelta2;
    }
    // if overlap, smaller abs mWorkingDelta is better, smaller abs majorDelta is better
    // if not overlap, positive mWorkingDelta is better
    mSideDistance = sideDistance;
}

bool CachedFrame::BestData::setDownDirection(const CachedHistory* history)
{
    const WebCore::IntRect& navBounds = history->navBounds();
    mMajorButt = mNodeBounds.y() - navBounds.bottom();
    int testX = mNodeBounds.x();
    int testRight = mNodeBounds.right();
    setNavOverlap(navBounds.width(), navBounds.right() - testX,
        testRight - navBounds.x());
    if (canBeReachedByAnotherDirection()) {
        mNode->setCondition(CachedNode::BEST_DIRECTION);
        return REJECT_TEST;
    }
    int inNavTop = mNodeBounds.y() - navBounds.y();
    mMajorDelta2 = inNavTop;
    mMajorDelta = mMajorDelta2 + ((mNodeBounds.height() -
        navBounds.height()) >> 1);
    if (mMajorDelta2 <= 1 && mMajorDelta <= 1) {
        mNode->setCondition(CachedNode::CENTER_FURTHER); // never move up or sideways
        return REJECT_TEST;
    }
    int inNavBottom = navBounds.bottom() - mNodeBounds.bottom();
    setNavInclusion(testRight - navBounds.right(), navBounds.x() - testX);
    bool subsumes = navBounds.height() > 0 && inOrSubsumesNav();
    if (inNavTop <= 0 && inNavBottom <= 0 && subsumes && !mNode->wantsKeyEvents()) {
        mNode->setCondition(CachedNode::NOT_ENCLOSING_CURSOR);
        return REJECT_TEST;
    }
    int maxV = history->maxWorkingVertical();
    int minV = history->minWorkingVertical();
    setWorkingOverlap(testRight - testX, maxV - testX, testRight - minV);
    setWorkingInclusion(testRight - maxV, minV - testX);
    if (mWorkingOverlap == 0 && mNavOverlap == 0 && inNavBottom >= 0) {
        mNode->setCondition(CachedNode::OVERLAP_OR_EDGE_FURTHER);
        return REJECT_TEST;
    }
    mInNav = history->directionChange() && inNavTop >= 0 &&
        inNavBottom > 0 && subsumes;
    return false;
}

bool CachedFrame::BestData::setLeftDirection(const CachedHistory* history)
{
    const WebCore::IntRect& navBounds = history->navBounds();
    mMajorButt = navBounds.x() - mNodeBounds.right();
    int testY = mNodeBounds.y();
    int testBottom = mNodeBounds.bottom();
    setNavOverlap(navBounds.height(), navBounds.bottom() - testY,
        testBottom - navBounds.y());
    if (canBeReachedByAnotherDirection()) {
        mNode->setCondition(CachedNode::BEST_DIRECTION);
        return REJECT_TEST;
    }
    int inNavRight = navBounds.right() - mNodeBounds.right();
    mMajorDelta2 = inNavRight;
    mMajorDelta = mMajorDelta2 - ((navBounds.width() -
        mNodeBounds.width()) >> 1);
    if (mMajorDelta2 <= 1 && mMajorDelta <= 1) {
        mNode->setCondition(CachedNode::CENTER_FURTHER); // never move right or sideways
        return REJECT_TEST;
    }
    int inNavLeft = mNodeBounds.x() - navBounds.x();
    setNavInclusion(navBounds.y() - testY, testBottom - navBounds.bottom());
    bool subsumes = navBounds.width() > 0 && inOrSubsumesNav();
    if (inNavLeft <= 0 && inNavRight <= 0 && subsumes && !mNode->wantsKeyEvents()) {
        mNode->setCondition(CachedNode::NOT_ENCLOSING_CURSOR);
        return REJECT_TEST;
    }
    int maxH = history->maxWorkingHorizontal();
    int minH = history->minWorkingHorizontal();
    setWorkingOverlap(testBottom - testY, maxH - testY, testBottom - minH);
    setWorkingInclusion(minH - testY, testBottom - maxH);
    if (mWorkingOverlap == 0 && mNavOverlap == 0 && inNavLeft >= 0) {
        mNode->setCondition(CachedNode::OVERLAP_OR_EDGE_FURTHER);
        return REJECT_TEST;
    }
    mInNav = history->directionChange() && inNavLeft >= 0 &&
        inNavRight > 0 && subsumes; /* both L/R in or out */
    return false;
}

bool CachedFrame::BestData::setRightDirection(const CachedHistory* history)
{
    const WebCore::IntRect& navBounds = history->navBounds();
    mMajorButt = mNodeBounds.x() - navBounds.right();
    int testY = mNodeBounds.y();
    int testBottom = mNodeBounds.bottom();
    setNavOverlap(navBounds.height(), navBounds.bottom() - testY,
        testBottom - navBounds.y());
    if (canBeReachedByAnotherDirection()) {
        mNode->setCondition(CachedNode::BEST_DIRECTION);
        return REJECT_TEST;
    }
    int inNavLeft = mNodeBounds.x() - navBounds.x();
    mMajorDelta2 = inNavLeft;
    mMajorDelta = mMajorDelta2 + ((mNodeBounds.width() -
        navBounds.width()) >> 1);
    if (mMajorDelta2 <= 1 && mMajorDelta <= 1) {
        mNode->setCondition(CachedNode::CENTER_FURTHER); // never move left or sideways
        return REJECT_TEST;
    }
    int inNavRight = navBounds.right() - mNodeBounds.right();
    setNavInclusion(testBottom - navBounds.bottom(), navBounds.y() - testY);
    bool subsumes = navBounds.width() > 0 && inOrSubsumesNav();
    if (inNavLeft <= 0 && inNavRight <= 0 && subsumes && !mNode->wantsKeyEvents()) {
        mNode->setCondition(CachedNode::NOT_ENCLOSING_CURSOR);
        return REJECT_TEST;
    }
    int maxH = history->maxWorkingHorizontal();
    int minH = history->minWorkingHorizontal();
    setWorkingOverlap(testBottom - testY, testBottom - minH, maxH - testY);
    setWorkingInclusion(testBottom - maxH, minH - testY);
    if (mWorkingOverlap == 0 && mNavOverlap == 0 && inNavRight >= 0) {
        mNode->setCondition(CachedNode::OVERLAP_OR_EDGE_FURTHER);
        return REJECT_TEST;
    }
    mInNav = history->directionChange() && inNavLeft >= 0 &&
        inNavRight > 0 && subsumes; /* both L/R in or out */
    return false;
}

bool CachedFrame::BestData::setUpDirection(const CachedHistory* history)
{
    const WebCore::IntRect& navBounds = history->navBounds();
    mMajorButt = navBounds.y() - mNodeBounds.bottom();
    int testX = mNodeBounds.x();
    int testRight = mNodeBounds.right();
    setNavOverlap(navBounds.width(), navBounds.right() - testX,
        testRight - navBounds.x());
    if (canBeReachedByAnotherDirection()) {
        mNode->setCondition(CachedNode::BEST_DIRECTION);
        return REJECT_TEST;
    }
    int inNavBottom = navBounds.bottom() - mNodeBounds.bottom();
    mMajorDelta2 = inNavBottom;
    mMajorDelta = mMajorDelta2 - ((navBounds.height() -
        mNodeBounds.height()) >> 1);
    if (mMajorDelta2 <= 1 && mMajorDelta <= 1) {
        mNode->setCondition(CachedNode::CENTER_FURTHER); // never move down or sideways
        return REJECT_TEST;
    }
    int inNavTop = mNodeBounds.y() - navBounds.y();
    setNavInclusion(navBounds.x() - testX, testRight - navBounds.right());
    bool subsumes = navBounds.height() > 0 && inOrSubsumesNav();
    if (inNavTop <= 0 && inNavBottom <= 0 && subsumes && !mNode->wantsKeyEvents()) {
        mNode->setCondition(CachedNode::NOT_ENCLOSING_CURSOR);
        return REJECT_TEST;
    }
    int maxV = history->maxWorkingVertical();
    int minV = history->minWorkingVertical();
    setWorkingOverlap(testRight - testX, testRight - minV, maxV - testX);
    setWorkingInclusion(minV - testX, testRight - maxV);
    if (mWorkingOverlap == 0 && mNavOverlap == 0 && inNavTop >= 0) {
        mNode->setCondition(CachedNode::OVERLAP_OR_EDGE_FURTHER);
        return REJECT_TEST;
    }
    mInNav = history->directionChange() && inNavTop >= 0 &&
        inNavBottom > 0 && subsumes; /* both L/R in or out */
    return false;
}

void CachedFrame::BestData::setNavInclusion(int left, int right)
{
    // if left and right <= 0, test node is completely in umbra of cursor
        // prefer leftmost center
    // if left and right > 0, test node subsumes cursor
    mNavDelta = left;
    mNavDelta2 = right;
}

void CachedFrame::BestData::setNavOverlap(int span, int left, int right)
{
    // if left or right < 0, test node is not in umbra of cursor
    mNavOutside = left < MIN_OVERLAP || right < MIN_OVERLAP;
    mNavOverlap = Overlap(span, left, right); // prefer smallest negative left
}

void CachedFrame::BestData::setWorkingInclusion(int left, int right)
{
    mWorkingDelta = left;
    mWorkingDelta2 = right;
}

// distance scale factor factor as a 16.16 scalar
void CachedFrame::BestData::setWorkingOverlap(int span, int left, int right)
{
    // if left or right < 0, test node is not in umbra of cursor
    mWorkingOutside = left < MIN_OVERLAP || right < MIN_OVERLAP;
    mWorkingOverlap = Overlap(span, left, right);
    mPreferred = left <= 0 ? 0 : left;
}

#if DUMP_NAV_CACHE

#define DEBUG_PRINT_RECT(prefix, debugName, field) \
    { const WebCore::IntRect& r = b->field; \
    DUMP_NAV_LOGD("%s DebugTestRect TEST%s_" #debugName "={%d, %d, %d, %d}; //" #field "\n", \
        prefix, mFrameName, r.x(), r.y(), r.width(), r.height()); }

CachedFrame* CachedFrame::Debug::base() const {
    CachedFrame* nav = (CachedFrame*) ((char*) this - OFFSETOF(CachedFrame, mDebug));
    return nav;
}

void CachedFrame::Debug::print() const
{
    CachedFrame* b = base();
    DEBUG_PRINT_RECT("//", CONTENTS, mContents);
    DEBUG_PRINT_RECT("", BOUNDS, mLocalViewBounds);
    DEBUG_PRINT_RECT("//", VIEW, mViewBounds);

    DUMP_NAV_LOGD("// CachedNode mCachedNodes={ // count=%d\n", b->mCachedNodes.size());
    for (CachedNode* node = b->mCachedNodes.begin();
            node != b->mCachedNodes.end(); node++) {
        node->mDebug.print();
        const CachedInput* input = b->textInput(node);
        if (input)
            input->mDebug.print();
        DUMP_NAV_LOGD("\n");
    }
    DUMP_NAV_LOGD("// }; // end of nodes\n");
#if USE(ACCELERATED_COMPOSITING)
    DUMP_NAV_LOGD("// CachedLayer mCachedLayers={ // count=%d\n", b->mCachedLayers.size());
    for (CachedLayer* layer = b->mCachedLayers.begin();
            layer != b->mCachedLayers.end(); layer++) {
        layer->mDebug.print();
    }
    DUMP_NAV_LOGD("// }; // end of layers\n");
#endif // USE(ACCELERATED_COMPOSITING)
    DUMP_NAV_LOGD("// CachedColor mCachedColors={ // count=%d\n", b->mCachedColors.size());
    for (CachedColor* color = b->mCachedColors.begin();
            color != b->mCachedColors.end(); color++) {
        color->mDebug.print();
    }
    DUMP_NAV_LOGD("// }; // end of colors\n");
    DUMP_NAV_LOGD("// CachedFrame mCachedFrames={ // count=%d\n", b->mCachedFrames.size());
    for (CachedFrame* child = b->mCachedFrames.begin();
            child != b->mCachedFrames.end(); child++)
    {
        child->mDebug.print();
    }
    DUMP_NAV_LOGD("// }; // end of child frames\n");
    DUMP_NAV_LOGD("// void* mFrame=(void*) %p;\n", b->mFrame);
    DUMP_NAV_LOGD("// CachedFrame* mParent=%p;\n", b->mParent);
    DUMP_NAV_LOGD("// int mIndexInParent=%d;\n", b->mIndexInParent);
    DUMP_NAV_LOGD("// const CachedRoot* mRoot=%p;\n", b->mRoot);
    DUMP_NAV_LOGD("// int mCursorIndex=%d;\n", b->mCursorIndex);
    DUMP_NAV_LOGD("// int mFocusIndex=%d;\n", b->mFocusIndex);
}

bool CachedFrame::Debug::validate(const CachedNode* node) const
{
    const CachedFrame* b = base();
    if (b->mCachedNodes.size() == 0)
        return false;
    if (node >= b->mCachedNodes.begin() && node < b->mCachedNodes.end())
        return true;
    for (const CachedFrame* child = b->mCachedFrames.begin();
            child != b->mCachedFrames.end(); child++)
        if (child->mDebug.validate(node))
            return true;
    return false;
}

#undef DEBUG_PRINT_RECT

#endif

}
