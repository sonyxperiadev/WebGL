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

// must include config.h first for webkit to fiddle with new/delete
#include "config.h"

#include "CString.h"
#include "JavaSharedClient.h"
#include "PluginClient.h"
#include "PluginPackage.h"
#include "PluginView.h"
#include "PluginWidgetAndroid.h"
#include "SkString.h"
#include "WebViewCore.h"

#include "ANPSystem_npapi.h"

static const char* gApplicationDataDir = NULL;

using namespace android;

static const char* anp_getApplicationDataDirectory() {
    if (NULL == gApplicationDataDir) {
        PluginClient* client = JavaSharedClient::GetPluginClient();
        if (!client)
            return NULL;

        WebCore::String path = client->getPluginSharedDataDirectory();
        int length = path.length();
        if (length == 0)
            return NULL;

        char* storage = (char*) malloc(length + 1);
        if (NULL == storage)
            return NULL;

        memcpy(storage, path.utf8().data(), length);
        storage[length] = '\0';

        // save this assignment for last, so that if multiple threads call us
        // (which should never happen), we never return an incomplete global.
        // At worst, we would allocate storage for the path twice.
        gApplicationDataDir = storage;
    }
    return gApplicationDataDir;
}

static WebCore::PluginView* pluginViewForInstance(NPP instance) {
    if (instance && instance->ndata)
        return static_cast<WebCore::PluginView*>(instance->ndata);
    return PluginView::currentPluginView();
}

static jclass anp_loadJavaClass(NPP instance, const char* className) {
    WebCore::PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();

    jclass result;
    result = pluginWidget->webViewCore()->getPluginClass(pluginView->plugin()->path(),
                                                         className);
    return result;
}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void ANPSystemInterfaceV0_Init(ANPInterface* v) {
    ANPSystemInterfaceV0* i = reinterpret_cast<ANPSystemInterfaceV0*>(v);

    ASSIGN(i, getApplicationDataDirectory);
    ASSIGN(i, loadJavaClass);
}
