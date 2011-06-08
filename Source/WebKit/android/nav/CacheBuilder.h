/*
 * Copyright 2006, The Android Open Source Project
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

#ifndef CacheBuilder_h
#define CacheBuilder_h

#include "CachedDebug.h"
#include "CachedNodeType.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "TextDirection.h"
#include <wtf/Forward.h>
#include <wtf/Vector.h>

#define NAVIGATION_MAX_PHONE_LENGTH 14

using namespace WebCore;

namespace WebCore {

class ColumnInfo;
class Document;
class Frame;
class HTMLAreaElement;
class InlineTextBox;
class LayerAndroid;
class Node;
class PlatformGraphicsContext;
class RenderBlock;
class RenderFlow;
class RenderLayer;
class RenderObject;
class Text;

}

namespace android {

class CachedFrame;
class CachedNode;
class CachedRoot;

class CacheBuilder {
public:
    enum Direction {
        UNINITIALIZED = -1,
        LEFT,
        RIGHT,
        UP,
        DOWN,
        DIRECTION_COUNT,
        UP_DOWN = UP & DOWN,  // mask and result
        RIGHT_DOWN = RIGHT & DOWN, // mask and result
    };
    enum FoundState {
        FOUND_NONE,
        FOUND_PARTIAL,
        FOUND_COMPLETE
    };
    CacheBuilder();
    void allowAllTextDetection() { mAllowableTypes = ALL_CACHEDNODE_BITS; }
    void buildCache(CachedRoot* root);
    static bool ConstructPartRects(Node* node, const IntRect& bounds, 
        IntRect* focusBounds, int x, int y, WTF::Vector<IntRect>* result,
        int* imageCountPtr);
    Node* currentFocus() const;
    void disallowAddressDetection() { mAllowableTypes = (CachedNodeBits) (
        mAllowableTypes & ~ADDRESS_CACHEDNODE_BIT); }
    void disallowEmailDetection() { mAllowableTypes = (CachedNodeBits) (
        mAllowableTypes & ~EMAIL_CACHEDNODE_BIT); }
    void disallowPhoneDetection() { mAllowableTypes = (CachedNodeBits) (
        mAllowableTypes & ~PHONE_CACHEDNODE_BIT); }
    static FoundState FindAddress(const UChar* , unsigned length, int* start,
        int* end, bool caseInsensitive);
    static IntRect getAreaRect(const HTMLAreaElement* area);
    static void GetGlobalOffset(Frame* , int* x, int * y);
    static void GetGlobalOffset(Node* , int* x, int * y);
    bool pictureSetDisabled() { return mPictureSetDisabled; }
    static bool validNode(Frame* startFrame, void* framePtr, void* nodePtr);
private:
    enum AddressProgress {
        NO_ADDRESS,
        SKIP_TO_SPACE,
        HOUSE_NUMBER,
        NUMBER_TRAILING_SPACE,
        ADDRESS_LINE,
        STATE_NAME,
        SECOND_HALF,
        ZIP_CODE,
        PLUS_4,
        FIND_STREET
    };
    struct NodeWalk {
        NodeWalk() { reset(); }
        int mStart;
        int mEnd;
        Node* mFinalNode;
        InlineTextBox* mLastInline;
        bool mMore;
        void reset() { mMore = false; }
    };
    struct BoundsPart {
        IntRect mRect;
        int mStart;
        int mEnd;
    };
    struct Bounds {
        typedef bool (*FindText)(BoundsPart* result, InlineTextBox* , const String& match);
        IntRect mNodeBounds;
        BoundsPart mPart;
        WTF::Vector<BoundsPart> mParts;   
        char mStore[NAVIGATION_MAX_PHONE_LENGTH + 1];
        int mPartIndex;
        Node* mNode;
        Node* mFinalNode;
        void reset() { mNode = NULL; }
    };
    struct FindState {
        int mStartResult;
        int mEndResult;
        const UChar* mCurrentStart;
        const UChar* mEnd;
        AddressProgress mProgress;
        int mNumberCount;
        int mLetterCount;
        unsigned mWordCount;
        int mLineCount;
        const UChar* mFirstLower;
        const UChar* mZipStart;
        const UChar* mBases[16]; // FIXME: random guess, maybe too small, maybe too big
        const UChar* mWords[16];
        const UChar* mEnds[16];
        const UChar* mStarts[16]; // text is not necessarily contiguous
        const char* mStates;
        int mEndWord;
        int mStateWord;
        int mZipHint;
        int mSectionLength;
        unsigned mNumberWords; // must contain as many bits as mWords contains elements
        char* mPattern;
        UChar mStore[NAVIGATION_MAX_PHONE_LENGTH + 1];
        UChar* mStorePtr;
        UChar mBackOne;
        UChar mBackTwo;
        UChar mCurrent;
        bool mUnparsed;
        bool mZipDelimiter;
        bool mOpenParen;
        bool mInitialized;
        bool mContinuationNode;
        bool mCaseInsensitive;
        void shiftWords(int shift) {
            memmove(mBases, &mBases[shift], (sizeof(mBases) /
                sizeof(mBases[0]) - shift) * sizeof(mBases[0]));
            memmove(mWords, &mWords[shift], (sizeof(mWords) /
                sizeof(mWords[0]) - shift) * sizeof(mWords[0]));
            memmove(mEnds, &mEnds[shift], (sizeof(mEnds) /
                sizeof(mEnds[0]) - shift) * sizeof(mEnds[0]));
            memmove(mStarts, &mStarts[shift], (sizeof(mStarts) /
                sizeof(mStarts[0]) - shift) * sizeof(mStarts[0]));
        }
        void newWord(const UChar* baseChars, const UChar* chars) {
            mBases[mWordCount] = baseChars;
            mWords[mWordCount] = chars;
            mEnds[mWordCount] = mEnd;
            mStarts[mWordCount] = mCurrentStart;
        }
    };
    struct Tracker {
        Node* mLastChild;
    };
    struct ClipColumnTracker : Tracker {
        Node* mNode;
        IntRect mBounds;
        ColumnInfo* mColumnInfo;
        int mColumnGap;
        TextDirection mDirection;
        bool mHasClip;
    };
    struct LayerTracker : Tracker {
        LayerAndroid* mLayer;
        RenderLayer* mRenderLayer;
        IntRect mBounds;
        IntPoint mScroll;
        ~LayerTracker();
    };
    struct TabIndexTracker : Tracker {
        int mTabIndex;
    };
    struct FocusTracker : TabIndexTracker {
        int mCachedNodeIndex;
        bool mSomeParentTakesFocus;
    };
    void adjustForColumns(const ClipColumnTracker& track, 
        CachedNode* node, IntRect* bounds, RenderBlock*);
    static bool AddPartRect(IntRect& bounds, int x, int y,
        WTF::Vector<IntRect>* result, IntRect* focusBounds);
    static bool AnyIsClick(Node* node);
    static bool AnyChildIsClick(Node* node);
    static bool NodeHasEventListeners(Node* node, AtomicString* eventTypes, int length);
    void BuildFrame(Frame* root, Frame* frame,
        CachedRoot* cachedRoot, CachedFrame* cachedFrame);
    bool CleanUpContainedNodes(CachedRoot* cachedRoot, CachedFrame* cachedFrame,
        const FocusTracker* last, int lastChildIndex);
    static bool ConstructTextRect(Text* textNode,
        InlineTextBox* textBox, int start, int relEnd, int x, int y, 
        IntRect* focusBounds, const IntRect& clip, WTF::Vector<IntRect>* result);
    static bool ConstructTextRects(Text* node, int start, 
        Text* last, int end, int x, int y, IntRect* focusBounds, 
        const IntRect& clip, WTF::Vector<IntRect>* result);
    static FoundState FindPartialAddress(const UChar* , const UChar* , unsigned length, FindState* );
    static FoundState FindPartialEMail(const UChar* , unsigned length, FindState* );
    static FoundState FindPartialNumber(const UChar* , unsigned length, FindState* );
    static FoundState FindPhoneNumber(const UChar* chars, unsigned length, int* start, int* end);
    static void FindReset(FindState* );
    static void FindResetNumber(FindState* );
    static Frame* FrameAnd(CacheBuilder* focusNav);
    static Frame* FrameAnd(const CacheBuilder* focusNav);
    static CacheBuilder* Builder(Frame* );
    static Frame* HasFrame(Node* );
    static bool HasOverOrOut(Node* );
    static bool HasTriggerEvent(Node* );
    static bool IsDomainChar(UChar ch);
    bool isFocusableText(NodeWalk* , bool oldMore, Node* , CachedNodeType* type,
        String* exported) const; //returns true if it is focusable
    static bool IsMailboxChar(UChar ch);
    static bool IsRealNode(Frame* , Node* );
    int overlap(int left, int right); // returns distance scale factor as 16.16 scalar
    bool setData(CachedFrame* );
#if USE(ACCELERATED_COMPOSITING)
    void TrackLayer(WTF::Vector<LayerTracker>& layerTracker,
        RenderObject* nodeRenderer, Node* lastChild, int offsetX, int offsetY);
#endif
    Node* tryFocus(Direction direction);
    Node* trySegment(Direction direction, int mainStart, int mainEnd);
    CachedNodeBits mAllowableTypes;
    bool mPictureSetDisabled;
#if DUMP_NAV_CACHE
public:
    class Debug {
public:
        void frameName(char*& namePtr, const char* max) const;
        void init(char* buffer, size_t size);
        static int ParentIndex(Node* node, int count, Node* parent);
        void print() { frames(); }
        void print(const char* name);
        void wideString(const String& str);
private:
        void attr(const AtomicString& name, const AtomicString& value);
        void comma(const char* str);
        void flush();
        Frame* frameAnd() const;
        void frames();
        void groups();
        bool isFocusable(Node* node);
        void localName(Node* node);
        void newLine(int indent = 0);
        void print(const char* name, unsigned len);
        void setIndent(int );
        void uChar(const UChar* name, unsigned len, bool hex);
        void validateFrame();
        void validateStringData();
        void wideString(const UChar* chars, int length, bool hex);
        char* mBuffer;
        size_t mBufferSize;
        int mIndex;
        const char* mPrefix;
        int mMinPrefix;
    } mDebug;
#endif
};

}

#endif
