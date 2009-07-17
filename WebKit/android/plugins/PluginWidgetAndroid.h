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

#ifndef PluginWidgetAndroid_H
#define PluginWidgetAndroid_H

#include "android_npapi.h"

#include <wtf/OwnPtr.h>

namespace WebCore {
    class PluginView;
}

namespace android {
    class PluginSurface;
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

    WebCore::PluginView* pluginView() const { return m_pluginView; }

    // Needed by PluginSurface to manage the java SurfaceView.
    android::WebViewCore* webViewCore() const { return m_core; }

    /*  Can't determine our core at construction time, so PluginView calls this
        as soon as it has a parent.
     */
    void init(android::WebViewCore*);
    /*  Called each time the PluginView gets a new size or position.
     */
    void setWindow(int x, int y, int width, int height, bool isTransparent);
    /*  Called whenever the plugin itself requests a new drawing model. If the
        hardware does not support the requested model then false is returned,
        otherwise true is returned.
     */
    bool setDrawingModel(ANPDrawingModel);

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

    /*  Send this event to the plugin instance, and return true if the plugin
        handled it.
     */
    bool sendEvent(const ANPEvent&);

    /*  Update the plugins event flags. If a flag is set to true then the plugin
        wants to be notified of events of this type.
     */
    void updateEventFlags(ANPEventFlags);

    /*  Called to check if a plugin wants to accept a given event type. It
        returns true if the plugin wants the events and false otherwise.
     */
    bool isAcceptingEvent(ANPEventFlag);

    /*  Create an ANPSurface that the plugin may draw in to. The drawing model
        must be kSurface_ANPDrawingModel for this call to succeed. The type
        specifies what kind of pixel access will be available.
     */
    ANPSurface* createSurface(ANPSurfaceType type);

private:
    WebCore::PluginView*    m_pluginView;
    android::WebViewCore*   m_core;
    SkFlipPixelRef*         m_flipPixelRef;
    ANPDrawingModel         m_drawingModel;
    ANPEventFlags           m_eventFlags;
    int                     m_x;
    int                     m_y;
    OwnPtr<android::PluginSurface> m_surface;
};

#endif
