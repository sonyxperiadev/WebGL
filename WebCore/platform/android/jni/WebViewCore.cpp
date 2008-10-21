/*
 * Copyright 2006, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *
 *     http://www.apache.org/licenses/LICENSE-2.0 
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License.
 */

#define LOG_TAG "webcoreglue"

#include <config.h>
#include <wtf/Platform.h>

#include "android_graphics.h"
#include "GraphicsJNI.h"
#include "SkPicture.h"

#include "AtomicString.h"
#include "CacheBuilder.h"
#include "CachedNode.h"
#include "CachedRoot.h"
#include "Color.h"
#include "Document.h"
#include "Element.h"
#include "Editor.h"
#include "EditorClientAndroid.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Font.h"
#include "FrameAndroid.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "HTMLAnchorElement.h"
#include "HTMLAreaElement.h"
#include "HTMLElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFieldInnerElement.h"
#include "InlineTextBox.h"
#include "KeyboardCodes.h"
#include "Node.h"
#include "Page.h"
#include "PlatformGraphicsContext.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformString.h"
#include "PluginInfoStore.h"
#include "Position.h"
#include "RenderLayer.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderThemeAndroid.h"
#include "RenderView.h"
#include "ResourceRequest.h"
#include "SelectionController.h"
#include "Settings.h"
#include "StringImpl.h"
#include "Text.h"
#include "TypingCommand.h"
#include "WebCoreFrameBridge.h"
#include "WebViewCore.h"
#include "HistoryItem.h"

#ifdef LOG
#undef LOG
#endif

#include <ui/KeycodeLabels.h>
#include <utils/Log.h>
#include "SkTDArray.h"
#include "SkTime.h"
#include <SkTypes.h>
#include <SkCanvas.h>
#include <SkUtils.h>
#include <JNIHelp.h>

#include "SystemTime.h"

#ifdef ANDROID_INSTRUMENT
static uint32_t sTotalTimeUsed = 0;
static uint32_t sTotalPaintTimeUsed = 0;
static uint32_t sCounter = 0;
    
namespace WebCore {
void Frame::resetWebViewCoreTimeCounter()
{
    sTotalTimeUsed = 0;
}

void Frame::reportWebViewCoreTimeCounter()
{
    LOG(LOG_DEBUG, "WebCore", "*-* Total native 4 (webview core) time: %d ms\n", 
            sTotalTimeUsed);   
}
// This should be in Frame.cpp, but android LOG is conflict with webcore LOG
void Frame::resetPaintTimeCounter()
{
    sTotalPaintTimeUsed = 0;
    sCounter = 0;
}

void Frame::reportPaintTimeCounter()
{
    LOG(LOG_DEBUG, "WebCore", "*-* Total draw time: %d ms called %d times\n", 
            sTotalPaintTimeUsed, sCounter);   
}
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////

namespace android {

#ifdef ANDROID_INSTRUMENT
class TimeCounterWV {
public:
    TimeCounterWV() {
        mStartTime = WebCore::get_thread_msec();
    }
    
    ~TimeCounterWV() {
        sTotalTimeUsed += WebCore::get_thread_msec() - mStartTime;
    }
    
private:
    uint32_t mStartTime;    
};
#endif

// ----------------------------------------------------------------------------

#define GET_NATIVE_VIEW(env, obj) ((WebViewCore*)env->GetIntField(obj, gWebViewCoreFields.mNativeClass))

// Field ids for WebViewCore
struct WebViewCoreFields {
    jfieldID    mNativeClass;
    jfieldID    mViewportWidth;
    jfieldID    mViewportHeight;
    jfieldID    mViewportInitialScale;
    jfieldID    mViewportMinimumScale;
    jfieldID    mViewportMaximumScale;
    jfieldID    mViewportUserScalable;
    jfieldID    mWebView;
} gWebViewCoreFields;
    
// ----------------------------------------------------------------------------
    
extern JavaVM* jnienv_to_javavm(JNIEnv* env);
extern JNIEnv* javavm_to_jnienv(JavaVM* vm);
    
// ----------------------------------------------------------------------------

/**
 * Helper method for checking java exceptions
 * @return true if an exception occurred.
 */
static bool checkException(JNIEnv *env)
{
    if (env->ExceptionCheck() != 0)
    {
        LOGE("*** Uncaught exception returned from Java call!\n");
        env->ExceptionDescribe();
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------

struct WebViewCore::JavaGlue
{
    JavaVM*     mJVM;
    jobject     mObj;
    jmethodID   mSpawnScrollTo;
    jmethodID   mScrollTo;
    jmethodID   mScrollBy;
    jmethodID   mContentInvalidate;
    jmethodID   mRequestListBox;
    jmethodID   mRequestSingleListBox;
    jmethodID   mJsAlert;
    jmethodID   mJsConfirm;
    jmethodID   mJsPrompt;
    jmethodID   mJsUnload;
    jmethodID   mDidFirstLayout;
    jmethodID   mSendMarkNodeInvalid;
    jmethodID   mSendNotifyFocusSet;
    jmethodID   mSendNotifyProgressFinished;
    jmethodID   mSendRecomputeFocus;
    jmethodID   mSendViewInvalidate;
    jmethodID   mUpdateTextfield;
    jmethodID   mRestoreScale;
};

/*
 * WebViewCore Implementation
 */

static jmethodID GetJMethod(JNIEnv* env, jclass clazz, const char name[], const char signature[])
{
    jmethodID m = env->GetMethodID(clazz, name, signature);
    LOG_ASSERT(m, "Could not find method %s", name);
    return m;
}

Mutex WebViewCore::gFrameCacheMutex;
Mutex WebViewCore::gFrameGenerationMutex;
Mutex WebViewCore::gRecomputeFocusMutex;
Mutex WebViewCore::gButtonMutex;

WebViewCore::WebViewCore(JNIEnv* env, jobject javaWebViewCore, WebCore::FrameView* view)
        : mView(view)
{
    mFrame = (WebCore::FrameAndroid*)mView->frame();
    // This will be derefed in WebCoreFrameBridge during destroy.
    mView->ref();
    
    mPopupReply = NULL;
    mBuildGeneration = 0;
    mMoveGeneration = 0;
    mGeneration = 0;
    mLastGeneration = 0;
    mTouchGeneration = 0;
    mBlockTextfieldUpdates = false;
    // just initial values. These should be set by client
    mMaxXScroll = 320/4;
    mMaxYScroll = 240/4;
    mButtons = NULL;
    mTextGeneration = 0;

    LOG_ASSERT(mFrame, "Uh oh, somehow a frameview was made without an initial frame!");

    jclass clazz = env->GetObjectClass(javaWebViewCore);
    mJavaGlue = new JavaGlue;
    mJavaGlue->mJVM = jnienv_to_javavm(env);
    mJavaGlue->mObj = env->NewGlobalRef(javaWebViewCore);
    mJavaGlue->mSpawnScrollTo = GetJMethod(env, clazz, "contentSpawnScrollTo", "(II)V");
    mJavaGlue->mScrollTo = GetJMethod(env, clazz, "contentScrollTo", "(II)V");
    mJavaGlue->mScrollBy = GetJMethod(env, clazz, "contentScrollBy", "(II)V");
    mJavaGlue->mContentInvalidate = GetJMethod(env, clazz, "contentInvalidate", "()V");
    mJavaGlue->mRequestListBox = GetJMethod(env, clazz, "requestListBox", "([Ljava/lang/String;[Z[I)V");
    mJavaGlue->mRequestSingleListBox = GetJMethod(env, clazz, "requestListBox", "([Ljava/lang/String;[ZI)V");
    mJavaGlue->mJsAlert = GetJMethod(env, clazz, "jsAlert", "(Ljava/lang/String;Ljava/lang/String;)V");
    mJavaGlue->mJsConfirm = GetJMethod(env, clazz, "jsConfirm", "(Ljava/lang/String;Ljava/lang/String;)Z");
    mJavaGlue->mJsPrompt = GetJMethod(env, clazz, "jsPrompt", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    mJavaGlue->mJsUnload = GetJMethod(env, clazz, "jsUnload", "(Ljava/lang/String;Ljava/lang/String;)Z");
    mJavaGlue->mDidFirstLayout = GetJMethod(env, clazz, "didFirstLayout", "(Ljava/lang/String;)V");
    mJavaGlue->mSendMarkNodeInvalid = GetJMethod(env, clazz, "sendMarkNodeInvalid", "(I)V");
    mJavaGlue->mSendNotifyFocusSet = GetJMethod(env, clazz, "sendNotifyFocusSet", "()V");
    mJavaGlue->mSendNotifyProgressFinished = GetJMethod(env, clazz, "sendNotifyProgressFinished", "()V");
    mJavaGlue->mSendRecomputeFocus = GetJMethod(env, clazz, "sendRecomputeFocus", "()V");
    mJavaGlue->mSendViewInvalidate = GetJMethod(env, clazz, "sendViewInvalidate", "()V");
    mJavaGlue->mUpdateTextfield = GetJMethod(env, clazz, "updateTextfield", "(IZLjava/lang/String;I)V");
    mJavaGlue->mRestoreScale = GetJMethod(env, clazz, "restoreScale", "(I)V");
    
    env->SetIntField(javaWebViewCore, gWebViewCoreFields.mNativeClass, (jint)this);

    mVisibleRect.setWidth(0);
    mVisibleRect.setHeight(0);

    reset(true);
}

WebViewCore::~WebViewCore()
{
    // Release the focused view
    Release(mPopupReply);
        
    if (mJavaGlue->mObj) {
        JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
        env->DeleteGlobalRef(mJavaGlue->mObj);
        mJavaGlue->mObj = 0;
    }    
    delete mJavaGlue;
    delete mFrameCacheKit;
    delete mNavPictureKit;
    if (mButtons != NULL) {
        mButtons->deleteAll();
        delete mButtons;
    }
}

void WebViewCore::reset(bool fromConstructor)
{
    if (fromConstructor) {
        mFrameCacheKit = NULL;
        mNavPictureKit = NULL;
    } else {
        gFrameCacheMutex.lock();
        delete mFrameCacheKit;
        delete mNavPictureKit;
        mFrameCacheKit = NULL;
        mNavPictureKit = NULL;
        gFrameCacheMutex.unlock();
    }

    mLastFocused = NULL;
    mLastFocusedBounds = WebCore::IntRect(0,0,0,0);
    mUpdatedFrameCache = true;
    mFrameCacheOutOfDate = true;
    mBlockFocusChange = false;
    mSnapAnchorNode = NULL;
    mUseReplay = false;
}

void WebViewCore::draw(WebCore::GraphicsContext* ctx, 
    const WebCore::IntRect& r, bool invalCache)
{
    LOG_ASSERT(mFrame, "%s has no frame for this view!", __FUNCTION__);
    LOG_ASSERT(mFrame == mView->frame(), "Our frames do not match!");
#ifdef ANDROID_INSTRUMENT
    uint32_t startTime = WebCore::get_thread_msec();
#endif
    if (NULL == mFrame->renderer()) {
        // We only do this if there is nothing else to draw.
        // If there is a renderer, it will fill the bg itself, so we don't want to
        // double-draw (slow)
        SkCanvas* canvas = ctx->platformContext()->mCanvas;
        canvas->drawColor(SK_ColorWHITE);
    }
    else {
        mFrame->paint(ctx, r);
    }
    if (invalCache) {
        WebCore::CacheBuilder& builder = mFrame->getCacheBuilder();
        WebCore::Node* oldFocusNode = builder.currentFocus();
        mFrameCacheOutOfDate = true;
        WebCore::IntRect oldBounds = oldFocusNode ? 
            oldFocusNode->getRect() : WebCore::IntRect(0,0,0,0);
        DBG_NAV_LOGD_ONCE("mLastFocused=%p oldFocusNode=%p"
            " mLastFocusedBounds={%d,%d,%d,%d} oldBounds={%d,%d,%d,%d}",
            mLastFocused, oldFocusNode, 
            mLastFocusedBounds.x(), mLastFocusedBounds.y(), mLastFocusedBounds.width(), mLastFocusedBounds.height(),
            oldBounds.x(), oldBounds.y(), oldBounds.width(), oldBounds.height());
        if (mLastFocused != oldFocusNode || mLastFocusedBounds != oldBounds) {
            mLastFocused = oldFocusNode;
            mLastFocusedBounds = oldBounds;
            DBG_NAV_LOG("call updateFrameCache");
            updateFrameCache();
        }
    }
#ifdef ANDROID_INSTRUMENT
    sTotalPaintTimeUsed += WebCore::get_thread_msec() - startTime;
    sCounter++;
#endif
}

void WebViewCore::recordPicture(SkPicture* picture, bool invalCache)
{
    // if there is no document yet, just return
    if (!mFrame->document())
        return;

    // Call layout to ensure that the contentWidth and contentHeight are correct
    WebCore::ScrollView* view = mFrame->view();
    view->ignoreUpdateContents(true);
    layout();
    view->ignoreUpdateContents(false);

#if USE(DOUBLE_TREE)
    // if haveStylesheetsLoaded() is false, render is not created, so don't call draw.
    // It can happen when we switch from fast display mode, new style sheets are requested,
    // if we call draw, we will have blank screen. 
    if (mFrame->document() && !mFrame->loader()->isComplete() &&
        !mFrame->document()->haveStylesheetsLoaded()) {
        LOGV("skip %s", __FUNCTION__);    
        return;
    }
#endif
    
    // draw into the picture's recording canvas
    SkAutoPictureRecord arp(picture, contentWidth(), contentHeight());
    SkAutoMemoryUsageProbe mup(__FUNCTION__);

    WebCore::PlatformGraphicsContext pgc(arp.getRecordingCanvas());
    WebCore::GraphicsContext gc(&pgc);
    WebCore::IntRect r = WebCore::IntRect(0, 0, INT_MAX, INT_MAX);
    draw(&gc, r, invalCache);
    
    // If this was done for the Java Picture that is being displayed, grab the 
    // info for the buttons, so the UI thread can grab them and update them
    // according to their state.
    if (invalCache) {
        gButtonMutex.lock();
        if (mButtons != NULL) {
            mButtons->deleteAll();
            delete mButtons;
        }
        mButtons = pgc.getAndClearButtonInfo();
        gButtonMutex.unlock();
    }
}

void WebViewCore::onResize()
{
    LOGV("view is resized, notifying the frame to relayout...");
    LOG_ASSERT(mFrame == mView->frame(), "Our frames do not match!");
    // r could be NULL because our frame has not processed any data. This happens
    // when an HTMLWidget is first created, attached and sizeChanged is called.
    WebCore::RenderObject *r = mFrame->renderer();
    if (r) {
        r->setNeedsLayoutAndPrefWidthsRecalc();
        mFrame->forceLayout(true);
    }
}

WebCore::RenderLayer* WebViewCore::getRenderLayer()
{
    LOG_ASSERT(mView, "A view was not associated with this view bridge!");
    LOG_ASSERT(mFrame, "A frame was not associated with our frame view!");
    LOG_ASSERT(mFrame == mView->frame(), "Our frames do not match!");
    WebCore::Document* doc = mFrame->document();
    if (doc == NULL)
        return NULL;
    WebCore::RenderObject* renderer = doc->renderer();
    return renderer->enclosingLayer();
}

bool WebViewCore::pinXToDocument(int* xPtr)
{
    WebCore::RenderLayer* layer = getRenderLayer();
    if (layer == NULL)
        return false;
    int docWidth = layer->width();
    int x = *xPtr;
    if (x < 0 || docWidth <= this->width())
        x = 0;
    else if (x + this->width() > docWidth)
        x = docWidth - this->width();
    *xPtr = x;
    return true;
}

bool WebViewCore::pinYToDocument(int* yPtr)
{
    WebCore::RenderLayer* layer = getRenderLayer();
    if (layer == NULL)
        return false;
    int docHeight = layer->height();
    int y = *yPtr;
    if (y < 0 || docHeight <= this->height()) 
        y = 0;
    else if (y + this->height() > docHeight)
        y = docHeight - this->height();
    *yPtr = y;
    return true;
}

void WebViewCore::scrollTo(int x, int y, bool animate)
{
    LOG_ASSERT(mParent == NULL, "Why do we have a parent");
    LOG_ASSERT(mJavaGlue->mObj, "A Java widget was not associated with this view bridge!");

//    LOGD("WebViewCore::scrollTo(%d %d)\n", x, y);

    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    env->CallVoidMethod(mJavaGlue->mObj, animate ? mJavaGlue->mSpawnScrollTo : mJavaGlue->mScrollTo, x, y);
    checkException(env);
}

void WebViewCore::sendMarkNodeInvalid(WebCore::Node* node)
{
    LOG_ASSERT(mJavaGlue->mObj, "A Java widget was not associated with this view bridge!");
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mSendMarkNodeInvalid, (int) node); 
    checkException(env);
}

void WebViewCore::sendNotifyFocusSet()
{
    LOG_ASSERT(mJavaGlue->mObj, "A Java widget was not associated with this view bridge!");
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mSendNotifyFocusSet); 
    checkException(env);
}

void WebViewCore::sendNotifyProgressFinished()
{
    LOG_ASSERT(mJavaGlue->mObj, "A Java widget was not associated with this view bridge!");
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mSendNotifyProgressFinished); 
    checkException(env);
}

void WebViewCore::sendRecomputeFocus()
{
    LOG_ASSERT(mJavaGlue->mObj, "A Java widget was not associated with this view bridge!");
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mSendRecomputeFocus); 
    checkException(env);
}

void WebViewCore::sendViewInvalidate()
{
    LOG_ASSERT(mJavaGlue->mObj, "A Java widget was not associated with this view bridge!");
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mSendViewInvalidate); 
    checkException(env);
}

void WebViewCore::scrollBy(int dx, int dy)
{
    LOG_ASSERT(mParent == NULL, "Why do we have a parent");
    if ((dx | dy) == 0)
        return;
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mScrollBy, dx, dy);
    checkException(env);
}

void WebViewCore::contentInvalidate(const WebCore::IntRect &rect)
{
//    DBG_NAV_LOGD("rect={%d,%d,%d,%d}", rect.x(), rect.y(), rect.width(), rect.height());
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mContentInvalidate);
    checkException(env);
}

void WebViewCore::contentInvalidate()
{
//    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__);
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mContentInvalidate);
    checkException(env);
}

static int pin_pos(int x, int width, int targetWidth)
{
    if (x + width > targetWidth)
        x = targetWidth - width;
    if (x < 0)
        x = 0;
    return x;
}

void WebViewCore::didFirstLayout()
{
    LOG_ASSERT(mParent == NULL, "Why do we have a parent");
	    
    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__);
    LOG_ASSERT(mJavaGlue->mObj, "A Java widget was not associated with this view bridge!");
    
    const WebCore::KURL& url = mFrame->loader()->url();
    if (url.isEmpty())
        return;
    LOGV("::WebCore:: didFirstLayout %s", url.string().ascii().data());

    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);        
    WebCore::String urlString(url.string());
    jstring urlStr = env->NewString((unsigned short*)urlString.characters(), urlString.length());
    env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mDidFirstLayout, urlStr);
    checkException(env);
    env->DeleteLocalRef(urlStr);

    DBG_NAV_LOG("call updateFrameCache");
    updateFrameCache();
    mHistory.setDidFirstLayout(true);
}

void WebViewCore::restoreScale(int scale)
{
    LOG_ASSERT(mParent == NULL, "Why do we have a parent");
        
    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__);
    LOG_ASSERT(mJavaGlue->mObj, "A Java widget was not associated with this view bridge!");
    
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);        
    env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mRestoreScale, scale);
    checkException(env);
}

static void layoutIfNeededRecursive(WebCore::Frame* f)
{
    if (!f)
        return;

    WebCore::FrameView* v = f->view();
    if (!v)
        return;

    if (v->needsLayout())
        v->layout();

    if (!f->tree())
        return;

    WebCore::Frame* child = f->tree()->firstChild();
    while (child) {
        WebCore::FrameView* childView = child->view();
        if (childView)
            layoutIfNeededRecursive(child);
        if (!child->tree())
            break;
        child = child->tree()->nextSibling();
    }
}

void WebViewCore::layout()
{
    LOG_ASSERT(mParent == NULL, "Why do we have a parent");

    SkASSERT(mView);
    layoutIfNeededRecursive(mFrame);
}

void WebViewCore::notifyFocusSet()
{
    sendNotifyFocusSet();
}

void WebViewCore::notifyProgressFinished()
{
    DBG_NAV_LOG("call updateFrameCache");
    updateFrameCache();
    sendNotifyProgressFinished();
}

void WebViewCore::doMaxScroll(WebCore::CacheBuilder::Direction dir)
{
    int dx = 0, dy = 0;

    switch (dir) {
    case WebCore::CacheBuilder::LEFT:
        dx = -this->getMaxXScroll();
        break;
    case WebCore::CacheBuilder::UP:
        dy = -this->getMaxXScroll();
        break;
    case WebCore::CacheBuilder::RIGHT:
        dx = this->getMaxYScroll();
        break;
    case WebCore::CacheBuilder::DOWN:
        dy = this->getMaxYScroll();
        break;
    case WebCore::CacheBuilder::UNINITIALIZED:
    default:
        LOG_ASSERT(0, "unexpected focus selector");
    }
    this->scrollBy(dx, dy);
}

void WebViewCore::getVisibleRect(WebCore::IntRect* rect) const
{
    SkASSERT(rect);
    *rect = mVisibleRect;
}

void WebViewCore::setVisibleRect(const WebCore::IntRect& r)
{
    DBG_NAV_LOGD("{%d,%d,%d,%d}", r.x(), r.y(), r.width(), r.height());
    if (mVisibleRect != r) {
        if (mVisibleRect.x() != r.x() || mVisibleRect.y() != r.y())
            mFrame->sendScrollEvent();
        mVisibleRect = r;
        // The visible rect is located within our coordinate space so it
        // contains the actual scroll position. Setting the location makes hit
        // testing work correctly.
        setLocation(r.x(), r.y());
    }
}

void WebViewCore::dump()
{
#if DUMP_NAV_CACHE
    if (mFrame != NULL)
        mFrame->getCacheBuilder().mDebug.print();
#endif
}

WebCore::String WebViewCore::retrieveHref(WebCore::Frame* frame, WebCore::Node* node)
{
    WebCore::CacheBuilder& builder = mFrame->getCacheBuilder();
    if (builder.validNode(frame, node) == false)
        return WebCore::String();
    if (node->hasTagName(WebCore::HTMLNames::aTag) == false)
        return WebCore::String();
    WebCore::HTMLAnchorElement* anchor = static_cast<WebCore::HTMLAnchorElement*>(node);
    return anchor->href();
}

WebCore::String WebViewCore::retrieveImageRef(int x, int y)
{
    WebCore::IntPoint point(x,y);
    WebCore::HitTestResult result = mFrame->eventHandler()->hitTestResultAtPoint(point, false);
    WebCore::Node* node = result.innerNode();
    if (node == NULL || node->hasTagName(WebCore::HTMLNames::imgTag) == false)
        return WebCore::String();
    WebCore::HTMLImageElement* img = static_cast<WebCore::HTMLImageElement*>(node);
    return img->src();
}

bool WebViewCore::prepareFrameCache()
{
    if (mFrameCacheOutOfDate == false) {
        DBG_NAV_LOG("mFrameCacheOutOfDate == false");
        return false;
    }
    mFrameCacheOutOfDate = false;
#if DEBUG_NAV_UI
    DBG_NAV_LOG("mFrameCacheOutOfDate was true");
    mNow = SkTime::GetMSecs();
#endif
    mTemp = new CachedRoot();
    mTemp->init(mFrame, &mHistory);
    mTemp->setGeneration(++mBuildGeneration);
    WebCore::CacheBuilder& builder = mFrame->getCacheBuilder();
    WebCore::Settings* settings = frame()->page()->settings();
    builder.allowAllTextDetection();
    if (settings->formatDetectionAddress() == false)
        builder.disallowAddressDetection();
    if (settings->formatDetectionEmail() == false)
        builder.disallowEmailDetection();
    if (settings->formatDetectionTelephone() == false)
        builder.disallowPhoneDetection();
    builder.buildCache(mTemp);
    mTempPict = new SkPicture();
    recordPicture(mTempPict, false);
    mTemp->setPicture(mTempPict);
    mTemp->setTextGeneration(mTextGeneration);
    return true;
}

void WebViewCore::releaseFrameCache(bool newCache)
{
    if (newCache == false) {
        DBG_NAV_LOG("newCache == false");
        return;
    }
    gFrameCacheMutex.lock();
    delete mFrameCacheKit;
    delete mNavPictureKit;
    mFrameCacheKit = mTemp;
    mNavPictureKit = mTempPict;
    mUpdatedFrameCache = true;
#if DEBUG_NAV_UI
    const CachedNode* cachedFocusNode = mFrameCacheKit->currentFocus();
    DBG_NAV_LOGD("cachedFocusNode=%d (nodePointer=%p)", 
        cachedFocusNode ? cachedFocusNode->index() : 0,
        cachedFocusNode ? cachedFocusNode->nodePointer() : NULL);
#endif
    gFrameCacheMutex.unlock();
    notifyFocusSet();
    // it's tempting to send an invalidate here, but it's a bad idea
    // the cache is now up to date, but the focus is not -- the event
    // may need to be recomputed from the prior history. An invalidate
    // will draw the stale location causing the ring to flash at the wrong place.
}

void WebViewCore::updateFrameCache()
{
    mUseReplay = false;
    releaseFrameCache(prepareFrameCache());
}

void WebViewCore::removeFrameGeneration(WebCore::Frame* frame)
{
    DBG_NAV_LOGD("frame=%p mGeneration=%d", frame, mGeneration);
    gFrameGenerationMutex.lock();
    int last = mFrameGenerations.size() - 1;
    for (int index = 0; index <= last; index++) {
        if (mFrameGenerations[index].mFrame == frame) {
            DBG_NAV_LOGD("index=%d last=%d", index, last);
            if (index != last)
                mFrameGenerations[index] = mFrameGenerations[last];
            mFrameGenerations.removeLast();
            break;
        }
    }
    gFrameGenerationMutex.unlock();
}

void WebViewCore::updateFrameGeneration(WebCore::Frame* frame)
{
    DBG_NAV_LOGD("frame=%p mGeneration=%d", frame, mGeneration);
    gFrameGenerationMutex.lock();
    ++mBuildGeneration;
    for (size_t index = 0; index < mFrameGenerations.size(); index++) {
        if (mFrameGenerations[index].mFrame == frame) {
            DBG_NAV_LOG("replace");
            mFrameGenerations[index].mGeneration = mBuildGeneration;
            goto done;
        }
    }
    {
        FrameGen frameGen = {frame, mBuildGeneration};
        mFrameGenerations.append(frameGen);
        DBG_NAV_LOG("append");
    }
done:
    gFrameGenerationMutex.unlock();
}

int WebViewCore::retrieveFrameGeneration(WebCore::Frame* frame)
{
    int result = INT_MAX;
    gFrameGenerationMutex.lock();
    for (size_t index = 0; index < mFrameGenerations.size(); index++) {
        if (mFrameGenerations[index].mFrame == frame) {
            result = mFrameGenerations[index].mGeneration;
            break;
        }
    }
    gFrameGenerationMutex.unlock();
    DBG_NAV_LOGD("frame=%p mGeneration=%d result=%d", frame, mGeneration, result);
    return result;
}

void WebViewCore::setFinalFocus(WebCore::Frame* frame, WebCore::Node* node, 
    int x, int y, bool block)
{
    DBG_NAV_LOGD("frame=%p node=%p x=%d y=%d", frame, node, x, y);
    bool result = finalKitFocus(frame, node, x, y);
    if (block) {
        mBlockFocusChange = true;
        if (result == false && node != NULL)
            touchUp(mTouchGeneration, 0, NULL, NULL, x, y, 0, true, true);
    }
}

void WebViewCore::setKitFocus(int moveGeneration, int buildGeneration, 
    WebCore::Frame* frame, WebCore::Node* node, int x, int y,
    bool ignoreNullFocus)
{
    DBG_NAV_LOGD("mMoveGeneration=%d moveGeneration=%d"
        " buildGeneration=%d frame=%p node=%p x=%d y=%d", 
        mMoveGeneration, moveGeneration, buildGeneration, frame, node, x, y);
    if (mBlockFocusChange) {
        DBG_NAV_LOG("mBlockFocusChange");
        return;
    }
    if (mMoveGeneration > moveGeneration) {
        DBG_NAV_LOGD("mMoveGeneration=%d > moveGeneration=%d", 
            mMoveGeneration, moveGeneration);
        return; // short-circuit if a newer move has already been generated
    }
    if (commonKitFocus(moveGeneration, buildGeneration, frame, node, x, y, 
            ignoreNullFocus) == false)
        return;
    mLastGeneration = moveGeneration;
}

bool WebViewCore::commonKitFocus(int generation, int buildGeneration, 
    WebCore::Frame* frame, WebCore::Node* node, int x, int y,
    bool ignoreNullFocus)
{
    DBG_NAV_LOGD("generation=%d buildGeneration=%d frame=%p"
        " node=%p x=%d y=%d", generation, buildGeneration, frame, node, x, y);
    mUseReplay = true;
    bool newCache = prepareFrameCache(); // must wait for possible recompute before using
    if (mMoveGeneration > generation) {
        DBG_NAV_LOGD("mMoveGeneration=%d > generation=%d",
            mMoveGeneration, generation);
        releaseFrameCache(newCache);
        return false; // short-circuit if a newer move has already been generated
    }
    // if the nav cache has been rebuilt since this focus request was generated,
    // send a request back to the UI side to recompute the kit-side focus
    if (mBuildGeneration > buildGeneration || (node != NULL && mFrame->getCacheBuilder().validNode(frame, node) == false)) {
        DBG_NAV_LOGD("mBuildGeneration=%d > buildGeneration=%d",
           mBuildGeneration, buildGeneration);
        gRecomputeFocusMutex.lock();
        bool first = mRecomputeEvents.size() == 0;
        mRecomputeEvents.append(generation);
        gRecomputeFocusMutex.unlock();
        releaseFrameCache(newCache);
        if (first)
            sendRecomputeFocus();
        return false;
    }
    releaseFrameCache(newCache);
    if (node == NULL && ignoreNullFocus)
        return true;
    finalKitFocus(frame, node, x, y);
    return true;
}

bool WebViewCore::finalKitFocus(WebCore::Frame* frame, WebCore::Node* node, 
    int x, int y)
{
    if (NULL == frame)
        frame = mFrame;
    WebCore::CacheBuilder& builder = mFrame->getCacheBuilder();
    WebCore::Node* oldFocusNode = builder.currentFocus();
    mMousePos = WebCore::IntPoint(x, y);
    // validNode will still return true if the node is null, as long as we have
    // a valid frame.  Do not want to make a call on frame unless it is valid.
    bool valid = builder.validNode(frame, node);
    if (valid) {
        WebCore::PlatformMouseEvent mouseEvent(mMousePos, mMousePos, WebCore::NoButton,
                WebCore::MouseEventMoved, 1, false, false, false, false, WebCore::currentTime());
        frame->eventHandler()->handleMouseMoveEvent(mouseEvent);
    }
    WebCore::Document* oldDoc = oldFocusNode ? oldFocusNode->document() : NULL;
    if (node == NULL) {
        if (oldFocusNode != NULL)
            oldDoc->setFocusedNode(NULL);
        return false;
    } else if (valid == false) {
        DBG_NAV_LOGD("sendMarkNodeInvalid node=%p", node);
        sendMarkNodeInvalid(node);
        if (oldFocusNode != NULL)
            oldDoc->setFocusedNode(NULL);
        return false;
    }
    // If we jump frames (docs), kill the focus on the old doc
    builder.setLastFocus(node);
    if (oldFocusNode != NULL && node->document() != oldDoc) {
        oldDoc->setFocusedNode(NULL);
    }
    WebCore::RenderObject* renderer = node->renderer();
    /*  <area> element doesn't have a renderer, but it can accept focus */
    LOG_ASSERT(!(renderer && renderer->isWidget()), "widgets unsupported");
    if (node->isTextNode() == false)
        static_cast<WebCore::Element*>(node)->focus(false);
    //setFocus on things that WebCore doesn't recognize as supporting focus
    //for instance, if there is an onclick element that does not support focus
    DBG_NAV_LOGD("setFocusedNode node=%p", node);
    node->document()->setFocusedNode(node);
    mLastFocused = node;
    mLastFocusedBounds = node->getRect();
    return true;
}

// helper function to find the frame that has focus
static WebCore::Frame* FocusedFrame(WebCore::FrameAndroid* frame)
{
    if (!frame)
        return NULL;
    WebCore::Node* focusNode = frame->getCacheBuilder().currentFocus();
    if (!focusNode)
        return NULL;
    WebCore::Document* doc = focusNode->document();
    if (!doc)
        return NULL;
    return doc->frame();
}

static WebCore::RenderTextControl* FocusedTextControl(WebCore::FrameAndroid* frame)
{
    WebCore::Node* focusNode = frame->getCacheBuilder().currentFocus();
    WebCore::RenderObject* renderer = focusNode->renderer();
    if (renderer && (renderer->isTextField() || renderer->isTextArea())) {
        return static_cast<WebCore::RenderTextControl*>(renderer);
    }
    return NULL;
}

WebCore::Frame* WebViewCore::changedKitFocus(WebCore::Frame* frame, 
    WebCore::Node* node, int x, int y)
{
    if (frame == NULL || node == NULL)
        return mFrame;
    WebCore::Node* current = mFrame->getCacheBuilder().currentFocus();
    if (current == node)
        return frame;
    return finalKitFocus(frame, node, x, y) ? frame : mFrame;
}

static int findTextBoxIndex(WebCore::Node* node, const WebCore::IntPoint& pt)
{
    if (node->isTextNode() == false) {
        DBG_NAV_LOGD("node=%p pt=(%d,%d) isText=false", node, pt.x(), pt.y());
        return -2; // error
    }
    WebCore::RenderText* renderText = (WebCore::RenderText*) node->renderer();
    if (renderText == NULL) {
        DBG_NAV_LOGD("node=%p pt=(%d,%d) renderText=NULL", node, pt.x(), pt.y());
        return -3; // error
    }
    int renderX, renderY;
    renderText->absolutePosition(renderX, renderY);
    WebCore::InlineTextBox *textBox = renderText->firstTextBox();
    int globalX, globalY;
    WebCore::CacheBuilder::GetGlobalOffset(node, &globalX, &globalY);
    int x = pt.x() - globalX;
    int y = pt.y() - globalY;
    do {
        int textBoxStart = textBox->start();
        int textBoxEnd = textBoxStart + textBox->len();
        if (textBoxEnd <= textBoxStart)
            continue;
        WebCore::IntRect bounds = textBox->selectionRect(renderX, renderY, 
            textBoxStart, textBoxEnd);
        if (bounds.contains(x, y) == false)
            continue;
        int offset = textBox->offsetForPosition(x - renderX);
#if DEBUG_NAV_UI
        int prior = offset > 0 ? textBox->positionForOffset(offset - 1) : -1;
        int current = textBox->positionForOffset(offset);
        int next = textBox->positionForOffset(offset + 1);
        DBG_NAV_LOGD(
            "offset=%d pt.x=%d globalX=%d renderX=%d x=%d "
            "textBox->x()=%d textBox->start()=%d prior=%d current=%d next=%d", 
            offset, pt.x(), globalX, renderX, x,
            textBox->xPos(), textBox->start(), prior, current, next
            );
#endif
        return textBox->start() + offset;
    } while ((textBox = textBox->nextTextBox()) != NULL);
    return -1; // couldn't find point, may have walked off end
}

static inline bool isSpace(UChar c)
{
    return c <= ' ' || WTF::Unicode::direction(c) == WTF::Unicode::WhiteSpaceNeutral;
}

static int centerX(const SkIRect& rect) { return (rect.fLeft + rect.fRight) >> 1; }
static int centerY(const SkIRect& rect) { return (rect.fTop + rect.fBottom) >> 1; }

WebCore::String WebViewCore::getSelection(SkRegion* selRgn)
{
    SkRegion::Iterator iter(*selRgn);
    WebCore::String result;
    for (; iter.done() == false; iter.next()) {
        const SkIRect& rect = iter.rect();
        DBG_NAV_LOGD("rect=(%d, %d, %d, %d)", rect.fLeft, rect.fTop, 
            rect.fRight, rect.fBottom);
        int cy = centerY(rect);
        WebCore::IntPoint startPt = WebCore::IntPoint(rect.fLeft + 1, cy);
        WebCore::HitTestResult hitTestResult = mFrame->eventHandler()->
            hitTestResultAtPoint(startPt, false);
        WebCore::Node* node = hitTestResult.innerNode();
        if (node == NULL) {
            DBG_NAV_LOG("node == NULL");
            return result;
        }
        WebCore::IntPoint endPt = WebCore::IntPoint(rect.fRight - 1, cy);
        hitTestResult = mFrame->eventHandler()->hitTestResultAtPoint(endPt, false);
        WebCore::Node* endNode = hitTestResult.innerNode();
        if (endNode == NULL) {
            DBG_NAV_LOG("endNode == NULL");
            return result;
        }
        int start = findTextBoxIndex(node, startPt);
        if (start < 0)
            continue;
        int end = findTextBoxIndex(endNode, endPt);
        if (end < -1) // use node if endNode is not valid
            endNode = node;
        if (end <= 0)
            end = static_cast<WebCore::Text*>(endNode)->string()->length();
        DBG_NAV_LOGD("node=%p start=%d endNode=%p end=%d", node, start, endNode, end);
        do {
            if (node->isTextNode() == false)
                continue;
            if (node->getRect().isEmpty())
                continue;
            WebCore::Text* textNode = static_cast<WebCore::Text*>(node);
            WebCore::StringImpl* string = textNode->string();
            if (string->length() == 0)
                continue;
            const UChar* chars = string->characters();
            int newLen = node == endNode ? end : string->length();
            int oldLen = result.length();
            if (oldLen == 0) {
                if (newLen < start) {
                    DBG_NAV_LOGD("newLen=%d < start=%d", newLen, start);
                    return result;
                }
                result.append(chars + start, newLen - start);
                continue;
            }
            if (isSpace(result.characters()[oldLen - 1]) == false &&
                    isSpace(chars[0]) == false) {
                result.append(" ");
            }
            result.append(chars, newLen);
        } while (node != endNode && (node = node->traverseNextNode()) != NULL);
    }
#if DUMP_NAV_CACHE
    { 
        char buffer[256];
        WebCore::CacheBuilder::Debug debug;
        debug.init(buffer, sizeof(buffer));
        debug.print("copy: ");
        debug.wideString(result);
        DUMP_NAV_LOGD("%s", buffer);
    }
#endif
    return result;
}

WebCore::Frame* WebViewCore::setSelection(WebCore::Frame* frame, WebCore::Node* node, 
    int x, int y, int start, int end)
{
    // FIXME: Consider using a generation number to avoid doing this many more times than necessary.
    frame = changedKitFocus(frame, node, x, y);
    ((WebCore::FrameAndroid*) frame)->select(start, end);
    return frame;
}

WebCore::Frame* WebViewCore::deleteSelection(WebCore::Frame* frame, WebCore::Node* node, 
    int x, int y, int start, int end)
{
    frame = setSelection(frame, node, x, y, start, end);
    if (start != end) {
        WebCore::PlatformKeyboardEvent downEvent(kKeyCodeDel, WebCore::VK_BACK, true, false, false, false, false);
        frame->eventHandler()->keyEvent(downEvent);
        WebCore::PlatformKeyboardEvent upEvent(kKeyCodeDel, WebCore::VK_BACK, false, false, false, false, false);
        frame->eventHandler()->keyEvent(upEvent);
    }
    return frame;
}

void WebViewCore::replaceTextfieldText(WebCore::Frame* frame, WebCore::Node* node, int x, int y, 
        int oldStart, int oldEnd, jstring replace, int start, int end)
{
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);        
    const jchar* outputChars = NULL == replace ? NULL : env->GetStringChars(replace, NULL);

    WebCore::String webcoreString;
    if (outputChars == NULL) {
        webcoreString = WebCore::String();
    }
    else {
        webcoreString = WebCore::String((const UChar*) outputChars, env->GetStringLength(replace));
    }
    frame = setSelection(frame, node, x, y, oldStart, oldEnd);
    WebCore::TypingCommand::insertText(frame->document(), webcoreString, false);
    ((WebCore::FrameAndroid*) frame)->select(start, end);
    if (replace && outputChars) 
        env->ReleaseStringChars(replace, outputChars);
    checkException(env);
}

void WebViewCore::passToJs(WebCore::Frame* frame, WebCore::Node* node, int x, int y, int generation,
     jstring currentText, int keyCode, int keyValue, bool down, bool cap, bool fn, bool sym)
{
    frame = changedKitFocus(frame, node, x, y);
    WebCore::PlatformKeyboardEvent event(keyCode, keyValue, down, false, cap, fn, sym);
    // Block text field updates during a key press.
    mBlockTextfieldUpdates = true;
    frame->eventHandler()->keyEvent(event);
    mBlockTextfieldUpdates = false;
    mTextGeneration = generation;

    WebCore::Node* currentFocus = mFrame->getCacheBuilder().currentFocus();
    // Make sure we have the same focus and it is a text field.
    if (node == currentFocus && currentFocus) {
        WebCore::RenderObject* renderer = currentFocus->renderer();
        if (renderer != NULL && (renderer->isTextField() || renderer->isTextArea())) {
            WebCore::RenderTextControl* renderText = static_cast<WebCore::RenderTextControl*>(renderer);
            JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
            const jchar* str = env->GetStringChars(currentText, NULL);
            WebCore::String current = WebCore::String(str, env->GetStringLength(currentText));
            env->ReleaseStringChars(currentText, str);
            WebCore::String test = renderText->text();
            // If the text changed during the key event, update the UI text field.
            if (test != current)
                updateTextfield(currentFocus, false, test);
        }
    }
}

void WebViewCore::saveDocumentState(WebCore::Frame* frame, WebCore::Node* node, int x, int y)
{
    frame = changedKitFocus(frame, node, x, y);
    WebCore::HistoryItem *item = frame->loader()->currentHistoryItem();
    
    // item can be null when there is no offical URL for the current page. This happens
    // when the content is loaded using with WebCoreFrameBridge::LoadData() and there
    // is no failing URL (common case is when content is loaded using data: scheme)
    if (item != 0) {
        item->setDocumentState(frame->document()->formElementsState());
    }
}

// Convert a WebCore::String into an array of characters where the first
// character represents the length, for easy conversion to java.
static uint16_t* stringConverter(const WebCore::String& text)
{
    size_t length = text.length();
    uint16_t* itemName = new uint16_t[length+1];
    itemName[0] = (uint16_t)length;
    uint16_t* firstChar = &(itemName[1]);
    memcpy((void*)firstChar, text.characters(), sizeof(UChar)*length);
    return itemName;
}

// Response to dropdown created for a listbox.
class ListBoxReply : public WebCoreReply {
public:
    ListBoxReply(WebCore::HTMLSelectElement* select, WebCore::Frame* frame, WebViewCore* view) 
        : m_select(select)
        , m_frame(frame)
        , m_viewImpl(view) 
    {}

    // Response used if the listbox only allows single selection.
    // index is listIndex of the selected item, or -1 if nothing is selected.
    virtual void replyInt(int index) {
        // If the select element no longer exists, do to a page change, etc, silently return.
        if (!m_select || m_viewImpl->mFrame->getCacheBuilder().validNode(m_frame, m_select) == false)
            return;
        if (-1 == index) {
            if (m_select->selectedIndex() != -1) {
                m_select->deselectItems();
                m_select->onChange();
                m_viewImpl->contentInvalidate();
            }
            return;
        }
        WebCore::HTMLOptionElement* option = static_cast<WebCore::HTMLOptionElement*>(
                m_select->item(m_select->listToOptionIndex(index)));
        if (!option->selected()) {
            option->setSelected(true);
            m_select->onChange();
            m_viewImpl->contentInvalidate();
        }
    }

    // Response if the listbox allows multiple selection.  array stores the listIndices
    // of selected positions.  
    virtual void replyIntArray(SkTDArray<int> array) {
        // If the select element no longer exists, do to a page change, etc, silently return.
        if (!m_select || m_viewImpl->mFrame->getCacheBuilder().validNode(m_frame, m_select) == false)
            return;
        m_select->deselectItems();
        int count = array.count();
        WebCore::HTMLOptionElement* option;
        for (int i = 0; i < count; i++) {
            option = static_cast<WebCore::HTMLOptionElement*>(
                    m_select->item(array[m_select->listToOptionIndex(i)]));
            option->setSelected(true);
        }
        m_viewImpl->contentInvalidate();
    }
private:
    // The select element associated with this listbox.
    WebCore::HTMLSelectElement* m_select;
    // The frame of this select element, to verify that it is valid.
    WebCore::Frame* m_frame;
    // For calling invalidate.
    WebViewCore* m_viewImpl;
};

// Create an array of java Strings.
static jobjectArray makeLabelArray(JNIEnv* env, const uint16_t** labels, size_t count)
{
    jclass stringClass = env->FindClass("java/lang/String");
    LOG_ASSERT(stringClass, "Could not find java/lang/String");
    jobjectArray array = env->NewObjectArray(count, stringClass, NULL);
    LOG_ASSERT(array, "Could not create new string array");

    for (size_t i = 0; i < count; i++) {
        jobject newString = env->NewString(&labels[i][1], labels[i][0]);
        env->SetObjectArrayElement(array, i, newString);
        env->DeleteLocalRef(newString);
        checkException(env);
    }
    env->DeleteLocalRef(stringClass);
    return array;
}

void WebViewCore::listBoxRequest(WebCoreReply* reply, const uint16_t** labels, size_t count, const int enabled[], size_t enabledCount, 
        bool multiple, const int selected[], size_t selectedCountOrSelection) {
    // Reuse mPopupReply
    Release(mPopupReply);
    mPopupReply = NULL;
    
    LOG_ASSERT(mJavaGlue->mObj, "No java widget associated with this view!");

    // Create an array of java Strings for the drop down.
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    jobjectArray labelArray = makeLabelArray(env, labels, count);

    // Create an array determining whether each item is enabled.
    jbooleanArray enabledArray = env->NewBooleanArray(enabledCount);
    checkException(env);
    jboolean* ptrArray = env->GetBooleanArrayElements(enabledArray, NULL);
    checkException(env);
    for (size_t i = 0; i < enabledCount; i++) {
        ptrArray[i] = enabled[i];
    }
    env->ReleaseBooleanArrayElements(enabledArray, NULL, enabledCount);
    checkException(env);

    if (multiple) {
        // Pass up an array representing which items are selected.
        jintArray selectedArray = env->NewIntArray(selectedCountOrSelection);
        checkException(env);
        jint* selArray = env->GetIntArrayElements(selectedArray, NULL);
        checkException(env);
        for (size_t i = 0; i < selectedCountOrSelection; i++) {
            selArray[i] = selected[i];
        }
        env->ReleaseIntArrayElements(selectedArray, NULL, selectedCountOrSelection);

        env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mRequestListBox, labelArray, enabledArray, selectedArray);
        env->DeleteLocalRef(selectedArray);
    } else {
        // Pass up the single selection.
        env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mRequestSingleListBox, labelArray, enabledArray, selectedCountOrSelection);
    }

    env->DeleteLocalRef(labelArray);
    env->DeleteLocalRef(enabledArray);
    checkException(env);

    Retain(reply);
    mPopupReply = reply;
}

bool WebViewCore::keyUp(KeyCode keyCode, int keyVal)
{
    DBG_NAV_LOGD("keyCode=%d", keyCode);
    bool keyHandled = false;
    WebCore::Node* focusNode = mFrame->getCacheBuilder().currentFocus();
    if (focusNode != NULL) { 
        WebCore::FrameAndroid* focusFrame = Android(focusNode->document()->frame());
        
        WebCoreFrameBridge* bridge = mFrame->bridge();
        switch (keyCode) {
            case kKeyCodeNewline:
            case kKeyCodeDpadCenter: {
                focusFrame->loader()->resetMultipleFormSubmissionProtection();
                bridge->setInKeyHandler(true);
                if ((focusNode->hasTagName(WebCore::HTMLNames::inputTag) &&
                        ((WebCore::HTMLInputElement*)focusNode)->isTextField()) ||
                        focusNode->hasTagName(WebCore::HTMLNames::textareaTag)) {
                    // Create the key down event.
                    WebCore::PlatformKeyboardEvent keydown(keyCode, keyVal, true, false, false, false, false);
                    // Create the key up event.
                    WebCore::PlatformKeyboardEvent keyup(keyCode, keyVal, false, false, false, false, false);
                    // Send both events.
                    keyHandled = focusFrame->eventHandler()->keyEvent(keydown);
                    keyHandled |= focusFrame->eventHandler()->keyEvent(keyup);
                } else {
                    keyHandled = handleMouseClick(focusFrame, focusNode);
                }
                bridge->setInKeyHandler(false);
                break;
            }
            default:
                keyHandled = false;
        }
    }
    mBlockFocusChange = false;
    return keyHandled;
}

void WebViewCore::touchUp(int touchGeneration, int buildGeneration, 
    WebCore::Frame* frame, WebCore::Node* node, int x, int y, int size, 
    bool isClick, bool retry)
{
    if (mTouchGeneration > touchGeneration) {
        DBG_NAV_LOGD("mTouchGeneration=%d > touchGeneration=%d"
            " x=%d y=%d", mTouchGeneration, touchGeneration, x, y);
        return; // short circuit if a newer touch has been generated
    }
    if (retry)
        finalKitFocus(frame, node, x, y);
    else if (commonKitFocus(touchGeneration, buildGeneration, 
            frame, node, x, y, false) == false) {
        return;
    }
    mLastGeneration = touchGeneration;
    // If this is just a touch and not a click, we have already done the change in focus,
    // so just leave the function now.
    if (!isClick)
        return;
    WebCore::EditorClientAndroid* client = static_cast<WebCore::EditorClientAndroid*>(mFrame->editor()->client());
    client->setFromClick(true);
    DBG_NAV_LOGD("touchGeneration=%d handleMouseClick frame=%p node=%p"
        " x=%d y=%d", touchGeneration, frame, node, x, y);
    handleMouseClick(frame, node);
    client->setFromClick(false);
}

bool WebViewCore::handleMouseClick(WebCore::Frame* framePtr, WebCore::Node* nodePtr) {
    if (framePtr != NULL && mFrame->getCacheBuilder().validNode(framePtr, nodePtr) == false)
        return false;
    WebCoreFrameBridge* bridge = mFrame->bridge();
    // Need to special case area tags because an image map could have an area element in the middle
    // so when attempting to get the default, the point chosen would be follow the wrong link.
    if (nodePtr != NULL && nodePtr->hasTagName(WebCore::HTMLNames::areaTag)) {
        bridge->setInKeyHandler(true);
        WebCore::EventTargetNodeCast(nodePtr)->dispatchSimulatedClick(0, true, true);
        bridge->setInKeyHandler(false);
        return true;
    }
    WebCore::RenderObject* renderer = nodePtr != NULL ? nodePtr->renderer() : NULL;
    if (renderer) {
        if (renderer->isMenuList()) {
            WebCore::HTMLSelectElement* select = static_cast<WebCore::HTMLSelectElement*>(nodePtr);
            const WTF::Vector<WebCore::HTMLElement*>& listItems = select->listItems();
            SkTDArray<const uint16_t*> names;
            SkTDArray<int> enabledArray;
            SkTDArray<int> selectedArray;
            int size = listItems.size();
            bool multiple = select->multiple();
            for (int i = 0; i < size; i++) {
                if (listItems[i]->hasTagName(WebCore::HTMLNames::optionTag)) {
                    WebCore::HTMLOptionElement* option = static_cast<WebCore::HTMLOptionElement*>(listItems[i]);
                    *names.append() = stringConverter(option->optionText());
                    *enabledArray.append() = option->disabled() ? 0 : 1;
                    if (multiple && option->selected())
                        *selectedArray.append() = i;
                } else if (listItems[i]->hasTagName(WebCore::HTMLNames::optgroupTag)) {
                    WebCore::HTMLOptGroupElement* optGroup = static_cast<WebCore::HTMLOptGroupElement*>(listItems[i]);
                    *names.append() = stringConverter(optGroup->groupLabelText());
                    *enabledArray.append() = 0;
                    if (multiple)
                        *selectedArray.append() = 0;
                }
            }
            WebCoreReply* reply = new ListBoxReply(select, select->document()->frame(), this);
            listBoxRequest(reply, names.begin(), size, enabledArray.begin(), enabledArray.count(), 
                    multiple, selectedArray.begin(), multiple ? selectedArray.count() : 
                    select->optionToListIndex(select->selectedIndex()));
            return true;
        }
    }
    if (NULL == framePtr)
        framePtr = mFrame;
    bridge->setInKeyHandler(true);
    DBG_NAV_LOGD("mMousePos={%d,%d}", mMousePos.x(), mMousePos.y());
    WebCore::PlatformMouseEvent mouseDown(mMousePos, mMousePos, WebCore::LeftButton,
            WebCore::MouseEventPressed, 1, false, false, false, false,
            WebCore::currentTime());
    // ignore the return from as it will return true if the hit point can trigger selection change
    framePtr->eventHandler()->handleMousePressEvent(mouseDown);
    WebCore::PlatformMouseEvent mouseUp(mMousePos, mMousePos, WebCore::LeftButton,
            WebCore::MouseEventReleased, 1, false, false, false, false,
            WebCore::currentTime());
    bool handled = framePtr->eventHandler()->handleMouseReleaseEvent(mouseUp);
    bridge->setInKeyHandler(false);
    return handled;
}

void WebViewCore::popupReply(int index)
{
    if (mPopupReply) {
        mPopupReply->replyInt(index);
        Release(mPopupReply);
        mPopupReply = NULL;
    }
}

void WebViewCore::popupReply(SkTDArray<int> array) {
    if (mPopupReply) {
        mPopupReply->replyIntArray(array);
        Release(mPopupReply);
        mPopupReply = NULL;
    }
}

void WebViewCore::jsAlert(const WebCore::String& url, const WebCore::String& text)
{
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    jstring jInputStr = env->NewString((unsigned short *)text.characters(), text.length());
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());
    env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mJsAlert, jUrlStr, jInputStr);
    env->DeleteLocalRef(jInputStr);
    env->DeleteLocalRef(jUrlStr);
    checkException(env);
}

bool WebViewCore::jsConfirm(const WebCore::String& url, const WebCore::String& text)
{
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    jstring jInputStr = env->NewString((unsigned short *)text.characters(), text.length());
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());
    jboolean result = env->CallBooleanMethod(mJavaGlue->mObj, mJavaGlue->mJsConfirm, jUrlStr, jInputStr);
    env->DeleteLocalRef(jInputStr);
    env->DeleteLocalRef(jUrlStr);
    checkException(env);
    return result;
}

bool WebViewCore::jsPrompt(const WebCore::String& url, const WebCore::String& text, const WebCore::String& defaultValue, WebCore::String& result)
{
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    jstring jInputStr = env->NewString((unsigned short *)text.characters(), text.length());
    jstring jDefaultStr = env->NewString((unsigned short *)defaultValue.characters(), defaultValue.length());
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());
    jstring returnVal = (jstring) env->CallObjectMethod(mJavaGlue->mObj, mJavaGlue->mJsPrompt, jUrlStr, jInputStr, jDefaultStr);
    // If returnVal is null, it means that the user cancelled the dialog.
    if (!returnVal)
        return false;
    
    const jchar* outputChars = env->GetStringChars(returnVal, NULL);
    if (outputChars == NULL) {
        result = WebCore::String();
    } else {
        result = WebCore::String((const UChar*) outputChars, env->GetStringLength(returnVal));
    }
    env->ReleaseStringChars(returnVal, outputChars);
    env->DeleteLocalRef(jInputStr);
    env->DeleteLocalRef(jDefaultStr);
    env->DeleteLocalRef(jUrlStr);
    checkException(env);
    return true;
}

bool WebViewCore::jsUnload(const WebCore::String& url, const WebCore::String& message)
{
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    jstring jInputStr = env->NewString((unsigned short *)message.characters(), message.length());
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());
    jboolean result = env->CallBooleanMethod(mJavaGlue->mObj, mJavaGlue->mJsUnload, jUrlStr, jInputStr);
    env->DeleteLocalRef(jInputStr);
    env->DeleteLocalRef(jUrlStr);
    checkException(env);
    return result;
}

WebCoreViewBridge*
WebViewCore::createBridgeForView(WebCore::FrameView* view)
{
    SkASSERT(view);
    WebViewCore* viewBridge = new WebViewCore(javavm_to_jnienv(mJavaGlue->mJVM), mJavaGlue->mObj, view);
    WebCore::IntRect r = getBounds();
    viewBridge->setBounds(r.x(), r.y(), r.right(), r.bottom());
    return viewBridge;
}

jobject
WebViewCore::getJavaObject()
{
    return mJavaGlue->mObj;
}

jobject
WebViewCore::getWebViewJavaObject()
{
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    return env->GetObjectField(mJavaGlue->mObj, gWebViewCoreFields.mWebView);
}

void WebViewCore::updateTextfield(WebCore::Node* ptr, bool changeToPassword, 
        const WebCore::String& text)
{
    if (mBlockTextfieldUpdates)
        return;
    JNIEnv* env = javavm_to_jnienv(mJavaGlue->mJVM);
    if (changeToPassword) {
        env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mUpdateTextfield,
                (int) ptr, true, NULL, mTextGeneration);
        checkException(env);
        return;
    }
    int length = text.length();
    jstring string = env->NewString((unsigned short *) text.characters(), length);
    env->CallVoidMethod(mJavaGlue->mObj, mJavaGlue->mUpdateTextfield,
            (int) ptr, false, string, mTextGeneration);
    env->DeleteLocalRef(string);
    checkException(env);
}

void WebViewCore::setSnapAnchor(int x, int y)
{
    mSnapAnchorNode = NULL;
    if (x == 0 && y == 0) {
        return;
    }

    WebCore::IntPoint point = WebCore::IntPoint(x, y);
    WebCore::Node* node = mFrame->eventHandler()->hitTestResultAtPoint(point, false).innerNode();
    if (node) {
//        LOGD("found focus node name: %s, type %d\n", node->nodeName().utf8().data(), node->nodeType());
        while (node) {
            if (node->hasTagName(WebCore::HTMLNames::divTag) || 
                    node->hasTagName(WebCore::HTMLNames::tableTag)) {
                mSnapAnchorNode = node;
                return;
            }
//            LOGD("parent node name: %s, type %d\n", node->nodeName().utf8().data(), node->nodeType());
            node = node->parentNode();
        }
    }
}

void WebViewCore::snapToAnchor()
{
    if (mSnapAnchorNode) {
        if (mSnapAnchorNode->inDocument()) {
            int rx, ry;
            mSnapAnchorNode->renderer()->absolutePosition(rx, ry);
            scrollTo(rx, ry);
        } else {
            mSnapAnchorNode = NULL;
        }
    }
}

void WebViewCore::setBackgroundColor(SkColor c)
{
    WebCore::FrameView* view = mFrame->view();
    if (view == NULL)
        return;
    WebCore::Color bcolor(SkColorGetR(c), SkColorGetG(c), SkColorGetB(c), 
        SkColorGetA(c));
    view->setBaseBackgroundColor(bcolor);
}

//----------------------------------------------------------------------
// Native JNI methods
//----------------------------------------------------------------------
static jstring WebCoreStringToJString(JNIEnv *env, WebCore::String string)
{
    int length = string.length();
    if (0 == length)
        return 0;
    jstring ret = env->NewString((jchar *)string.characters(), length);
    env->DeleteLocalRef(ret);
    return ret;
}

void WebViewCore::SetSize(JNIEnv *env, jobject obj, jint width, jint height,
        jint screenWidth, jfloat scale)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterWV counter;
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);    
    LOGV("webviewcore::nativeSetSize(%u %u)\n viewImpl: %p", (unsigned)width, (unsigned)height, viewImpl);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeSetSize");
    // convert the scale to an int
    int s = (int) (scale * 100);
    // a negative value indicates that we should not change the scale
    if (scale < 0)
        s = viewImpl->scale();
    
    viewImpl->setSizeScreenWidthAndScale(width, height, screenWidth, s);
}

void WebViewCore::SetVisibleRect(JNIEnv *env, jobject obj,
                                 jint x, jint y, jint width, jint height)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterWV counter;
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);    
    LOG_ASSERT(viewImpl, "need viewImpl");
    
    viewImpl->setVisibleRect(WebCore::IntRect(x, y, width, height));
}

jboolean WebViewCore::KeyUp(JNIEnv *env, jobject obj, jint key, jint val)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterWV counter;
#endif
//    LOGV("webviewcore::nativeKeyUp(%u)\n", (unsigned)key);
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);    
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeKeyUp");
    return viewImpl->keyUp((KeyCode)key, val);
}

void WebViewCore::DeleteSelection(JNIEnv *env, jobject obj, 
    jint frame, jint node, jint x, jint y, jint start, jint end)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterWV counter;
#endif
    LOGV("webviewcore::nativeDeleteSelection()\n");
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);    
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeDeleteSelection");
    viewImpl->deleteSelection((WebCore::Frame*) frame, (WebCore::Node*) node,
        x, y, start, end);
}

void WebViewCore::SetSelection(JNIEnv *env, jobject obj, 
    jint frame, jint node, jint x, jint y, jint start, jint end)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterWV counter;
#endif
    LOGV("webviewcore::nativeSetSelection()\n");
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);    
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeDeleteSelection");
    viewImpl->setSelection((WebCore::Frame*) frame, (WebCore::Node*) node,
        x, y, start, end);
}


void WebViewCore::ReplaceTextfieldText(JNIEnv *env, jobject obj, 
        jint framePtr, jint nodePtr, jint x, jint y, jint oldStart, jint oldEnd,
        jstring replace, jint start, jint end)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterWV counter;
#endif
    LOGV("webviewcore::nativeReplaceTextfieldText()\n");
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);    
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeReplaceTextfieldText");
    viewImpl->replaceTextfieldText((WebCore::Frame*) framePtr, (WebCore::Node*) nodePtr, x, y, oldStart, 
            oldEnd, replace, start, end);
}

void WebViewCore::PassToJs(JNIEnv *env, jobject obj, jint frame, jint node, 
    jint x, jint y, jint generation, jstring currentText, jint keyCode,
    jint keyValue, jboolean down, jboolean cap, jboolean fn, jboolean sym)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterWV counter;
#endif
    LOGV("webviewcore::nativePassToJs()\n");
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativePassToJs");
    viewImpl->passToJs((WebCore::Frame*) frame, (WebCore::Node*) node,
        x, y, generation, currentText, keyCode, keyValue, down, cap, fn, sym);
}

void WebViewCore::SaveDocumentState(JNIEnv *env, jobject obj, jint frame, jint node, jint x, jint y)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterWV counter;
#endif
    LOGV("webviewcore::nativeSaveDocumentState()\n");
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeSaveDocumentState");
    viewImpl->saveDocumentState((WebCore::Frame*) frame, (WebCore::Node*) node, x, y);
}

void WebViewCore::Draw(JNIEnv *env, jobject obj, jobject contentPict)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterWV counter;
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeDraw");
    LOG_ASSERT(contentPict, "need a valid java picture for nativeDraw");
    SkPicture* picture = GraphicsJNI::getNativePicture(env, contentPict);
    LOG_ASSERT(picture, "need native picture in nativeDraw");
    viewImpl->recordPicture(picture, true);
}

void WebViewCore::SendListBoxChoice(JNIEnv* env, jobject obj, jint choice)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterWV counter;
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeSendListBoxChoice");
    viewImpl->popupReply(choice);
}

void WebViewCore::SendListBoxChoices(JNIEnv* env, jobject obj, jbooleanArray jArray, jint size)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterWV counter;
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeSendListBoxChoices");
    jboolean* ptrArray = env->GetBooleanArrayElements(jArray, NULL);
    SkTDArray<int> array;
    for (int i = 0; i < size; i++) {
        if (ptrArray[i]) {
            *array.append() = i;
        }
    }
    viewImpl->popupReply(array);
}

void WebViewCore::ClearMatches(JNIEnv *env, jobject obj)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeClearMatches");
    
    WebCore::Frame* frame = viewImpl->frame();
    while (frame) {
        if (frame->document())
            frame->document()->removeMarkers();
        WebCore::SelectionController* controller = frame->selectionController();
        if (controller->isRange()) {
            WebCore::Position pos = controller->start();
            WebCore::Position end = controller->end();
            do {
                if (pos.inRenderedText()) {
                    WebCore::RenderObject* renderer = pos.node()->renderer();
                    if (renderer != NULL) {
                        renderer->setSelectionState(WebCore::RenderObject::SelectionNone);
                    }
                }
                pos = pos.next();
            } while (pos != end);
            controller->clear();
        }
        frame = frame->tree()->traverseNext();
    }
}

jboolean WebViewCore::Find(JNIEnv *env, jobject obj, jstring find,
        jboolean forward, jboolean fromSelection) {
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeFind");
    
    if (NULL == find)
        return false;
    int length = env->GetStringLength(find);
    if (0 == length)
        return false;   // perhaps we want to return true, in order to show the correct dialog
    const jchar* findChars = env->GetStringChars(find, NULL);
    // Will this ever return NULL if find is not NULL and its length is not zero?
    if (NULL == findChars)
        return false;
    WebCore::String findString  = WebCore::String((const UChar*) findChars, length);
    env->ReleaseStringChars(find, findChars);
    WebCore::Frame* mainFrame = viewImpl->frame();
    WebCore::Frame* frame = mainFrame;
    bool found = false;
    if (fromSelection) {
        // Need to figure out which frame has a selection, then start from there
        while (frame && frame->selectionController()->isNone()) {
            frame = frame->tree()->traverseNext();
        }
        if (!frame) {
            // No selection, so start from the beginning.
            frame = mainFrame;
        }
        // Now we have the frame with the selection.
        WebCore::Frame* starterFrame = frame;
        bool firstTimeThroughLoop = true;
        if (forward) { 
            while (!(found = frame->findString(findString, true, false, false, true))) {
                if (firstTimeThroughLoop) {
                    firstTimeThroughLoop = false;
                    // FIXME: Clearing the selection is necessary for the search to 
                    // work properly.  However, the invalidation does not remove
                    // the drawn rectangle from old frames.
                    frame->selectionController()->clear();
                }
                frame = frame->tree()->traverseNextWithWrap(true);
                if (frame == starterFrame)
                    break;
            }
        } else {
            // Search backwards, searching the frames in reverse order as well.
            while (!(found = frame->findString(findString, false, false, false, true))) {
                if (firstTimeThroughLoop) {
                    firstTimeThroughLoop = false;
                    frame->selectionController()->clear();
                }
                frame = frame->tree()->traversePreviousWithWrap(true);
                if (frame == starterFrame)
                    break;
            }
        }
        // In order to wrap to the beginning of the initial frame.
        if (!found) {
            found = frame->findString(findString, forward, false, false, true);
        }
    } else {
        // Clear any old selections so find will work properly.
        while (frame) {
            frame->selectionController()->clear();
            frame->invalidateSelection();
            frame = frame->tree()->traverseNext();
        }
        frame = mainFrame;
        do {
            found = frame->findString(findString, forward, false, false, false);
        } while (!found && (frame = frame->tree()->traverseNext()) != NULL);
    }
    if (found) {
        frame->view()->scrollRectIntoViewRecursively(frame->selectionController()->caretRect());
        return true;
    }
    return false;
}

jstring WebViewCore::FindAddress(JNIEnv *env, jobject obj, jstring addr)
{
    if (NULL == addr)
        return NULL;
    int length = env->GetStringLength(addr);
    if (0 == length)
        return NULL;
    const jchar* addrChars = env->GetStringChars(addr, NULL);
    int start, end;
    bool success = WebCore::CacheBuilder::FindAddress(addrChars, length, 
        &start, &end) == WebCore::CacheBuilder::FOUND_COMPLETE;
    jstring ret = NULL;
    if (success) {
        ret = env->NewString((jchar*) addrChars + start, end - start);
        env->DeleteLocalRef(ret);
    }
    env->ReleaseStringChars(addr, addrChars);
    return ret;
}

jint WebViewCore::FindAll(JNIEnv *env, jobject obj, jstring find) {
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeFindAll");
    if (NULL == find)
        return 0;
    int length = env->GetStringLength(find);
    if (0 == length)
        return 0;
    const jchar* findChars = env->GetStringChars(find, NULL);
    // Will this ever return NULL if find is not NULL and its length is not zero?
    if (NULL == findChars)
        return 0;
    WebCore::String findString  = WebCore::String((const UChar*) findChars, length);
    env->ReleaseStringChars(find, findChars);
    WebCore::Frame* frame = viewImpl->frame();
    int numFound = 0;
    ClearMatches(env, obj);
    while (frame) {
        frame->selectionController()->clear();
        frame->invalidateSelection();
        if (frame->document())
            frame->document()->removeMarkers();
        frame->setMarkedTextMatchesAreHighlighted(true);
        numFound += frame->markAllMatchesForText(findString, false, 0);
        frame = frame->tree()->traverseNext();
    }
    if (numFound > 0)
        Find(env, obj, find, true, false);
    return numFound;
}

void WebViewCore::TouchUp(JNIEnv *env, jobject obj, jint touchGeneration,
        jint buildGeneration, jint frame, jint node, jint x, jint y, jint size,
        jboolean isClick, jboolean retry)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterWV counter;
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    viewImpl->touchUp(touchGeneration, buildGeneration,
        (WebCore::Frame*) frame, (WebCore::Node*) node, x, y, size, isClick, retry);
}

jstring WebViewCore::RetrieveHref(JNIEnv *env, jobject obj, jint frame,
        jint node)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    WebCore::String result = viewImpl->retrieveHref((WebCore::Frame*) frame,
            (WebCore::Node*) node);
    if (!result.isEmpty())
        return WebCoreStringToJString(env, result);
    return NULL;
}

jstring WebViewCore::RetrieveImageRef(JNIEnv *env, jobject obj, jint x, jint y)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    WebCore::String result = viewImpl->retrieveImageRef(x, y);
    if (!result.isEmpty())
        return WebCoreStringToJString(env, result);
    return NULL;
}

void WebViewCore::SetFinalFocus(JNIEnv *env, jobject obj, jint frame, jint node,
        jint x, jint y, jboolean block)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterWV counter;
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    viewImpl->setFinalFocus((WebCore::Frame*) frame, (WebCore::Node*) node, x, 
        y, block);
}

void WebViewCore::SetKitFocus(JNIEnv *env, jobject obj, jint moveGeneration,
        jint buildGeneration, jint frame, jint node, jint x, jint y,
        jboolean ignoreNullFocus)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterWV counter;
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    viewImpl->setKitFocus(moveGeneration, buildGeneration,
        (WebCore::Frame*) frame, (WebCore::Node*) node, x, y,
        ignoreNullFocus);
}

void WebViewCore::UnblockFocus(JNIEnv *env, jobject obj)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    viewImpl->unblockFocus();
}

void WebViewCore::UpdateFrameCache(JNIEnv *env, jobject obj)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    viewImpl->updateFrameCache();
}

jint WebViewCore::GetContentMinPrefWidth(JNIEnv *env, jobject obj)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);

    WebCore::Frame* frame = viewImpl->frame();
    if (frame) {
        WebCore::Document* document = frame->document();
        if (document) {
            WebCore::RenderObject* renderer = document->renderer();
            if (renderer && renderer->isRenderView()) {
                return renderer->minPrefWidth();
            }    
        }
    }
    return 0;
}

void WebViewCore::SetViewportSettingsFromNative(JNIEnv *env, jobject obj)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);

    WebCore::Settings* s = viewImpl->frame()->page()->settings();
    if (s == NULL)
        return;
    
    env->SetIntField(obj, gWebViewCoreFields.mViewportWidth, s->viewportWidth());
    env->SetIntField(obj, gWebViewCoreFields.mViewportHeight, s->viewportHeight());
    env->SetIntField(obj, gWebViewCoreFields.mViewportInitialScale, s->viewportInitialScale());
    env->SetIntField(obj, gWebViewCoreFields.mViewportMinimumScale, s->viewportMinimumScale());
    env->SetIntField(obj, gWebViewCoreFields.mViewportMaximumScale, s->viewportMaximumScale());
    env->SetBooleanField(obj, gWebViewCoreFields.mViewportUserScalable, s->viewportUserScalable());
}

void WebViewCore::SetSnapAnchor(JNIEnv *env, jobject obj, jint x, jint y)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    
    viewImpl->setSnapAnchor(x, y);
}

void WebViewCore::SnapToAnchor(JNIEnv *env, jobject obj)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);

    viewImpl->snapToAnchor();
}

void WebViewCore::SetBackgroundColor(JNIEnv *env, jobject obj, jint color)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);

    viewImpl->setBackgroundColor((SkColor) color);
}

static void jrect_to_skirect(JNIEnv* env, jobject obj, SkIRect* result)
{
    int L, T, R, B;
    GraphicsJNI::get_jrect(env, obj, &L, &T, &R, &B);
    result->set(L, T, R, B);
}

jstring WebViewCore::GetSelection(JNIEnv *env, jobject obj, jobject selRgn)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    SkRegion* selectionRegion = GraphicsJNI::getNativeRegion(env, selRgn);
    WebCore::String result = viewImpl->getSelection(selectionRegion);
    if (!result.isEmpty())
        return WebCoreStringToJString(env, result);
    return NULL;
}

void WebViewCore::Dump(JNIEnv *env, jobject obj)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);

    viewImpl->dump();
}

void WebViewCore::RefreshPlugins(JNIEnv *env,
                                 jobject obj,
                                 jboolean reloadOpenPages)
{
    // Refresh the list of plugins, optionally reloading all open
    // pages.
    WebCore::refreshPlugins(reloadOpenPages);
}

// ----------------------------------------------------------------------------

/*
 * JNI registration.
 */
static JNINativeMethod gJavaWebViewCoreMethods[] = {
    { "nativeClearMatches", "()V",
        (void*) WebViewCore::ClearMatches } ,
    { "nativeDraw", "(Landroid/graphics/Picture;)V",
        (void*) WebViewCore::Draw },
    { "nativeKeyUp", "(II)Z",
        (void*) WebViewCore::KeyUp },
    { "nativeSendListBoxChoices", "([ZI)V",
        (void*) WebViewCore::SendListBoxChoices },
    { "nativeSendListBoxChoice", "(I)V",
        (void*) WebViewCore::SendListBoxChoice },
    { "nativeSetSize", "(IIIF)V",
        (void*) WebViewCore::SetSize },
    { "nativeSetVisibleRect", "(IIII)V",
        (void*) WebViewCore::SetVisibleRect },
    { "nativeSetSelection", "(IIIIII)V",
        (void*) WebViewCore::SetSelection } ,
    { "nativeDeleteSelection", "(IIIIII)V",
        (void*) WebViewCore::DeleteSelection } ,
    { "nativeReplaceTextfieldText", "(IIIIIILjava/lang/String;II)V",
        (void*) WebViewCore::ReplaceTextfieldText } ,
    { "passToJs", "(IIIIILjava/lang/String;IIZZZZ)V",
        (void*) WebViewCore::PassToJs } ,
    { "nativeSaveDocumentState", "(IIII)V",
        (void*) WebViewCore::SaveDocumentState },
    { "nativeFind", "(Ljava/lang/String;ZZ)Z",
        (void*) WebViewCore::Find } ,
    { "nativeFindAddress", "(Ljava/lang/String;)Ljava/lang/String;", 
        (void*) WebViewCore::FindAddress },
    { "nativeFindAll", "(Ljava/lang/String;)I",
        (void*) WebViewCore::FindAll } ,
    { "nativeTouchUp", "(IIIIIIIZZ)V",
        (void*) WebViewCore::TouchUp },
    { "nativeRetrieveImageRef", "(II)Ljava/lang/String;",
        (void*) WebViewCore::RetrieveImageRef },
    { "nativeRetrieveHref", "(II)Ljava/lang/String;",
        (void*) WebViewCore::RetrieveHref },
    { "nativeSetFinalFocus", "(IIIIZ)V",
        (void*) WebViewCore::SetFinalFocus },
    { "nativeSetKitFocus", "(IIIIIIZ)V",
        (void*) WebViewCore::SetKitFocus },
    { "nativeUnblockFocus", "()V",
        (void*) WebViewCore::UnblockFocus },
    { "nativeUpdateFrameCache", "()V",
        (void*) WebViewCore::UpdateFrameCache },
    { "nativeGetContentMinPrefWidth", "()I",
        (void*) WebViewCore::GetContentMinPrefWidth },
    { "setViewportSettingsFromNative", "()V",
        (void*) WebViewCore::SetViewportSettingsFromNative },
    { "nativeSetSnapAnchor", "(II)V",
        (void*) WebViewCore::SetSnapAnchor },
    { "nativeSnapToAnchor", "()V",
        (void*) WebViewCore::SnapToAnchor },
    { "nativeSetBackgroundColor", "(I)V",
        (void*) WebViewCore::SetBackgroundColor },
    { "nativeGetSelection", 
        "(Landroid/graphics/Region;)Ljava/lang/String;",
        (void*) WebViewCore::GetSelection },
    { "nativeRefreshPlugins", "(Z)V",
        (void*) WebViewCore::RefreshPlugins },
    { "nativeDump", "()V",
        (void*) WebViewCore::Dump }
};

int register_webviewcore(JNIEnv* env)
{
    jclass widget = env->FindClass("android/webkit/WebViewCore");
    LOG_ASSERT(widget != NULL,
            "Unable to find class android/webkit/WebViewCore");
    gWebViewCoreFields.mNativeClass = env->GetFieldID(widget, "mNativeClass",
            "I");
    LOG_ASSERT(gWebViewCoreFields.mNativeClass != NULL,
            "Unable to find android/webkit/WebViewCore.mNativeClass");
    gWebViewCoreFields.mViewportWidth = env->GetFieldID(widget,
            "mViewportWidth", "I");
    LOG_ASSERT(gWebViewCoreFields.mViewportWidth != NULL,
            "Unable to find android/webkit/WebViewCore.mViewportWidth");
    gWebViewCoreFields.mViewportHeight = env->GetFieldID(widget,
            "mViewportHeight", "I");
    LOG_ASSERT(gWebViewCoreFields.mViewportHeight != NULL,
            "Unable to find android/webkit/WebViewCore.mViewportHeight");
    gWebViewCoreFields.mViewportInitialScale = env->GetFieldID(widget,
            "mViewportInitialScale", "I");
    LOG_ASSERT(gWebViewCoreFields.mViewportInitialScale != NULL,
            "Unable to find android/webkit/WebViewCore.mViewportInitialScale");
    gWebViewCoreFields.mViewportMinimumScale = env->GetFieldID(widget,
            "mViewportMinimumScale", "I");
    LOG_ASSERT(gWebViewCoreFields.mViewportMinimumScale != NULL,
            "Unable to find android/webkit/WebViewCore.mViewportMinimumScale");
    gWebViewCoreFields.mViewportMaximumScale = env->GetFieldID(widget,
            "mViewportMaximumScale", "I");
    LOG_ASSERT(gWebViewCoreFields.mViewportMaximumScale != NULL,
            "Unable to find android/webkit/WebViewCore.mViewportMaximumScale");
    gWebViewCoreFields.mViewportUserScalable = env->GetFieldID(widget,
            "mViewportUserScalable", "Z");
    LOG_ASSERT(gWebViewCoreFields.mViewportUserScalable != NULL,
            "Unable to find android/webkit/WebViewCore.mViewportUserScalable");
    gWebViewCoreFields.mWebView = env->GetFieldID(widget,
            "mWebView", "Landroid/webkit/WebView;");
    LOG_ASSERT(gWebViewCoreFields.mWebView != NULL,
            "Unable to find android/webkit/WebViewCore.mWebView");

    return jniRegisterNativeMethods(env, "android/webkit/WebViewCore",
            gJavaWebViewCoreMethods, NELEM(gJavaWebViewCoreMethods));
}

} /* namespace android */
