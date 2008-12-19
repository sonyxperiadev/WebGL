/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

