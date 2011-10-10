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

#include "config.h"
#include "PluginWidgetAndroid.h"

#if ENABLE(TOUCH_EVENTS)
#include "ChromeClient.h"
#endif
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "Page.h"
#include "PluginPackage.h"
#include "PluginView.h"
#include "PluginWidgetAndroid.h"
#include "ScrollView.h"
#include "SkANP.h"
#include "SkFlipPixelRef.h"
#include "SkString.h"
#include "SkTime.h"
#include "WebViewCore.h"
#include "android_graphics.h"
#include <JNIUtility.h>

//#define PLUGIN_DEBUG_LOCAL // controls the printing of log messages
#define DEBUG_EVENTS 0 // logs event contents, return value, and processing time
#define DEBUG_VISIBLE_RECTS 0 // temporary debug printfs and fixes

#define MAX( a, b ) ( ((a) > (b)) ? (a) : (b) )

// this include statement must follow the declaration of PLUGIN_DEBUG_LOCAL
#include "PluginDebugAndroid.h"

PluginWidgetAndroid::PluginWidgetAndroid(WebCore::PluginView* view)
        : m_pluginView(view) {
    m_flipPixelRef = NULL;
    m_core = NULL;
    m_drawingModel = kBitmap_ANPDrawingModel;
    m_eventFlags = 0;
    m_pluginWindow = NULL;
    m_requestedVisibleRectCount = 0;
    m_requestedVisibleRect.setEmpty();
    m_visibleDocRect.setEmpty();
    m_pluginBounds.setEmpty();
    m_hasFocus = false;
    m_isFullScreen = false;
    m_visible = false;
    m_cachedZoomLevel = 0;
    m_embeddedView = NULL;
    m_embeddedViewAttached = false;
    m_acceptEvents = false;
    m_isSurfaceClippedOut = false;
    m_layer = 0;
    m_powerState = kDefault_ANPPowerState;
    m_fullScreenOrientation = -1;
    m_drawEventDelayed = false;
}

PluginWidgetAndroid::~PluginWidgetAndroid() {
    PLUGIN_LOG("%p Deleting Plugin", m_pluginView->instance());
    m_acceptEvents = false;
    if (m_core) {
        setPowerState(kDefault_ANPPowerState);
        m_core->removePlugin(this);
        if (m_isFullScreen) {
            exitFullScreen(true);
        }
        if (m_embeddedView) {
            m_core->destroySurface(m_embeddedView);
        }
    }

    // cleanup any remaining JNI References
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (m_embeddedView) {
        env->DeleteGlobalRef(m_embeddedView);
    }

    SkSafeUnref(m_flipPixelRef);
    SkSafeUnref(m_layer);
}

void PluginWidgetAndroid::init(android::WebViewCore* core) {
    m_core = core;
    m_core->addPlugin(this);
    m_acceptEvents = true;
    PLUGIN_LOG("%p Initialized Plugin", m_pluginView->instance());
}

static SkBitmap::Config computeConfig(bool isTransparent) {
    return isTransparent ? SkBitmap::kARGB_8888_Config
                         : SkBitmap::kRGB_565_Config;
}

void PluginWidgetAndroid::setWindow(NPWindow* window, bool isTransparent) {

    // store the reference locally for easy lookup
    m_pluginWindow = window;

    // make a copy of the previous bounds
    SkIRect oldPluginBounds = m_pluginBounds;

    // keep a local copy of the plugin bounds because the m_pluginWindow pointer
    // gets updated values prior to this method being called
    m_pluginBounds.set(m_pluginWindow->x, m_pluginWindow->y,
                       m_pluginWindow->x + m_pluginWindow->width,
                       m_pluginWindow->y + m_pluginWindow->height);

    PLUGIN_LOG("%p PluginBounds (%d,%d,%d,%d)", m_pluginView->instance(),
               m_pluginBounds.fLeft, m_pluginBounds.fTop,
               m_pluginBounds.fRight, m_pluginBounds.fBottom);

    const bool boundsChanged = m_pluginBounds != oldPluginBounds;

    //TODO hack to ensure that we grab the most recent screen dimensions and scale
    ANPRectI screenCoords;
    m_core->getVisibleScreen(screenCoords);
    float scale = m_core->scale();
    bool scaleChanged = m_cachedZoomLevel != scale;
    setVisibleScreen(screenCoords, scale);

    // if the scale changed then setVisibleScreen will call this function and
    // this call will potentially fire a duplicate draw event
    if (!scaleChanged) {
        sendSizeAndVisibilityEvents(boundsChanged);
    }
    layoutSurface(boundsChanged);

    if (m_drawingModel != kSurface_ANPDrawingModel) {
        SkSafeUnref(m_flipPixelRef);
        m_flipPixelRef = new SkFlipPixelRef(computeConfig(isTransparent),
                                            window->width, window->height);
    }
}

bool PluginWidgetAndroid::setDrawingModel(ANPDrawingModel model) {

    if (model == kOpenGL_ANPDrawingModel && m_layer == 0) {
        jobject webview = m_core->getWebViewJavaObject();
        m_layer = new WebCore::MediaLayer(webview);
    }
    else if (model != kOpenGL_ANPDrawingModel && m_layer != 0) {
        m_layer->unref();
        m_layer = 0;
    }

    if (m_drawingModel != model) {
        // Trigger layer computation in RenderLayerCompositor
        m_pluginView->getElement()->setNeedsStyleRecalc(SyntheticStyleChange);
    }

    m_drawingModel = model;
    return true;
}

void PluginWidgetAndroid::checkSurfaceReady() {
    if(!m_drawEventDelayed)
        return;

    m_drawEventDelayed = false;
    sendSizeAndVisibilityEvents(true);
}

// returned rect is in the page coordinate
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
            rect->offset(m_pluginWindow->x, m_pluginWindow->y);
        }
        return true;
    }
}

void PluginWidgetAndroid::inval(const WebCore::IntRect& rect,
                                bool signalRedraw) {
    // nothing to do if we haven't had setWindow() called yet. m_flipPixelRef
    // will also be null if this is a Surface model.
    if (NULL == m_flipPixelRef) {
        return;
    }

    m_flipPixelRef->inval(rect);

    if (signalRedraw && m_flipPixelRef->isDirty()) {
        m_core->invalPlugin(this);
    }
}

void PluginWidgetAndroid::viewInvalidate() {
    WebCore::IntRect rect(m_pluginBounds.fLeft, m_pluginBounds.fTop,
            m_pluginBounds.width(), m_pluginBounds.height());
    m_core->viewInvalidate(rect);
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

    event.data.draw.model = m_drawingModel;
    SkANP::SetRect(&event.data.draw.clip, dirty.getBounds());

    switch (m_drawingModel) {
        case kBitmap_ANPDrawingModel: {
            WebCore::PluginPackage* pkg = m_pluginView->plugin();
            NPP instance = m_pluginView->instance();

            if (SkANP::SetBitmap(&event.data.draw.data.bitmap,
                                 bitmap) &&
                    pkg->pluginFuncs()->event(instance, &event)) {

                if (canvas && m_pluginWindow) {
                    SkBitmap bm(bitmap);
                    bm.setPixelRef(m_flipPixelRef);
                    canvas->drawBitmap(bm, 0, 0);
                }
            }
            break;
        }
        default:
            break;
    }
}

void PluginWidgetAndroid::setSurfaceClip(const SkIRect& clip) {

    if (m_drawingModel != kSurface_ANPDrawingModel)
        return;

    /* don't display surfaces that are either entirely clipped or only 1x1 in
       size. It appears that when an element is absolutely positioned and has
       been completely clipped in CSS that webkit still sends a clip of 1x1.
     */
    bool clippedOut = (clip.width() <= 1 && clip.height() <= 1);
    if(clippedOut != m_isSurfaceClippedOut) {
        m_isSurfaceClippedOut = clippedOut;
        layoutSurface();
    }
}

void PluginWidgetAndroid::layoutSurface(bool pluginBoundsChanged) {

    if (m_drawingModel != kSurface_ANPDrawingModel)
        return;
    if (!m_pluginWindow)
        return;


    bool displayPlugin = m_pluginView->isVisible() && !m_isSurfaceClippedOut;
    PLUGIN_LOG("%p DisplayPlugin[%d] visible=[%d] clipped=[%d]",
            m_pluginView->instance(), displayPlugin,
            m_pluginView->isVisible(), m_isSurfaceClippedOut);

    // if the surface does not exist then create a new surface
    if (!m_embeddedView && displayPlugin) {

        WebCore::PluginPackage* pkg = m_pluginView->plugin();
        NPP instance = m_pluginView->instance();

        jobject pluginSurface;
        pkg->pluginFuncs()->getvalue(instance, kJavaSurface_ANPGetValue,
                                     static_cast<void*>(&pluginSurface));

        jobject tempObj = m_core->addSurface(pluginSurface,
                m_pluginWindow->x, m_pluginWindow->y,
                m_pluginWindow->width, m_pluginWindow->height);

        if (tempObj) {
            JNIEnv* env = JSC::Bindings::getJNIEnv();
            m_embeddedView = env->NewGlobalRef(tempObj);
            m_embeddedViewAttached = true;
        }
    // if the view is unattached but visible then attach it
    } else if (m_embeddedView && !m_embeddedViewAttached && displayPlugin && !m_isFullScreen) {
        m_core->updateSurface(m_embeddedView, m_pluginWindow->x, m_pluginWindow->y,
                              m_pluginWindow->width, m_pluginWindow->height);
        m_embeddedViewAttached = true;
    // if the view is attached but invisible then remove it
    } else if (m_embeddedView && m_embeddedViewAttached && !displayPlugin) {
        m_core->destroySurface(m_embeddedView);
        m_embeddedViewAttached = false;
    // if the plugin's bounds have changed and it's visible then update it
    } else if (pluginBoundsChanged && displayPlugin && !m_isFullScreen) {
        m_core->updateSurface(m_embeddedView, m_pluginWindow->x, m_pluginWindow->y,
                              m_pluginWindow->width, m_pluginWindow->height);

    }
}

int16_t PluginWidgetAndroid::sendEvent(const ANPEvent& evt) {
    if (!m_acceptEvents)
        return 0;
    WebCore::PluginPackage* pkg = m_pluginView->plugin();
    NPP instance = m_pluginView->instance();
    // "missing" plugins won't have these
    if (pkg && instance) {

        // if the plugin is gaining focus then update our state now to allow
        // the plugin's event handler to perform actions that require focus
        if (evt.eventType == kLifecycle_ANPEventType &&
                evt.data.lifecycle.action == kGainFocus_ANPLifecycleAction) {
            m_hasFocus = true;
        }

#if DEBUG_EVENTS
        SkMSec startTime = SkTime::GetMSecs();
#endif

        // make a localCopy since the actual plugin may not respect its constness,
        // and so we don't want our caller to have its param modified
        ANPEvent localCopy = evt;
        int16_t result = pkg->pluginFuncs()->event(instance, &localCopy);

#if DEBUG_EVENTS
        SkMSec endTime = SkTime::GetMSecs();
        PLUGIN_LOG_EVENT(instance, &evt, result, endTime - startTime);
#endif

        // if the plugin is losing focus then delay the update of our state
        // until after we notify the plugin and allow them to perform actions
        // that may require focus
        if (evt.eventType == kLifecycle_ANPEventType &&
                evt.data.lifecycle.action == kLoseFocus_ANPLifecycleAction) {
            m_hasFocus = false;
        }

        return result;
    }
    return 0;
}

void PluginWidgetAndroid::updateEventFlags(ANPEventFlags flags) {

    // if there are no differences then immediately return
    if (m_eventFlags == flags) {
        return;
    }

    Document* doc = m_pluginView->parentFrame()->document();
#if ENABLE(TOUCH_EVENTS)
    if((m_eventFlags ^ flags) & kTouch_ANPEventFlag) {
        if (flags & kTouch_ANPEventFlag)
           doc->addListenerTypeIfNeeded(eventNames().touchstartEvent);
    }
#endif

    m_eventFlags = flags;
}

bool PluginWidgetAndroid::isAcceptingEvent(ANPEventFlag flag) {
    return m_eventFlags & flag;
}

void PluginWidgetAndroid::sendSizeAndVisibilityEvents(const bool updateDimensions) {

    if (m_drawingModel == kOpenGL_ANPDrawingModel &&
            !m_layer->acquireNativeWindowForContent()) {
        m_drawEventDelayed = true;
        return;
    }

    // TODO update the bitmap size based on the zoom? (for kBitmap_ANPDrawingModel)
    const float zoomLevel = m_core->scale();

    // notify the plugin of the new size
    if (m_drawingModel == kOpenGL_ANPDrawingModel && updateDimensions && m_pluginWindow) {
        PLUGIN_LOG("%s (%d,%d)[%f]", __FUNCTION__, m_pluginWindow->width,
                m_pluginWindow->height, zoomLevel);
        ANPEvent event;
        SkANP::InitEvent(&event, kDraw_ANPEventType);
        event.data.draw.model = kOpenGL_ANPDrawingModel;
        event.data.draw.data.surface.width = m_pluginWindow->width * zoomLevel;
        event.data.draw.data.surface.height = m_pluginWindow->height * zoomLevel;
        sendEvent(event);
    }

    bool visible = SkIRect::Intersects(m_visibleDocRect, m_pluginBounds);
    if(m_visible != visible) {

#if DEBUG_VISIBLE_RECTS
        PLUGIN_LOG("%p changeVisiblity[%d] pluginBounds(%d,%d,%d,%d)",
                   m_pluginView->instance(), visible,
                   m_pluginBounds.fLeft, m_pluginBounds.fTop,
                   m_pluginBounds.fRight, m_pluginBounds.fBottom);
#endif

        // change the visibility
        m_visible = visible;
        // send the event
        ANPEvent event;
        SkANP::InitEvent(&event, kLifecycle_ANPEventType);
        event.data.lifecycle.action = visible ? kOnScreen_ANPLifecycleAction
                                              : kOffScreen_ANPLifecycleAction;
        sendEvent(event);
    }
}

void PluginWidgetAndroid::setVisibleScreen(const ANPRectI& visibleDocRect, float zoom) {
#if DEBUG_VISIBLE_RECTS
    PLUGIN_LOG("%s (%d,%d,%d,%d)[%f]", __FUNCTION__, visibleDocRect.left,
            visibleDocRect.top, visibleDocRect.right,
            visibleDocRect.bottom, zoom);
#endif
    int oldScreenW = m_visibleDocRect.width();
    int oldScreenH = m_visibleDocRect.height();

    const bool zoomChanged = m_cachedZoomLevel != zoom;

    // make local copies of the parameters
    m_cachedZoomLevel = zoom;
    m_visibleDocRect.set(visibleDocRect.left,
                         visibleDocRect.top,
                         visibleDocRect.right,
                         visibleDocRect.bottom);

    int newScreenW = m_visibleDocRect.width();
    int newScreenH = m_visibleDocRect.height();

    // if the screen dimensions have changed by more than 5 pixels in either
    // direction then recompute the plugin's visible rectangle
    if (abs(oldScreenW - newScreenW) > 5 || abs(oldScreenH - newScreenH) > 5) {
        PLUGIN_LOG("%s VisibleDoc old=[%d,%d] new=[%d,%d] ", __FUNCTION__,
                   oldScreenW, oldScreenH, newScreenW, newScreenH);
        computeVisiblePluginRect();
    }

    sendSizeAndVisibilityEvents(zoomChanged);
}

ANPRectI PluginWidgetAndroid::visibleRect() {

    SkIRect visibleRect;
    visibleRect.setEmpty();

    // compute the interesection of the visible screen and the plugin
    bool visible = visibleRect.intersect(m_visibleDocRect, m_pluginBounds);
    if (visible) {
        // convert from absolute coordinates to the plugin's relative coordinates
        visibleRect.offset(-m_pluginBounds.fLeft, -m_pluginBounds.fTop);
    }

    // convert from SkRect to ANPRect
    ANPRectI result;
    memcpy(&result, &visibleRect, sizeof(ANPRectI));
    return result;
}

void PluginWidgetAndroid::setVisibleRects(const ANPRectI rects[], int32_t count) {
#if DEBUG_VISIBLE_RECTS
    PLUGIN_LOG("%s count=%d", __FUNCTION__, count);
#endif
    // ensure the count does not exceed our allocated space
    if (count > MAX_REQUESTED_RECTS)
        count = MAX_REQUESTED_RECTS;

    // store the values in member variables
    m_requestedVisibleRectCount = count;
    memcpy(m_requestedVisibleRects, rects, count * sizeof(rects[0]));

#if DEBUG_VISIBLE_RECTS // FIXME: this fixes bad data from the plugin
    // take it out once plugin supplies better data
    for (int index = 0; index < count; index++) {
        PLUGIN_LOG("%s [%d](%d,%d,%d,%d)", __FUNCTION__, index,
            m_requestedVisibleRects[index].left,
            m_requestedVisibleRects[index].top,
            m_requestedVisibleRects[index].right,
            m_requestedVisibleRects[index].bottom);
        if (m_requestedVisibleRects[index].left ==
                m_requestedVisibleRects[index].right) {
            m_requestedVisibleRects[index].right += 1;
        }
        if (m_requestedVisibleRects[index].top ==
                m_requestedVisibleRects[index].bottom) {
            m_requestedVisibleRects[index].bottom += 1;
        }
    }
#endif
    computeVisiblePluginRect();
}

void PluginWidgetAndroid::computeVisiblePluginRect() {

    // ensure the visibleDocRect has been set (i.e. not equal to zero)
    if (m_visibleDocRect.isEmpty() || !m_pluginWindow || m_requestedVisibleRectCount < 1)
        return;

    // create a rect that will contain as many of the rects that will fit on screen
    SkIRect visibleRect;
    visibleRect.setEmpty();

    for (int counter = 0; counter < m_requestedVisibleRectCount; counter++) {

        ANPRectI* rect = &m_requestedVisibleRects[counter];

        // create skia rect for easier manipulation and convert it to page coordinates
        SkIRect pluginRect;
        pluginRect.set(rect->left, rect->top, rect->right, rect->bottom);
        pluginRect.offset(m_pluginWindow->x, m_pluginWindow->y);

        // ensure the rect falls within the plugin's bounds
        if (!m_pluginBounds.contains(pluginRect)) {
#if DEBUG_VISIBLE_RECTS
            PLUGIN_LOG("%s (%d,%d,%d,%d) !contain (%d,%d,%d,%d)", __FUNCTION__,
                       m_pluginBounds.fLeft, m_pluginBounds.fTop,
                       m_pluginBounds.fRight, m_pluginBounds.fBottom,
                       pluginRect.fLeft, pluginRect.fTop,
                       pluginRect.fRight, pluginRect.fBottom);
            // assume that the desired outcome is to clamp to the container
            if (pluginRect.intersect(m_pluginBounds)) {
                visibleRect = pluginRect;
            }
#endif
            continue;
        }

        // combine this new rect with the higher priority rects
        pluginRect.join(visibleRect);

        // check to see if the new rect could be made to fit within the screen
        // bounds. If this is the highest priority rect then attempt to center
        // even if it doesn't fit on the screen.
        if (counter > 0 && (m_visibleDocRect.width() < pluginRect.width() ||
                            m_visibleDocRect.height() < pluginRect.height()))
          break;

        // set the new visible rect
        visibleRect = pluginRect;
    }

    m_requestedVisibleRect = visibleRect;
    scrollToVisiblePluginRect();
}

void PluginWidgetAndroid::scrollToVisiblePluginRect() {

    if (!m_hasFocus || m_requestedVisibleRect.isEmpty() || m_visibleDocRect.isEmpty()) {
#if DEBUG_VISIBLE_RECTS
        PLUGIN_LOG("%s call m_hasFocus=%d m_requestedVisibleRect.isEmpty()=%d"
                " m_visibleDocRect.isEmpty()=%d", __FUNCTION__, m_hasFocus,
                m_requestedVisibleRect.isEmpty(), m_visibleDocRect.isEmpty());
#endif
        return;
    }
    // if the entire rect is already visible then we don't need to scroll
    if (m_visibleDocRect.contains(m_requestedVisibleRect))
        return;

    // find the center of the visibleRect in document coordinates
    int rectCenterX = m_requestedVisibleRect.fLeft + m_requestedVisibleRect.width()/2;
    int rectCenterY = m_requestedVisibleRect.fTop + m_requestedVisibleRect.height()/2;

    // position the corner of the visible doc to center the requested rect
    int scrollDocX = MAX(0, rectCenterX - (m_visibleDocRect.width()/2));
    int scrollDocY = MAX(0, rectCenterY - (m_visibleDocRect.height()/2));

    ScrollView* scrollView = m_pluginView->parent();
    android::WebViewCore* core = android::WebViewCore::getWebViewCore(scrollView);
#if DEBUG_VISIBLE_RECTS
    PLUGIN_LOG("%s call scrollTo (%d,%d) to center (%d,%d)", __FUNCTION__,
            scrollDocX, scrollDocX, rectCenterX, rectCenterY);
#endif
    core->scrollTo(scrollDocX, scrollDocX, true);
}

void PluginWidgetAndroid::requestFullScreen() {
    if (m_isFullScreen) {
        return;
    }

    if (!m_embeddedView && m_drawingModel == kOpenGL_ANPDrawingModel) {
        WebCore::PluginPackage* pkg = m_pluginView->plugin();
        NPP instance = m_pluginView->instance();

        jobject pluginSurface;
        pkg->pluginFuncs()->getvalue(instance, kJavaSurface_ANPGetValue,
                                     static_cast<void*>(&pluginSurface));

        // create the surface, but do not add it to the view hierarchy
        jobject tempObj = m_core->createSurface(pluginSurface);

        if (tempObj) {
            JNIEnv* env = JSC::Bindings::getJNIEnv();
            m_embeddedView = env->NewGlobalRef(tempObj);
            m_embeddedViewAttached = false;
        }
    }

    if (!m_embeddedView) {
        return;
    }

    // send event to notify plugin of full screen change
    ANPEvent event;
    SkANP::InitEvent(&event, kLifecycle_ANPEventType);
    event.data.lifecycle.action = kEnterFullScreen_ANPLifecycleAction;
    sendEvent(event);

    // remove the embedded surface from the view hierarchy
    if (m_drawingModel != kOpenGL_ANPDrawingModel)
        m_core->destroySurface(m_embeddedView);

    // add the full screen view
    m_core->showFullScreenPlugin(m_embeddedView, m_fullScreenOrientation,
                                 m_pluginView->instance());
    m_isFullScreen = true;
}

void PluginWidgetAndroid::exitFullScreen(bool pluginInitiated) {
    if (!m_isFullScreen || !m_embeddedView) {
        return;
    }

    // remove the full screen surface from the view hierarchy
    if (pluginInitiated) {
        m_core->hideFullScreenPlugin();
    }

    // add the embedded view back
    if (m_drawingModel != kOpenGL_ANPDrawingModel)
        m_core->updateSurface(m_embeddedView, m_pluginWindow->x, m_pluginWindow->y,
                m_pluginWindow->width, m_pluginWindow->height);

    // send event to notify plugin of full screen change
    ANPEvent event;
    SkANP::InitEvent(&event, kLifecycle_ANPEventType);
    event.data.lifecycle.action = kExitFullScreen_ANPLifecycleAction;
    sendEvent(event);

    m_isFullScreen = false;
}

void PluginWidgetAndroid::setFullScreenOrientation(ANPScreenOrientation orientation) {

    int internalOrienationId;
    /* We need to validate that the input is legitimate and then convert the
     * value from the plugin enum to the enum used by the android view system.
     * The view system values correspond to those values for the
     * screenOrientation attribute in R.java (see also ActivityInfo.java).
     */
    switch (orientation) {
        case kFixedLandscape_ANPScreenOrientation:
            internalOrienationId = 0;
            break;
        case kFixedPortrait_ANPScreenOrientation:
            internalOrienationId = 1;
            break;
        case kLandscape_ANPScreenOrientation:
            internalOrienationId = 6;
            break;
        case kPortrait_ANPScreenOrientation:
            internalOrienationId = 7;
            break;
        default:
            internalOrienationId = -1;
    }

    PLUGIN_LOG("%s orientation (%d)", __FUNCTION__, internalOrienationId);
    m_fullScreenOrientation = internalOrienationId;
}

void PluginWidgetAndroid::requestCenterFitZoom() {
    m_core->centerFitRect(m_pluginWindow->x, m_pluginWindow->y,
            m_pluginWindow->width, m_pluginWindow->height);
}

void PluginWidgetAndroid::setPowerState(ANPPowerState powerState) {
    if(m_powerState == powerState)
        return;

    // cleanup the old power state
    switch (m_powerState) {
        case kDefault_ANPPowerState:
            break;
        case kScreenOn_ANPPowerState:
            m_core->keepScreenOn(false);
            break;
    }

    // setup the new power state
    switch (powerState) {
        case kDefault_ANPPowerState:
            break;
        case kScreenOn_ANPPowerState:
            m_core->keepScreenOn(true);
            break;
    }

    m_powerState = powerState;
}

