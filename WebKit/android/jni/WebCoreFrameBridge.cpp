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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include "CString.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DragClientAndroid.h"
#include "EditorClientAndroid.h"
#include "Element.h"
#include "Font.h"
#include "Frame.h"
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
#include "JSDOMWindow.h"
#include <runtime/InitializeThreading.h>
#include <runtime/JSLock.h>
#include "KURL.h"
#include "Page.h"
#include "PageCache.h"
#include "PlatformString.h"
#include "RenderPart.h"
#include "RenderSkinAndroid.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "ResourceHandle.h"
#include "ScriptController.h"
#include "ScriptValue.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SubstituteData.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreJni.h"
#include "WebCoreResourceLoader.h"
#include "WebHistory.h"
#include "WebIconDatabase.h"
#include "WebFrameView.h"
#include "WebViewCore.h"
#include "wds/DebugServer.h"

#include <runtime_root.h>
#include <runtime_object.h>
#include <jni_utility.h>
#include "jni.h"
#include "jni_instance.h"

#include <JNIHelp.h>
#include <SkGraphics.h>
#include <SkImageRef_GlobalPool.h>
#include <utils/misc.h>
#include <utils/AssetManager.h>
#include <android_runtime/android_util_AssetManager.h>

#ifdef ANDROID_INSTRUMENT
#include "TimeCounter.h"
#include <runtime/JSLock.h>
#endif

using namespace JSC;
using namespace JSC::Bindings;

namespace android {

// ----------------------------------------------------------------------------

#define WEBCORE_MEMORY_CAP 15 * 1024 * 1024

// ----------------------------------------------------------------------------

struct WebFrame::JavaBrowserFrame
{
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
    jmethodID   mGetRawResFilename;
    AutoJObject frame(JNIEnv* env) {
        return getRealObject(env, mObj);
    }
    AutoJObject history(JNIEnv* env) {
        return getRealObject(env, mHistoryList);
    }
};

static jfieldID gFrameField;
#define GET_NATIVE_FRAME(env, obj) ((WebCore::Frame*)env->GetIntField(obj, gFrameField))
#define SET_NATIVE_FRAME(env, obj, frame) (env->SetIntField(obj, gFrameField, frame))

// ----------------------------------------------------------------------------

WebFrame::WebFrame(JNIEnv* env, jobject obj, jobject historyList, WebCore::Page* page)
    : mPage(page)
{
    jclass clazz = env->GetObjectClass(obj);
    mJavaFrame = new JavaBrowserFrame;
    mJavaFrame->mObj = adoptGlobalRef(env, obj);
    mJavaFrame->mHistoryList = adoptGlobalRef(env, historyList);
    mJavaFrame->mStartLoadingResource = env->GetMethodID(clazz, "startLoadingResource",
            "(ILjava/lang/String;Ljava/lang/String;Ljava/util/HashMap;[BIZZ)Landroid/webkit/LoadListener;");
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
    mJavaFrame->mGetRawResFilename = env->GetMethodID(clazz, "getRawResFilename",
            "(I)Ljava/lang/String;");

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
    LOG_ASSERT(mJavaFrame->mGetRawResFilename, "Could not find method getRawResFilename");

    mUserAgent = WebCore::String();
    mUserInitiatedClick = false;
}

WebFrame::~WebFrame()
{
    if (mJavaFrame->mObj) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        env->DeleteGlobalRef(mJavaFrame->mObj);
        env->DeleteGlobalRef(mJavaFrame->mHistoryList);
        mJavaFrame->mObj = 0;
    }
    delete mJavaFrame;
}

WebFrame* WebFrame::getWebFrame(const WebCore::Frame* frame)
{
    FrameLoaderClientAndroid* client =
            static_cast<FrameLoaderClientAndroid*> (frame->loader()->client());
    return client->webFrame();
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
        if (i->first.length() == 0 || i->second.length() == 0)
            continue;
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
WebFrame::startLoadingResource(WebCore::ResourceHandle* loader,
                                  const WebCore::ResourceRequest& request,
                                  bool isHighPriority, bool synchronous)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    LOGV("::WebCore:: startLoadingResource(%p, %s)",
            loader, request.url().string().ascii().data());

    WebCore::String method = request.httpMethod();
    WebCore::HTTPHeaderMap headers = request.httpHeaderFields();

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    WebCore::String urlStr = request.url().string();
    jstring jUrlStr = env->NewString(urlStr.characters(), urlStr.length());
    jstring jMethodStr = NULL;
    if (!method.isEmpty())
        jMethodStr = env->NewString(method.characters(), method.length());
    jbyteArray jPostDataStr = NULL;
    WebCore::FormData* formdata = request.httpBody();
    if (formdata) {
        // We can use the formdata->flatten() but it will result in two 
        // memcpys, first through loading up the vector with the form data
        // then another to copy it out of the vector and into the java byte
        // array. Instead, we copy the form data ourselves below saving a
        // memcpy.
        const WTF::Vector<WebCore::FormDataElement>& elements = 
                formdata->elements();

        // Sizing pass
        int size = 0;
        size_t n = elements.size();
        for (size_t i = 0; i < n; ++i) {
            const WebCore::FormDataElement& e = elements[i];
            if (e.m_type == WebCore::FormDataElement::data) {
                size += e.m_data.size();
            }
        }

        // Only create the byte array if there is POST data to pass up.
        // The Java code is expecting null if there is no data.
        if (size > 0) {
            // Copy the actual form data.
            jPostDataStr = env->NewByteArray(size);
            if (jPostDataStr) {
                // Write  the form data to the java array.
                jbyte* bytes = env->GetByteArrayElements(jPostDataStr, NULL);
                int offset = 0;
                for (size_t i = 0; i < n; ++i) {
                    const WebCore::FormDataElement& e = elements[i];
                    if (e.m_type == WebCore::FormDataElement::data) {
                        int delta = e.m_data.size();
                        memcpy(bytes + offset, e.m_data.data(), delta);
                        offset += delta;
                    }
                }
                env->ReleaseByteArrayElements(jPostDataStr, bytes, 0);
            }
        }
    }

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

    LOGV("::WebCore:: startLoadingResource %s with cacheMode %d", urlStr.ascii().data(), cacheMode);


    jobject jLoadListener =
        env->CallObjectMethod(mJavaFrame->frame(env).get(), mJavaFrame->mStartLoadingResource,
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
WebFrame::reportError(int errorCode, const WebCore::String& description,
        const WebCore::String& failingUrl)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    LOGV("::WebCore:: reportError(%d, %s)", errorCode, description.ascii().data());
    JNIEnv* env = JSC::Bindings::getJNIEnv();

    jstring descStr = env->NewString((unsigned short*)description.characters(), description.length());
    jstring failUrl = env->NewString((unsigned short*)failingUrl.characters(), failingUrl.length());
    env->CallVoidMethod(mJavaFrame->frame(env).get(), mJavaFrame->mReportError, errorCode, descStr, failUrl);
    env->DeleteLocalRef(descStr);
    env->DeleteLocalRef(failUrl);
}

void
WebFrame::loadStarted(WebCore::Frame* frame)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    const WebCore::KURL& url = frame->loader()->activeDocumentLoader()->url();
    if (url.isEmpty())
        return;
    LOGV("::WebCore:: loadStarted %s", url.string().ascii().data());

    bool isMainFrame = (!frame->tree() || !frame->tree()->parent());
    WebCore::FrameLoadType loadType = frame->loader()->loadType();

    if (loadType == WebCore::FrameLoadTypeReplace ||
            loadType == WebCore::FrameLoadTypeSame ||
            (loadType == WebCore::FrameLoadTypeRedirectWithLockedBackForwardList &&
             !isMainFrame))
        return;

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    WebCore::String urlString(url.string());
    // If this is the main frame and we already have a favicon in the database,
    // send it along with the page started notification.
    jobject favicon = NULL;
    if (isMainFrame) {
        WebCore::Image* icon = WebCore::iconDatabase()->iconForPageURL(urlString, WebCore::IntSize(16, 16));
        if (icon)
            favicon = webcoreImageToJavaBitmap(env, icon);
        LOGV("favicons", "Starting load with icon %p for %s", icon, url.string().utf8().data());
    }
    jstring urlStr = env->NewString((unsigned short*)urlString.characters(), urlString.length());

    env->CallVoidMethod(mJavaFrame->frame(env).get(), mJavaFrame->mLoadStarted, urlStr, favicon,
            (int)loadType, isMainFrame);
    checkException(env);
    env->DeleteLocalRef(urlStr);
    if (favicon)
        env->DeleteLocalRef(favicon);
}

void
WebFrame::transitionToCommitted(WebCore::Frame* frame)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    WebCore::FrameLoadType loadType = frame->loader()->loadType();
    bool isMainFrame = (!frame->tree() || !frame->tree()->parent());
    env->CallVoidMethod(mJavaFrame->frame(env).get(), mJavaFrame->mTransitionToCommitted,
            (int)loadType, isMainFrame);
    checkException(env);
}

void
WebFrame::didFinishLoad(WebCore::Frame* frame)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    WebCore::FrameLoader* loader = frame->loader();
    const WebCore::KURL& url = loader->activeDocumentLoader()->url();
    if (url.isEmpty())
        return;
    LOGV("::WebCore:: didFinishLoad %s", url.string().ascii().data());

    bool isMainFrame = (!frame->tree() || !frame->tree()->parent());
    WebCore::FrameLoadType loadType = loader->loadType();
    WebCore::String urlString(url.string());
    jstring urlStr = env->NewString((unsigned short*)urlString.characters(), urlString.length());
    env->CallVoidMethod(mJavaFrame->frame(env).get(), mJavaFrame->mLoadFinished, urlStr,
            (int)loadType, isMainFrame);
    checkException(env);
    env->DeleteLocalRef(urlStr);
}

void
WebFrame::addHistoryItem(WebCore::HistoryItem* item)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    LOGV("::WebCore:: addHistoryItem");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    WebHistory::AddItem(mJavaFrame->history(env), item);
}

void
WebFrame::removeHistoryItem(int index)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    LOGV("::WebCore:: removeHistoryItem at %d", index);
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    WebHistory::RemoveItem(mJavaFrame->history(env), index);
}

void
WebFrame::updateHistoryIndex(int newIndex)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    LOGV("::WebCore:: updateHistoryIndex to %d", newIndex);
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    WebHistory::UpdateHistoryIndex(mJavaFrame->history(env), newIndex);
}

void
WebFrame::setTitle(const WebCore::String& title)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
#ifndef NDEBUG
    LOGV("setTitle(%s)", title.ascii().data());
#endif
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jstring jTitleStr = env->NewString((unsigned short *)title.characters(), title.length());

    env->CallVoidMethod(mJavaFrame->frame(env).get(), mJavaFrame->mSetTitle,
                                        jTitleStr);
    checkException(env);
    env->DeleteLocalRef(jTitleStr);
}

void
WebFrame::windowObjectCleared(WebCore::Frame* frame)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    LOGV("::WebCore:: windowObjectCleared");
    JNIEnv* env = JSC::Bindings::getJNIEnv();

    env->CallVoidMethod(mJavaFrame->frame(env).get(), mJavaFrame->mWindowObjectCleared, (int)frame);
    checkException(env);
}

void
WebFrame::setProgress(float newProgress)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    int progress = (int) (100 * newProgress);
    env->CallVoidMethod(mJavaFrame->frame(env).get(), mJavaFrame->mSetProgress, progress);
    checkException(env);
}

const WebCore::String
WebFrame::userAgentForURL(const WebCore::KURL* url)
{
    return mUserAgent;
}

void
WebFrame::didReceiveIcon(WebCore::Image* icon)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    LOG_ASSERT(icon, "DidReceiveIcon called without an image!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jobject bitmap = webcoreImageToJavaBitmap(env, icon);
    if (!bitmap)
        return;

    env->CallVoidMethod(mJavaFrame->frame(env).get(), mJavaFrame->mDidReceiveIcon, bitmap);
    env->DeleteLocalRef(bitmap);
    checkException(env);
}

void
WebFrame::updateVisitedHistory(const WebCore::KURL& url, bool reload)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    WebCore::String urlStr(url.string());
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jstring jUrlStr = env->NewString((unsigned short*)urlStr.characters(), urlStr.length());

    env->CallVoidMethod(mJavaFrame->frame(env).get(), mJavaFrame->mUpdateVisitedHistory, jUrlStr, reload);
    checkException(env);
}

bool
WebFrame::canHandleRequest(const WebCore::ResourceRequest& request)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    // Internal loads are ok but any request that is due to a user hitting a key
    // should be checked.
    bool userGesture = false;
#ifdef ANDROID_USER_GESTURE
    userGesture = request.userGesture();
#endif
    // always handle "POST" in place
    if (equalIgnoringCase(request.httpMethod(), "POST"))
        return true;
    WebCore::KURL requestUrl = request.url();
    if (!mUserInitiatedClick && !userGesture &&
        (requestUrl.protocolIs("http") || requestUrl.protocolIs("https") ||
            requestUrl.protocolIs("file") || requestUrl.protocolIs("about") ||
            requestUrl.protocolIs("javascript")))
        return true;
    WebCore::String url(request.url().string());
    // Empty urls should not be sent to java
    if (url.isEmpty())
        return true;
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());

    // check to see whether browser app wants to hijack url loading.
    // if browser app handles the url, we will return false to bail out WebCore loading
    jboolean ret = env->CallBooleanMethod(mJavaFrame->frame(env).get(), mJavaFrame->mHandleUrl, jUrlStr);
    checkException(env);
    return (ret == 0);
}

WebCore::Frame*
WebFrame::createWindow(bool dialog, bool userGesture)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jobject obj = env->CallObjectMethod(mJavaFrame->frame(env).get(),
            mJavaFrame->mCreateWindow, dialog, userGesture);
    if (obj) {
        WebCore::Frame* frame = GET_NATIVE_FRAME(env, obj);
        return frame;
    }
    return NULL;
}

void
WebFrame::requestFocus() const
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(mJavaFrame->frame(env).get(), mJavaFrame->mRequestFocus);
    checkException(env);
}

void
WebFrame::closeWindow(WebViewCore* webViewCore)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    assert(webViewCore);
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(mJavaFrame->frame(env).get(), mJavaFrame->mCloseWindow,
            webViewCore->getJavaObject().get());
}

struct PolicyFunctionWrapper {
    WebCore::FramePolicyFunction func;
};

void
WebFrame::decidePolicyForFormResubmission(WebCore::FramePolicyFunction func)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::JavaCallbackTimeCounter);
#endif
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    PolicyFunctionWrapper* p = new PolicyFunctionWrapper;
    p->func = func;
    env->CallVoidMethod(mJavaFrame->frame(env).get(), mJavaFrame->mDecidePolicyForFormResubmission, p);
}

WebCore::String
WebFrame::getRawResourceFilename(RAW_RES_ID id) const
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jstring ret = (jstring) env->CallObjectMethod(mJavaFrame->frame(env).get(),
            mJavaFrame->mGetRawResFilename, (int)id);

    return to_string(env, ret);
}

// ----------------------------------------------------------------------------
static void CallPolicyFunction(JNIEnv* env, jobject obj, jint func, jint decision)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeCallPolicyFunction must take a valid frame pointer!");
    PolicyFunctionWrapper* pFunc = (PolicyFunctionWrapper*)func;
    LOG_ASSERT(pFunc, "nativeCallPolicyFunction must take a valid function pointer!");

    (pFrame->loader()->*(pFunc->func))((WebCore::PolicyAction)decision);
}

static void CreateFrame(JNIEnv* env, jobject obj, jobject javaview, jobject jAssetManager, jobject historyList)
{
    JSC::initializeThreading();
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    ChromeClientAndroid*      chromeC = new ChromeClientAndroid;
    EditorClientAndroid*      editorC = new EditorClientAndroid;
    WebCore::ContextMenuClient* contextMenuC = new ContextMenuClientAndroid;
    WebCore::DragClient*        dragC = new DragClientAndroid;
    InspectorClientAndroid* inspectorC = new InspectorClientAndroid;
    // Create a new page
    WebCore::Page* page = new WebCore::Page(chromeC, contextMenuC, editorC, dragC, inspectorC);
    // css files without explicit MIMETYPE is treated as generic text files in
    // the Java side. So we can't enforce CSS MIMETYPE.
    page->settings()->setEnforceCSSMIMETypeInStrictMode(false);
    /* TODO: Don't turn on PageCache until we can restore the ScrollView State.
     * This caused bug http://b/issue?id=1202983
    page->settings()->setUsesPageCache(true);
    // 10 is a random number chosen because it is small enough to give the user
    // a good back/forward page cache without allowing the page cache to get too
    // big.
    WebCore::pageCache()->setCapacity(10);
    */
    editorC->setPage(page);
    page->setGroupName("android.webkit");

    // Create a WebFrame to access the Java BrowserFrame associated with this page
    WebFrame* webFrame = new WebFrame(env, obj, historyList, page);
    // Attach webFrame to chromeC and release our ownership
    chromeC->setWebFrame(webFrame);
    Release(webFrame);

    FrameLoaderClientAndroid* loaderC = new FrameLoaderClientAndroid(webFrame);
    // Create a Frame and the page holds its reference
    WebCore::Frame* frame = WebCore::Frame::create(page, NULL, loaderC).get();
    loaderC->setFrame(frame);
#if ENABLE(WDS)
    WDS::server()->addFrame(frame);
#endif

    // Create a WebViewCore to access the Java WebViewCore associated with this page
    WebViewCore* webViewCore = new WebViewCore(env, javaview, frame);

    // Create a FrameView
    WebCore::FrameView* frameView = new WebCore::FrameView(frame);
    // Create a WebFrameView
    WebFrameView* webFrameView = new WebFrameView(frameView, webViewCore);
    // As webFrameView Retains webViewCore, release our ownership
    Release(webViewCore);
    // As frameView Retains webFrameView, release our ownership
    Release(webFrameView);
    // Attach the frameView to the frame and release our ownership
    frame->setView(frameView);
    frameView->deref();

    // Set the frame to active to turn on keyboard focus.
    frame->init();
    frame->selection()->setFocused(true);

    // Allow local access to file:/// and substitute data
    WebCore::FrameLoader::setLocalLoadPolicy(
            WebCore::FrameLoader::AllowLocalLoadsForLocalAndSubstituteData);

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
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeDestroyFrame must take a valid frame pointer!");

    LOGV("::WebCore:: deleting frame %p", pFrame);

    WebCore::FrameView* view = pFrame->view();
    view->ref();
    // detachFromParent will cause the page to be closed.
    WebCore::FrameLoader* fl = pFrame->loader();
    // retain a pointer because detachFromParent will set the page to null.
    WebCore::Page* page = pFrame->page();
    if (fl)
        fl->detachFromParent();
    delete page;
    view->deref();

    SET_NATIVE_FRAME(env, obj, 0);
#if ENABLE(WDS)
    WDS::server()->removeFrame(pFrame);
#endif
}

static void LoadUrl(JNIEnv *env, jobject obj, jstring url)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeLoadUrl must take a valid frame pointer!");

    WebCore::String webcoreUrl = to_string(env, url);
    WebCore::ResourceRequest request(webcoreUrl);
    LOGV("LoadUrl %s", webcoreUrl.latin1().data());
    pFrame->loader()->load(request, false);
}


static void LoadData(JNIEnv *env, jobject obj, jstring baseUrl, jstring data,
        jstring mimeType, jstring encoding, jstring failUrl)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeLoadData must take a valid frame pointer!");

    // Setup the resource request
    WebCore::ResourceRequest request(to_string(env, baseUrl));

    // Setup the substituteData
    const char* dataStr = env->GetStringUTFChars(data, NULL);
    WTF::PassRefPtr<WebCore::SharedBuffer> sharedBuffer =
        WebCore::SharedBuffer::create();
    LOG_ASSERT(dataStr, "nativeLoadData has a null data string.");
    sharedBuffer->append(dataStr, strlen(dataStr));
    env->ReleaseStringUTFChars(data, dataStr);

    WebCore::SubstituteData substituteData(sharedBuffer,
            to_string(env, mimeType), to_string(env, encoding),
            WebCore::KURL(to_string(env, failUrl)));

    // Perform the load
    pFrame->loader()->load(request, substituteData, false);
}

static void StopLoading(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeStopLoading must take a valid frame pointer!");
    LOGV("::WebCore:: stopLoading %p", pFrame);

    // Stop loading the page and do not send an unload event
    pFrame->loader()->stopForUserCancel();
}

static jstring ExternalRepresentation(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "android_webcore_nativeExternalRepresentation must take a valid frame pointer!");

    // Request external representation of the render tree
    WebCore::String renderDump = WebCore::externalRepresentation(pFrame->contentRenderer());
    unsigned len = renderDump.length();
    if (!len)
        return NULL;
    return env->NewString(renderDump.characters(), len);
}

static jstring DocumentAsText(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "android_webcore_nativeDocumentAsText must take a valid frame pointer!");

    WebCore::Element *documentElement = pFrame->document()->documentElement();
    if (!documentElement)
        return NULL;
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
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "nativeReload must take a valid frame pointer!");

    WebCore::FrameLoader* loader = pFrame->loader();
    if (allowStale) {
        // load the current page with FrameLoadTypeIndexedBackForward so that it
        // will use cache when it is possible
        WebCore::Page* page = pFrame->page();
        WebCore::HistoryItem* item = page->backForwardList()->currentItem();
        if (item)
            page->goToItem(item, FrameLoadTypeIndexedBackForward);
    } else
        loader->reload(true);
}

static void GoBackOrForward(JNIEnv *env, jobject obj, jint pos)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
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
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "stringByEvaluatingJavaScriptFromString must take a valid frame pointer!");

    WebCore::ScriptValue value =
            pFrame->loader()->executeScript(to_string(env, script), true);
    WebCore::String result = WebCore::String();
    if (!value.getString(result))
        return NULL;
    unsigned len = result.length();
    if (len == 0)
        return NULL;
    return env->NewString((unsigned short*)result.characters(), len);
}

// Wrap the JavaInstance used when binding custom javascript interfaces. Use a
// weak reference so that the gc can collect the WebView. Override virtualBegin
// and virtualEnd and swap the weak reference for the real object.
class WeakJavaInstance : public JavaInstance {
public:
    static PassRefPtr<WeakJavaInstance> create(jobject obj,
            PassRefPtr<RootObject> root) {
        return adoptRef(new WeakJavaInstance(obj, root));
    }

protected:
    WeakJavaInstance(jobject instance, PassRefPtr<RootObject> rootObject)
        : JavaInstance(instance, rootObject)
    {
        JNIEnv* env = getJNIEnv();
        // JavaInstance creates a global ref to instance in its constructor.
        env->DeleteGlobalRef(_instance->_instance);
        // Set the object to our WeakReference wrapper.
        _instance->_instance = adoptGlobalRef(env, instance);
    }

    virtual void virtualBegin() {
        _weakRef = _instance->_instance;
        JNIEnv* env = getJNIEnv();
        // This is odd. getRealObject returns an AutoJObject which is used to
        // cleanly create and delete a local reference. But, here we need to
        // maintain the local reference across calls to virtualBegin() and
        // virtualEnd(). So, release the local reference from the AutoJObject
        // and delete the local reference in virtualEnd().
        _realObject = getRealObject(env, _weakRef).release();
        // Point to the real object
        _instance->_instance = _realObject;
        // Call the base class method
        INHERITED::virtualBegin();
    }

    virtual void virtualEnd() {
        // Call the base class method first to pop the local frame.
        INHERITED::virtualEnd();
        // Get rid of the local reference to the real object.
        getJNIEnv()->DeleteLocalRef(_realObject);
        // Point back to the WeakReference.
        _instance->_instance = _weakRef;
    }

private:
    typedef JavaInstance INHERITED;
    jobject _realObject;
    jobject _weakRef;
};

static void AddJavascriptInterface(JNIEnv *env, jobject obj, jint nativeFramePointer,
        jobject javascriptObj, jstring interfaceName)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = (WebCore::Frame*)nativeFramePointer;
    LOG_ASSERT(pFrame, "nativeAddJavascriptInterface must take a valid frame pointer!");

    JavaVM* vm;
    env->GetJavaVM(&vm);
    LOGV("::WebCore:: addJSInterface: %p", pFrame);

    // Copied from qwebframe.cpp
    JSC::JSLock lock(false);
    WebCore::JSDOMWindow *window = WebCore::toJSDOMWindow(pFrame);
    JSC::Bindings::RootObject *root = pFrame->script()->bindingRootObject();
    if (window) {
        JSC::Bindings::setJavaVM(vm);
        // Add the binding to JS environment
        JSC::ExecState* exec = window->globalExec();
        JSC::JSObject *addedObject = WeakJavaInstance::create(javascriptObj,
                root)->createRuntimeObject(exec);
        const jchar* s = env->GetStringChars(interfaceName, NULL);
        if (s) {
            // Add the binding name to the window's table of child objects.
            JSC::PutPropertySlot slot;
            window->put(exec, JSC::Identifier(exec, (const UChar *)s, 
                    env->GetStringLength(interfaceName)), addedObject, slot);
            env->ReleaseStringChars(interfaceName, s);
            checkException(env);
        }
    }
}

static void SetCacheDisabled(JNIEnv *env, jobject obj, jboolean disabled)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::cache()->setDisabled(disabled);
}

static jboolean CacheDisabled(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    return WebCore::cache()->disabled();
}

static void ClearCache(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);

    JSC::JSLock lock(false);
    JSC::Heap::Statistics jsHeapStatistics = WebCore::JSDOMWindow::commonJSGlobalData()->heap.statistics();
    LOGD("About to gc and JavaScript heap size is %d and has %d bytes free",
            jsHeapStatistics.size, jsHeapStatistics.free);
    LOGD("About to clear cache and current cache has %d bytes live and %d bytes dead", 
            cache()->getLiveSize(), cache()->getDeadSize());
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
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "DocumentHasImages must take a valid frame pointer!");

    return pFrame->document()->images()->length() > 0;
}

static jboolean HasPasswordField(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "HasPasswordField must take a valid frame pointer!");

    bool found = false;
    WTF::PassRefPtr<WebCore::HTMLCollection> form = pFrame->document()->forms();
    WebCore::Node* node = form->firstItem();
    while (node && !found) {
        WTF::Vector<WebCore::HTMLFormControlElement*> elements =
        		((WebCore::HTMLFormElement*)node)->formElements;
        size_t size = elements.size();
        for (size_t i = 0; i< size && !found; i++) {
            WebCore::HTMLFormControlElement* e = elements[i];
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
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "GetUsernamePassword must take a valid frame pointer!");
    jobjectArray strArray = NULL;

    WebCore::String username, password;
    bool found = false;
    WTF::PassRefPtr<WebCore::HTMLCollection> form = pFrame->document()->forms();
    WebCore::Node* node = form->firstItem();
    while (node && !found) {
        WTF::Vector<WebCore::HTMLFormControlElement*> elements =
        		((WebCore::HTMLFormElement*)node)->formElements;
        size_t size = elements.size();
        for (size_t i = 0; i< size && !found; i++) {
            WebCore::HTMLFormControlElement* e = elements[i];
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
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
    LOG_ASSERT(pFrame, "SetUsernamePassword must take a valid frame pointer!");

    WebCore::HTMLInputElement* usernameEle = NULL;
    WebCore::HTMLInputElement* passwordEle = NULL;
    bool found = false;
    WTF::PassRefPtr<WebCore::HTMLCollection> form = pFrame->document()->forms();
    WebCore::Node* node = form->firstItem();
    while (node && !found) {
        WTF::Vector<WebCore::HTMLFormControlElement*> elements =
        		((WebCore::HTMLFormElement*)node)->formElements;
        size_t size = elements.size();
        for (size_t i = 0; i< size && !found; i++) {
            WebCore::HTMLFormControlElement* e = elements[i];
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
        usernameEle->setValue(to_string(env, username));
        passwordEle->setValue(to_string(env, password));
    }
}

static jobject GetFormTextData(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::NativeCallbackTimeCounter);
#endif
    WebCore::Frame* pFrame = GET_NATIVE_FRAME(env, obj);
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
                WTF::Vector<WebCore::HTMLFormControlElement*> elements = form->formElements;
                size_t size = elements.size();
                for (size_t i = 0; i < size; i++) {
                    WebCore::HTMLFormControlElement* e = elements[i];
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
    { "nativeCreateFrame", "(Landroid/webkit/WebViewCore;Landroid/content/res/AssetManager;Landroid/webkit/WebBackForwardList;)V",
        (void*) CreateFrame },
    { "nativeDestroyFrame", "()V",
        (void*) DestroyFrame },
    { "nativeStopLoading", "()V",
        (void*) StopLoading },
    { "nativeLoadUrl", "(Ljava/lang/String;)V",
        (void*) LoadUrl },
    { "nativeLoadData", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        (void*) LoadData },
    { "externalRepresentation", "()Ljava/lang/String;",
        (void*) ExternalRepresentation },
    { "documentAsText", "()Ljava/lang/String;",
        (void*) DocumentAsText },
    { "reload", "(Z)V",
        (void*) Reload },
    { "nativeGoBackOrForward", "(I)V",
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

int register_webframe(JNIEnv* env)
{
    jclass clazz = env->FindClass("android/webkit/BrowserFrame");
    LOG_ASSERT(clazz, "Cannot find BrowserFrame");
    gFrameField = env->GetFieldID(clazz, "mNativeFrame", "I");
    LOG_ASSERT(gFrameField, "Cannot find mNativeFrame on BrowserFrame");

    return jniRegisterNativeMethods(env, "android/webkit/BrowserFrame",
            gBrowserFrameNativeMethods, NELEM(gBrowserFrameNativeMethods));
}

} /* namespace android */

