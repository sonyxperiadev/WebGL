/* 
**
** Copyright 2007, The Android Open Source Project
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

#define LOG_TAG "websettings"

#include <config.h>
#include <wtf/Platform.h>

#include "Document.h"
#include "Frame.h"
#include "FrameAndroid.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "DocLoader.h"
#include "Page.h"
#ifdef ANDROID_PLUGINS
#include "PluginDatabaseAndroid.h"
#endif
#include "Settings.h"
#include "WebCoreFrameBridge.h"

#undef LOG
#include <JNIHelp.h>
#include <utils/Log.h>
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
#ifdef ANDROID_PLUGINS
        mPluginsPath = env->GetFieldID(clazz, "mPluginsPath", "Ljava/lang/String;");
#endif
        mJavaScriptCanOpenWindowsAutomatically = env->GetFieldID(clazz,
                "mJavaScriptCanOpenWindowsAutomatically", "Z");
        mUseWideViewport = env->GetFieldID(clazz, "mUseWideViewport", "Z");
        mSupportMultipleWindows = env->GetFieldID(clazz, "mSupportMultipleWindows", "Z");
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
#ifdef ANDROID_PLUGINS
        LOG_ASSERT(mPluginsPath, "Could not find field mPluginsPath");
#endif
        LOG_ASSERT(mJavaScriptCanOpenWindowsAutomatically,
                "Could not find field mJavaScriptCanOpenWindowsAutomatically");
        LOG_ASSERT(mUseWideViewport, "Could not find field mUseWideViewport");
        LOG_ASSERT(mSupportMultipleWindows, "Could not find field mSupportMultipleWindows");
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
#ifdef ANDROID_PLUGINS
    jfieldID mPluginsPath;
#endif
    jfieldID mJavaScriptCanOpenWindowsAutomatically;
    jfieldID mUseWideViewport;
    jfieldID mSupportMultipleWindows;
    jfieldID mUseDoubleTree;

    // Ordinal() method and value field for enums
    jmethodID mOrdinal;
    jfieldID  mTextSizeValue;
};

static struct FieldIds* gFieldIds;

class WebSettings {
public:
    static void Sync(JNIEnv* env, jobject obj, jint frame)
    {
        WebCore::FrameAndroid* pFrame = (WebCore::FrameAndroid*)frame;
        LOG_ASSERT(pFrame, "%s must take a valid frame pointer!", __FUNCTION__);
        WebCore::Settings* s = pFrame->page()->settings();
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
                    pFrame->cleanupForFullLayout(pFrame->document()->renderer());
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
        jint zoomFactor = env->GetIntField(textSize, gFieldIds->mTextSizeValue);
        if (pFrame->zoomFactor() != zoomFactor)
            pFrame->setZoomFactor(zoomFactor);

        jstring str = (jstring)env->GetObjectField(obj, gFieldIds->mStandardFontFamily);
        const char* font = env->GetStringUTFChars(str, NULL);
        s->setStandardFontFamily(font);
        env->ReleaseStringUTFChars(str, font);

        str = (jstring)env->GetObjectField(obj, gFieldIds->mFixedFontFamily);
        font = env->GetStringUTFChars(str, NULL);
        s->setFixedFontFamily(font);
        env->ReleaseStringUTFChars(str, font);

        str = (jstring)env->GetObjectField(obj, gFieldIds->mSansSerifFontFamily);
        font = env->GetStringUTFChars(str, NULL);
        s->setSansSerifFontFamily(font);
        env->ReleaseStringUTFChars(str, font);

        str = (jstring)env->GetObjectField(obj, gFieldIds->mSerifFontFamily);
        font = env->GetStringUTFChars(str, NULL);
        s->setSerifFontFamily(font);
        env->ReleaseStringUTFChars(str, font);

        str = (jstring)env->GetObjectField(obj, gFieldIds->mCursiveFontFamily);
        font = env->GetStringUTFChars(str, NULL);
        s->setCursiveFontFamily(font);
        env->ReleaseStringUTFChars(str, font);

        str = (jstring)env->GetObjectField(obj, gFieldIds->mFantasyFontFamily);
        font = env->GetStringUTFChars(str, NULL);
        s->setFantasyFontFamily(font);
        env->ReleaseStringUTFChars(str, font);

        str = (jstring)env->GetObjectField(obj, gFieldIds->mDefaultTextEncoding);
        const char* encoding = env->GetStringUTFChars(str, NULL);
        s->setDefaultTextEncodingName(encoding);
        env->ReleaseStringUTFChars(str, encoding);

        str = (jstring)env->GetObjectField(obj, gFieldIds->mUserAgent);
        const char* userAgentStr = env->GetStringUTFChars(str, NULL);
        pFrame->bridge()->setUserAgent(userAgentStr);
        env->ReleaseStringUTFChars(str, userAgentStr);

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

#ifdef ANDROID_PLUGINS
        str = (jstring)env->GetObjectField(obj, gFieldIds->mPluginsPath);
        if (str) {
            const char* pluginsPathStr = env->GetStringUTFChars(str, NULL);
            s->setPluginsPath(pluginsPathStr);
            // Also sync PluginDatabaseAndroid with the new default path.
            ::WebCore::PluginDatabaseAndroid::setDefaultPluginsPath(pluginsPathStr);
            env->ReleaseStringUTFChars(str, pluginsPathStr);
        }
#endif

        flag = env->GetBooleanField(obj, gFieldIds->mJavaScriptCanOpenWindowsAutomatically);
        s->setJavaScriptCanOpenWindowsAutomatically(flag);

        flag = env->GetBooleanField(obj, gFieldIds->mUseWideViewport);
        s->setUseWideViewport(flag);

#ifdef ANDROID_MULTIPLE_WINDOWS
        flag = env->GetBooleanField(obj, gFieldIds->mSupportMultipleWindows);
        s->setSupportMultipleWindows(flag);
#endif

#if USE(LOW_BANDWIDTH_DISPLAY)
        flag = env->GetBooleanField(obj, gFieldIds->mUseDoubleTree);
        pFrame->loader()->setUseLowBandwidthDisplay(flag);
#endif
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
