/* libs/WebKitLib/WebKit/platform/bridge/android/CacheBuilder.h
**
** Copyright 2006, The Android Open Source Project
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

#ifndef CacheBuilder_H
#define CacheBuilder_H

#include "CachedDebug.h"
#include "CachedNodeType.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "TextDirection.h"
#include "wtf/Vector.h"

#define NAVIGATION_MAX_PHONE_LENGTH 14

namespace android {
    class CachedFrame;
    class CachedNode;
    class CachedRoot;
}

namespace WebCore {
    
class Document;
class Frame;
class FrameAndroid;
class InlineTextBox;
class Node;
class PlatformGraphicsContext;
class RenderFlow;
class RenderObject;
class RenderLayer;
class Text;

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
    void allowAllTextDetection() { mAllowableTypes = android::ALL_CACHEDNODETYPES; }
    void buildCache(android::CachedRoot* root);
    static bool ConstructPartRects(Node* node, const IntRect& bounds, 
        IntRect* focusBounds, int x, int y, WTF::Vector<IntRect>* result);
    Node* currentFocus() const;
    void disallowAddressDetection() { mAllowableTypes = (android::CachedNodeType) (
        mAllowableTypes & ~android::ADDRESS_CACHEDNODETYPE); }
    void disallowEmailDetection() { mAllowableTypes = (android::CachedNodeType) (
        mAllowableTypes & ~android::EMAIL_CACHEDNODETYPE); }
    void disallowPhoneDetection() { mAllowableTypes = (android::CachedNodeType) (
        mAllowableTypes & ~android::PHONE_CACHEDNODETYPE); }
    static FoundState FindAddress(const UChar* , unsigned length, int* start, int* end);
    Node* findByCenter(int x, int y) const;
    static void GetGlobalOffset(Frame* , int* x, int * y);
    static void GetGlobalOffset(Node* , int* x, int * y);
    bool outOfDate();
    void setLastFocus(Node* );
    bool validNode(void* framePtr, void* nodePtr) const;
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
        android::CachedNodeType mStoreType;
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
    };
    struct ClipColumnTracker {
        IntRect mBounds;
        Node* mLastChild;
        Node* mNode;
        WTF::Vector<IntRect>* mColumns;
        int mColumnGap;
        TextDirection mDirection;
        bool mHasClip;
    };
    struct Tracker {
        int mCachedNodeIndex;
        Node* mLastChild;
        Node* mParentLastChild;
        bool mSomeParentTakesFocus;
    };
    void adjustForColumns(const ClipColumnTracker& track, 
        android::CachedNode* node, IntRect* bounds);
    static bool AddPartRect(IntRect& bounds, int x, int y,
        WTF::Vector<IntRect>* result, IntRect* focusBounds);
    static bool AnyIsClick(Node* node);
    static bool AnyChildIsClick(Node* node);
    void BuildFrame(FrameAndroid* root, FrameAndroid* frame,
        android::CachedRoot* cachedRoot, android::CachedFrame* cachedFrame);
    bool CleanUpContainedNodes(android::CachedFrame* cachedFrame, 
        const Tracker* last, int lastChildIndex);
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
    static FrameAndroid* FrameAnd(CacheBuilder* focusNav);
    static FrameAndroid* FrameAnd(const CacheBuilder* focusNav);
    static CacheBuilder* Builder(Frame* );
    static Frame* HasFrame(Node* );
    static bool HasOverOrOut(Node* );
    static bool HasTriggerEvent(Node* );
    static bool IsDomainChar(UChar ch);
    bool isFocusableText(NodeWalk* , bool oldMore, Node* , android::CachedNodeType* type,
        String* exported) const; //returns true if it is focusable
    static bool IsMailboxChar(UChar ch);
    static bool IsRealNode(Frame* , Node* );
    int overlap(int left, int right); // returns distance scale factor as 16.16 scalar
    bool setData(android::CachedFrame* );
    Node* tryFocus(Direction direction);
    Node* trySegment(Direction direction, int mainStart, int mainEnd);
    Node* mLastKnownFocus;
    IntRect mLastKnownFocusBounds;
    android::CachedNodeType mAllowableTypes;
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
        int flowBoxes(RenderFlow* flow, int ifIndex, int indent);
        void flush();
        FrameAndroid* frameAnd() const;
        void frames();
        void groups();
        bool isFocusable(Node* node);
        void localName(Node* node);
        void newLine(int indent = 0);
        void print(const char* name, unsigned len);
        void renderTree(RenderObject* , int indent, Node* , int count);
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
