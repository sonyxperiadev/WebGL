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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "android_graphics.h"
#include "PluginPackage.h"
#include "PluginView.h"
#include "PluginWidgetAndroid.h"
#include "SkANP.h"
#include "SkFlipPixelRef.h"
#include "WebViewCore.h"

PluginWidgetAndroid::PluginWidgetAndroid(WebCore::PluginView* view)
        : m_pluginView(view) {
    m_flipPixelRef = NULL;
    m_core = NULL;
    m_drawingModel = kBitmap_ANPDrawingModel;
    m_x = m_y = 0;
}

PluginWidgetAndroid::~PluginWidgetAndroid() {
    if (m_core) {
        m_core->removePlugin(this);
    }
    m_flipPixelRef->safeUnref();
}

void PluginWidgetAndroid::init(android::WebViewCore* core) {
    m_core = core;
    m_core->addPlugin(this);
}

static SkBitmap::Config computeConfig(bool isTransparent) {
    return isTransparent ? SkBitmap::kARGB_8888_Config
                         : SkBitmap::kRGB_565_Config;
}

void PluginWidgetAndroid::setWindow(int x, int y, int width, int height,
                                    bool isTransparent) {
    m_x = x;
    m_y = y;
    m_flipPixelRef->safeUnref();
    m_flipPixelRef = new SkFlipPixelRef(computeConfig(isTransparent),
                                        width, height);
}

void PluginWidgetAndroid::setDrawingModel(ANPDrawingModel model) {
    m_drawingModel = model;
}

void PluginWidgetAndroid::localToPageCoords(SkIRect* rect) const {
    rect->offset(m_x, m_y);
}

bool PluginWidgetAndroid::isDirty(SkIRect* rect) const {
    // nothing to report if we haven't had setWindow() called yet
    if (NULL == m_flipPixelRef) {
        return false;
    }

    const SkRegion& dirty = m_flipPixelRef->dirtyRgn();
    if (dirty.isEmpty()) {
        return false;
    } else {
        if (rect) {
            *rect = dirty.getBounds();
        }
        return true;
    }
}

void PluginWidgetAndroid::inval(const WebCore::IntRect& rect,
                                bool signalRedraw) {
    // nothing to do if we haven't had setWindow() called yet
    if (NULL == m_flipPixelRef) {
        return;
    }

    SkIRect r;
    m_flipPixelRef->inval(*android_setrect(&r, rect));
    
    if (signalRedraw && m_flipPixelRef->isDirty()) {
        m_core->invalPlugin(this);
    }
}

void PluginWidgetAndroid::draw(SkCanvas* canvas) {
    if (NULL == m_flipPixelRef || !m_flipPixelRef->isDirty()) {
        return;
    }
    
    SkAutoFlipUpdate update(m_flipPixelRef);
    const SkBitmap& bitmap = update.bitmap();
    const SkRegion& dirty = update.dirty();

    ANPEvent    event;
    SkANP::InitEvent(&event, kDraw_ANPEventType);

    event.data.drawContext.model = m_drawingModel;
    SkANP::SetRect(&event.data.drawContext.clip, dirty.getBounds());
    
    switch (m_drawingModel) {
        case kBitmap_ANPDrawingModel: {
            WebCore::PluginPackage* pkg = m_pluginView->plugin();
            NPP instance = m_pluginView->instance();
            
            if (SkANP::SetBitmap(&event.data.drawContext.data.bitmap,
                                 bitmap) &&
                    pkg->pluginFuncs()->event(instance, &event)) {
            
                if (canvas) {
                    SkBitmap bm(bitmap);
                    bm.setPixelRef(m_flipPixelRef);
                    canvas->drawBitmap(bm, SkIntToScalar(m_x),
                                           SkIntToScalar(m_y), NULL);
                }
            }
            break;
        }
        default:
            break;
    }
}

