/*
** Copyright 2006-2008, The Android Open Source Project
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

#define LOG_TAG "webcoreglue"

#include <config.h>
#include <wtf/Platform.h>

#include "android_graphics.h"
#include "Arena.h"
#include "AtomicString.h"
#include "Cache.h"
#include "ChromeClientAndroid.h"
#include "ContextMenuClientAndroid.h"
#include "DeprecatedString.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DragClientAndroid.h"
#include "EditorClientAndroid.h"
#include "Element.h"
#include "Font.h"
#include "FrameAndroid.h"
#include "FrameLoader.h"
#include "FrameLoaderClientAndroid.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GCController.h"
#include "GraphicsContext.h"
#include "HistoryItem.h"
#include "HTMLElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "IconDatabase.h"
#include "Image.h"
#include "InspectorClientAndroid.h"
#include "KURL.h"
#include "Page.h"
#include "PageCache.h"
#include "PlatformString.h"
#include "RenderPart.h"
#include "RenderSkinAndroid.h"
#include "RenderTreeAsText.h"
#include "ResourceHandle.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SubstituteData.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreResourceLoader.h"
#include "WebHistory.h"
#include "WebIconDatabase.h"
#include "WebViewCore.h"

#ifdef LOG
#undef LOG
#endif

#include <JNIHelp.h>
#include <SkGraphics.h>
#include <SkImageRef_GlobalPool.h>
#include <utils/Log.h>
#include <utils/misc.h>
#include <utils/AssetManager.h>
#include <android_runtime/android_util_AssetManager.h>

#ifdef ANDROID_INSTRUMENT
#include "SystemTime.h"

static uint32_t sTotalJavaTimeUsed = 0;
static uint32_t sTotalNativeTimeUsed = 0;

namespace WebCore {
void Frame::resetFramebridgeTimeCounter()
{
    sTotalJavaTimeUsed = 0;
    sTotalNativeTimeUsed = 0;
}

void Frame::reportFramebridgeTimeCounter()
{
    LOG(LOG_DEBUG, "WebCore", "*-* Total Java callback (frame bridge) time: %d ms\n",
            sTotalJavaTimeUsed);
    LOG(LOG_DEBUG, "WebCore", "*-* Total native 1 (frame bridge) time: %d ms\n",
            sTotalNativeTimeUsed);
}
}
#endif

namespace android {

#ifdef ANDROID_INSTRUMENT
class TimeCounterFB {
public:
    TimeCounterFB(bool native = false) {
        mNative = native;
        mStartTime = WebCore::get_thread_msec();
    }

    ~TimeCounterFB() {
        if (mNative)
            sTotalNativeTimeUsed += WebCore::get_thread_msec() - mStartTime;
        else
            sTotalJavaTimeUsed += WebCore::get_thread_msec() - mStartTime;
    }
private:
    bool mNative;
    uint32_t mStartTime;
};
#endif

// ----------------------------------------------------------------------------

#define WEBCORE_MEMORY_CAP 15 * 1024 * 1024

// ----------------------------------------------------------------------------

JavaVM* jnienv_to_javavm(JNIEnv* env)
{
    JavaVM* vm;
    return env->GetJavaVM(&vm) >= 0 ? vm : NULL;
}

JNIEnv* javavm_to_jnienv(JavaVM* vm)
{
    JNIEnv* env;
    return vm->GetEnv((void **)&env, JNI_VERSION_1_4) >= 0 ? env : NULL;
}

// ----------------------------------------------------------------------------

struct WebCoreFrameBridge::JavaBrowserFrame
{
    JavaVM*     mJVM;
    jobject     mObj;
    jobject     mHistoryList; // WebBackForwardList object
    jmethodID   mStartLoadingResource;
    jmethodID   mLoadStarted;
    jmethodID   mTransitionToCommitted;
    jmethodID   mLoadFinished;
    jmethodID   mReportError;
    jmethodID   mSetTitle;
    jmethodID   mWindowObjectCleared;
    jmethodID   mSetProgress;
    jmethodID   mDidReceiveIcon;
    jmethodID   mUpdateVisitedHistory;
    jmethodID   mHandleUrl;
    jmethodID   mCreateWindow;
    jmethodID   mCloseWindow;
    jmethodID   mDecidePolicyForFormResubmission;
    jmethodID   mRequestFocus;
};

static jfieldID gFrameAndroidField;
#define GET_NATIVE_FRAME(env, obj) ((WebCore::FrameAndroid*)env->GetIntField(obj, gFrameAndroidField))
#define SET_NATIVE_FRAME(env, obj, frame) (env->SetIntField(obj, gFrameAndroidField, frame))

// ----------------------------------------------------------------------------

/**
 * Helper method for checking java exceptions
 * @return true if an exception occurred.
 */
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

// ----------------------------------------------------------------------------

WebCoreFrameBridge::WebCoreFrameBridge(JNIEnv* env, jobject obj, jobject historyList)
{
    jclass clazz = env->GetObjectClass(obj);
    mJavaFrame = new JavaBrowserFrame;
    mJavaFrame->mJVM = jnienv_to_javavm(env);
    mJavaFrame->mObj = env->NewGlobalRef(obj);
    mJavaFrame->mHistoryList = env->NewGlobalRef(historyList);
    mJavaFrame->mStartLoadingResource = env->GetMethodID(clazz, "startLoadingResource",
            "(ILjava/lang/String;Ljava/lang/String;Ljava/util/HashMap;Ljava/lang/String;IZZ)Landroid/webkit/LoadListener;");
    mJavaFrame->mLoadStarted = env->GetMethodID(clazz, "loadStarted",
            "(Ljava/lang/String;Landroid/graphics/Bitmap;IZ)V");
    mJavaFrame->mTransitionToCommitted = env->GetMethodID(clazz, "transitionToCommitted",
            "(IZ)V");
    mJavaFrame->mLoadFinished = env->GetMethodID(clazz, "loadFinished",
            "(Ljava/lang/String;IZ)V");
    mJavaFrame->mReportError = env->GetMethodID(clazz, "reportError",
            "(ILjava/lang/String;Ljava/lang/String;)V");
    mJavaFrame->mSetTitle = env->GetMethodID(clazz, "setTitle",
            "(Ljava/lang/String;)V");
    mJavaFrame->mWindowObjectCleared = env->GetMethodID(clazz, "windowObjectCleared",
            "(I)V");
    mJavaFrame->mSetProgress = env->GetMethodID(clazz, "setProgress",
            "(I)V");
    mJavaFrame->mDidReceiveIcon = env->GetMethodID(clazz, "didReceiveIcon",
            "(Landroid/graphics/Bitmap;)V");
    mJavaFrame->mUpdateVisitedHistory = env->GetMethodID(clazz, "updateVisitedHistory",
            "(Ljava/lang/String;Z)V");
    mJavaFrame->mHandleUrl = env->GetMethodID(clazz, "handleUrl",
            "(Ljava/lang/String;)Z");
    mJavaFrame->mCreateWindow = env->GetMethodID(clazz, "createWindow",
            "(ZZ)Landroid/webkit/BrowserFrame;");
    mJavaFrame->mCloseWindow = env->GetMethodID(clazz, "closeWindow",
            "(Landroid/webkit/WebViewCore;)V");
    mJavaFrame->mDecidePolicyForFormResubmission = env->GetMethodID(clazz,
            "decidePolicyForFormResubmission", "(I)V");
    mJavaFrame->mRequestFocus = env->GetMethodID(clazz, "requestFocus",
            "()V");

    LOG_ASSERT(mJavaFrame->mStartLoadingResource, "Could not find method startLoadingResource");
    LOG_ASSERT(mJavaFrame->mLoadStarted, "Could not find method loadStarted");
    LOG_ASSERT(mJavaFrame->mTransitionToCommitted, "Could not find method transitionToCommitted");
    LOG_ASSERT(mJavaFrame->mLoadFinished, "Could not find method loadFinished");
    LOG_ASSERT(mJavaFrame->mReportError, "Could not find method reportError");
    LOG_ASSERT(mJavaFrame->mSetTitle, "Could not find method setTitle");
    LOG_ASSERT(mJavaFrame->mWindowObjectCleared, "Could not find method windowObjectCleared");
    LOG_ASSERT(mJavaFrame->mSetProgress, "Could not find method setProgress");
    LOG_ASSERT(mJavaFrame->mDidReceiveIcon, "Could not find method didReceiveIcon");
    LOG_ASSERT(mJavaFrame->mUpdateVisitedHistory, "Could not find method updateVisitedHistory");
    LOG_ASSERT(mJavaFrame->mHandleUrl, "Could not find method handleUrl");
    LOG_ASSERT(mJavaFrame->mCreateWindow, "Could not find method createWindow");
    LOG_ASSERT(mJavaFrame->mCloseWindow, "Could not find method closeWindow");
    LOG_ASSERT(mJavaFrame->mDecidePolicyForFormResubmission, "Could not find method decidePolicyForFormResubmission");
    LOG_ASSERT(mJavaFrame->mRequestFocus, "Could not find method requestFocus");

    mUserAgent = WebCore::String();
    mInKeyHandler = false;
}

WebCoreFrameBridge::~WebCoreFrameBridge()
{
    if (mJavaFrame->mObj) {
        JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
        env->DeleteGlobalRef(mJavaFrame->mObj);
        env->DeleteGlobalRef(mJavaFrame->mHistoryList);
        mJavaFrame->mObj = 0;
    }
    delete mJavaFrame;
}

static jobject createJavaMapFromHTTPHeaders(JNIEnv* env, const WebCore::HTTPHeaderMap& map)
{
    jclass mapClass = env->FindClass("java/util/HashMap");
    LOG_ASSERT(mapClass, "Could not find HashMap class!");
    jmethodID init = env->GetMethodID(mapClass, "<init>", "(I)V");
    LOG_ASSERT(init, "Could not find constructor for HashMap");
    jobject hashMap = env->NewObject(mapClass, init, map.size());
    LOG_ASSERT(hashMap, "Could not create a new HashMap");
    jmethodID put = env->GetMethodID(mapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    LOG_ASSERT(put, "Could not find put method on HashMap");

    WebCore::HTTPHeaderMap::const_iterator end = map.end();
    for (WebCore::HTTPHeaderMap::const_iterator i = map.begin(); i != end; ++i) {
        jstring key = env->NewString((unsigned short *)i->first.characters(), i->first.length());
        jstring val = env->NewString((unsigned short *)i->second.characters(), i->second.length());
        if (key && val) {
            env->CallObjectMethod(hashMap, put, key, val);
            env->DeleteLocalRef(key);
            env->DeleteLocalRef(val);
        }
    }
    env->DeleteLocalRef(mapClass);

    return hashMap;
}

WebCoreResourceLoader*
WebCoreFrameBridge::startLoadingResource(WebCore::ResourceHandle* loader,
                                  const WebCore::ResourceRequest& request,
                                  bool isHighPriority, bool synchronous)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    LOGV("::WebCore:: startLoadingResource(%p, %s)",
            loader, request.url().string().ascii().data());

    WebCore::String method = request.httpMethod();
    WebCore::String postData;
    if (request.httpBody())
       postData = request.httpBody()->flattenToString();
    WebCore::HTTPHeaderMap headers = request.httpHeaderFields();

    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    WebCore::DeprecatedString urlStr = request.url().deprecatedString();
    jstring jUrlStr = env->NewString((unsigned short *)urlStr.unicode(), urlStr.length());
    jstring jMethodStr = NULL;
    if (!method.isEmpty())
        jMethodStr = env->NewString((unsigned short *)method.characters(), method.length());
    jstring jPostDataStr = NULL;
    if (!postData.isEmpty())
        jPostDataStr =
            env->NewString((unsigned short *)postData.characters(), postData.length());

    jobject jHeaderMap = createJavaMapFromHTTPHeaders(env, headers);

    // Convert the WebCore Cache Policy to a WebView Cache Policy.
    int cacheMode = 0;  // WebView.LOAD_NORMAL
    switch (request.cachePolicy()) {
        case WebCore::ReloadIgnoringCacheData:
            cacheMode = 2; // WebView.LOAD_NO_CACHE
            break;
        case WebCore::ReturnCacheDataDontLoad:
            cacheMode = 3; // WebView.LOAD_CACHE_ONLY
            break;
        case WebCore::ReturnCacheDataElseLoad:
            cacheMode = 1;   // WebView.LOAD_CACHE_ELSE_NETWORK
            break;
        case WebCore::UseProtocolCachePolicy:
        default:
            break;
    }

    LOGV("::WebCore:: startLoadingResource %s with cacheMode %d", urlStr.ascii(), cacheMode);


    jobject jLoadListener =
        env->CallObjectMethod(mJavaFrame->mObj, mJavaFrame->mStartLoadingResource,
                                              (int)loader, jUrlStr, jMethodStr, jHeaderMap,
                                              jPostDataStr, cacheMode, isHighPriority, synchronous);

    env->DeleteLocalRef(jUrlStr);
    env->DeleteLocalRef(jMethodStr);
    env->DeleteLocalRef(jPostDataStr);
    env->DeleteLocalRef(jHeaderMap);
    if (checkException(env))
        return NULL;

    WebCoreResourceLoader* h = NULL;
    if (jLoadListener)
        h = new WebCoreResourceLoader(env, jLoadListener);
    env->DeleteLocalRef(jLoadListener);
    return h;
}

void
WebCoreFrameBridge::reportError(int errorCode, const WebCore::String& description,
        const WebCore::String& failingUrl)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    LOGV("::WebCore:: reportError(%d, %s)", errorCode, description.ascii().data());
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);

    jstring descStr = env->NewString((unsigned short*)description.characters(), description.length());
    jstring failUrl = env->NewString((unsigned short*)failingUrl.characters(), failingUrl.length());
    env->CallVoidMethod(mJavaFrame->mObj, mJavaFrame->mReportError, errorCode, descStr, failUrl);
    env->DeleteLocalRef(descStr);
    env->DeleteLocalRef(failUrl);
}

void
WebCoreFrameBridge::loadStarted(WebCore::FrameAndroid* frame)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    const WebCore::KURL& url = frame->loader()->activeDocumentLoader()->url();
    if (url.isEmpty())
        return;
    LOGV("::WebCore:: loadStarted %s", url.string().ascii().data());

    bool isMainFrame = (!frame->tree() || !frame->tree()->parent());
    WebCore::FrameLoadType loadType = frame->loader()->loadType();

    if (loadType == WebCore::FrameLoadTypeReplace ||
            loadType == WebCore::FrameLoadTypeSame ||
            (loadType == WebCore::FrameLoadTypeRedirectWithLockedHistory &&
             !isMainFrame))
        return;

    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    WebCore::String urlString(url.string());
    // If this is the main frame and we already have a favicon in the database,
    // send it along with the page started notification.
    jobject favicon = NULL;
    if (isMainFrame) {
        WebCore::Image* icon = WebCore::iconDatabase()->iconForPageURL(urlString, WebCore::IntSize(16, 16));
        if (icon)
            favicon = webcoreImageToJavaBitmap(env, icon);
        LOGV("favicons", "Starting load with icon %p for %s", icon, url.deprecatedString().ascii());
    }
    jstring urlStr = env->NewString((unsigned short*)urlString.characters(), urlString.length());

    env->CallVoidMethod(mJavaFrame->mObj, mJavaFrame->mLoadStarted, urlStr, favicon,
            (int)loadType, isMainFrame);
    checkException(env);
    env->DeleteLocalRef(urlStr);
    if (favicon)
        env->DeleteLocalRef(favicon);
}

void
WebCoreFrameBridge::transitionToCommitted(WebCore::Frame* frame)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    WebCore::FrameLoadType loadType = frame->loader()->loadType();
    bool isMainFrame = (!frame->tree() || !frame->tree()->parent());
    env->CallVoidMethod(mJavaFrame->mObj, mJavaFrame->mTransitionToCommitted,
            (int)loadType, isMainFrame);
    checkException(env);
}

void
WebCoreFrameBridge::didFinishLoad(WebCore::Frame* frame)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    WebCore::FrameLoader* loader = frame->loader();
    const WebCore::KURL& url = loader->activeDocumentLoader()->url();
    if (url.isEmpty())
        return;
    LOGV("::WebCore:: didFinishLoad %s", url.string().ascii().data());

    bool isMainFrame = (!frame->tree() || !frame->tree()->parent());
    WebCore::FrameLoadType loadType = loader->loadType();
    WebCore::String urlString(url.string());
    jstring urlStr = env->NewString((unsigned short*)urlString.characters(), urlString.length());
    env->CallVoidMethod(mJavaFrame->mObj, mJavaFrame->mLoadFinished, urlStr,
            (int)loadType, isMainFrame);
    checkException(env);
    env->DeleteLocalRef(urlStr);
}

void
WebCoreFrameBridge::addHistoryItem(WebCore::HistoryItem* item)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    LOGV("::WebCore:: addHistoryItem");
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    WebHistory::AddItem(env, mJavaFrame->mHistoryList, item);
}

void
WebCoreFrameBridge::removeHistoryItem(int index)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    LOGV("::WebCore:: removeHistoryItem at %d", index);
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    WebHistory::RemoveItem(env, mJavaFrame->mHistoryList, index);
}

void
WebCoreFrameBridge::updateHistoryIndex(int newIndex)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    LOGV("::WebCore:: updateHistoryIndex to %d", newIndex);
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    WebHistory::UpdateHistoryIndex(env, mJavaFrame->mHistoryList, newIndex);
}

void
WebCoreFrameBridge::setTitle(const WebCore::String& title)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
#ifndef NDEBUG
    LOGV("setTitle(%s)", title.ascii().data());
#endif
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    jstring jTitleStr = env->NewString((unsigned short *)title.characters(), title.length());

    env->CallVoidMethod(mJavaFrame->mObj, mJavaFrame->mSetTitle,
                                        jTitleStr);
    checkException(env);
    env->DeleteLocalRef(jTitleStr);
}

void
WebCoreFrameBridge::windowObjectCleared(WebCore::FrameAndroid* frame)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    LOGV("::WebCore:: windowObjectCleared");
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);

    env->CallVoidMethod(mJavaFrame->mObj, mJavaFrame->mWindowObjectCleared, (int)frame);
    checkException(env);
}

void
WebCoreFrameBridge::setProgress(float newProgress)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    int progress = (int) (100 * newProgress);
    env->CallVoidMethod(mJavaFrame->mObj, mJavaFrame->mSetProgress, progress);
    checkException(env);
}

const WebCore::String
WebCoreFrameBridge::userAgentForURL(const WebCore::KURL* url)
{
    return mUserAgent;
}

void
WebCoreFrameBridge::didReceiveIcon(WebCore::Image* icon)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    LOG_ASSERT(icon, "DidReceiveIcon called without an image!");
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    jobject bitmap = webcoreImageToJavaBitmap(env, icon);
    if (!bitmap)
        return;

    env->CallVoidMethod(mJavaFrame->mObj, mJavaFrame->mDidReceiveIcon, bitmap);
    env->DeleteLocalRef(bitmap);
    checkException(env);
}

void
WebCoreFrameBridge::updateVisitedHistory(const WebCore::KURL& url, bool reload)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    WebCore::String urlStr(url.string());
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    jstring jUrlStr = env->NewString((unsigned short*)urlStr.characters(), urlStr.length());

    env->CallVoidMethod(mJavaFrame->mObj, mJavaFrame->mUpdateVisitedHistory, jUrlStr, reload);
    checkException(env);
}

bool
WebCoreFrameBridge::canHandleRequest(const WebCore::ResourceRequest& request)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    // Internal loads are ok but any request that is due to a user hitting a key
    // should be checked.
    bool userGesture = false;
#ifdef ANDROID_USER_GESTURE
    userGesture = request.userGesture();
#endif
    WebCore::KURL requestUrl = request.url();
    if (!mInKeyHandler && !userGesture &&
        (requestUrl.protocolIs("http") || requestUrl.protocolIs("https") ||
            requestUrl.protocolIs("file") || requestUrl.protocolIs("about") ||
            requestUrl.protocolIs("javascript")))
        return true;
    WebCore::String url(request.url().string());
    // Empty urls should not be sent to java
    if (url.isEmpty())
        return true;
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());

    // check to see whether browser app wants to hijack url loading.
    // if browser app handles the url, we will return false to bail out WebCore loading
    jboolean ret = env->CallBooleanMethod(mJavaFrame->mObj, mJavaFrame->mHandleUrl, jUrlStr);
    checkException(env);
    return (ret == 0);
}

WebCore::Frame*
WebCoreFrameBridge::createWindow(bool dialog, bool userGesture)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    jobject obj = env->CallObjectMethod(mJavaFrame->mObj,
            mJavaFrame->mCreateWindow, dialog, userGesture);
    if (obj) {
        WebCore::FrameAndroid* frame = GET_NATIVE_FRAME(env, obj);
        return frame;
    }
    return NULL;
}

void
WebCoreFrameBridge::requestFocus() const
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    env->CallVoidMethod(mJavaFrame->mObj, mJavaFrame->mRequestFocus);
    checkException(env);
}

void
WebCoreFrameBridge::closeWindow(WebCoreViewBridge* viewBridge)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    assert(viewBridge);
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    // TODO: This is a hack until WebCoreViewBridge is no longer an interface.
    env->CallVoidMethod(mJavaFrame->mObj, mJavaFrame->mCloseWindow,
            ((WebViewCore*)viewBridge)->getJavaObject());
}

struct PolicyFunctionWrapper {
    WebCore::FramePolicyFunction func;
};

void
WebCoreFrameBridge::decidePolicyForFormResubmission(WebCore::FramePolicyFunction func)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter;
#endif
    JNIEnv* env = javavm_to_jnienv(mJavaFrame->mJVM);
    PolicyFunctionWrapper* p = new PolicyFunctionWrapper;
    p->func = func;
    env->CallVoidMethod(mJavaFrame->mObj, mJavaFrame->mDecidePolicyForFormResubmission, p);
}

// ----------------------------------------------------------------------------
static void CallPolicyFunction(JNIEnv* env, jobject obj, jint func, jint decision)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeCallPolicyFunction must take a valid frame pointer!");
    PolicyFunctionWrapper* pFunc = (PolicyFunctionWrapper*)func;
    LOG_ASSERT(pFunc, "nativeCallPolicyFunction must take a valid function pointer!");

    (pFrame->loader()->*(pFunc->func))((WebCore::PolicyAction)decision);
}

static void CreateFrame(JNIEnv* env, jobject obj, jobject jAssetManager, jobject historyList)
{
    // Register this thread as the main thread.
    KJS::Collector::registerAsMainThread();
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::ChromeClientAndroid*      chromeC = new WebCore::ChromeClientAndroid;
    WebCore::EditorClientAndroid*      editorC = new WebCore::EditorClientAndroid;
    WebCore::ContextMenuClient* contextMenuC = new WebCore::ContextMenuClientAndroid;
    WebCore::DragClient*        dragC = new WebCore::DragClientAndroid;
    WebCore::FrameLoaderClientAndroid* loaderC = new WebCore::FrameLoaderClientAndroid;
    WebCore::InspectorClientAndroid* inspectorC = new WebCore::InspectorClientAndroid;
    // Create a new page
    WebCore::Page* page = new WebCore::Page(chromeC, contextMenuC, editorC, dragC, inspectorC);
    /* TODO: Don't turn on PageCache until we can restore the ScrollView State.
     * This caused bug http://b/issue?id=1202983
    page->settings()->setUsesPageCache(true);
    // 10 is a random number chosen because it is small enough to give the user
    // a good back/forward page cache without allowing the page cache to get too
    // big.
    WebCore::pageCache()->setCapacity(10);
    */
    editorC->setPage(page);
    page->setGroupName("com.android.browser");
    // Frames are automatically refed to 1, keep this ref because BrowserFrame will
    // maintain a native frame pointer.
    WebCore::FrameAndroid* frame = new WebCore::FrameAndroid(page, NULL, loaderC);
    chromeC->setFrame(frame);
    loaderC->setFrame(frame);

    // Create one instance of WebCoreFrameBridge for calling into Java from WebCore
    WebCoreFrameBridge* frameBridge = new WebCoreFrameBridge(env, obj, historyList);

    // Pass the bridge to the frame and release our ownership.
    frame->setBridge(frameBridge);
    Release(frameBridge);

    LOGV("::WebCore:: createFrame %p", frame);

    // Set the mNativeFrame field in Frame
    SET_NATIVE_FRAME(env, obj, (int)frame);

    // Setup the asset manager.
    AssetManager* am = assetManagerForJavaObject(env, jAssetManager);
    // Initialize our skinning classes
    WebCore::RenderSkinAndroid::Init(am);
}

static void DestroyFrame(JNIEnv* env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeDestroyFrame must take a valid frame pointer!");

    LOGV("::WebCore:: deleting frame %p", pFrame);

    WebCore::FrameView* view = pFrame->view();
    // detachFromParent will cause the page to be closed.
    WebCore::FrameLoader* fl = pFrame->loader();
    // retain a pointer because detachFromParent will set the page to null.
    WebCore::Page* page = pFrame->page();
    if (fl)
        fl->detachFromParent();
    delete page;
    view->deref();

    SET_NATIVE_FRAME(env, obj, 0);
}

static void CreateView(JNIEnv *env, jobject obj, jobject widget)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeCreateView must take a valid frame pointer!");

    // Create a new FrameView and attach an initial WebViewCore. FrameView begins with an initial ref
    // of 1 and that ref is in WebViewCore.
    WebCore::FrameView* view = new WebCore::FrameView(pFrame);
    // The viewBridge will make a java call to maintain a pointer to the view.
    WebViewCore* viewBridge = new WebViewCore(env, widget, view);
    view->setWebCoreViewBridge(viewBridge);
    view->deref();
    Release(viewBridge);

    // Attach the view to the frame.
    pFrame->setView(view);

    // Set the frame to active to turn on keyboard focus.
    pFrame->init();
    pFrame->selectionController()->setFocused(true);

    LOGV("::WebCore:: created view %p with bridge %p", view, viewBridge);
}

static void DetachView(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeDetachView must take a valid frame pointer!");
    LOGV("::WebCore:: detaching view from frame %p", pFrame);

    WebCore::FrameView* view = pFrame->view();
    LOG_ASSERT(view, "cannot detach a null view!");

    // Remove keyboard focus
    pFrame->selectionController()->setFocused(false);

    // Remove the FrameView from the frame.
    pFrame->setView(NULL);

}

static jboolean LoadUrl(JNIEnv *env, jobject obj, jstring url)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeLoadUrl must take a valid frame pointer!");

    const char* urlStr = env->GetStringUTFChars(url, NULL);
    WebCore::String webcoreUrl(urlStr);
    WebCore::ResourceRequest request(webcoreUrl);
    WebCore::DeprecatedString protocol = request.url().protocol();
    // If the url is http(s) and doesn't have a host, it is a bad url.
    if ((WebCore::equalIgnoringCase(protocol, "http") ||
            WebCore::equalIgnoringCase(protocol, "https")) &&
            request.url().host().isEmpty()) {
        env->ReleaseStringUTFChars(url, urlStr);
        return false;
    }

    pFrame->loader()->load(request);
    env->ReleaseStringUTFChars(url, urlStr);
    return true;
}


static void LoadData(JNIEnv *env, jobject obj, jstring baseUrl, jstring data,
        jstring mimeType, jstring encoding, jstring failUrl)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeLoadData must take a valid frame pointer!");

    // Setup the resource request
    const char* baseUrlStr = env->GetStringUTFChars(baseUrl, NULL);
    WebCore::String baseUrlString(baseUrlStr);
    WebCore::ResourceRequest request(baseUrlString);

    // Setup the substituteData
    const char* dataStr = env->GetStringUTFChars(data, NULL);
    WTF::RefPtr<WebCore::SharedBuffer> sharedBuffer =
        new WebCore::SharedBuffer();
    LOG_ASSERT(dataStr, "nativeLoadData has a null data string.");
    sharedBuffer->append(dataStr, strlen(dataStr));
    const char* mimeTypeStr = env->GetStringUTFChars(mimeType, NULL);
    WebCore::String mimeTypeString(mimeTypeStr);
    const char* encodingStr = env->GetStringUTFChars(encoding, NULL);
    WebCore::String encodingString(encodingStr);
    const char* failUrlStr = env->GetStringUTFChars(failUrl, NULL);
    WebCore::KURL failURL(failUrlStr);
    WebCore::SubstituteData substituteData(sharedBuffer, mimeTypeString,
            encodingString, failURL);

    // Perform the load
    pFrame->loader()->load(request, substituteData);

    // Release the Java strings
    env->ReleaseStringUTFChars(baseUrl, baseUrlStr);
    env->ReleaseStringUTFChars(data, dataStr);
    env->ReleaseStringUTFChars(mimeType, mimeTypeStr);
    env->ReleaseStringUTFChars(encoding, encodingStr);
    env->ReleaseStringUTFChars(failUrl, failUrlStr);
}

static void StopLoading(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeStopLoading must take a valid frame pointer!");
    LOGV("::WebCore:: stopLoading %p", pFrame);

    // Stop loading the page and do not send an unload event
    pFrame->loader()->stopForUserCancel();
}

static jstring ExternalRepresentation(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "android_webcore_nativeExternalRepresentation must take a valid frame pointer!");

    // Request external representation of the render tree
    WebCore::DeprecatedString renderDump = externalRepresentation(pFrame->renderer());
    unsigned len = renderDump.length();
    if (!len)
        return NULL;
    return env->NewString((unsigned short*)renderDump.unicode(), len);
}

static jstring DocumentAsText(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "android_webcore_nativeDocumentAsText must take a valid frame pointer!");

    WebCore::Element *documentElement = pFrame->document()->documentElement();
    WebCore::String renderDump = ((WebCore::HTMLElement*)documentElement)->innerText();
    renderDump.append("\n");
    unsigned len = renderDump.length();
    if (!len)
        return NULL;
    return env->NewString((unsigned short*)renderDump.characters(), len);
}

static void Reload(JNIEnv *env, jobject obj, jboolean allowStale)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeReload must take a valid frame pointer!");

    WebCore::FrameLoader* loader = pFrame->loader();
    if (allowStale)
        loader->reloadAllowingStaleData(loader->documentLoader()->overrideEncoding());
    else
        loader->reload();
}

static void GoBackOrForward(JNIEnv *env, jobject obj, jint pos)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeGoBackOrForward must take a valid frame pointer!");

    if (pos == 1)
        pFrame->page()->goForward();
    else if (pos == -1)
        pFrame->page()->goBack();
    else
        pFrame->loader()->goBackOrForward(pos);
}

static jobject StringByEvaluatingJavaScriptFromString(JNIEnv *env, jobject obj, jstring script)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "stringByEvaluatingJavaScriptFromString must take a valid frame pointer!");

    const char* scriptStr = env->GetStringUTFChars(script, NULL);
    WebCore::String result = pFrame->stringByEvaluatingJavaScriptFromString(scriptStr);
    env->ReleaseStringUTFChars(script, scriptStr);

    unsigned len = result.length();
    if (len == 0)
        return NULL;
    return env->NewString((unsigned short*)result.characters(), len);
}

static void AddJavascriptInterface(JNIEnv *env, jobject obj, jint nativeFramePointer,
        jobject javascriptObj, jstring interfaceName)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = (WebCore::FrameAndroid*)nativeFramePointer;
    LOG_ASSERT(pFrame, "nativeAddJavascriptInterface must take a valid frame pointer!");

    const char* interfaceNameStr = env->GetStringUTFChars(interfaceName, NULL);
    JavaVM* vm;
    env->GetJavaVM(&vm);
    LOGV("::WebCore:: addJSInterface: %p", pFrame);
    pFrame->addJavascriptInterface((void*)(vm), javascriptObj, interfaceNameStr);

    env->ReleaseStringUTFChars(interfaceName, interfaceNameStr);
}

static void SetCacheDisabled(JNIEnv *env, jobject obj, jboolean disabled)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::cache()->setDisabled(disabled);
}

static jboolean CacheDisabled(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    return WebCore::cache()->disabled();
}

static void ClearCache(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    if (!WebCore::cache()->disabled()) {
        // Disabling the cache will remove all resources from the cache.  They may
        // still live on if they are referenced by some Web page though.
        WebCore::cache()->setDisabled(true);
        WebCore::cache()->setDisabled(false);
    }
    // force JavaScript to GC when clear cache
    WebCore::gcController().garbageCollectSoon();
    // clear image cache
    SkImageRef_GlobalPool::SetRAMUsed(0);
}

static jboolean DocumentHasImages(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "DocumentHasImages must take a valid frame pointer!");

    return pFrame->document()->images()->length() > 0;
}

static jboolean HasPasswordField(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "HasPasswordField must take a valid frame pointer!");

    bool found = false;
    WTF::PassRefPtr<WebCore::HTMLCollection> form = pFrame->document()->forms();
    WebCore::Node* node = form->firstItem();
    while (node && !found) {
        WTF::Vector<WebCore::HTMLGenericFormElement*> elements =
        		((WebCore::HTMLFormElement*)node)->formElements;
        size_t size = elements.size();
        for (size_t i = 0; i< size && !found; i++) {
            WebCore::HTMLGenericFormElement* e = elements[i];
            if (e->hasLocalName(WebCore::HTMLNames::inputTag)) {
                if (((WebCore::HTMLInputElement*)e)->inputType() ==
                		WebCore::HTMLInputElement::PASSWORD)
                    found = true;
            }
        }
        node = form->nextItem();
    }
    return found;
}

static jobjectArray GetUsernamePassword(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "GetUsernamePassword must take a valid frame pointer!");
    jobjectArray strArray = NULL;

    WebCore::String username, password;
    bool found = false;
    WTF::PassRefPtr<WebCore::HTMLCollection> form = pFrame->document()->forms();
    WebCore::Node* node = form->firstItem();
    while (node && !found) {
        WTF::Vector<WebCore::HTMLGenericFormElement*> elements =
        		((WebCore::HTMLFormElement*)node)->formElements;
        size_t size = elements.size();
        for (size_t i = 0; i< size && !found; i++) {
            WebCore::HTMLGenericFormElement* e = elements[i];
            if (e->hasLocalName(WebCore::HTMLNames::inputTag)) {
                WebCore::HTMLInputElement* input = (WebCore::HTMLInputElement*)e;
                if (input->autoComplete() == false)
                    continue;
                if (input->inputType() == WebCore::HTMLInputElement::PASSWORD)
                    password = input->value();
                else if (input->inputType() == WebCore::HTMLInputElement::TEXT)
                    username = input->value();
                if (!username.isNull() && !password.isNull())
                    found = true;
            }
        }
        node = form->nextItem();
    }
    if (found) {
        jclass stringClass = env->FindClass("java/lang/String");
        strArray = env->NewObjectArray(2, stringClass, NULL);
        env->SetObjectArrayElement(strArray, 0, env->NewString((unsigned short *)
                username.characters(), username.length()));
        env->SetObjectArrayElement(strArray, 1, env->NewString((unsigned short *)
                password.characters(), password.length()));
    }
    return strArray;
}

static void SetUsernamePassword(JNIEnv *env, jobject obj,
    jstring username, jstring password)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "SetUsernamePassword must take a valid frame pointer!");

    WebCore::HTMLInputElement* usernameEle = NULL;
    WebCore::HTMLInputElement* passwordEle = NULL;
    bool found = false;
    WTF::PassRefPtr<WebCore::HTMLCollection> form = pFrame->document()->forms();
    WebCore::Node* node = form->firstItem();
    while (node && !found) {
        WTF::Vector<WebCore::HTMLGenericFormElement*> elements =
        		((WebCore::HTMLFormElement*)node)->formElements;
        size_t size = elements.size();
        for (size_t i = 0; i< size && !found; i++) {
            WebCore::HTMLGenericFormElement* e = elements[i];
            if (e->hasLocalName(WebCore::HTMLNames::inputTag)) {
                WebCore::HTMLInputElement* input = (WebCore::HTMLInputElement*)e;
                if (input->autoComplete() == false)
                    continue;
                if (input->inputType() == WebCore::HTMLInputElement::PASSWORD)
                    passwordEle = input;
                else if (input->inputType() == WebCore::HTMLInputElement::TEXT)
                    usernameEle = input;
                if (usernameEle != NULL && passwordEle != NULL)
                    found = true;
            }
        }
        node = form->nextItem();
    }
    if (found) {
        const char* usernameStr = env->GetStringUTFChars(username, NULL);
        const char* passwordStr = env->GetStringUTFChars(password, NULL);
        usernameEle->setValue(usernameStr);
        passwordEle->setValue(passwordStr);
        env->ReleaseStringUTFChars(username, usernameStr);
        env->ReleaseStringUTFChars(password, passwordStr);
    }
}

static jobject GetFormTextData(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterFB counter(true);
#endif
    WebCore::FrameAndroid* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "GetFormTextData must take a valid frame pointer!");
    jobject hashMap = NULL;

    WTF::PassRefPtr<WebCore::HTMLCollection> collection = pFrame->document()->forms();
    if (collection->length() > 0) {
        jclass mapClass = env->FindClass("java/util/HashMap");
        LOG_ASSERT(mapClass, "Could not find HashMap class!");
        jmethodID init = env->GetMethodID(mapClass, "<init>", "(I)V");
        LOG_ASSERT(init, "Could not find constructor for HashMap");
        hashMap = env->NewObject(mapClass, init, 1);
        LOG_ASSERT(hashMap, "Could not create a new HashMap");
        jmethodID put = env->GetMethodID(mapClass, "put",
                "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
        LOG_ASSERT(put, "Could not find put method on HashMap");

        static const WebCore::AtomicString text("text");
        static const WebCore::AtomicString off("off");

        WebCore::HTMLFormElement* form;
        WebCore::HTMLInputElement* input;
        for (WebCore::Node* node = collection->firstItem(); node; node = collection->nextItem()) {
            form = static_cast<WebCore::HTMLFormElement*>(node);
            if (form->autoComplete()) {
                WTF::Vector<WebCore::HTMLGenericFormElement*> elements = form->formElements;
                size_t size = elements.size();
                for (size_t i = 0; i < size; i++) {
                    WebCore::HTMLGenericFormElement* e = elements[i];
                    if (e->type() == text) {
                        if (e->hasAttribute(WebCore::HTMLNames::autocompleteAttr)) {
                            const WebCore::AtomicString& attr = e->getAttribute(WebCore::HTMLNames::autocompleteAttr);
                            if (attr == off)
                                continue;
                        }
                        input = (WebCore::HTMLInputElement*) e;
                        WebCore::String value = input->value();
                        int len = value.length();
                        if (len) {
                            const WebCore::AtomicString& name = input->name();
                            jstring key = env->NewString((jchar *)name.characters(), name.length());
                            jstring val = env->NewString((jchar *)value.characters(), len);
                            LOG_ASSERT(key && val, "name or value not set");
                            env->CallObjectMethod(hashMap, put, key, val);
                            env->DeleteLocalRef(key);
                            env->DeleteLocalRef(val);
                        }
                    }
                }
            }
        }
        env->DeleteLocalRef(mapClass);

    }
    return hashMap;
}

// ----------------------------------------------------------------------------

/*
 * JNI registration.
 */
static JNINativeMethod gBrowserFrameNativeMethods[] = {
    /* name, signature, funcPtr */
    { "nativeCallPolicyFunction", "(II)V",
        (void*) CallPolicyFunction },
    { "nativeCreateFrame", "(Landroid/content/res/AssetManager;Landroid/webkit/WebBackForwardList;)V",
        (void*) CreateFrame },
    { "nativeCreateView", "(Landroid/webkit/WebViewCore;)V",
        (void*) CreateView },
    { "nativeDestroyFrame", "()V",
        (void*) DestroyFrame },
    { "nativeDetachView", "()V",
        (void*) DetachView },
    { "stopLoading", "()V",
        (void*) StopLoading },
    { "nativeLoadUrl", "(Ljava/lang/String;)Z",
        (void*) LoadUrl },
    { "nativeLoadData", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        (void*) LoadData },
    { "externalRepresentation", "()Ljava/lang/String;",
        (void*) ExternalRepresentation },
    { "documentAsText", "()Ljava/lang/String;",
        (void*) DocumentAsText },
    { "reload", "(Z)V",
        (void*) Reload },
    { "goBackOrForward", "(I)V",
        (void*) GoBackOrForward },
    { "nativeAddJavascriptInterface", "(ILjava/lang/Object;Ljava/lang/String;)V",
        (void*) AddJavascriptInterface },
    { "stringByEvaluatingJavaScriptFromString",
            "(Ljava/lang/String;)Ljava/lang/String;",
        (void*) StringByEvaluatingJavaScriptFromString },
    { "setCacheDisabled", "(Z)V",
        (void*) SetCacheDisabled },
    { "cacheDisabled", "()Z",
        (void*) CacheDisabled },
    { "clearCache", "()V",
        (void*) ClearCache },
    { "documentHasImages", "()Z",
        (void*) DocumentHasImages },
    { "hasPasswordField", "()Z",
        (void*) HasPasswordField },
    { "getUsernamePassword", "()[Ljava/lang/String;",
        (void*) GetUsernamePassword },
    { "setUsernamePassword", "(Ljava/lang/String;Ljava/lang/String;)V",
        (void*) SetUsernamePassword },
    { "getFormTextData", "()Ljava/util/HashMap;",
        (void*) GetFormTextData }
};

int register_webcoreframebridge(JNIEnv* env)
{
    jclass clazz = env->FindClass("android/webkit/BrowserFrame");
    LOG_ASSERT(clazz, "Cannot find BrowserFrame");
    gFrameAndroidField = env->GetFieldID(clazz, "mNativeFrame", "I");
    LOG_ASSERT(gFrameAndroidField, "Cannot find mNativeFrame on BrowserFrame");

    return jniRegisterNativeMethods(env, "android/webkit/BrowserFrame",
            gBrowserFrameNativeMethods, NELEM(gBrowserFrameNativeMethods));
}

} /* namespace android */

