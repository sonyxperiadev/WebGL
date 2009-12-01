/*
 * Copyright 2009, The Android Open Source Project
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

#define LOG_TAG "pluginActivity"

#include <config.h>

#include "PluginView.h"
#include "PluginWidgetAndroid.h"
#include "WebCoreJni.h"
#include "jni_utility.h"
#include <JNIHelp.h>

namespace android {


static WebCore::PluginView* pluginViewForInstance(NPP instance) {
    if (instance && instance->ndata)
        return static_cast<WebCore::PluginView*>(instance->ndata);
    return NULL;
}

//--------------------------------------------------------------------------
// PluginActivity native methods.
//--------------------------------------------------------------------------

static jobject getWebkitPlugin(JNIEnv* env, jobject obj, jint npp)
{
    NPP instance = (NPP)npp;

    WebCore::PluginView* pluginView = pluginViewForInstance(instance);
    LOG_ASSERT(pluginView, "Unable to resolve the plugin using the given NPP");

    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();
    LOG_ASSERT(pluginWidget, "Unable to retrieve the android specific portion of the plugin");

    return pluginWidget->getJavaPluginInstance();
}

//---------------------------------------------------------
// JNI registration
//---------------------------------------------------------
static JNINativeMethod gPluginActivityMethods[] = {
    { "nativeGetWebkitPlugin", "(I)Landroid/webkit/plugin/WebkitPlugin;",
        (void*) getWebkitPlugin }
};


int register_plugin_activity(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, "android/webkit/PluginActivity",
            gPluginActivityMethods, NELEM(gPluginActivityMethods));
}

} /* namespace android */
