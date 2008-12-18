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

#ifndef PluginWidgetAndroid_H
#define PluginWidgetAndroid_H

#include "android_npapi.h"

namespace WebCore {
    class PluginView;
}

namespace android {
    class WebViewCore;
}

class SkCanvas;
class SkFlipPixelRef;

/*
    This is our extended state in a PluginView. This object is created and
    kept insync with the PluginView, but is also available to WebViewCore
    to allow its draw() method to be called from outside of the PluginView.
 */
struct PluginWidgetAndroid {
    // initialize with our host pluginview. This will delete us when it is
    // destroyed.
    PluginWidgetAndroid(WebCore::PluginView* view);
    ~PluginWidgetAndroid();
    
    /*  Can't determine our core at construction time, so PluginView calls this
        as soon as it has a parent.
     */
    void init(android::WebViewCore*);
    /*  Called each time the PluginView gets a new size or position.
     */
    void setWindow(int x, int y, int width, int height, bool isTransparent);
    /*  Called whenever the plugin itself requests a new drawing model
     */
    void setDrawingModel(ANPDrawingModel);
    
    /*  Utility method to convert from local (plugin) coordinates to docuemnt
        coordinates. Needed (for instance) to convert the dirty rectangle into
        document coordinates to inturn inval the screen.
     */
    void localToPageCoords(SkIRect*) const;

    /*  Returns true (and optionally updates rect with the dirty bounds) if
        the plugin has invalidate us.
     */
    bool isDirty(SkIRect* dirtyBounds = NULL) const;
    /*  Called by PluginView to invalidate a portion of the plugin area (in
        local plugin coordinates). If signalRedraw is true, this also triggers
        a subsequent call to draw(NULL).
     */
    void inval(const WebCore::IntRect&, bool signalRedraw);
    
    /*  Called to draw into the plugin's bitmap. If canvas is non-null, the
        bitmap itself is then drawn into the canvas.
     */
    void draw(SkCanvas* canvas = NULL);
    
private:
    WebCore::PluginView*    m_pluginView;
    android::WebViewCore*   m_core;
    SkFlipPixelRef*         m_flipPixelRef;
    ANPDrawingModel         m_drawingModel;
    int                     m_x;
    int                     m_y;
};

#endif
