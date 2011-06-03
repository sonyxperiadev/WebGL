/*
 * Copyright 2008, The Android Open Source Project
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
#include "SkANP.h"
#include "WebViewCore.h"
#include "PluginView.h"
#include "PluginWidgetAndroid.h"

static PluginView* pluginViewForInstance(NPP instance) {
    if (instance && instance->ndata)
        return static_cast<PluginView*>(instance->ndata);
    return PluginView::currentPluginView();
}

static void anp_setVisibleRects(NPP instance, const ANPRectI rects[], int32_t count) {
    PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();
    pluginWidget->setVisibleRects(rects, count);
}

static void anp_clearVisibleRects(NPP instance) {
    anp_setVisibleRects(instance, NULL, 0);
}

static void anp_showKeyboard(NPP instance, bool value) {
    PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();
    if(pluginWidget->hasFocus())
        pluginWidget->webViewCore()->requestKeyboard(value);
}

static void anp_requestFullScreen(NPP instance) {
    PluginView* pluginView = pluginViewForInstance(instance);
    // call focusPluginElement() so that the pluginView receives keyboard events
    pluginView->focusPluginElement();
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();
    pluginWidget->requestFullScreen();
}

static void anp_exitFullScreen(NPP instance) {
    PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();
    pluginWidget->exitFullScreen(true);
}

static void anp_requestCenterFitZoom(NPP instance) {
    PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();
    pluginWidget->requestCenterFitZoom();
}

static ANPRectI anp_visibleRect(NPP instance) {
    PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();
    return pluginWidget->visibleRect();
}

static void anp_requestFullScreenOrientation(NPP instance, ANPScreenOrientation orientation) {
    PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();
    pluginWidget->setFullScreenOrientation(orientation);
}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void ANPWindowInterfaceV0_Init(ANPInterface* value) {
    ANPWindowInterfaceV0* i = reinterpret_cast<ANPWindowInterfaceV0*>(value);

    ASSIGN(i, setVisibleRects);
    ASSIGN(i, clearVisibleRects);
    ASSIGN(i, showKeyboard);
    ASSIGN(i, requestFullScreen);
    ASSIGN(i, exitFullScreen);
    ASSIGN(i, requestCenterFitZoom);
}

void ANPWindowInterfaceV1_Init(ANPInterface* value) {
    // initialize the functions from the previous interface
    ANPWindowInterfaceV0_Init(value);
    // add any new functions or override existing functions
    ANPWindowInterfaceV1* i = reinterpret_cast<ANPWindowInterfaceV1*>(value);
    ASSIGN(i, visibleRect);
}

void ANPWindowInterfaceV2_Init(ANPInterface* value) {
    // initialize the functions from the previous interface
    ANPWindowInterfaceV1_Init(value);
    // add any new functions or override existing functions
    ANPWindowInterfaceV2* i = reinterpret_cast<ANPWindowInterfaceV2*>(value);
    ASSIGN(i, requestFullScreenOrientation);
}
