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

#ifndef CachedNode_H
#define CachedNode_H

#include "CachedDebug.h"
#include "CachedNodeType.h"
#include "IntRect.h"
#include "PlatformString.h"

#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

class SkPicture;

namespace WebCore {
    class Node;
}

namespace android {

class CachedFrame;
class CachedRoot;

class CachedNode {
public:
// Nodes are rejected because either they are spacially not the best (first set)
// or because they have the wrong DOM attribute (in focus, a focused child, etc)
// findClosest() gives only spacially rejected nodes a second chance
    enum Condition { // if bigger than 32, increase bitfield size below
        // rejections that get a second chance
        NOT_REJECTED = 0,
        SECOND_CHANCE_START = NOT_REJECTED, // must be first in list
        BUTTED_UP,
        CENTER_FURTHER,
        CLOSER,
        CLOSER_IN_CURSOR,
        CLOSER_OVERLAP,
        CLOSER_TOP,
        NAVABLE,
        FURTHER,
        IN_UMBRA,
        IN_WORKING,
        LEFTMOST,
        NOT_ENCLOSING_CURSOR,
        OVERLAP_OR_EDGE_FURTHER,
        PREFERRED, // better overlap measure
        SECOND_CHANCE_END = PREFERRED, // must be last in list
        // rejections that don't get a second chance
        ANCHOR_IN_ANCHOR,
        BEST_DIRECTION, // can be reached by another direction
        CHILD,
        DISABLED,
        HIGHER_TAB_INDEX,
        IN_CURSOR,
        IN_CURSOR_CHILDREN,
        NOT_CURSOR_NODE,
        OUTSIDE_OF_BEST, // containership
        OUTSIDE_OF_ORIGINAL, // containership
        UNDER_LAYER,
        CONDITION_SIZE // FIXME: test that CONDITION_SIZE fits in mCondition
    };
    CachedNode() {
        // The node is initiaized to 0 in its array, so nothing to do in the
        // constructor
    }

    WebCore::IntRect bounds(const CachedFrame* ) const;
    int childFrameIndex() const { return isFrame() ? mDataIndex : -1; }
    void clearCondition() const { mCondition = NOT_REJECTED; }
    void clearCursor(CachedFrame* );
    static bool Clip(const WebCore::IntRect& outer, WebCore::IntRect* inner,
        WTF::Vector<WebCore::IntRect>* rings);
    bool clip(const WebCore::IntRect& );
    bool clippedOut() { return mClippedOut; }
    int colorIndex() const { return mColorIndex; }
    WebCore::IntRect cursorRingBounds(const CachedFrame* ) const;
    void cursorRings(const CachedFrame* , WTF::Vector<WebCore::IntRect>* ) const;
    bool disabled() const { return mDisabled; }
    const CachedNode* document() const { return &this[-mIndex]; }
    void fixUpCursorRects(const CachedFrame* frame);
    const WTF::String& getExport() const { return mExport; }
    bool hasCursorRing() const { return mHasCursorRing; }
    bool hasMouseOver() const { return mHasMouseOver; }
    void hideCursor(CachedFrame* );
    WebCore::IntRect hitBounds(const CachedFrame* ) const;
    int index() const { return mIndex; }
    void init(WebCore::Node* node);
    bool isAnchor() const { return mType == ANCHOR_CACHEDNODETYPE; }
    bool isContentEditable() const { return mType == CONTENT_EDITABLE_CACHEDNODETYPE; }
    bool isCursor() const { return mIsCursor; }
    bool isArea() const { return mType == AREA_CACHEDNODETYPE; }
    bool isFocus() const { return mIsFocus; }
    bool isFrame() const { return mType == FRAME_CACHEDNODETYPE; }
    bool isHidden() const { return mIsHidden; }
    bool isInLayer() const { return mIsInLayer; }
    bool isNavable(const CachedFrame* frame, const WebCore::IntRect& clip) const {
        return clip.intersects(bounds(frame));
    }
    bool isPlugin() const { return mType == PLUGIN_CACHEDNODETYPE; }
    bool isSelect() const { return mType == SELECT_CACHEDNODETYPE; }
    bool isSyntheticLink() const {
        return mType >= ADDRESS_CACHEDNODETYPE && mType <= PHONE_CACHEDNODETYPE;
    }
    bool isTextField(const CachedFrame*) const;
    bool isTextInput() const { return mType == TEXT_INPUT_CACHEDNODETYPE; }
    bool isTransparent() const { return mIsTransparent; }
    bool isUnclipped() const { return mIsUnclipped; }
    // localXXX functions are used only for drawing cursor rings
    WebCore::IntRect localBounds(const CachedFrame* ) const;
    void localCursorRings(const CachedFrame* ,
        WTF::Vector<WebCore::IntRect>* ) const;
    WebCore::IntRect localHitBounds(const CachedFrame* ) const;
    WebCore::IntRect localRing(const CachedFrame* , size_t part) const;
    void move(int x, int y);
    int navableRects() const { return mNavableRects; }
    void* nodePointer() const { return mNode; }
    bool noSecondChance() const { return mCondition > SECOND_CHANCE_END; }
    const WebCore::IntRect& originalAbsoluteBounds() const {
        return mOriginalAbsoluteBounds; }
    const CachedNode* parent() const { return document() + mParentIndex; }
    void* parentGroup() const { return mParentGroup; }
    int parentIndex() const { return mParentIndex; }
    bool partRectsContains(const CachedNode* other) const;
    const WebCore::IntRect& rawBounds() const { return mBounds; }
    void reset();
    WebCore::IntRect ring(const CachedFrame* , size_t part) const;
    const WTF::Vector<WebCore::IntRect>& rings() const { return mCursorRing; }
    void setBounds(const WebCore::IntRect& bounds) { mBounds = bounds; }
    void setClippedOut(bool clipped) { mClippedOut = clipped; }
    void setColorIndex(int index) { mColorIndex = index; }
    void setCondition(Condition condition) const { mCondition = condition; }
    void setDataIndex(int index) { mDataIndex = index; }
    void setDisabled(bool disabled) { mDisabled = disabled; }
    void setExport(const WTF::String& exported) { mExport = exported; }
    void setHasCursorRing(bool hasRing) { mHasCursorRing = hasRing; }
    void setHasMouseOver(bool hasMouseOver) { mHasMouseOver = hasMouseOver; }
    void setHitBounds(const WebCore::IntRect& bounds) { mHitBounds = bounds; }
    void setOriginalAbsoluteBounds(const WebCore::IntRect& bounds) {
        mOriginalAbsoluteBounds = bounds; }
    void setIndex(int index) { mIndex = index; }
    void setIsCursor(bool isCursor) { mIsCursor = isCursor; }
    void setIsFocus(bool isFocus) { mIsFocus = isFocus; }
    void setIsInLayer(bool isInLayer) { mIsInLayer = isInLayer; }
    void setIsParentAnchor(bool isAnchor) { mIsParentAnchor = isAnchor; }
    void setIsTransparent(bool isTransparent) { mIsTransparent = isTransparent; }
    void setIsUnclipped(bool unclipped) { mIsUnclipped = unclipped; }
    void setLast() { mLast = true; }
    void setNavableRects() { mNavableRects = mCursorRing.size(); }
    void setParentGroup(void* group) { mParentGroup = group; }
    void setParentIndex(int parent) { mParentIndex = parent; }
    void setSingleImage(bool single) { mSingleImage = single; }
    void setTabIndex(int index) { mTabIndex = index; }
    void setType(CachedNodeType type) { mType = type; }
    void show() { mIsHidden = false; }
    bool singleImage() const { return mSingleImage; }
    int tabIndex() const { return mTabIndex; }
    int textInputIndex() const { return isTextInput() ? mDataIndex : -1; }
    const CachedNode* traverseNextNode() const { return mLast ? NULL : &this[1]; }
    bool useBounds() const { return mUseBounds; }
    bool useHitBounds() const { return mUseHitBounds; }
    bool wantsKeyEvents() const { return isTextInput() || isPlugin()
        || isContentEditable() || isFrame(); }
private:
    friend class CacheBuilder;
    WTF::String mExport;
    WebCore::IntRect mBounds;
    WebCore::IntRect mHitBounds;
    WebCore::IntRect mOriginalAbsoluteBounds;
    WTF::Vector<WebCore::IntRect> mCursorRing;
    void* mNode; // WebCore::Node*, only used to match pointers
    void* mParentGroup; // WebCore::Node*, only used to match pointers
    int mDataIndex; // child frame if a frame; input data index; or -1
    int mIndex; // index of itself, to find first in array (document)
    int mNavableRects; // FIXME: could be bitfield once I limit max number of rects
    int mParentIndex;
    int mTabIndex;
    int mColorIndex; // index to ring color and other stylable properties
    mutable Condition mCondition : 5; // why the node was not chosen on the first pass
    CachedNodeType mType : 4;
    bool mClippedOut : 1;
    bool mDisabled : 1;
    bool mFixedUpCursorRects : 1;
    bool mHasCursorRing : 1;
    bool mHasMouseOver : 1;
    bool mIsCursor : 1;
    bool mIsFocus : 1;
    bool mIsHidden : 1;
    bool mIsInLayer : 1;
    bool mIsParentAnchor : 1;
    bool mIsTransparent : 1;
    bool mIsUnclipped : 1;
    bool mLast : 1;             // true if this is the last node in a group
    bool mSingleImage : 1;
    bool mUseBounds : 1;
    bool mUseHitBounds : 1;
#ifdef BROWSER_DEBUG
public:
    WebCore::Node* webCoreNode() const { return (WebCore::Node*) mNode; }
    bool mDisplayMeasure;
    mutable bool mInCompare;
    int mSideDistance;
    int mSecondSide;
#endif    
#if DEBUG_NAV_UI || DUMP_NAV_CACHE
public:
    class Debug {
public:
        CachedNode* base() const;
        const char* condition(Condition t) const;
        void print() const;
        const char* type(CachedNodeType t) const;
#if DUMP_NAV_CACHE
        int mNodeIndex;
        int mParentGroupIndex;
#endif
    } mDebug;
    friend class CachedNode::Debug;
#endif
};

}

#endif
