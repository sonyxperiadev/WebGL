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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "websettings"

#include <config.h>
#include <wtf/Platform.h>

#include "ApplicationCacheStorage.h"
#include "DatabaseTracker.h"
#include "DocLoader.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "Geolocation.h"
#include "GeolocationPermissions.h"
#include "Page.h"
#include "RenderTable.h"
#include "Settings.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreJni.h"
#if USE(V8)
#include "WorkerContextExecutionProxy.h"
#endif

#include <JNIHelp.h>
#include <utils/misc.h>

namespace android {

struct FieldIds {
    FieldIds(JNIEnv* env, jclass clazz) {
        mLayoutAlgorithm = env->GetFieldID(clazz, "mLayoutAlgorithm",
                "Landroid/webkit/WebSettings$LayoutAlgorithm;");
        mTextSize = env->GetFieldID(clazz, "mTextSize",
                "Landroid/webkit/WebSettings$TextSize;");
        mStandardFontFamily = env->GetFieldID(clazz, "mStandardFontFamily",
                "Ljava/lang/String;");
        mFixedFontFamily = env->GetFieldID(clazz, "mFixedFontFamily",
                "Ljava/lang/String;");
        mSansSerifFontFamily = env->GetFieldID(clazz, "mSansSerifFontFamily",
                "Ljava/lang/String;");
        mSerifFontFamily = env->GetFieldID(clazz, "mSerifFontFamily",
                "Ljava/lang/String;");
        mCursiveFontFamily = env->GetFieldID(clazz, "mCursiveFontFamily",
                "Ljava/lang/String;");
        mFantasyFontFamily = env->GetFieldID(clazz, "mFantasyFontFamily",
                "Ljava/lang/String;");
        mDefaultTextEncoding = env->GetFieldID(clazz, "mDefaultTextEncoding",
                "Ljava/lang/String;");
        mUserAgent = env->GetFieldID(clazz, "mUserAgent",
                "Ljava/lang/String;");
        mMinimumFontSize = env->GetFieldID(clazz, "mMinimumFontSize", "I");
        mMinimumLogicalFontSize = env->GetFieldID(clazz, "mMinimumLogicalFontSize", "I");
        mDefaultFontSize = env->GetFieldID(clazz, "mDefaultFontSize", "I");
        mDefaultFixedFontSize = env->GetFieldID(clazz, "mDefaultFixedFontSize", "I");
        mLoadsImagesAutomatically = env->GetFieldID(clazz, "mLoadsImagesAutomatically", "Z");
#ifdef ANDROID_BLOCK_NETWORK_IMAGE
        mBlockNetworkImage = env->GetFieldID(clazz, "mBlockNetworkImage", "Z");
#endif
        mJavaScriptEnabled = env->GetFieldID(clazz, "mJavaScriptEnabled", "Z");
        mPluginsEnabled = env->GetFieldID(clazz, "mPluginsEnabled", "Z");
#if ENABLE(DATABASE)
        mDatabaseEnabled = env->GetFieldID(clazz, "mDatabaseEnabled", "Z");
#endif
#if ENABLE(DOM_STORAGE)
        mDomStorageEnabled = env->GetFieldID(clazz, "mDomStorageEnabled", "Z");
#endif
#if ENABLE(DATABASE) || ENABLE(DOM_STORAGE)
        // The databases saved to disk for both the SQL and DOM Storage APIs are stored
        // in the same base directory.
        mDatabasePath = env->GetFieldID(clazz, "mDatabasePath", "Ljava/lang/String;");
#endif
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        mAppCacheEnabled = env->GetFieldID(clazz, "mAppCacheEnabled", "Z");
        mAppCachePath = env->GetFieldID(clazz, "mAppCachePath", "Ljava/lang/String;");
        mAppCacheMaxSize = env->GetFieldID(clazz, "mAppCacheMaxSize", "J");
#endif
#if ENABLE(WORKERS)
        mWorkersEnabled = env->GetFieldID(clazz, "mWorkersEnabled", "Z");
#endif
        mGeolocationEnabled = env->GetFieldID(clazz, "mGeolocationEnabled", "Z");
        mGeolocationDatabasePath = env->GetFieldID(clazz, "mGeolocationDatabasePath", "Ljava/lang/String;");
        mJavaScriptCanOpenWindowsAutomatically = env->GetFieldID(clazz,
                "mJavaScriptCanOpenWindowsAutomatically", "Z");
        mUseWideViewport = env->GetFieldID(clazz, "mUseWideViewport", "Z");
        mSupportMultipleWindows = env->GetFieldID(clazz, "mSupportMultipleWindows", "Z");
        mShrinksStandaloneImagesToFit = env->GetFieldID(clazz, "mShrinksStandaloneImagesToFit", "Z");
        mUseDoubleTree = env->GetFieldID(clazz, "mUseDoubleTree", "Z");

        LOG_ASSERT(mLayoutAlgorithm, "Could not find field mLayoutAlgorithm");
        LOG_ASSERT(mTextSize, "Could not find field mTextSize");
        LOG_ASSERT(mStandardFontFamily, "Could not find field mStandardFontFamily");
        LOG_ASSERT(mFixedFontFamily, "Could not find field mFixedFontFamily");
        LOG_ASSERT(mSansSerifFontFamily, "Could not find field mSansSerifFontFamily");
        LOG_ASSERT(mSerifFontFamily, "Could not find field mSerifFontFamily");
        LOG_ASSERT(mCursiveFontFamily, "Could not find field mCursiveFontFamily");
        LOG_ASSERT(mFantasyFontFamily, "Could not find field mFantasyFontFamily");
        LOG_ASSERT(mDefaultTextEncoding, "Could not find field mDefaultTextEncoding");
        LOG_ASSERT(mUserAgent, "Could not find field mUserAgent");
        LOG_ASSERT(mMinimumFontSize, "Could not find field mMinimumFontSize");
        LOG_ASSERT(mMinimumLogicalFontSize, "Could not find field mMinimumLogicalFontSize");
        LOG_ASSERT(mDefaultFontSize, "Could not find field mDefaultFontSize");
        LOG_ASSERT(mDefaultFixedFontSize, "Could not find field mDefaultFixedFontSize");
        LOG_ASSERT(mLoadsImagesAutomatically, "Could not find field mLoadsImagesAutomatically");
#ifdef ANDROID_BLOCK_NETWORK_IMAGE
        LOG_ASSERT(mBlockNetworkImage, "Could not find field mBlockNetworkImage");
#endif
        LOG_ASSERT(mJavaScriptEnabled, "Could not find field mJavaScriptEnabled");
        LOG_ASSERT(mPluginsEnabled, "Could not find field mPluginsEnabled");
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        LOG_ASSERT(mAppCacheEnabled, "Could not find field mAppCacheEnabled");
        LOG_ASSERT(mAppCachePath, "Could not find field mAppCachePath");
        LOG_ASSERT(mAppCacheMaxSize, "Could not find field mAppCacheMaxSize");
#endif
#if ENABLE(WORKERS)
        LOG_ASSERT(mWorkersEnabled, "Could not find field mWorkersEnabled");
#endif
        LOG_ASSERT(mJavaScriptCanOpenWindowsAutomatically,
                "Could not find field mJavaScriptCanOpenWindowsAutomatically");
        LOG_ASSERT(mUseWideViewport, "Could not find field mUseWideViewport");
        LOG_ASSERT(mSupportMultipleWindows, "Could not find field mSupportMultipleWindows");
        LOG_ASSERT(mShrinksStandaloneImagesToFit, "Could not find field mShrinksStandaloneImagesToFit");
        LOG_ASSERT(mUseDoubleTree, "Could not find field mUseDoubleTree");

        jclass c = env->FindClass("java/lang/Enum");
        LOG_ASSERT(c, "Could not find Enum class!");
        mOrdinal = env->GetMethodID(c, "ordinal", "()I");
        LOG_ASSERT(mOrdinal, "Could not find method ordinal");
        c = env->FindClass("android/webkit/WebSettings$TextSize");
        LOG_ASSERT(c, "Could not find TextSize enum");
        mTextSizeValue = env->GetFieldID(c, "value", "I");
    }

    // Field ids
    jfieldID mLayoutAlgorithm;
    jfieldID mTextSize;
    jfieldID mStandardFontFamily;
    jfieldID mFixedFontFamily;
    jfieldID mSansSerifFontFamily;
    jfieldID mSerifFontFamily;
    jfieldID mCursiveFontFamily;
    jfieldID mFantasyFontFamily;
    jfieldID mDefaultTextEncoding;
    jfieldID mUserAgent;
    jfieldID mMinimumFontSize;
    jfieldID mMinimumLogicalFontSize;
    jfieldID mDefaultFontSize;
    jfieldID mDefaultFixedFontSize;
    jfieldID mLoadsImagesAutomatically;
#ifdef ANDROID_BLOCK_NETWORK_IMAGE
    jfieldID mBlockNetworkImage;
#endif
    jfieldID mJavaScriptEnabled;
    jfieldID mPluginsEnabled;
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    jfieldID mAppCacheEnabled;
    jfieldID mAppCachePath;
    jfieldID mAppCacheMaxSize;
#endif
#if ENABLE(WORKERS)
    jfieldID mWorkersEnabled;
#endif
    jfieldID mJavaScriptCanOpenWindowsAutomatically;
    jfieldID mUseWideViewport;
    jfieldID mSupportMultipleWindows;
    jfieldID mShrinksStandaloneImagesToFit;
    jfieldID mUseDoubleTree;
    // Ordinal() method and value field for enums
    jmethodID mOrdinal;
    jfieldID  mTextSizeValue;

#if ENABLE(DATABASE)
    jfieldID mDatabaseEnabled;
#endif
#if ENABLE(DOM_STORAGE)
    jfieldID mDomStorageEnabled;
#endif
    jfieldID mGeolocationEnabled;
    jfieldID mGeolocationDatabasePath;
#if ENABLE(DATABASE) || ENABLE(DOM_STORAGE)
    jfieldID mDatabasePath;
#endif
};

static struct FieldIds* gFieldIds;

// Note: This is moved from the old FrameAndroid.cpp
static void recursiveCleanupForFullLayout(WebCore::RenderObject* obj)
{
    obj->setNeedsLayout(true, false);
#ifdef ANDROID_LAYOUT
    if (obj->isTable())
        (static_cast<WebCore::RenderTable *>(obj))->clearSingleColumn();
#endif
    for (WebCore::RenderObject* n = obj->firstChild(); n; n = n->nextSibling())
        recursiveCleanupForFullLayout(n);
}

class WebSettings {
public:
    static void Sync(JNIEnv* env, jobject obj, jint frame)
    {
        WebCore::Frame* pFrame = (WebCore::Frame*)frame;
        LOG_ASSERT(pFrame, "%s must take a valid frame pointer!", __FUNCTION__);
        WebCore::Settings* s = pFrame->settings();
        if (!s)
            return;
        WebCore::DocLoader* docLoader = pFrame->document()->docLoader();

#ifdef ANDROID_LAYOUT
        jobject layout = env->GetObjectField(obj, gFieldIds->mLayoutAlgorithm);
        WebCore::Settings::LayoutAlgorithm l = (WebCore::Settings::LayoutAlgorithm)
                env->CallIntMethod(layout, gFieldIds->mOrdinal);
        if (s->layoutAlgorithm() != l) {
            s->setLayoutAlgorithm(l);
            if (pFrame->document()) {
                pFrame->document()->updateStyleSelector();
                if (pFrame->document()->renderer()) {
                    recursiveCleanupForFullLayout(pFrame->document()->renderer());
                    LOG_ASSERT(pFrame->view(), "No view for this frame when trying to relayout");
                    pFrame->view()->layout();
                    // FIXME: This call used to scroll the page to put the focus into view.
                    // It worked on the WebViewCore, but now scrolling is done outside of the
                    // WebViewCore, on the UI side, so there needs to be a new way to do this.
                    //pFrame->makeFocusVisible();
                }
            }
        }
#endif
        jobject textSize = env->GetObjectField(obj, gFieldIds->mTextSize);
        float zoomFactor = env->GetIntField(textSize, gFieldIds->mTextSizeValue) / 100.0f;
        if (pFrame->zoomFactor() != zoomFactor)
            pFrame->setZoomFactor(zoomFactor, /*isTextOnly*/true);

        jstring str = (jstring)env->GetObjectField(obj, gFieldIds->mStandardFontFamily);
        s->setStandardFontFamily(to_string(env, str));

        str = (jstring)env->GetObjectField(obj, gFieldIds->mFixedFontFamily);
        s->setFixedFontFamily(to_string(env, str));

        str = (jstring)env->GetObjectField(obj, gFieldIds->mSansSerifFontFamily);
        s->setSansSerifFontFamily(to_string(env, str));

        str = (jstring)env->GetObjectField(obj, gFieldIds->mSerifFontFamily);
        s->setSerifFontFamily(to_string(env, str));

        str = (jstring)env->GetObjectField(obj, gFieldIds->mCursiveFontFamily);
        s->setCursiveFontFamily(to_string(env, str));

        str = (jstring)env->GetObjectField(obj, gFieldIds->mFantasyFontFamily);
        s->setFantasyFontFamily(to_string(env, str));

        str = (jstring)env->GetObjectField(obj, gFieldIds->mDefaultTextEncoding);
        s->setDefaultTextEncodingName(to_string(env, str));

        str = (jstring)env->GetObjectField(obj, gFieldIds->mUserAgent);
        WebFrame::getWebFrame(pFrame)->setUserAgent(to_string(env, str));

        jint size = env->GetIntField(obj, gFieldIds->mMinimumFontSize);
        s->setMinimumFontSize(size);

        size = env->GetIntField(obj, gFieldIds->mMinimumLogicalFontSize);
        s->setMinimumLogicalFontSize(size);

        size = env->GetIntField(obj, gFieldIds->mDefaultFontSize);
        s->setDefaultFontSize(size);

        size = env->GetIntField(obj, gFieldIds->mDefaultFixedFontSize);
        s->setDefaultFixedFontSize(size);

        jboolean flag = env->GetBooleanField(obj, gFieldIds->mLoadsImagesAutomatically);
        s->setLoadsImagesAutomatically(flag);
        if (flag)
            docLoader->setAutoLoadImages(true);

#ifdef ANDROID_BLOCK_NETWORK_IMAGE
        flag = env->GetBooleanField(obj, gFieldIds->mBlockNetworkImage);
        s->setBlockNetworkImage(flag);
        if(!flag)
            docLoader->setBlockNetworkImage(false);
#endif

        flag = env->GetBooleanField(obj, gFieldIds->mJavaScriptEnabled);
        s->setJavaScriptEnabled(flag);

        flag = env->GetBooleanField(obj, gFieldIds->mPluginsEnabled);
        s->setPluginsEnabled(flag);

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        flag = env->GetBooleanField(obj, gFieldIds->mAppCacheEnabled);
        s->setOfflineWebApplicationCacheEnabled(flag);
        str = (jstring)env->GetObjectField(obj, gFieldIds->mAppCachePath);
        if (str) {
            WebCore::String path = to_string(env, str);
            if (path.length() && WebCore::cacheStorage().cacheDirectory().isNull()) {
                WebCore::cacheStorage().setCacheDirectory(path);
            }
        }
        jlong maxsize = env->GetIntField(obj, gFieldIds->mAppCacheMaxSize);
        WebCore::cacheStorage().setMaximumSize(maxsize);
#endif

        flag = env->GetBooleanField(obj, gFieldIds->mJavaScriptCanOpenWindowsAutomatically);
        s->setJavaScriptCanOpenWindowsAutomatically(flag);

#ifdef ANDROID_LAYOUT
        flag = env->GetBooleanField(obj, gFieldIds->mUseWideViewport);
        s->setUseWideViewport(flag);
#endif

#ifdef ANDROID_MULTIPLE_WINDOWS
        flag = env->GetBooleanField(obj, gFieldIds->mSupportMultipleWindows);
        s->setSupportMultipleWindows(flag);
#endif
        flag = env->GetBooleanField(obj, gFieldIds->mShrinksStandaloneImagesToFit);
        s->setShrinksStandaloneImagesToFit(flag);
#if ENABLE(DATABASE)
        flag = env->GetBooleanField(obj, gFieldIds->mDatabaseEnabled);
        s->setDatabasesEnabled(flag);
        str = (jstring)env->GetObjectField(obj, gFieldIds->mDatabasePath);
        WebCore::DatabaseTracker::tracker().setDatabaseDirectoryPath(to_string(env, str));
#endif
#if ENABLE(DOM_STORAGE)
        flag = env->GetBooleanField(obj, gFieldIds->mDomStorageEnabled);
        s->setLocalStorageEnabled(flag);
        str = (jstring)env->GetObjectField(obj, gFieldIds->mDatabasePath);
        if (str) {
            WebCore::String localStorageDatabasePath = to_string(env,str);
            if (localStorageDatabasePath.length()) {
                s->setLocalStorageDatabasePath(localStorageDatabasePath);
            }
        }
#endif

        flag = env->GetBooleanField(obj, gFieldIds->mGeolocationEnabled);
        GeolocationPermissions::setAlwaysDeny(!flag);
        str = (jstring)env->GetObjectField(obj, gFieldIds->mGeolocationDatabasePath);
        if (str) {
            GeolocationPermissions::setDatabasePath(to_string(env,str));
            WebCore::Geolocation::setDatabasePath(to_string(env,str));
        }
    }
};

//-------------------------------------------------------------
// JNI registration
//-------------------------------------------------------------

static JNINativeMethod gWebSettingsMethods[] = {
    { "nativeSync", "(I)V",
        (void*) WebSettings::Sync }
};

int register_websettings(JNIEnv* env)
{
    jclass clazz = env->FindClass("android/webkit/WebSettings");
    LOG_ASSERT(clazz, "Unable to find class WebSettings!");
    gFieldIds = new FieldIds(env, clazz);
    return jniRegisterNativeMethods(env, "android/webkit/WebSettings",
            gWebSettingsMethods, NELEM(gWebSettingsMethods));
}

}
