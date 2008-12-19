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
#define LOG_TAG "WebCore"

#include "config.h"
#include "ScrollView.h"

#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "HostWindow.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreViewBridge.h"
#include "WebViewCore.h"

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

IntRect ScrollView::platformVisibleContentRect(bool includeScrollbars) const
{
    IntRect rect = platformWidget()->getBounds();
    // This makes subframes draw correctly, since subframes cannot scroll.
    if (parent())
        return IntRect(0, 0, rect.width(), rect.height());
    return rect;
}

IntSize ScrollView::platformContentsSize() const
{
    return m_contentsSize;
}

void ScrollView::platformSetScrollPosition(const WebCore::IntPoint& pt)
{
    if (parent()) // don't attempt to scroll subframes; they're fully visible
        return;
    android::WebViewCore::getWebViewCore(this)->scrollTo(pt.x(), pt.y());
}

void ScrollView::platformScrollbarModes(ScrollbarMode& h, ScrollbarMode& v) const
{
    h = v = ScrollbarAlwaysOff;
}

bool ScrollView::platformProhibitsScrolling()
{
    if (!isFrameView())
        return false;
    FrameView* view = static_cast<FrameView*>(this);
    // We want to ignore requests to scroll that were not initiated by the user.  An
    // example of this is when text is inserted into a textfield/area, which results in
    // a scroll.  We ignore this because we now how to do this ourselves in the UI thread.
    // An example of it being initiated by the user is if the user clicks an anchor
    // element which simply scrolls the page.
    return !android::WebFrame::getWebFrame(view->frame())->userInitiatedClick();
}

void ScrollView::platformRepaintContentRectangle(const IntRect &rect, bool now)
{
    android::WebViewCore::getWebViewCore(this)->contentInvalidate(rect);
}

#ifdef ANDROID_CAPTURE_OFFSCREEN_PAINTS
void ScrollView::platformOffscreenContentRectangle(const IntRect& rect)
{
    android::WebViewCore::getWebViewCore(this)->offInvalidate(rect);
}
#endif

}
