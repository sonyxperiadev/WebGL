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
#include "SkANP.h"
#include "WebViewCore.h"
#include "PluginView.h"
#include "PluginWidgetAndroid.h"

#include "JavaSharedClient.h"

using namespace android;

struct WrappedANPEvent {
    WebViewCore*            fWVC;
    PluginWidgetAndroid*    fPWA;
    ANPEvent                fEvent;
};

/*  Its possible we may be called after the plugin that initiated the event
    has been torn-down. Thus we check that the assicated webviewcore and
    pluginwidget are still active before dispatching the event.
 */
static void send_anpevent(void* data) {
    WrappedANPEvent* wrapper = static_cast<WrappedANPEvent*>(data);
    WebViewCore* core = wrapper->fWVC;
    PluginWidgetAndroid* widget = wrapper->fPWA;

    // be sure we're still alive before delivering the event
    if (WebViewCore::isInstance(core) && core->isPlugin(widget)) {
        widget->sendEvent(wrapper->fEvent);
    }
    delete wrapper;
}

static void anp_postEvent(NPP instance, const ANPEvent* event) {
    if (instance && instance->ndata && event) {
        PluginView* pluginView = static_cast<PluginView*>(instance->ndata);
        PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();
        WebViewCore* wvc = pluginWidget->webViewCore();

        WrappedANPEvent* wrapper = new WrappedANPEvent;
        // recored these, and recheck that they are valid before delivery
        // in send_anpevent
        wrapper->fWVC = pluginWidget->webViewCore();
        wrapper->fPWA = pluginWidget;
        // make a copy of the event
        wrapper->fEvent = *event;
        JavaSharedClient::EnqueueFunctionPtr(send_anpevent, wrapper);
    }
}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void ANPEventInterfaceV0_Init(ANPInterface* value) {
    ANPEventInterfaceV0* i = reinterpret_cast<ANPEventInterfaceV0*>(value);

    ASSIGN(i, postEvent);
}
