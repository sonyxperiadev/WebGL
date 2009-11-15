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
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "PluginPackage.h"
#include "PluginView.h"
#include "PluginWidgetAndroid.h"
#include "ScrollView.h"
#include "SkANP.h"
#include "SkFlipPixelRef.h"
#include "SkString.h"
#include "WebViewCore.h"

PluginWidgetAndroid::PluginWidgetAndroid(WebCore::PluginView* view)
        : m_pluginView(view) {
    m_flipPixelRef = NULL;
    m_core = NULL;
    m_drawingModel = kBitmap_ANPDrawingModel;
    m_eventFlags = 0;
    m_pluginWindow = NULL;
    m_requestedVisibleRectCount = 0;
    m_requestedFrameRect.setEmpty();
    m_visibleDocRect.setEmpty();
    m_hasFocus = false;
    m_zoomLevel = 0;
    m_javaClassName = NULL;
    m_childView = NULL;
}

PluginWidgetAndroid::~PluginWidgetAndroid() {
    if (m_core) {
        m_core->removePlugin(this);
        if (m_childView) {
            m_core->destroySurface(m_childView);
        }
    }
    if (m_javaClassName) {
        free(m_javaClassName);
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

void PluginWidgetAndroid::setWindow(NPWindow* window, bool isTransparent) {
    m_pluginWindow = window;

    if (m_drawingModel == kSurface_ANPDrawingModel) {
        if (!m_childView) {
            IntPoint docPoint = frameToDocumentCoords(window->x, window->y);

            const String& libName = m_pluginView->plugin()->path();
            SkString skLibName;
            skLibName.setUTF16(libName.characters(), libName.length());

            m_childView = m_core->createSurface(skLibName.c_str(), m_javaClassName,
                                                m_pluginView->instance(),
                                                docPoint.x(), docPoint.y(),
                                                window->width, window->height);
        }
    } else {
        m_flipPixelRef->safeUnref();
        m_flipPixelRef = new SkFlipPixelRef(computeConfig(isTransparent),
                                            window->width, window->height);
    }
}

bool PluginWidgetAndroid::setPluginStubJavaClassName(const char* className) {

    if (m_javaClassName) {
        free(m_javaClassName);
    }

    // don't call strdup() if the className is to be set to NULL
    if (!className) {
        m_javaClassName = NULL;
        return true;
    }

    // make a local copy of the className
    m_javaClassName = strdup(className);
    return (m_javaClassName != NULL);
}

void PluginWidgetAndroid::requestFullScreenMode() {

    if (!m_javaClassName) {
        return;
    }

    const String& libName = m_pluginView->plugin()->path();
    SkString skLibName;
    skLibName.setUTF16(libName.characters(), libName.length());

    m_core->startFullScreenPluginActivity(skLibName.c_str(), m_javaClassName,
                                          m_pluginView->instance());
}

bool PluginWidgetAndroid::setDrawingModel(ANPDrawingModel model) {

    // disallow the surface drawing model if no java class name has been given
    if (model == kSurface_ANPDrawingModel && m_javaClassName == NULL) {
        return false;
    }

    m_drawingModel = model;
    return true;
}

void PluginWidgetAndroid::localToDocumentCoords(SkIRect* rect) const {
    if (m_pluginWindow) {
        IntPoint pluginDocCoords = frameToDocumentCoords(m_pluginWindow->x,
                                                         m_pluginWindow->y);
        rect->offset(pluginDocCoords.x(), pluginDocCoords.y());
    }
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
                    canvas->drawBitmap(bm, SkIntToScalar(m_pluginWindow->x),
                                           SkIntToScalar(m_pluginWindow->y), NULL);
                }
            }
            break;
        }
        default:
            break;
    }
}

bool PluginWidgetAndroid::sendEvent(const ANPEvent& evt) {
    WebCore::PluginPackage* pkg = m_pluginView->plugin();
    NPP instance = m_pluginView->instance();
    // "missing" plugins won't have these
    if (pkg && instance) {

        // keep track of whether or not the plugin currently has focus
        if (evt.eventType == kLifecycle_ANPEventType) {
           if (evt.data.lifecycle.action == kLoseFocus_ANPLifecycleAction)
               m_hasFocus = false;
           else if (evt.data.lifecycle.action == kGainFocus_ANPLifecycleAction)
               m_hasFocus = true;
        }

        // make a localCopy since the actual plugin may not respect its constness,
        // and so we don't want our caller to have its param modified
        ANPEvent localCopy = evt;
        return pkg->pluginFuncs()->event(instance, &localCopy);
    }
    return false;
}

void PluginWidgetAndroid::updateEventFlags(ANPEventFlags flags) {

    // if there are no differences then immediately return
    if (m_eventFlags == flags) {
        return;
    }

    Document* doc = m_pluginView->getParentFrame()->document();
    if((m_eventFlags ^ flags) & kTouch_ANPEventFlag) {
        if(flags & kTouch_ANPEventFlag)
            doc->addTouchEventListener(m_pluginView->getElement());
        else
            doc->removeTouchEventListener(m_pluginView->getElement());
    }

    m_eventFlags = flags;
}

bool PluginWidgetAndroid::isAcceptingEvent(ANPEventFlag flag) {
    return m_eventFlags & flag;
}

void PluginWidgetAndroid::setVisibleScreen(const ANPRectI& visibleDocRect, float zoom) {

    // TODO update the bitmap size based on the zoom? (for kBitmap_ANPDrawingModel)

    int oldScreenW = m_visibleDocRect.width();
    int oldScreenH = m_visibleDocRect.height();

    m_visibleDocRect.set(visibleDocRect.left, visibleDocRect.top,
                         visibleDocRect.right, visibleDocRect.bottom);

    int newScreenW = m_visibleDocRect.width();
    int newScreenH = m_visibleDocRect.height();

    if (oldScreenW != newScreenW || oldScreenH != newScreenH)
        computeVisibleFrameRect();
}

void PluginWidgetAndroid::setVisibleRects(const ANPRectI rects[], int32_t count) {

    // ensure the count does not exceed our allocated space
    if (count > MAX_REQUESTED_RECTS)
        count = MAX_REQUESTED_RECTS;

    // store the values in member variables
    m_requestedVisibleRectCount = count;
    memcpy(m_requestedVisibleRect, rects, count * sizeof(rects[0]));

    computeVisibleFrameRect();
}

void PluginWidgetAndroid::computeVisibleFrameRect() {

    // ensure the visibleDocRect has been set (i.e. not equal to zero)
    if (m_visibleDocRect.isEmpty() || !m_pluginWindow)
        return;

    // create a rect that represents the plugin's bounds
    SkIRect pluginBounds;
    pluginBounds.set(m_pluginWindow->x, m_pluginWindow->y,
                     m_pluginWindow->x + m_pluginWindow->width,
                     m_pluginWindow->y + m_pluginWindow->height);

    // create a rect that will contain as many of the rects that will fit on screen
    SkIRect visibleRect;
    visibleRect.setEmpty();

    for (int counter = 0; counter < m_requestedVisibleRectCount; counter++) {

        ANPRectI* rect = &m_requestedVisibleRect[counter];

        // create skia rect for easier manipulation and convert it to frame coordinates
        SkIRect pluginRect;
        pluginRect.set(rect->left, rect->top, rect->right, rect->bottom);
        pluginRect.offset(m_pluginWindow->x, m_pluginWindow->y);

        // ensure the rect falls within the plugin's bounds
        if (!pluginBounds.contains(pluginRect))
          continue;

        // combine this new rect with the higher priority rects
        pluginRect.join(visibleRect);

        // check to see if the new rect fits within the screen bounds. If this
        // is the highest priority rect then attempt to center even if it doesn't
        // fit on the screen.
        if (counter > 0 && (m_visibleDocRect.width() < pluginRect.width() ||
                               m_visibleDocRect.height() < pluginRect.height()))
          break;

        // set the new visible rect
        visibleRect = pluginRect;
    }

    m_requestedFrameRect = visibleRect;
    scrollToVisibleFrameRect();
}

void PluginWidgetAndroid::scrollToVisibleFrameRect() {

    if (!m_hasFocus || m_requestedFrameRect.isEmpty() || m_visibleDocRect.isEmpty())
        return;

    // if the entire rect is already visible then we don't need to scroll, which
    // requires converting the m_requestedFrameRect from frame to doc coordinates
    IntPoint pluginDocPoint = frameToDocumentCoords(m_requestedFrameRect.fLeft,
                                                    m_requestedFrameRect.fTop);
    SkIRect requestedDocRect;
    requestedDocRect.set(pluginDocPoint.x(), pluginDocPoint.y(),
                         pluginDocPoint.x() + m_requestedFrameRect.width(),
                         pluginDocPoint.y() + m_requestedFrameRect.height());

    if (m_visibleDocRect.contains(requestedDocRect))
        return;

    // find the center of the visibleRect in document coordinates
    int rectCenterX = requestedDocRect.fLeft + requestedDocRect.width()/2;
    int rectCenterY = requestedDocRect.fTop + requestedDocRect.height()/2;

    // find document coordinates for center of the visible screen
    int screenCenterX = m_visibleDocRect.fLeft + m_visibleDocRect.width()/2;
    int screenCenterY = m_visibleDocRect.fTop + m_visibleDocRect.height()/2;

    //compute the delta of the two points
    int deltaX = rectCenterX - screenCenterX;
    int deltaY = rectCenterY - screenCenterY;

    ScrollView* scrollView = m_pluginView->parent();
    android::WebViewCore* core = android::WebViewCore::getWebViewCore(scrollView);
    core->scrollBy(deltaX, deltaY, true);
}

IntPoint PluginWidgetAndroid::frameToDocumentCoords(int frameX, int frameY) const {
    IntPoint docPoint = IntPoint(frameX, frameY);

    const ScrollView* currentScrollView = m_pluginView->parent();
    if (currentScrollView) {
        const ScrollView* parentScrollView = currentScrollView->parent();
        while (parentScrollView) {

            docPoint.move(currentScrollView->x(), currentScrollView->y());

            currentScrollView = parentScrollView;
            parentScrollView = parentScrollView->parent();
        }
    }

    return docPoint;
}
