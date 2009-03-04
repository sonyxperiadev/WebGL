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

#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "DocLoader.h"
#include "Page.h"
#include "RenderTable.h"
#ifdef ANDROID_PLUGINS
#include "PlatformString.h"
#include "PluginDatabase.h"
#endif
#include "Settings.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreJni.h"

#include <JNIHelp.h>
#include <utils/misc.h>

namespace WebCore {
// Defined in FileSystemAndroid.cpp
extern String sPluginPath;
}

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
#ifdef ANDROID_PLUGINS
        LOG_ASSERT(mPluginsPath, "Could not find field mPluginsPath");
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
#ifdef ANDROID_PLUGINS
    jfieldID mPluginsPath;
#endif
    jfieldID mJavaScriptCanOpenWindowsAutomatically;
    jfieldID mUseWideViewport;
    jfieldID mSupportMultipleWindows;
    jfieldID mShrinksStandaloneImagesToFit;
    jfieldID mUseDoubleTree;

    // Ordinal() method and value field for enums
    jmethodID mOrdinal;
    jfieldID  mTextSizeValue;
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

#ifdef ANDROID_PLUGINS
        ::WebCore::PluginDatabase *pluginDatabase =
                  ::WebCore::PluginDatabase::installedPlugins();
        str = (jstring)env->GetObjectField(obj, gFieldIds->mPluginsPath);
        if (str) {
            WebCore::String pluginsPath = to_string(env, str);
            // When a new browser Tab is created, the corresponding
            // Java WebViewCore object will sync (with the native
            // side) its associated WebSettings at initialization
            // time. However, at that point, the WebSettings object's
            // mPluginsPaths member is set to the empty string. The
            // real plugin path will be set later by the tab and the
            // WebSettings will be synced again.
            //
            // There is no point in instructing WebCore's
            // PluginDatabase instance to set the plugin path to the
            // empty string. Furthermore, if the PluginDatabase
            // instance is already initialized, setting the path to
            // the empty string will cause the PluginDatabase to
            // forget about the plugin files it has already
            // inspected. When the path is subsequently set to the
            // correct value, the PluginDatabase will attempt to load
            // and initialize plugins that are already loaded and
            // initialized.
            if (pluginsPath.length()) {
                s->setPluginsPath(pluginsPath);
                // Set the plugin directories to this single entry.
                Vector< ::WebCore::String > paths(1);
                paths[0] = pluginsPath;
                pluginDatabase->setPluginDirectories(paths);
                // Set the home directory for plugin temporary files
                WebCore::sPluginPath = paths[0];
                // Reload plugins. We call Page::refreshPlugins() instead
                // of pluginDatabase->refresh(), as we need to ensure that
                // the list of mimetypes exposed by the browser are also
                // updated.
                WebCore::Page::refreshPlugins(false);
            }
        }
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
