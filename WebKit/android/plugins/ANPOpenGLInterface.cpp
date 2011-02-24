/*
 * Copyright 2010, The Android Open Source Project
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

#include "ANPOpenGL_npapi.h"
#include "PluginView.h"
#include "PluginWidgetAndroid.h"
#include "MediaLayer.h"
#include "WebViewCore.h"
#include "Frame.h"
#include "Page.h"
#include "Chrome.h"
#include "ChromeClient.h"

using namespace android;

static WebCore::PluginView* pluginViewForInstance(NPP instance) {
    if (instance && instance->ndata)
        return static_cast<WebCore::PluginView*>(instance->ndata);
    return WebCore::PluginView::currentPluginView();
}

static EGLContext anp_acquireContext(NPP instance) {
    WebCore::PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();
    WebCore::MediaLayer* mediaLayer = pluginWidget->getLayer();

    if (!mediaLayer)
        return EGL_NO_CONTEXT;

    return mediaLayer->getTexture()->producerAcquireContext();
}

static ANPTextureInfo anp_lockTexture(NPP instance) {
    WebCore::PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();
    WebCore::MediaLayer* mediaLayer = pluginWidget->getLayer();
    WebCore::DoubleBufferedTexture* texture = mediaLayer->getTexture();

    // lock the texture and cache the internal info
    WebCore::TextureInfo* info = texture->producerLock();
    mediaLayer->setCurrentTextureInfo(info);

    ANPTextureInfo anpInfo;
    anpInfo.textureId = info->m_textureId;
    anpInfo.width = (int32_t) info->m_width;
    anpInfo.height = (int32_t) info->m_height;
    anpInfo.internalFormat = info->m_internalFormat;
    return anpInfo;
}

static void anp_releaseTexture(NPP instance, const ANPTextureInfo* textureInfo) {
    WebCore::PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();
    WebCore::MediaLayer* mediaLayer = pluginWidget->getLayer();
    WebCore::DoubleBufferedTexture* texture = mediaLayer->getTexture();

    //copy the info into our internal structure
    WebCore::TextureInfo* info =  mediaLayer->getCurrentTextureInfo();
    info->m_textureId = textureInfo->textureId;
    info->m_width = textureInfo->width;
    info->m_height = textureInfo->height;
    info->m_internalFormat = textureInfo->internalFormat;

    texture->producerReleaseAndSwap();

    // invalidate the java view so that this content is drawn
    pluginWidget->viewInvalidate();
}

static void anp_invertPluginContent(NPP instance, bool isContentInverted) {
    WebCore::PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();
    WebCore::MediaLayer* mediaLayer = pluginWidget->getLayer();

    mediaLayer->invertContents(isContentInverted);

    //force the layer to sync to the UI thread
    WebViewCore* wvc = pluginWidget->webViewCore();
    if (wvc)
        wvc->mainFrame()->page()->chrome()->client()->scheduleCompositingLayerSync();
}



///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void ANPOpenGLInterfaceV0_Init(ANPInterface* v) {
    ANPOpenGLInterfaceV0* i = reinterpret_cast<ANPOpenGLInterfaceV0*>(v);

    ASSIGN(i, acquireContext);
    ASSIGN(i, lockTexture);
    ASSIGN(i, releaseTexture);
    ASSIGN(i, invertPluginContent);
}
