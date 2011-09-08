/*
 * Copyright 2011, The Android Open Source Project
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
#include "ANPVideo_npapi.h"
#include "SkANP.h"

#include "PluginView.h"
#include "PluginWidgetAndroid.h"
#include "MediaLayer.h"

static WebCore::PluginView* pluginViewForInstance(NPP instance) {
    if (instance && instance->ndata)
        return static_cast<WebCore::PluginView*>(instance->ndata);
    return WebCore::PluginView::currentPluginView();
}

static WebCore::MediaLayer* mediaLayerForInstance(NPP instance) {
    WebCore::PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();
    return pluginWidget->getLayer();
}

static ANativeWindow* anp_acquireNativeWindow(NPP instance) {
    WebCore::MediaLayer* mediaLayer = mediaLayerForInstance(instance);

    return mediaLayer->acquireNativeWindowForVideo();
}

static void anp_setWindowDimensions(NPP instance, const ANativeWindow* window,
        const ANPRectF* dimensions) {

    WebCore::MediaLayer* mediaLayer = mediaLayerForInstance(instance);
    if (!mediaLayer)
        return;

    SkRect rect;
    mediaLayer->setWindowDimensionsForVideo(window, *SkANP::SetRect(&rect, *dimensions));
}


static void anp_releaseNativeWindow(NPP instance, ANativeWindow* window) {
    WebCore::MediaLayer* mediaLayer = mediaLayerForInstance(instance);
    if (!mediaLayer)
        return;

    mediaLayer->releaseNativeWindowForVideo(window);
}

static void anp_setFramerateCallback(NPP instance, const ANativeWindow* window, ANPVideoFrameCallbackProc callback) {
    WebCore::MediaLayer* mediaLayer = mediaLayerForInstance(instance);
    if (!mediaLayer)
        return;

    mediaLayer->setFramerateCallback(window, callback);
}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void ANPVideoInterfaceV0_Init(ANPInterface* value) {
    ANPVideoInterfaceV0* i = reinterpret_cast<ANPVideoInterfaceV0*>(value);

    ASSIGN(i, acquireNativeWindow);
    ASSIGN(i, setWindowDimensions);
    ASSIGN(i, releaseNativeWindow);
}

void ANPVideoInterfaceV1_Init(ANPInterface* value) {
    // initialize the functions from the previous interface
    ANPVideoInterfaceV0_Init(value);
    // add any new functions or override existing functions
    ANPVideoInterfaceV1* i = reinterpret_cast<ANPVideoInterfaceV1*>(value);
    ASSIGN(i, setFramerateCallback);
}
