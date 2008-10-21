/* 
**
** Copyright 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "config.h"
#include "ScrollView.h"

#include "FloatRect.h"
#include "IntRect.h"

#include "WebCoreViewBridge.h"

#define LOG_TAG "WebCore"
#undef LOG
#include "utils/Log.h"

/*  hack to allow the DOM to communicate how to interpret inval requests, since
    it doesn't have the notion of distinguishing between the screen and the DOM
    but we do, since we have a copy of the display in our picture, and we don't
    want to rebuild the picture unless the DOM has actually been changed.
*/
bool gAndroid_treatInvalForScreen;

#define verifiedOk()    // no need to do anything in this function

/*
    This class implementation does NOT actually emulate the Qt ScrollView.
    It does provide an implementation that khtml will use to interact with
    WebKit's WebFrameView documentView and our NSScrollView subclass.

    ScrollView's view is a NSScrollView (or subclass of NSScrollView)
    in most cases. That scrollview is a subview of an
    WebCoreFrameView. The WebCoreFrameView's documentView will also be
    the scroll view's documentView.
    
    The WebCoreFrameView's size is the frame size.  The WebCoreFrameView's documentView
    corresponds to the frame content size.  The scrollview itself is autosized to the
    WebCoreFrameView's size (see Widget::resize).
*/

namespace WebCore {

struct ScrollView::ScrollViewPrivate
{
public:
    ScrollViewPrivate() :
        hasStaticBackground(false), ignoreUpdateContents(false),
        vScrollbarMode(ScrollbarAuto),
        hScrollbarMode(ScrollbarAuto) {}
    IntSize contentsSize;
    bool hasStaticBackground;
    bool ignoreUpdateContents;
    ScrollbarMode vScrollbarMode;
    ScrollbarMode hScrollbarMode;
};

ScrollView::ScrollView()
{
    m_data = new ScrollViewPrivate;
}

ScrollView::~ScrollView()
{
    delete m_data;
}

int ScrollView::visibleWidth() const
{
    LOG_ASSERT(this->getWebCoreViewBridge(), "ScrollView does not have a WebCoreViewBridge");
    return this->getWebCoreViewBridge()->width();
}

int ScrollView::visibleHeight() const
{
    LOG_ASSERT(this->getWebCoreViewBridge(), "ScrollView does not have a WebCoreViewBridge");
    return this->getWebCoreViewBridge()->height();
}

FloatRect ScrollView::visibleContentRect() const
{
    LOG_ASSERT(this->getWebCoreViewBridge(), "ScrollView does not have a WebCoreViewBridge");
    IntRect rect = this->getWebCoreViewBridge()->getBounds();
    // FIXME: This is a hack to get subframes drawing correctly. Since subframes cannot
    // scroll, we know that if this view has a parent, the visible rect is (0, 0, w, h)
    if (this->getWebCoreViewBridge()->getParent())
        return FloatRect(0, 0, rect.width(), rect.height());
    return FloatRect(rect.x(), rect.y(), rect.width(), rect.height());
}

int ScrollView::contentsWidth() const
{
    return m_data->contentsSize.width();
}

int ScrollView::contentsHeight() const
{
    return m_data->contentsSize.height();
}

int ScrollView::contentsX() const
{
    return scrollOffset().width();
}

int ScrollView::contentsY() const
{
    return scrollOffset().height();
}

IntSize ScrollView::scrollOffset() const
{
    LOG_ASSERT(this->getWebCoreViewBridge(), "ScrollView does not have a WebCoreViewBridge");
    WebCoreViewBridge* bridge = this->getWebCoreViewBridge();
    // FIXME: This is a hack to get subframes drawing correctly. Since subframes cannot
    // scroll, we know that if this view has a parent, the scroll offset is always (0, 0)
    if (bridge->getParent())
        return IntSize(0, 0);
    return IntSize(bridge->locX(), bridge->locY());
}

void ScrollView::scrollBy(int dx, int dy)
{
    LOG_ASSERT(this->getWebCoreViewBridge(), "ScrollView does not have a WebCoreViewBridge");
    this->getWebCoreViewBridge()->scrollBy(dx, dy);
}

void WebCore::ScrollView::update() 
{
    LOG_ASSERT(this->getWebCoreViewBridge(), "ScrollView does not have a WebCoreViewBridge");
    this->getWebCoreViewBridge()->contentInvalidate();
}

void WebCore::ScrollView::scrollRectIntoViewRecursively(WebCore::IntRect const& r) 
{
    LOG_ASSERT(this->getWebCoreViewBridge(), "ScrollView does not have a WebCoreViewBridge");
    int x = r.x();
    int y = r.y();
    IntPoint p(x > 0 ? x : 0, y > 0 ? y : 0);
    ScrollView* view = this;
    while (view) {
        view->setContentsPos(p.x(), p.y());
        p.move(view->x() - view->scrollOffset().width(), view->y() - view->scrollOffset().height());
        if (view->getWebCoreViewBridge()->getParent())
            view = static_cast<ScrollView*>(view->getWebCoreViewBridge()->getParent()->widget());
        else
            view = NULL;
    }
}

WebCore::FloatRect WebCore::ScrollView::visibleContentRectConsideringExternalScrollers() const {
    return FloatRect(contentsX(), contentsY(), contentsWidth(), contentsHeight()); 
}

void ScrollView::setContentsPos(int x, int y)
{
    LOG_ASSERT(this->getWebCoreViewBridge(), "ScrollView does not have a WebCoreViewBridge");
    this->getWebCoreViewBridge()->scrollTo(x, y);
}

//---------------------------------------------------------------------
// Scroll bar methods
//
// These methods are largely unimplemented meaning that the
// state of the scroll bars in the only data maintained. If
// a scroll bar is needed, the set(V|H)ScrollbarMode methods
// need to update the scroll bar widget through the java
// layer and display the new scroll bars.
// 
//---------------------------------------------------------------------

void ScrollView::setVScrollbarMode(ScrollbarMode vMode)
{
    m_data->vScrollbarMode = vMode;
}

void ScrollView::setHScrollbarMode(ScrollbarMode hMode)
{
    m_data->hScrollbarMode = hMode;
}

void ScrollView::setScrollbarsMode(ScrollbarMode mode)
{
    m_data->hScrollbarMode = mode;
    m_data->vScrollbarMode = mode;
}

ScrollbarMode ScrollView::vScrollbarMode() const
{
    return m_data->vScrollbarMode;
}

ScrollbarMode ScrollView::hScrollbarMode() const
{
    return m_data->hScrollbarMode;
}

void ScrollView::suppressScrollbars(bool suppressed,  bool repaintOnUnsuppress)
{
    verifiedOk();
}

//---------------------------------------------------------------------
// End Scroll bar methods
//---------------------------------------------------------------------

void ScrollView::addChild(Widget* child)
{
    LOG_ASSERT(this->getWebCoreViewBridge(), "ScrollView does not have a WebCoreViewBridge");
    WebCoreViewBridge* childBridge = child->getWebCoreViewBridge();
    // FIXME: For now just check this, it should be an assert down the road.
    if (!childBridge)
    {
        LOGV("childBridge is not set");
        return;
    }
    LOG_ASSERT(child != this, "Child has no view bridge or child == this!");
    WebCoreViewBridge* thisBridge = this->getWebCoreViewBridge();
    LOG_ASSERT(thisBridge && thisBridge != childBridge, "Our bridge is not set or thisBridge == childBridge!");
    LOGV("Adding parent");
    childBridge->setParent(thisBridge);
}

void ScrollView::ignoreUpdateContents(bool ignore)
{
    m_data->ignoreUpdateContents = ignore;
}

void ScrollView::removeChild(Widget* child)
{
    // FIXME: Make this only an assert once all widgets have views
    if (!child->getWebCoreViewBridge())
    {
        LOGV("child has no bridge");
        return;
    }
    LOG_ASSERT(child->getWebCoreViewBridge(), "Child has no view bridge");
    child->getWebCoreViewBridge()->setParent(NULL);
}

void ScrollView::resizeContents(int w, int h)
{
    LOG_ASSERT(this->getWebCoreViewBridge(), "ScrollView does not have a WebCoreViewBridge");
    if (w < 0)
        w = 0;
    if (h < 0)
        h = 0;

    IntSize newSize(w, h);
    m_data->contentsSize = newSize;
}

void ScrollView::updateContents(const IntRect &rect, bool now)
{
    LOG_ASSERT(this->getWebCoreViewBridge(),
               "ScrollView does not have a WebCoreViewBridge");

    WebCoreViewBridge* bridge = this->getWebCoreViewBridge();
    
    if (gAndroid_treatInvalForScreen) {
//        SkDebugf("------ contentInvalidate sent to viewInvalidate [%d %d]\n", rect.width(), rect.height());
        bridge->viewInvalidate();
    } else if (m_data->ignoreUpdateContents == false) {
        bridge->contentInvalidate(rect);
    }
}

IntPoint ScrollView::windowToContents(const IntPoint& contentsPoint) const
{
    WebCoreViewBridge* bridge = this->getWebCoreViewBridge();
    WebCoreViewBridge* parent = bridge->getParent();
    int x = 0, y= 0;
    while (parent) {
        x += bridge->locX();
        y += bridge->locY();
        bridge = parent;
        parent = bridge->getParent();
    }
    return IntPoint(contentsPoint.x() - x, contentsPoint.y() - y);
}

IntPoint ScrollView::contentsToWindow(const IntPoint& viewportPoint) const
{
    WebCoreViewBridge* bridge = this->getWebCoreViewBridge();
    WebCoreViewBridge* parent = bridge->getParent();
    int x = 0, y= 0;
    while (parent) {
        x += bridge->locX();
        y += bridge->locY();
        bridge = parent;
        parent = bridge->getParent();
    }
    return IntPoint(viewportPoint.x() + x, viewportPoint.y() + y);
}

void ScrollView::setStaticBackground(bool)
{
    // we don't have any optimizations for this
    verifiedOk();
}

bool ScrollView::inWindow() const
{
    LOGV("inWindow Unimplemented");
#if 0
    return [getView() window];
#endif
    return true;
}

void ScrollView::wheelEvent(PlatformWheelEvent&) 
{
    verifiedOk();
}

PlatformScrollbar* ScrollView::scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent) 
{
    verifiedOk();
    return NULL; 
}

}
