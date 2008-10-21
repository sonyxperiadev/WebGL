/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#define LOG_TAG "webviewglue"

#include <config.h>
 
#include "android_graphics.h"
#include "AtomicString.h"
#include "CachedFrame.h"
#include "CachedNode.h"
#include "CachedRoot.h"
#include "Frame.h"
#include "GraphicsJNI.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "Node.h"
#include "PlatformGraphicsContext.h"
#include "PlatformString.h"
#include "SelectText.h"
#include "SkCanvas.h"
#include "SkPath.h"
#include "SkPicture.h"
#include "SkPixelXorXfermode.h"
#include "SkRect.h"
#include "SkTime.h"
#include "WebViewCore.h"

#ifdef LOG // WebKit may define this as well; if so, undefine it so LOGD will work
#undef LOG
#endif

#ifdef GET_NATIVE_VIEW
#undef GET_NATIVE_VIEW
#endif

#define GET_NATIVE_VIEW(env, obj) ((WebView*)env->GetIntField(obj, gWebViewField))

#include <ui/KeycodeLabels.h>
#include <utils/Log.h>
#include <JNIHelp.h>
#include <jni.h>

#define REPLAY_BUFFER_SIZE 4096

namespace android {

// This class provides JNI for making calls into native code from the UI side
// of the multi-threaded WebView.
struct CommonParams {
    enum Trigger {
        NO_DATA,
        CLEAR_FOCUS_PARAMS,
        FIRST_MOVE_FOCUS_PARAMS,
        MOVE_FOCUS_PARAMS,
        MOTION_UP_PARAMS
    }  mTrigger;
    int mGeneration;
};

struct CacheParams {
    void setFocus(const CachedNode* node, 
        const CachedFrame* frame, const CachedRoot* root, 
            const WebCore::IntPoint& focusLocation)
    {
        mNode = (WebCore::Node* ) (node ? node->nodePointer() : NULL);
        mFrame = (WebCore::Frame* ) (node ? frame->framePointer() : NULL);
        mX = focusLocation.x();
        mY = focusLocation.y();
    }

    WebCore::Node* mNode;
    WebCore::Frame* mFrame;
    int mX;
    int mY;
};

struct ClearFocusParams {
    CommonParams d;
    CacheParams c;
    int mX;
    int mY;
};

struct MotionUpParams {
    CommonParams d;
    int mX;
    int mY;
    int mSlop;
    bool mIsClick;
};

struct FirstMoveFocusParams {
    CommonParams d;
    int mKeyCode;
    int mCount;
    bool mIgnoreScroll;
};

struct MoveFocusParams {
    FirstMoveFocusParams d;
    CacheParams c;
    void* mSentFocus;
    WebCore::IntRect mClipRect;
    WebCore::IntRect mSentBounds;
    WebCore::IntRect mVisibleRect;
    CachedHistory mHistory; // FIXME: make this a subset
    int mXMax;
    int mYMax;
};

typedef MoveFocusParams LargestParams;
    
#if DEBUG_NAV_UI
static const char* TriggerNames[] = {
    "*** no data ! ***",
    "clearFocus",
    "firstMoveFocus",
    "moveFocus",
    "motionUp"
};
#endif

class FocusReplay {
public:
FocusReplay() : mStart(mBuffer), mEnd(mBuffer), mLastGeneration(0) 
{
}

// find the most recent common data
void add(const CommonParams& data, size_t len)
{
    DBG_NAV_LOGD("mStart=%d mEnd=%d trigger=%s moveGeneration=%d", mStart - mBuffer,
        mEnd - mBuffer, TriggerNames[data.mTrigger], data.mGeneration);
    mLastGeneration = data.mGeneration;
    char* limit = mBuffer + sizeof(mBuffer);
    int used = mEnd - mStart;
    if (used < 0)
        used += sizeof(mBuffer);
    int needed = (int) len - ((int) sizeof(mBuffer) - used);
    if (needed >= 0)
        reclaim(++needed);
    if (mEnd + len <= limit) {
        memcpy(mEnd, (void*) &data, len);
        mEnd += len;
        DBG_NAV_LOGD("mStart=%d mEnd=%d", mStart - mBuffer, mEnd - mBuffer);
        return;
    }
    size_t partial = limit - mEnd;
    memcpy(mEnd, (void*) &data, partial);
    const void* remainder = (const void*) ((const char*) &data + partial);
    partial = len - partial;
    memcpy(mBuffer, remainder, partial);
    mEnd = mBuffer + partial;
    DBG_NAV_LOGD("wrap mStart=%d mEnd=%d", 
        mStart - mBuffer, mEnd - mBuffer);
}

int count()
{
    DBG_NAV_LOGD("mStart=%d mEnd=%d", 
        mStart - mBuffer, mEnd - mBuffer);
    if (mStart == mEnd)
        return 0;
    char* limit = mBuffer + sizeof(mBuffer);
    char* saveStart = mStart;
    int result = 0;
    while (true) {
        ++result;
        mStart += triggerSize();
        if (mStart == mEnd)
            break;
        if (mStart >= limit)
            mStart -= sizeof(mBuffer);
    }
    mStart = saveStart;
    DBG_NAV_LOGD("count=%d", result);
    return result;
}

void discard(int generation)
{
    DBG_NAV_LOGD("generation=%d", generation);
    LargestParams storage;
    const CommonParams& params = storage.d.d;
    char* pos = position();
    retrieve(&storage.d.d);
    if (params.mGeneration > generation) {
        DBG_NAV_LOGD("params.mGeneration=%d > generation=%d", 
            params.mGeneration, generation);
        rewind(pos);
        DBG_NAV_LOGD("mStart=%d mEnd=%d", mStart - mBuffer, mEnd - mBuffer);
        return;
    }
    LOG_ASSERT(params.mGeneration == generation, "params.mGeneration != generation");
    DBG_NAV_LOGD("mStart=%d mEnd=%d", mStart - mBuffer, mEnd - mBuffer);
}

int lastAdd()
{
    return mLastGeneration;
}

char* position() 
{ 
    return mStart; 
}

int retrieve(CommonParams* data) 
{
    if (mEnd == mStart) {
        // changed from LOGD to LOGV, as it always fires when I click to center
        // text (mrr)
        LOGV("%s *** no data to retrieve (error condition) ***", __FUNCTION__);
        data->mTrigger = CommonParams::NO_DATA;
        return data->mGeneration = INT_MAX;
    }
    DBG_NAV_LOGD("mStart=%d mEnd=%d", 
        mStart - mBuffer, mEnd - mBuffer);
    char* limit = mBuffer + sizeof(mBuffer);
    size_t size = triggerSize();
    if (mStart < mEnd) {
        LOG_ASSERT((size_t) (mEnd - mStart) >= size, "mEnd - mStart < size");
        memcpy(data, mStart, size);
        mStart += size;
    } else {
        int partial = limit - mStart;
        if (partial > (int) size)
            partial = size;
        memcpy(data, mStart, partial);
        mStart += partial;
        void* remainder = (void*) ((char*) data + partial);
        partial = size - partial;
        if (partial > 0) {
            memcpy(remainder, mBuffer, partial);
            mStart = mBuffer + partial;
            LOG_ASSERT(mStart <= mEnd, "mStart > mEnd");
        }
    }
    if (mStart == limit) {
        mStart = mBuffer;
        if (mEnd == limit)
            mEnd = mBuffer;
    }
    DBG_NAV_LOGD("mStart=%d mEnd=%d trigger=%s moveGeneration=%d", 
        mStart - mBuffer, mEnd - mBuffer, TriggerNames[data->mTrigger],
        data->mGeneration);
    return data->mGeneration;
}

void rewind(char* pos) 
{ 
    mStart = pos; 
}

private:
void reclaim(int needed)
{
    DBG_NAV_LOGD("needed=%d", needed);
    char* limit = mBuffer + sizeof(mBuffer);
    do {
        size_t size = triggerSize();
        mStart += size;
        needed -= size;
        if (mStart >= limit) {
            mStart = mBuffer + (mStart - limit);
            if (mEnd == limit)
                mEnd = mBuffer;
        }
    } while (needed > 0 && mStart != mEnd);
    DBG_NAV_LOGD("mStart=%d mEnd=%d", 
        mStart - mBuffer, mEnd - mBuffer);
}

size_t triggerSize()
{
    LOG_ASSERT(mStart != mEnd, "mStart == mEnd");
    char* limit = mBuffer + sizeof(mBuffer);
    LOG_ASSERT(mStart + sizeof(CommonParams::Trigger) <= limit, "trigger not in limit");
    CommonParams::Trigger trigger;
    memcpy(&trigger, mStart, sizeof(trigger));
    switch (trigger) {
        case CommonParams::CLEAR_FOCUS_PARAMS:
            return sizeof(ClearFocusParams);
        case CommonParams::FIRST_MOVE_FOCUS_PARAMS:
            return sizeof(FirstMoveFocusParams);
        case CommonParams::MOVE_FOCUS_PARAMS:
            return sizeof(MoveFocusParams);
        case CommonParams::MOTION_UP_PARAMS:
            return sizeof(MotionUpParams);
        default:
            LOG_ASSERT(0, "trigger undefined");
    }
    return 0;
}

char mBuffer[REPLAY_BUFFER_SIZE];
char* mStart;
char* mEnd;
int mLastGeneration;
}; // end of helper class ReplayFocus

static jfieldID gWebViewField;
 
//-------------------------------------
 
extern JavaVM* jnienv_to_javavm(JNIEnv* env);
extern JNIEnv* javavm_to_jnienv(JavaVM* vm);
 
static bool checkException(JNIEnv* env)
{
    if (env->ExceptionCheck() != 0)
    {
        LOGE("*** Uncaught exception returned from Java call!\n");
        env->ExceptionDescribe();
        return true;
    }
    return false;
}

static jmethodID GetJMethod(JNIEnv* env, jclass clazz, const char name[], const char signature[])
{
    jmethodID m = env->GetMethodID(clazz, name, signature);
    LOG_ASSERT(m, "Could not find method %s", name);
    return m;
}

//-------------------------------------
class WebView
{
public:
enum FrameCachePermission {
    DONT_ALLOW_NEWER,
    ALLOW_NEWER,
    ALLOW_NEWEST
};

enum OutOfFocusFix {
    DO_NOTHING,
    CLEAR_TEXT_ENTRY,
    UPDATE_TEXT_ENTRY
};

struct JavaGlue
{
    JavaVM*     mJVM;
    jobject     mObj;
    jmethodID   mClearTextEntry;
    jmethodID   mScrollBy;
    jmethodID   mSendFinalFocus;
    jmethodID   mSendKitFocus;
    jmethodID   mSendMotionUp;
    jmethodID   mSetFocusData;
    jmethodID   mGetScaledMaxXScroll;
    jmethodID   mGetScaledMaxYScroll;
    jmethodID   mGetVisibleRect;
    jmethodID   mUpdateTextEntry;
    jmethodID   mViewInvalidate;
    jmethodID   mPostInvalidateDelayed;
    jfieldID    mRectLeft;
    jfieldID    mRectTop;
    jmethodID   mRectWidth;
    jmethodID   mRectHeight;
    jfieldID    mFocusNode;
    jmethodID   mSetAll;
} mJavaGlue;

WebView(JNIEnv* env, jobject javaWebView, int viewImpl)
{
    jclass clazz = env->FindClass("android/webkit/WebView");
 //   mJavaGlue = new JavaGlue;
    mJavaGlue.mJVM = jnienv_to_javavm(env);
    mJavaGlue.mObj = env->NewGlobalRef(javaWebView);
    mJavaGlue.mScrollBy = GetJMethod(env, clazz, "setContentScrollBy", "(II)V");
    mJavaGlue.mClearTextEntry = GetJMethod(env, clazz, "clearTextEntry", "()V");
    mJavaGlue.mSendFinalFocus = GetJMethod(env, clazz, "sendFinalFocus", "(IIII)V");
    mJavaGlue.mSendKitFocus = GetJMethod(env, clazz, "sendKitFocus", "()V");
    mJavaGlue.mSendMotionUp = GetJMethod(env, clazz, "sendMotionUp", "(IIIIIIIZZ)V");
    mJavaGlue.mSetFocusData = GetJMethod(env, clazz, "setFocusData", "(IIIIIIZ)V");
    mJavaGlue.mGetScaledMaxXScroll = GetJMethod(env, clazz, "getScaledMaxXScroll", "()I");
    mJavaGlue.mGetScaledMaxYScroll = GetJMethod(env, clazz, "getScaledMaxYScroll", "()I");
    mJavaGlue.mGetVisibleRect = GetJMethod(env, clazz, "sendOurVisibleRect", "()Landroid/graphics/Rect;");
    mJavaGlue.mUpdateTextEntry = GetJMethod(env, clazz, "updateTextEntry", "()V");
    mJavaGlue.mViewInvalidate = GetJMethod(env, clazz, "viewInvalidate", "()V");
    jclass viewClass = env->FindClass("android/view/View");
    mJavaGlue.mPostInvalidateDelayed = GetJMethod(env, viewClass, 
        "postInvalidateDelayed", "(JIIII)V");
    jclass rectClass = env->FindClass("android/graphics/Rect");
    LOG_ASSERT(rectClass, "Could not find Rect class");
    mJavaGlue.mRectLeft = env->GetFieldID(rectClass, "left", "I");
    mJavaGlue.mRectTop = env->GetFieldID(rectClass, "top", "I");
    mJavaGlue.mRectWidth = GetJMethod(env, rectClass, "width", "()I");
    mJavaGlue.mRectHeight = GetJMethod(env, rectClass, "height", "()I");
    
    // Set up class for updateFocusNode
    jclass focusnodeClass = env->FindClass("android/webkit/WebView$FocusNode");
    LOG_ASSERT(focusnodeClass, "Could not find FocusNode class!");
    mJavaGlue.mFocusNode = env->GetFieldID(clazz, "mFocusNode", "Landroid/webkit/WebView$FocusNode;");
    mJavaGlue.mSetAll = GetJMethod(env, focusnodeClass, "setAll", "(ZZZZZIIIIIIIILjava/lang/String;Ljava/lang/String;I)V");
    env->DeleteLocalRef(focusnodeClass);

    env->SetIntField(javaWebView, gWebViewField, (jint)this);
    mViewImpl = (WebViewCore*) viewImpl;
    mFrameCacheUI = NULL;
    mNavPictureUI = NULL;
    mInvalidNode = NULL;
    mGeneration = 0;
    mHeightCanMeasure = false;
    mFollowedLink = false;
    mLastDx = 0;
    mLastDxTime = 0;
    mRingAnimationEnd = 0;
    mUIButtons = NULL;
    mSelStart.setEmpty();
    mSelEnd.setEmpty();
}

~WebView()
{
    if (mJavaGlue.mObj)
    {
        JNIEnv* env = javavm_to_jnienv(mJavaGlue.mJVM);
        env->DeleteGlobalRef(mJavaGlue.mObj);
        mJavaGlue.mObj = 0;
    }
    delete mFrameCacheUI;
    delete mNavPictureUI;
    if (mUIButtons != NULL) {
        mUIButtons->deleteAll();
        delete mUIButtons;
    }
}

void clearFocus(int x, int y, bool inval)
{
    DBG_NAV_LOGD("x=%d y=%d inval=%s", x, y, 
        inval ? "true" : "false"); 
    clearTextEntry();
    CachedRoot* root = getFrameCache(ALLOW_NEWER);
    if (root == NULL)
        return;
    const CachedFrame* oldFrame = NULL;
    const CachedNode* oldFocusNode = root->currentFocus(&oldFrame);
    WebCore::IntPoint focusLocation = WebCore::IntPoint(0, 0);
    setFocusData(root->generation(), NULL, NULL, x, y, oldFocusNode == NULL);
    sendKitFocus();
    if (oldFocusNode != NULL) {
        DBG_NAV_LOG("oldFocusNode != NULL");
        focusLocation = root->focusLocation();
        root->setCachedFocus(NULL, NULL);
        if (inval)
            viewInvalidate();
    }
    ClearFocusParams params;
    params.d.mTrigger = CommonParams::CLEAR_FOCUS_PARAMS;
    params.d.mGeneration = mGeneration;
    params.c.setFocus(oldFocusNode, oldFrame, root, focusLocation);
    params.mX = x;
    params.mY = y;
    mReplay.add(params.d, sizeof(params));
}

void clearTextEntry()
{
    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__); 
    JNIEnv* env = javavm_to_jnienv(mJavaGlue.mJVM);
    env->CallVoidMethod(mJavaGlue.mObj, mJavaGlue.mClearTextEntry);
    checkException(env);
}

#if DUMP_NAV_CACHE
void debugDump()
{
    CachedRoot* root = getFrameCache(DONT_ALLOW_NEWER);
    if (root)
        root->mDebug.print();
}
#endif

#ifdef BUCKET_EXPERIMENT
void clearContentPicture() {    
    if (mViewImpl != NULL) {
        mViewImpl->mUIBuckets.clear();
    }
}

void drawContentPicture(SkCanvas* canvas) {
    WTF::Vector<WebViewCore::Bucket>& buckets = mViewImpl->mUIBuckets;
    const WebViewCore::Bucket* const end = buckets.end();
    for (WebViewCore::Bucket* bucket = buckets.begin(); bucket != end; bucket++) {
        SkRect bounds;
        const WebCore::IntRect& buckB = bucket->mBounds;
        bounds.set(SkIntToScalar(buckB.x()), SkIntToScalar(buckB.y()),
                   SkIntToScalar(buckB.right()), SkIntToScalar(buckB.bottom()));
        if (canvas->quickReject(bounds, SkCanvas::kBW_EdgeType)) {
            continue;
        }
        canvas->drawPicture(*bucket->mPicture);
    }
}
#endif

// Traverse our stored array of buttons that are in our picture, and update
// their subpictures according to their current focus state.
// Called from the UI thread.  This is the one place in the UI thread where we 
// access the buttons stored in the WebCore thread.
void nativeRecordButtons()
{
    // Check to see if the WebCore thread has created a new set of buttons.
    // We don't have to grab the mutex for this test, since the only place it 
    // could be set to null is right below, or in WebViewCore's constructor.
    if (mViewImpl->mButtons != NULL) {
        if (mUIButtons != NULL) {
            mUIButtons->deleteAll();
            delete mUIButtons;
        }
        // Transfer ownership of the array from the WebCore thread (viewimpl) to 
        // the UI thread
        mViewImpl->gButtonMutex.lock();
        mUIButtons = mViewImpl->mButtons;
        mViewImpl->mButtons = NULL;
        mViewImpl->gButtonMutex.unlock();
    }
    
    // If mUIButtons has never been set, or it has been set with an array of no 
    // buttons (which happens if a page does not have any buttons), then return.
    if (mUIButtons == NULL || mUIButtons->count() == 0)
        return;

    // Find the focused node so we can determine which node has focus, and
    // therefore which state to paint them in.
    // FIXME: In a future change, we should keep track of whether the focus has
    // changed to short circuit (note that we would still need to update if we
    // received new buttons from the WebCore thread).
    WebCore::Node* focus = NULL;
    CachedRoot* root = getFrameCache(DONT_ALLOW_NEWER);
    if (root != NULL) {
        const CachedNode* cachedNode = root->currentFocus();
        if (cachedNode != NULL)
            focus = (WebCore::Node*) cachedNode->nodePointer();
    }
    
    // Traverse the array, and update each button, depending on whether it is 
    // focused.
    Container** end = mUIButtons->end();
    for (Container** ptr = mUIButtons->begin(); ptr != end; ptr++) {
        (*ptr)->updateFocusState(focus);
    }
}

void drawFocusRing(SkCanvas* canvas)
{    
    const CachedRoot* root = getFrameCache(ALLOW_NEWER);
    if (root == NULL) {
        DBG_NAV_LOG("root==NULL"); 
        mFollowedLink = false;
        return;
    }
    const CachedNode* node = root->currentFocus();
    if (node == NULL) {
        DBG_NAV_LOG("node==NULL"); 
        mFollowedLink = false;
        return;
    }
    if (node->hasFocusRing() == false) {
        DBG_NAV_LOG("node->hasFocusRing() == false"); 
        return;
    }
    const WTF::Vector<WebCore::IntRect>& rings = node->focusRings();
    if (0 == rings.size()) {
        DBG_NAV_LOG("rings.size()==0"); 
        return;
    }
    
    bool isButton = false;
    // If this is a button drawn by us (rather than webkit) do not draw the 
    // focus ring, since its focus will be shown by a change in what we draw.
    if (mUIButtons != NULL) {
        // Should be in sync with recordButtons, since that will be called 
        // before this.
        if (mUIButtons->count() > 0) {
            WebCore::Node* focusPointer = (WebCore::Node*) node->nodePointer();
            Container** end = mUIButtons->end();
            for (Container** ptr = mUIButtons->begin(); ptr != end; ptr++) {
                if ((*ptr)->matches(focusPointer)) {
                    isButton = true;
                    break;
                }
            }
        }
    }
    FocusRing::Flavor flavor = isButton ? FocusRing::BUTTON_NO_RING :
        node->type() != NORMAL_CACHEDNODETYPE ? 
        FocusRing::FAKE_FLAVOR : node->nodePointer() == mInvalidNode ?
        FocusRing::INVALID_FLAVOR : FocusRing::NORMAL_FLAVOR;
    if (flavor != FocusRing::INVALID_FLAVOR && mFollowedLink) {
        flavor = (FocusRing::Flavor) (flavor + FocusRing::NORMAL_ANIMATING);
    }
#if DEBUG_NAV_UI
    const WebCore::IntRect& ring = rings[0];
    DBG_NAV_LOGD("cachedFocusNode=%d (nodePointer=%p) flavor=%s rings=%d"
        " (%d, %d, %d, %d)", node->index(), node->nodePointer(), 
        flavor == FocusRing::FAKE_FLAVOR ? "FAKE_FLAVOR" : 
        flavor == FocusRing::INVALID_FLAVOR ? "INVALID_FLAVOR" :
        flavor == FocusRing::NORMAL_ANIMATING ? "NORMAL_ANIMATING" :
        flavor == FocusRing::FAKE_ANIMATING ? "FAKE_ANIMATING" :
        flavor == FocusRing::BUTTON_NO_RING ? "BUTTON_NO_RING" :
        flavor == FocusRing::BUTTON_ANIMATING ? "BUTTON_ANIMATING" : "NORMAL_FLAVOR",
        rings.size(), ring.x(), ring.y(), ring.width(), ring.height());
#endif
    if (flavor >= FocusRing::NORMAL_ANIMATING) {
        SkMSec time = SkTime::GetMSecs();
        if (time < mRingAnimationEnd) {
            WebCore::IntRect bounds = node->bounds();
            bounds.inflate(SkScalarCeil(FOCUS_RING_OUTER_DIAMETER));
            postInvalidateDelayed(mRingAnimationEnd - time, bounds); 
        } else {
            mFollowedLink = false;
            flavor = (FocusRing::Flavor) (flavor - FocusRing::NORMAL_ANIMATING);
        }
    }
    FocusRing::DrawRing(canvas, rings, flavor);
}

OutOfFocusFix fixOutOfDateFocus(bool useReplay)
{
    if (mFrameCacheUI == NULL) {
        DBG_NAV_LOG("mFrameCacheUI == NULL");
        return DO_NOTHING;
    }
    const CachedFrame* cachedFrame = NULL;
    const CachedNode* cachedFocusNode = mFrameCacheUI->currentFocus(&cachedFrame);
    if (cachedFocusNode == NULL) {
        DBG_NAV_LOG("cachedFocusNode == NULL");
        return DO_NOTHING;
    }
    CachedRoot* webRoot = mViewImpl->mFrameCacheKit;
    if (webRoot == NULL) {
        DBG_NAV_LOG("webRoot == NULL");
        return DO_NOTHING;
    }
    int uiWidth = mFrameCacheUI->width();
    int webWidth = webRoot->width();
    if (uiWidth != webWidth) {
        DBG_NAV_LOGD("uiWidth=%d webWidth=%d", uiWidth, webWidth);
    } else {
        WebCore::IntRect cachedBounds = cachedFocusNode->bounds();
        const CachedFrame* webFrame = NULL;
        const CachedNode* webFocusNode = webRoot->currentFocus(&webFrame);
        if (webFocusNode != NULL && webRoot->containsFrame(cachedFrame)) {
            if (useReplay && mReplay.count() == 0) {
                DBG_NAV_LOG("mReplay.count() == 0");
                return DO_NOTHING;
            }
            if (webFocusNode->index() == cachedFocusNode->index() &&
                    webFrame->indexInParent() == cachedFrame->indexInParent()) 
            {
                DBG_NAV_LOG("index ==");
                return DO_NOTHING;
            }
            WebCore::IntRect webBounds = webFocusNode->bounds();
            if (cachedBounds.contains(webBounds)) {
                DBG_NAV_LOG("contains");
                return DO_NOTHING;
            }
            if (webBounds.contains(cachedBounds)) {
                DBG_NAV_LOG("webBounds contains");
                return DO_NOTHING;
            }
        }
        const CachedFrame* foundFrame = NULL;
        int x, y;
        const CachedNode* found = findAt(webRoot, cachedBounds, &foundFrame, &x, &y);
#if DEBUG_NAV_UI
        DBG_NAV_LOGD("found=%p (%d) frame=%p (%d)", 
            found, found != NULL ? found->index() : -1,
            foundFrame, foundFrame != NULL ? foundFrame->indexInParent() : -1);
        if (found != NULL) {
            WebCore::IntRect newBounds = found->bounds();
            DBG_NAV_LOGD("cachedBounds=(%d,%d,%d,%d) found=(%d,%d,%d,%d)", 
                cachedBounds.x(), cachedBounds.y(), cachedBounds.width(), cachedBounds.height(), 
                newBounds.x(), newBounds.y(), newBounds.width(), newBounds.height());
        }
#endif
        webRoot->setCachedFocus(const_cast<CachedFrame*>(foundFrame), 
            const_cast<CachedNode*>(found));
        if (found != NULL)
            webRoot->rootHistory()->setNavBounds(found->bounds());
        WebCore::Frame* framePointer = foundFrame != NULL ? (WebCore::Frame*) foundFrame->framePointer() : NULL;
        WebCore::Node* nodePointer = found != NULL ? (WebCore::Node*) found->nodePointer() : NULL;
        setFocusData(webRoot->generation(), framePointer, nodePointer, x, y, found == NULL);
        sendFinalFocus(framePointer, nodePointer, x, y);
        if (found && (found->isTextArea() || found->isTextField()))
            return UPDATE_TEXT_ENTRY;
    }
checkOldFocus:
    return cachedFocusNode->isTextArea() || cachedFocusNode->isTextField() ? CLEAR_TEXT_ENTRY : DO_NOTHING;
}

bool focusIsTextArea(FrameCachePermission allowNewer)
{
    CachedRoot* root = getFrameCache(allowNewer);
    if (root == NULL) {
        DBG_NAV_LOG("root==NULL"); 
        return false;
    }
    const CachedNode* focus = root->currentFocus();
    if (focus == NULL)
        return false;
    return focus->isTextArea() || focus->isTextField();
}

void focusRingBounds(WebCore::IntRect* bounds)
{
    DBG_NAV_LOGD("%s", "");
    CachedRoot* root = getFrameCache(DONT_ALLOW_NEWER);
    if (root != NULL) {
        const CachedNode* cachedNode = root->currentFocus();
        if (cachedNode != NULL) {
            cachedNode->focusRingBounds(bounds);
            DBG_NAV_LOGD("bounds={%d,%d,%d,%d}", bounds->x(), bounds->y(),
                bounds->width(), bounds->height());
            return;
        }
    }
    *bounds = WebCore::IntRect(0, 0, 0, 0);
}

CachedRoot* getFrameCache(FrameCachePermission allowNewer)
{
    if (mViewImpl->mUpdatedFrameCache == false)
        return mFrameCacheUI;
    mViewImpl->gRecomputeFocusMutex.lock();
    bool recomputeInProgress = mViewImpl->mRecomputeEvents.size() > 0;
    mViewImpl->gRecomputeFocusMutex.unlock();
    if (allowNewer != ALLOW_NEWEST && recomputeInProgress)
        return mFrameCacheUI;
    if (allowNewer == DONT_ALLOW_NEWER && mViewImpl->mLastGeneration < mGeneration) 
        return mFrameCacheUI;
    DBG_NAV_LOGD("%s", "mViewImpl->mUpdatedFrameCache == true");
    mViewImpl->gFrameCacheMutex.lock();
    OutOfFocusFix fix = DO_NOTHING;
    if (allowNewer != DONT_ALLOW_NEWER)
        fix = fixOutOfDateFocus(mViewImpl->mUseReplay);
    delete mFrameCacheUI;
    delete mNavPictureUI;
    mViewImpl->mUpdatedFrameCache = false;
    mFrameCacheUI = mViewImpl->mFrameCacheKit;
    mNavPictureUI = mViewImpl->mNavPictureKit;
    mViewImpl->mFrameCacheKit = NULL;
    mViewImpl->mNavPictureKit = NULL;
    mViewImpl->gFrameCacheMutex.unlock();
    if (fix == UPDATE_TEXT_ENTRY)
        updateTextEntry();
    else if (fix == CLEAR_TEXT_ENTRY)
        clearTextEntry();
    return mFrameCacheUI;
}

int getScaledMaxXScroll()
{
    LOG_ASSERT(mJavaGlue.mObj, "A java object was not associated with this native WebView!");
    JNIEnv* env = javavm_to_jnienv(mJavaGlue.mJVM);
    int result = env->CallIntMethod(mJavaGlue.mObj, mJavaGlue.mGetScaledMaxXScroll);
    checkException(env);    
    return result;
}

int getScaledMaxYScroll()
{
    LOG_ASSERT(mJavaGlue.mObj, "A java object was not associated with this native WebView!");
    JNIEnv* env = javavm_to_jnienv(mJavaGlue.mJVM);
    int result = env->CallIntMethod(mJavaGlue.mObj, mJavaGlue.mGetScaledMaxYScroll);
    checkException(env);    
    return result;
}

void getVisibleRect(WebCore::IntRect* rect)
{
    LOG_ASSERT(mJavaGlue.mObj, "A java object was not associated with this native WebView!");
    JNIEnv* env = javavm_to_jnienv(mJavaGlue.mJVM);
    jobject jRect = env->CallObjectMethod(mJavaGlue.mObj, mJavaGlue.mGetVisibleRect);
    checkException(env);    
    int left = (int) env->GetIntField(jRect, mJavaGlue.mRectLeft);
    checkException(env);
    rect->setX(left);
    int top = (int) env->GetIntField(jRect, mJavaGlue.mRectTop);
    checkException(env);
    rect->setY(top);
    int width = (int) env->CallIntMethod(jRect, mJavaGlue.mRectWidth);
    checkException(env);
    rect->setWidth(width);
    int height = (int) env->CallIntMethod(jRect, mJavaGlue.mRectHeight);
    checkException(env);
    rect->setHeight(height);
    env->DeleteLocalRef(jRect);
    checkException(env);
}

bool hasSrcUrl()
{
    const CachedRoot* root = getFrameCache(DONT_ALLOW_NEWER);
    if (NULL == root)
        return false;
    const CachedNode* cachedNode = root->currentFocus();
    return NULL == cachedNode ? false : cachedNode->anchorHasSrcUrl();
}

static CachedFrame::Direction KeyToDirection(KeyCode keyCode)
{
    switch (keyCode) {
        case kKeyCodeDpadRight:
            DBG_NAV_LOGD("keyCode=%s", "right");
            return CachedFrame::RIGHT;
        case kKeyCodeDpadLeft:
            DBG_NAV_LOGD("keyCode=%s", "left");
            return CachedFrame::LEFT;
        case kKeyCodeDpadDown:
            DBG_NAV_LOGD("keyCode=%s", "down");
            return CachedFrame::DOWN;
        case kKeyCodeDpadUp:
            DBG_NAV_LOGD("keyCode=%s", "up");
            return CachedFrame::UP;
        default:
            LOGD("------- bad key sent to WebView::moveFocus");
            return CachedFrame::UNINITIALIZED;
    }
}

bool invalidFrame(WebCore::Frame* frame, const CachedRoot* root)
{
    if (frame == NULL)
        return false;
    int frameBuild = mViewImpl->retrieveFrameGeneration(frame);
    int rootBuild = root->generation();
    return frameBuild > rootBuild;
}

bool isImage(int x, int y)
{
    const CachedRoot* root = getFrameCache(DONT_ALLOW_NEWER);
    return root ? root->isImage(x, y) : false;
}

/* returns true if the key had no effect (neither scrolled nor changed focus) */
bool moveFocus(int keyCode, int count, bool ignoreScroll, bool inval,
    void* lastSentFocus, const WebCore::IntRect* lastSentBounds)
{
    CachedRoot* root = getFrameCache(ALLOW_NEWER);
    if (root == NULL) {
        DBG_NAV_LOG("root == NULL");
        setFocusData(0, NULL, NULL, 0, 0, true);
        sendKitFocus(); // will build cache and retry
        FirstMoveFocusParams params;
        params.d.mTrigger = CommonParams::FIRST_MOVE_FOCUS_PARAMS;
        params.d.mGeneration = mGeneration;
        params.mKeyCode = keyCode;
        params.mCount = count;
        params.mIgnoreScroll = ignoreScroll;
        mReplay.add(params.d, sizeof(params));
        return true;
    }
    
    CachedFrame::Direction direction = KeyToDirection((KeyCode) keyCode);
    const CachedFrame* cachedFrame, * oldFrame = NULL;
    const CachedNode* focus = root->currentFocus(&oldFrame);
    WebCore::IntPoint focusLocation = root->focusLocation();
    DBG_NAV_LOGD("old focus %d (nativeNode=%p) focusLocation={%d, %d}", 
        focus ? focus->index() : 0,
        focus ? focus->nodePointer() : NULL, focusLocation.x(), focusLocation.y());
    WebCore::IntRect visibleRect;
    getVisibleRect(&visibleRect);
    DBG_NAV_LOGD("getVisibleRect %d,%d,%d,%d", 
        visibleRect.x(), visibleRect.y(), visibleRect.width(), visibleRect.height());
    root->setVisibleRect(visibleRect);
    int xMax = getScaledMaxXScroll();
    int yMax = getScaledMaxYScroll();
    root->setMaxScroll(xMax, yMax);
    CachedHistory savedHistory = *root->rootHistory();
#if 0 && DUMP_NAV_CACHE
    savedHistory.mDebug.print(root);
#endif
    bool oldNodeIsTextArea = focusIsTextArea(DONT_ALLOW_NEWER);
    const CachedNode* cachedNode = NULL;
    int dx = 0;
    int dy = 0;
    int counter = count;
    root->setScrollOnly(mFollowedLink);
    while (--counter >= 0) {
        WebCore::IntPoint scroll = WebCore::IntPoint(0, 0);
        cachedNode = root->moveFocus(direction, &cachedFrame, &scroll);
        dx += scroll.x();
        dy += scroll.y();
    }
    DBG_NAV_LOGD("new focus %d (nativeNode=%p) focusLocation={%d, %d}", 
        cachedNode ? cachedNode->index() : 0,
        cachedNode ? cachedNode->nodePointer() : NULL, root->focusLocation().x(),
            root->focusLocation().y());
    // If !mHeightCanMeasure (such as in the browser), we want to scroll no
    // matter what
#if 0 && DUMP_NAV_CACHE
    root->rootHistory()->mDebug.print(root);
#endif
    if (ignoreScroll == false && (mHeightCanMeasure == false ||
            NULL == cachedNode || 
            (focus && focus->nodePointer() == cachedNode->nodePointer()))) 
    {
        if (count == 1 && dx != 0 && dy == 0 && -mLastDx == dx && 
                SkTime::GetMSecs() - mLastDxTime < 1000)
            root->checkForJiggle(&dx);
        DBG_NAV_LOGD("scrollBy %d,%d", dx, dy);
        if ((dx | dy) != 0)
            this->scrollBy(dx, dy);
        mLastDx = dx;
        mLastDxTime = SkTime::GetMSecs();
        ignoreScroll = true; // if move re-executes, don't scroll the second time
    }
    bool result = false;
    if (cachedNode != NULL) {
        WebCore::IntPoint pos;
        root->setCachedFocus((CachedFrame*) cachedFrame, (CachedNode*) cachedNode);
        root->getSimulatedMousePosition(&pos);
        if (lastSentFocus == cachedNode->nodePointer() && lastSentBounds &&
                *lastSentBounds == cachedNode->bounds())
        {
            sendFinalFocus((WebCore::Frame*) cachedFrame->framePointer(), 
                (WebCore::Node*) cachedNode->nodePointer(), pos.x(), pos.y());
        } else {
            setFocusData(root->generation(),
                (WebCore::Frame*) cachedFrame->framePointer(), 
                (WebCore::Node*) cachedNode->nodePointer(), pos.x(), pos.y(),
                true);
            sendKitFocus();
            if (inval)
                viewInvalidate();
            MoveFocusParams params;
            params.d.d.mTrigger = CommonParams::MOVE_FOCUS_PARAMS;
            params.d.d.mGeneration = mGeneration;
            params.c.setFocus(focus, oldFrame, root, focusLocation);
            params.mSentFocus = cachedNode->nodePointer();
            params.mSentBounds = cachedNode->bounds();
            params.mVisibleRect = visibleRect;
            params.mHistory = savedHistory;
            DBG_NAV_LOGD("history.mDidFirstLayout=%s", 
                    params.mHistory.didFirstLayout() ? "true" : "false");
            params.mXMax = xMax;
            params.mYMax = yMax;
            params.d.mKeyCode = keyCode;
            params.d.mCount = count;
            params.d.mIgnoreScroll = ignoreScroll;
            mReplay.add(params.d.d, sizeof(params));
        }
    } else {
        if (visibleRect.intersects(root->rootHistory()->focusBounds()) == false) {
            setFocusData(root->generation(), NULL, NULL, 0, 0, true);
            sendKitFocus(); // will build cache and retry
        }
        FirstMoveFocusParams params;
        params.d.mTrigger = CommonParams::FIRST_MOVE_FOCUS_PARAMS;
        params.d.mGeneration = mGeneration;
        params.mKeyCode = keyCode;
        params.mCount = count;
        params.mIgnoreScroll = ignoreScroll;
        mReplay.add(params.d, sizeof(params));
        int docHeight = root->documentHeight();
        int docWidth = root->documentWidth();
        if (visibleRect.bottom() + dy > docHeight)
            dy = docHeight - visibleRect.bottom();
        else if (visibleRect.y() + dy < 0)
            dy = -visibleRect.y();
        if (visibleRect.right() + dx > docWidth)
            dx = docWidth - visibleRect.right();
        else if (visibleRect.x() < 0)
            dx = -visibleRect.x();
        result = direction == CachedFrame::LEFT ? dx >= 0 :
            direction == CachedFrame::RIGHT ? dx <= 0 :
            direction == CachedFrame::UP ? dy >= 0 : dy <= 0;
    }
    if (focusIsTextArea(DONT_ALLOW_NEWER))
        updateTextEntry();
    else if (oldNodeIsTextArea)
        clearTextEntry();
    return result;
}

void notifyFocusSet(FrameCachePermission inEditingMode)
{
    if (focusIsTextArea(inEditingMode))
        updateTextEntry();
    else if (inEditingMode)
        clearTextEntry();
#if DEBUG_NAV_UI
    if (mFrameCacheUI != NULL) {
        const CachedNode* focus = mFrameCacheUI->currentFocus();
        DBG_NAV_LOGD("focus %d (nativeNode=%p)", 
            focus ? focus->index() : 0,
            focus ? focus->nodePointer() : NULL);
    }
#endif
}

void notifyProgressFinished()
{
    DBG_NAV_LOGD("focusIsTextArea=%d", focusIsTextArea(DONT_ALLOW_NEWER)); 
    updateTextEntry();
#if DEBUG_NAV_UI
    if (mFrameCacheUI != NULL) {
        const CachedNode* focus = mFrameCacheUI->currentFocus();
        DBG_NAV_LOGD("focus %d (nativeNode=%p)", 
            focus ? focus->index() : 0,
            focus ? focus->nodePointer() : NULL);
    }
#endif
}

void recomputeFocus()
{
    int generation;
    do {
        mViewImpl->gRecomputeFocusMutex.lock();
        if (mViewImpl->mRecomputeEvents.size() == 0) {
            mViewImpl->gRecomputeFocusMutex.unlock();
            return;
        }
        generation = mViewImpl->mRecomputeEvents.first();
        mViewImpl->mRecomputeEvents.remove(0);
        mViewImpl->gRecomputeFocusMutex.unlock();
        DBG_NAV_LOGD("generation=%d", generation);
        CachedRoot* root = getFrameCache(ALLOW_NEWEST);
        if (root == NULL) {
            DBG_NAV_LOG("root==NULL"); 
            return;
        }
        LargestParams storage;
        const CommonParams& params = storage.d.d;
        char* pos = mReplay.position();
        while (mReplay.retrieve(&storage.d.d) < generation)
            DBG_NAV_LOGD("dropped ", params.mGeneration);
        if (params.mGeneration > generation) {
            DBG_NAV_LOGD("params.mGeneration=%d > generation=%d",
                params.mGeneration, generation);
            mReplay.rewind(pos);
            return;
        }
        int lastAdd = mReplay.lastAdd();
        do {
            LOG_ASSERT(params.mTrigger != CommonParams::NO_DATA, "expected data");
            bool inval = generation == mGeneration;
            switch (params.mTrigger) {
                case CommonParams::CLEAR_FOCUS_PARAMS: {
                    const ClearFocusParams& sParams = *(ClearFocusParams*) &storage;
                    const CacheParams& cParams = sParams.c;
                    if (invalidFrame(cParams.mFrame, root)) {
                        DBG_NAV_LOGD("dropped %s generation=%d", 
                            TriggerNames[params.mTrigger], generation);
                        return;
                    }
                    root->setFocus(cParams.mFrame, cParams.mNode, cParams.mX, cParams.mY);
                    clearFocus(sParams.mX, sParams.mY, inval);
                    DBG_NAV_LOGD("clearFocus(x,y)={%d,%d}", sParams.mX, sParams.mY);
                    } break;
                case CommonParams::MOTION_UP_PARAMS: {
                    const MotionUpParams& mParams = *(MotionUpParams*) &storage;
                //    const CacheParams& cParams = mParams.c;
                //    if (invalidFrame(cParams.mFrame, root) == false)
                //        root->setFocus(cParams.mFrame, cParams.mNode, 
                //            cParams.mX, cParams.mY);
                    motionUp(mParams.mX, mParams.mY, mParams.mSlop, mParams.mIsClick, inval, true);
                    DBG_NAV_LOGD("motionUp mX=%d mY=%d", mParams.mX, mParams.mY);
                    } break;
                case CommonParams::FIRST_MOVE_FOCUS_PARAMS: {
                    if (invalidFrame((WebCore::Frame*) root->framePointer(), root)) {
                        DBG_NAV_LOGD("dropped %s generation=%d", 
                            TriggerNames[params.mTrigger], generation);
                        return;
                    }
                    const FirstMoveFocusParams& fParams = *(FirstMoveFocusParams*) &storage;
                    DBG_NAV_LOGD("first moveFocus keyCode=%d count=%d" 
                        " ignoreScroll=%s", fParams.mKeyCode, fParams.mCount, 
                        fParams.mIgnoreScroll ? "true" : "false");
                    moveFocus(fParams.mKeyCode, fParams.mCount, 
                        fParams.mIgnoreScroll, inval, NULL, NULL);
                    } break;
                case CommonParams::MOVE_FOCUS_PARAMS: {
                    const MoveFocusParams& mParams = *(MoveFocusParams*) &storage;
                    const CacheParams& cParams = mParams.c;
                    if (invalidFrame(cParams.mFrame, root)) {
                        DBG_NAV_LOGD("dropped %s generation=%d", 
                            TriggerNames[params.mTrigger], generation);
                        return;
                    }
                    DBG_NAV_LOGD("moveFocus keyCode=%d count=%d ignoreScroll=%s "
                        "history.mDidFirstLayout=%s", mParams.d.mKeyCode, 
                        mParams.d.mCount, mParams.d.mIgnoreScroll ? "true" : "false",
                        mParams.mHistory.didFirstLayout() ? "true" : "false");
                    if (root->setFocus(cParams.mFrame, cParams.mNode, 
                            cParams.mX, cParams.mY) == false) {
                        DBG_NAV_LOGD("can't restore focus frame=%p node=%p", 
                            "x=%d y=%d %s", cParams.mFrame, cParams.mNode, 
                            cParams.mX, cParams.mY, TriggerNames[params.mTrigger]);
                        return;
                    }
                    root->setVisibleRect(mParams.mVisibleRect);
                    root->setMaxScroll(mParams.mXMax, mParams.mYMax);
                    *root->rootHistory() = mParams.mHistory;
                    moveFocus(mParams.d.mKeyCode, mParams.d.mCount, 
                        mParams.d.mIgnoreScroll, inval,
                        mParams.mSentFocus, &mParams.mSentBounds);
                    } break;
                default:
                    LOG_ASSERT(0, "unknown trigger");
            }
            if (params.mGeneration >= lastAdd)
                break;
            root = getFrameCache(DONT_ALLOW_NEWER); // re-execution may have retrieved newer cache
            mReplay.retrieve(&storage.d.d);
            DBG_NAV_LOGD("continuation mGeneration %d", params.mGeneration);
        } while (true);
    } while (true);
}

void resetFocus()
{
    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__); 
    CachedRoot* root = getFrameCache(ALLOW_NEWER);
    if (root == NULL)
        return;
    root->setCachedFocus(NULL, NULL);
}

const CachedNode* findAt(CachedRoot* root, const WebCore::IntRect& rect, 
    const CachedFrame** framePtr, int* rxPtr, int* ryPtr)
{
    *rxPtr = 0;
    *ryPtr = 0;
    *framePtr = NULL;
    if (root == NULL)
        return NULL;
    WebCore::IntRect visibleRect;
    getVisibleRect(&visibleRect);
    root->setVisibleRect(visibleRect);
    return root->findAt(rect, framePtr, rxPtr, ryPtr);
}

void selectBestAt(const WebCore::IntRect& rect)
{
    const CachedFrame* frame;
    int rx, ry;
    CachedRoot* root = getFrameCache(DONT_ALLOW_NEWER);
    const CachedNode* node = findAt(root, rect, &frame, &rx, &ry);
    int rootGeneration = root != NULL ? root->generation() : 0;
    setFocusData(rootGeneration, 
        frame ? (WebCore::Frame*) frame->framePointer() : NULL, 
        node ? (WebCore::Node*) node->nodePointer() : NULL, rx, ry, false);
    if (node == NULL) {
        DBG_NAV_LOGD("no nodes found root=%p", root);
        if (root != NULL) {
            root->clearFocus();
            root->setCachedFocus(NULL, NULL);
        }
        sendKitFocus();
        viewInvalidate();
        clearTextEntry();
        return;
    }
    DBG_NAV_LOGD("CachedNode:%p (%d)", node, node->index());
    const CachedFrame* oldFrame = NULL;
    const CachedNode* oldFocusNode = root->currentFocus(&oldFrame);
    bool oldNodeIsTextArea = focusIsTextArea(DONT_ALLOW_NEWER);
    root->setCachedFocus(const_cast<CachedFrame*>(frame), 
        const_cast<CachedNode*>(node));
    viewInvalidate();
    if (focusIsTextArea(DONT_ALLOW_NEWER))
        updateTextEntry();
    else if (oldNodeIsTextArea)
        clearTextEntry();
}

WebCore::IntRect getNavBounds()
{
    CachedRoot* root = getFrameCache(DONT_ALLOW_NEWER);
    if (NULL == root)
        return WebCore::IntRect(0, 0, 0, 0);
    return root->rootHistory()->navBounds();
}

void setNavBounds(const WebCore::IntRect& rect)
{
    CachedRoot* root = getFrameCache(DONT_ALLOW_NEWER);
    if (NULL == root)
        return;
    root->rootHistory()->setNavBounds(rect);
}

void markNodeInvalid(WebCore::Node* node)
{
    DBG_NAV_LOGD("node=%p", node);
    mInvalidNode = node;
    viewInvalidate();
}

void motionUp(int x, int y, int slop, bool isClick, bool inval, bool retry)
{
    mFollowedLink = false;
    const CachedFrame* frame;
    WebCore::IntRect rect = WebCore::IntRect(x - slop, y - slop, slop * 2, slop * 2);
    int rx, ry;
    CachedRoot* root = getFrameCache(ALLOW_NEWER);
    const CachedNode* result = findAt(root, rect, &frame, &rx, &ry);
    if (result == NULL) {
        DBG_NAV_LOGD("no nodes found root=%p", root);
        int rootGeneration = 0;
        if (root != NULL) {
            root->clearFocus();
            rootGeneration = root->generation();
            if (retry == false) { // scroll first time only
                int dx = root->checkForCenter(x, y);
                if (dx != 0) {
                    scrollBy(dx, 0);
                    retry = true; // don't recompute later since we scrolled
                }
            }
        }
        sendMotionUp(rootGeneration, frame ? 
            (WebCore::Frame*) frame->framePointer() : NULL,
            NULL, x, y, slop, isClick, retry);
        if (inval)
            viewInvalidate();
        if (retry == false) {
            MotionUpParams params;
            params.d.mTrigger = CommonParams::MOTION_UP_PARAMS;
            params.d.mGeneration = mGeneration;
            params.mX = x;
            params.mY = y;
            params.mSlop = slop;
	    params.mIsClick = isClick;
            mReplay.add(params.d, sizeof(params));
        }
        clearTextEntry();
        return;
    }
    DBG_NAV_LOGD("CachedNode:%p (%d) x=%d y=%d rx=%d ry=%d", result, 
        result->index(), x, y, rx, ry);
 //   const CachedFrame* oldFrame = NULL;
 //   const CachedNode* oldFocusNode = root->currentFocus(&oldFrame);
 //   WebCore::IntPoint focusLocation = root->focusLocation();
    bool oldNodeIsTextArea = retry == false && focusIsTextArea(DONT_ALLOW_NEWER);
    root->setCachedFocus(const_cast<CachedFrame*>(frame), 
        const_cast<CachedNode*>(result));
    bool newNodeIsTextArea = focusIsTextArea(DONT_ALLOW_NEWER);
    const WebCore::String& exported = result->getExport();
    if (exported.length() == 0 || newNodeIsTextArea) {
        sendMotionUp(root->generation(), 
            frame ? (WebCore::Frame*) frame->framePointer() : NULL, 
            result ? (WebCore::Node*) result->nodePointer() : NULL, rx, ry, 
            slop, isClick, retry);
        if (inval)
            viewInvalidate();
        if (retry == false) {
            MotionUpParams params;
            params.d.mTrigger = CommonParams::MOTION_UP_PARAMS;
            params.d.mGeneration = mGeneration;
            params.mX = x;
            params.mY = y;
            params.mSlop = slop;
            params.mIsClick = isClick;
        //    params.c.setFocus(oldFocusNode, oldFrame, root, focusLocation);
            mReplay.add(params.d, sizeof(params));
        }
    } else if (inval)
        viewInvalidate();
    if (newNodeIsTextArea)
        updateTextEntry();
    else {
        if (isClick)
            setFollowedLink(true);
        if (oldNodeIsTextArea)
            clearTextEntry();
    }
}

void setFollowedLink(bool followed)
{
    if ((mFollowedLink = followed) != false) {
        mRingAnimationEnd = SkTime::GetMSecs() + 500;
        viewInvalidate();
    }
}

void setHeightCanMeasure(bool measure) 
{ 
    mHeightCanMeasure = measure;
}

SkIRect mSelStart, mSelEnd;
SkRegion mSelRegion;
#define MIN_ARROW_DISTANCE (20 * 20)

void moveSelection(int x, int y, bool extendSelection) {
    CachedRoot* root = getFrameCache(DONT_ALLOW_NEWER);
    if (NULL == root)
        return;
    const SkPicture& picture = *mNavPictureUI;
    WebCore::IntRect r;
    getVisibleRect(&r);
    SkIRect area;
    area.set(r.x(), r.y(), r.right(), r.bottom());
    if (extendSelection == false)
        mSelStart = mSelEnd = CopyPaste::findClosest(picture, area, x, y);
    else
        mSelEnd = CopyPaste::findClosest(picture, area, x, y);
    DBG_NAV_LOGD("x=%d y=%d extendSelection=%s mSelStart=(%d, %d, %d, %d)"
        " mSelEnd=(%d, %d, %d, %d)", x, y, extendSelection ? "true" : "false", 
        mSelStart.fLeft, mSelStart.fTop, mSelStart.fRight, mSelStart.fBottom, 
        mSelEnd.fLeft, mSelEnd.fTop, mSelEnd.fRight, mSelEnd.fBottom);
}

const SkRegion& getSelection() {
    return mSelRegion;
}

void drawSelection(SkCanvas* canvas, int x, int y, bool extendSelection) {
    if (extendSelection == false) {
        int dx = x - mSelStart.fLeft;
        dx *= dx;
        int otherX = x - mSelStart.fRight;
        if (dx > (otherX *= otherX))
            dx = otherX;
        int dy = y - mSelStart.fTop;
        int dist = dx * dx + dy * dy;
        if (dist > MIN_ARROW_DISTANCE)
            drawSelectionArrow(canvas, x, y);
        else 
            drawSelectionPointer(canvas, x, y, true);
    } else {
        drawSelectionRegion(canvas);
        drawSelectionPointer(canvas, x, y, false);
    }
}

void drawSelectionRegion(SkCanvas* canvas) {
    CachedRoot* root = getFrameCache(DONT_ALLOW_NEWER);
    if (NULL == root)
        return;
    WebCore::IntRect r;
    getVisibleRect(&r);
    SkIRect area;
    area.set(r.x(), r.y(), r.right(), r.bottom());
    mSelRegion.setEmpty();
    CopyPaste::buildSelection(*mNavPictureUI, area, mSelStart, mSelEnd, &mSelRegion);
    SkPath path;
    mSelRegion.getBoundaryPath(&path);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SkColorSetARGB(0x40, 255, 51, 204));
    canvas->drawPath(path, paint);
}

void drawSelectionPointer(SkCanvas* canvas, int x, int y, bool gridded) {
    SkPath path;
    getSelectionCaret(&path);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(SK_ColorBLACK);
    SkPixelXorXfermode xorMode(SK_ColorWHITE);
    paint.setXfermode(&xorMode);
    int sc = canvas->save();
    if (gridded) {
        bool useLeft = x <= (mSelStart.fLeft + mSelStart.fRight) >> 1;
        canvas->translate(SkIntToScalar(useLeft ? mSelStart.fLeft : 
            mSelStart.fRight), SkIntToScalar(mSelStart.fTop));
    } else
        canvas->translate(SkIntToScalar(x), SkIntToScalar(y));
    canvas->drawPath(path, paint);
    canvas->restoreToCount(sc);
}

void drawSelectionArrow(SkCanvas* canvas, int x, int y) {
    SkPath path;
    getSelectionArrow(&path);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(SK_ColorBLACK);
    paint.setStrokeWidth(SK_Scalar1 * 2);
    int sc = canvas->save();
    canvas->translate(SkIntToScalar(x), SkIntToScalar(y));
    canvas->drawPath(path, paint);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SK_ColorWHITE);
    canvas->drawPath(path, paint);    
    canvas->restoreToCount(sc);
}

void getSelectionArrow(SkPath* path) {
    const int arrow[] = {
        0, 14, 3, 11, 5, 15, 9, 15, 7, 11, 11, 11
    };
    for (unsigned index = 0; index < sizeof(arrow)/sizeof(arrow[0]); index += 2)
        path->lineTo(SkIntToScalar(arrow[index]), SkIntToScalar(arrow[index + 1]));
    path->close();
}

void getSelectionCaret(SkPath* path) {
    SkScalar height = SkIntToScalar(mSelStart.fBottom - mSelStart.fTop);
    SkScalar dist = height / 4;
    path->lineTo(0, height);
    SkScalar bottom = height + dist;
    path->lineTo(-dist, bottom);
    SkScalar edge = bottom - SK_Scalar1/2;
    path->moveTo(-dist, edge);
    path->lineTo(dist, edge);
    path->moveTo(dist, bottom);
    path->lineTo(0, height);
}

void sendFinalFocus(WebCore::Frame* framePtr, WebCore::Node* nodePtr, int x, int y)
{
    DBG_NAV_LOGD("framePtr=%p nodePtr=%p x=%d y=%d", framePtr, nodePtr, x, y);
    LOG_ASSERT(mJavaGlue.mObj, "A java object was not associated with this native WebView!");
    JNIEnv* env = javavm_to_jnienv(mJavaGlue.mJVM);
    env->CallVoidMethod(mJavaGlue.mObj, mJavaGlue.mSendFinalFocus, 
        (jint) framePtr, (jint) nodePtr, x, y);
    checkException(env);
}

void sendKitFocus()
{
    LOG_ASSERT(mJavaGlue.mObj, "A java object was not associated with this native WebView!");
    JNIEnv* env = javavm_to_jnienv(mJavaGlue.mJVM);
    env->CallVoidMethod(mJavaGlue.mObj, mJavaGlue.mSendKitFocus);
    checkException(env);
}

void sendMotionUp(int buildGeneration, 
    WebCore::Frame* framePtr, WebCore::Node* nodePtr, int x, int y, int slop,
    bool isClick, bool retry)
{
    mViewImpl->mTouchGeneration = mViewImpl->mGeneration = ++mGeneration;
    DBG_NAV_LOGD("buildGeneration=%d mGeneration=%d framePtr=%p nodePtr=%p"
        " x=%d y=%d slop=%d", buildGeneration, 
        mGeneration, framePtr, nodePtr, x, y, slop);
    LOG_ASSERT(mJavaGlue.mObj, "A WebView was not associated with this WebViewNative!");
    JNIEnv* env = javavm_to_jnienv(mJavaGlue.mJVM);
    env->CallVoidMethod(mJavaGlue.mObj, mJavaGlue.mSendMotionUp, mGeneration,
        buildGeneration, (jint) framePtr, (jint) nodePtr, x, y, slop, isClick, retry);
    checkException(env);
}

void setFocusData(int buildGeneration, WebCore::Frame* framePtr, 
    WebCore::Node* nodePtr, int x, int y, bool ignoreNullFocus)
{
    mViewImpl->mMoveGeneration = mViewImpl->mGeneration = ++mGeneration;
    DBG_NAV_LOGD("moveGeneration=%d buildGeneration=%d framePtr=%p nodePtr=%p"
        " x=%d y=%d", mGeneration, buildGeneration, framePtr, nodePtr, x, y);
    LOG_ASSERT(mJavaGlue.mObj, "A java object was not associated with this native WebView!");
    JNIEnv* env = javavm_to_jnienv(mJavaGlue.mJVM);
    env->CallVoidMethod(mJavaGlue.mObj, mJavaGlue.mSetFocusData, mGeneration,
        buildGeneration, (jint) framePtr, (jint) nodePtr, x, y, ignoreNullFocus);
    checkException(env);
}

void scrollBy(int dx, int dy)
{
    LOG_ASSERT(mJavaGlue.mObj, "A java object was not associated with this native WebView!");
    
    JNIEnv* env = javavm_to_jnienv(mJavaGlue.mJVM);
    env->CallVoidMethod(mJavaGlue.mObj, mJavaGlue.mScrollBy, dx, dy);
    checkException(env);
}

bool updateFocusNode(JNIEnv* env)
{
    CachedRoot* root = getFrameCache(DONT_ALLOW_NEWER);
    if (root == NULL) {
        DBG_NAV_LOG("root==NULL"); 
        return false;
    }
    const CachedFrame* cachedFrame = NULL;
    const CachedNode* cachedFocusNode = root->currentFocus(&cachedFrame);
    if (cachedFocusNode == NULL) {
        DBG_NAV_LOG("cachedFocusNode==NULL"); 
        return false;
    }
    DBG_NAV_LOGD("cachedFocusNode=%d (nodePointer=%p)", 
        cachedFocusNode->index(),
        cachedFocusNode->nodePointer());
    jobject focusnode = env->GetObjectField(mJavaGlue.mObj, mJavaGlue.mFocusNode);
    LOG_ASSERT(focusnode, "Could not find WebView's FocusNode");
    
    bool isTextArea = cachedFocusNode->isTextArea();
    bool isTextField = cachedFocusNode->isTextField();
    int maxLength;
    jstring jName;
    if (isTextField) {
        maxLength = cachedFocusNode->maxLength();
        const WebCore::String& name = cachedFocusNode->name();
        jName = env->NewString((jchar*)name.characters(), name.length());
    } else {
        maxLength = -1;
        jName = NULL;
    }
    WebCore::IntRect bounds = cachedFocusNode->bounds();
    WebCore::String value = cachedFocusNode->getExport();
    jstring val = !value.isEmpty() ? env->NewString((jchar *)value.characters(), value.length()) : NULL;
    env->CallVoidMethod(focusnode, mJavaGlue.mSetAll, isTextField, isTextArea, cachedFocusNode->isPassword(), 
            cachedFocusNode->isAnchor(), cachedFocusNode->isRtlText(), maxLength, cachedFocusNode->textSize(),
            bounds.x(), bounds.y(), bounds.right(), bounds.bottom(), (int)(cachedFocusNode->nodePointer()),
            (int)(cachedFrame->framePointer()), val, jName, root->textGeneration());
    env->DeleteLocalRef(val);
    env->DeleteLocalRef(focusnode);
    if (isTextField)
        env->DeleteLocalRef(jName);
    return true;
}

void updateTextEntry()
{
    JNIEnv* env = javavm_to_jnienv(mJavaGlue.mJVM);
    env->CallVoidMethod(mJavaGlue.mObj, mJavaGlue.mUpdateTextEntry);
    checkException(env);
}

void viewInvalidate()
{
    JNIEnv* env = javavm_to_jnienv(mJavaGlue.mJVM);
    env->CallVoidMethod(mJavaGlue.mObj, mJavaGlue.mViewInvalidate);
    checkException(env);
}

void postInvalidateDelayed(int64_t delay, const WebCore::IntRect& bounds)
{
    JNIEnv* env = javavm_to_jnienv(mJavaGlue.mJVM);
    env->CallVoidMethod(mJavaGlue.mObj, mJavaGlue.mPostInvalidateDelayed, 
        delay, bounds.x(), bounds.y(), bounds.right(), bounds.bottom());
    checkException(env);
}

private: // local state for WebView
    // private to getFrameCache(); other functions operate in a different thread
    CachedRoot* mFrameCacheUI; // navigation data ready for use
    FocusReplay mReplay;
    WebViewCore* mViewImpl;
    SkTDArray<Container*>* mUIButtons;
    WebCore::Node* mInvalidNode;
    int mGeneration; // associate unique ID with sent kit focus to match with ui
    SkPicture* mNavPictureUI;
    bool mFollowedLink;
    SkMSec mRingAnimationEnd;
    // Corresponds to the same-named boolean on the java side.
    bool mHeightCanMeasure;
    int mLastDx;
    SkMSec mLastDxTime;
}; // end of WebView class

/*
 * Native JNI methods
 */
static jstring WebCoreStringToJString(JNIEnv *env, WebCore::String string)
{
    int length = string.length();
    if (0 == length)
        return 0;
    jstring ret = env->NewString((jchar *)string.characters(), length);
    env->DeleteLocalRef(ret);
    return ret;
}

#ifdef BUCKET_EXPERIMENT
static void nativeClearContentPicture(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->clearContentPicture();
}
#endif

static void nativeClearFocus(JNIEnv *env, jobject obj, int x, int y)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->clearFocus(x, y, true);
}

static void nativeCreate(JNIEnv *env, jobject obj, int viewImpl)
{
    WebView* webview = new WebView(env, obj, viewImpl);
    // NEED THIS OR SOMETHING LIKE IT!
    //Release(obj);
}

static void nativeDebugDump(JNIEnv *env, jobject obj)
{
#if DUMP_NAV_CACHE
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->debugDump();
#endif
}

#ifdef BUCKET_EXPERIMENT
static void nativeDrawContentPicture(JNIEnv *env, jobject obj,
                                       jobject canv)
{
    SkCanvas* canvas = GraphicsJNI::getNativeCanvas(env, canv);
    if (canv == NULL) {
        DBG_NAV_LOG("canv==NULL"); 
        return;
    }
    WebView* view = GET_NATIVE_VIEW(env, obj);
    if (view == NULL) {
        DBG_NAV_LOG("view==NULL"); 
        return;
    }
    view->drawContentPicture(canvas);
}
#endif

static void nativeDrawFocusRing(JNIEnv *env, jobject obj,
                                       jobject canv)
{
    SkCanvas* canvas = GraphicsJNI::getNativeCanvas(env, canv);
    if (canv == NULL) {
        DBG_NAV_LOG("canv==NULL"); 
        return;
    }
    WebView* view = GET_NATIVE_VIEW(env, obj);
    if (view == NULL) {
        DBG_NAV_LOG("view==NULL"); 
        return;
    }
    view->drawFocusRing(canvas);
}

static void nativeDrawSelection(JNIEnv *env, jobject obj,
                                       jobject canv, jint x, jint y, bool ex)
{
    SkCanvas* canvas = GraphicsJNI::getNativeCanvas(env, canv);
    if (canv == NULL) {
        DBG_NAV_LOG("canv==NULL"); 
        return;
    }
    WebView* view = GET_NATIVE_VIEW(env, obj);
    if (view == NULL) {
        DBG_NAV_LOG("view==NULL"); 
        return;
    }
    view->drawSelection(canvas, x, y, ex);
}

static bool nativeHasSrcUrl(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return view->hasSrcUrl();
}

static bool nativeIsImage(JNIEnv *env, jobject obj, jint x, jint y)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return view->isImage(x,y);
}

static WebCore::IntRect jrect_to_webrect(JNIEnv* env, jobject obj)
{
    int L, T, R, B;
    GraphicsJNI::get_jrect(env, obj, &L, &T, &R, &B);
    return WebCore::IntRect(L, T, R - L, B - T);
}

static void nativeSelectBestAt(JNIEnv *env, jobject obj, jobject jrect)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    WebCore::IntRect rect = jrect_to_webrect(env, jrect);
    view->selectBestAt(rect);
}

static void nativeMarkNodeInvalid(JNIEnv *env, jobject obj, int node)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->markNodeInvalid((WebCore::Node*) node);
} 

static void nativeMotionUp(JNIEnv *env, jobject obj,
    int x, int y, int slop, bool isClick)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->motionUp(x, y, slop, isClick, true, false);
}

static bool nativeUpdateFocusNode(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return view->updateFocusNode(env);
}

static bool nativeMoveFocus(JNIEnv *env, jobject obj,
    int key, int count, bool ignoreScroll)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    DBG_NAV_LOGD("env=%p obj=%p view=%p", env, obj, view);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return view->moveFocus(key, count, ignoreScroll, true, NULL, NULL);
}

static void nativeNotifyFocusSet(JNIEnv *env, jobject obj, bool inEditingMode)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->notifyFocusSet((WebView::FrameCachePermission) inEditingMode);
}

static void nativeRecomputeFocus(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->recomputeFocus();
}

static void nativeRecordButtons(JNIEnv* env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->nativeRecordButtons();
}

static void nativeResetFocus(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->resetFocus();
}

static void nativeSetFollowedLink(JNIEnv *env, jobject obj, bool followed)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->setFollowedLink(followed);
}

static void nativeSetHeightCanMeasure(JNIEnv *env, jobject obj, bool measure)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in nativeSetHeightCanMeasure");
    view->setHeightCanMeasure(measure);
}

static jobject nativeGetFocusRingBounds(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    jclass rectClass = env->FindClass("android/graphics/Rect");
    LOG_ASSERT(rectClass, "Could not find Rect class!");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    LOG_ASSERT(init, "Could not find constructor for Rect");
    WebCore::IntRect webRect;
    view->focusRingBounds(&webRect);
    jobject rect = env->NewObject(rectClass, init, webRect.x(), 
        webRect.y(), webRect.right(), webRect.bottom());
    return rect;
}

static jobject nativeGetNavBounds(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    jclass rectClass = env->FindClass("android/graphics/Rect");
    LOG_ASSERT(rectClass, "Could not find Rect class!");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    LOG_ASSERT(init, "Could not find constructor for Rect");
    WebCore::IntRect webRect = view->getNavBounds();
    jobject rect = env->NewObject(rectClass, init, webRect.x(), 
        webRect.y(), webRect.right(), webRect.bottom());
    return rect;
}

static void nativeSetNavBounds(JNIEnv *env, jobject obj, jobject jrect)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    WebCore::IntRect rect = jrect_to_webrect(env, jrect);
    view->setNavBounds(rect);
}

static void nativeUpdateCachedTextfield(JNIEnv *env, jobject obj, jstring updatedText, jint generation)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in nativeUpdateCachedTextfield");
    CachedRoot* root = view->getFrameCache(WebView::DONT_ALLOW_NEWER);
    if (!root) 
        return;
    const CachedNode* cachedFocusNode = root->currentFocus();
    if (!cachedFocusNode || (!cachedFocusNode->isTextField() && !cachedFocusNode->isTextArea()))
        return;
    WebCore::String webcoreString;
    const jchar* outputChars = NULL == updatedText ? NULL : env->GetStringChars(updatedText, NULL);
    if (outputChars != NULL) {
        webcoreString = WebCore::String((const UChar*) outputChars, env->GetStringLength(updatedText));
    }
    (const_cast<CachedNode*>(cachedFocusNode))->setExport(webcoreString);
    root->setTextGeneration(generation);
    if (outputChars) 
        env->ReleaseStringChars(updatedText, outputChars);
    checkException(env);
}

static void nativeDestroy(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOGD("nativeDestroy view: %p", view);
    LOG_ASSERT(view, "view not set in nativeDestroy");
    delete view;
}

static void nativeMoveSelection(JNIEnv *env, jobject obj, int x, int y, bool ex)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->moveSelection(x, y, ex);
}

static jobject nativeGetSelection(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return GraphicsJNI::createRegion(env, new SkRegion(view->getSelection()));
}

/*
 * JNI registration
 */
static JNINativeMethod gJavaWebViewMethods[] = {
#ifdef BUCKET_EXPERIMENT
    { "nativeClearContentPicture", "()V",
        (void*) nativeClearContentPicture },
#endif
    { "nativeClearFocus", "(II)V",
        (void*) nativeClearFocus },
    { "nativeCreate", "(I)V",
        (void*) nativeCreate },
    { "nativeDebugDump", "()V", 
        (void*) nativeDebugDump },
    { "nativeDestroy", "()V",
        (void*) nativeDestroy },
#ifdef BUCKET_EXPERIMENT
    { "nativeDrawContentPicture", "(Landroid/graphics/Canvas;)V",
        (void*) nativeDrawContentPicture },
#endif
    { "nativeDrawFocusRing", "(Landroid/graphics/Canvas;)V",
        (void*) nativeDrawFocusRing },
    { "nativeDrawSelection", "(Landroid/graphics/Canvas;IIZ)V",
        (void*) nativeDrawSelection },
    { "nativeUpdateFocusNode", "()Z", 
        (void*) nativeUpdateFocusNode },
    { "nativeGetFocusRingBounds", "()Landroid/graphics/Rect;", 
        (void*) nativeGetFocusRingBounds },
    { "nativeGetNavBounds", "()Landroid/graphics/Rect;", 
        (void*) nativeGetNavBounds },
    { "nativeHasSrcUrl", "()Z", 
        (void*) nativeHasSrcUrl },
    { "nativeMarkNodeInvalid", "(I)V",
        (void*) nativeMarkNodeInvalid },
    { "nativeMotionUp", "(IIIZ)V",
        (void*) nativeMotionUp },
    { "nativeMoveFocus", "(IIZ)Z",
        (void*) nativeMoveFocus },
    { "nativeNotifyFocusSet", "(Z)V",
        (void*) nativeNotifyFocusSet },
    { "nativeRecomputeFocus", "()V",
        (void*) nativeRecomputeFocus },
    { "nativeRecordButtons", "()V",
        (void*) nativeRecordButtons },
    { "nativeResetFocus", "()V",
        (void*) nativeResetFocus },
    { "nativeSelectBestAt", "(Landroid/graphics/Rect;)V", 
        (void*) nativeSelectBestAt },
    { "nativeSetFollowedLink", "(Z)V",
        (void*) nativeSetFollowedLink },
    { "nativeSetHeightCanMeasure", "(Z)V",
        (void*) nativeSetHeightCanMeasure },
    { "nativeSetNavBounds", "(Landroid/graphics/Rect;)V", 
        (void*) nativeSetNavBounds },
    { "nativeIsImage", "(II)Z", 
        (void*) nativeIsImage },
    { "nativeUpdateCachedTextfield", "(Ljava/lang/String;I)V",
        (void*) nativeUpdateCachedTextfield },
    { "nativeMoveSelection", "(IIZ)V", 
        (void*) nativeMoveSelection },
    { "nativeGetSelection", "()Landroid/graphics/Region;",
        (void*) nativeGetSelection }
};

int register_webview(JNIEnv* env)
{
    jclass clazz = env->FindClass("android/webkit/WebView");
    LOG_ASSERT(clazz != NULL, "Unable to find class android/webkit/WebView");
    gWebViewField = env->GetFieldID(clazz, "mNativeClass", "I");
    LOG_ASSERT(gWebViewField != NULL, "Unable to find android/webkit/WebView.mNativeClass");
    
    return jniRegisterNativeMethods(env, "android/webkit/WebView", gJavaWebViewMethods, NELEM(gJavaWebViewMethods));
}

} // namespace android
