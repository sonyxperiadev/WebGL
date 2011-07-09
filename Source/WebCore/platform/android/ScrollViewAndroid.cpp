/*
 * Copyright 2007, The Android Open Source Project
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
#define LOG_TAG "WebCore"

#include "config.h"
#include "ScrollView.h"

#include "FloatRect.h"
#include "FrameView.h"
#include "IntRect.h"
#include "PlatformBridge.h"
#include "SkRegion.h"
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
    // iframe's visible content rect is relative to its parent, not the viewport.
    // As we auto expand the iframe, the frame rect is the content rect.
    if (parent())
        return IntRect(0, 0, width(), height());
    else
        return platformWidget()->getVisibleBounds();
}

IntSize ScrollView::platformContentsSize() const
{
    return m_contentsSize;
}

int ScrollView::platformActualWidth() const
{
    if (parent())
        return width();
    return platformWidget()->visibleWidth();
}

int ScrollView::platformActualHeight() const
{
    if (parent())
        return height();
    return platformWidget()->visibleHeight();
}

int ScrollView::platformActualScrollX() const
{
    if (parent())
        return scrollX();
    return platformWidget()->visibleX();
}

int ScrollView::platformActualScrollY() const
{
    if (parent())
        return scrollY();
    return platformWidget()->visibleY();
}

void ScrollView::platformSetScrollPosition(const WebCore::IntPoint& pt)
{
    if (parent()) // don't attempt to scroll subframes; they're fully visible
        return;
    PlatformBridge::setScrollPosition(this, pt.x(), pt.y());
}

void ScrollView::platformSetScrollbarModes()
{
    if (parent()) // no scrollbar for the subframes
        return;
    android::WebViewCore::getWebViewCore(this)->setScrollbarModes(m_horizontalScrollbarMode, m_verticalScrollbarMode);
}

void ScrollView::platformScrollbarModes(ScrollbarMode& h, ScrollbarMode& v) const
{
    // m_horizontalScrollbarMode and m_verticalScrollbarMode are set in ScrollView::setScrollbarModes()
    h = m_horizontalScrollbarMode;
    v = m_verticalScrollbarMode;
}

void ScrollView::platformRepaintContentRectangle(const IntRect &rect, bool now)
{
    android::WebViewCore::getWebViewCore(this)->contentInvalidate(rect);
}

#ifdef ANDROID_CAPTURE_OFFSCREEN_PAINTS
//  Compute the offscreen parts of the drawn rectangle by subtracting
//  vis from rect. This can compute up to four rectangular slices.
void ScrollView::platformOffscreenContentRectangle(const IntRect& vis, const IntRect& rect)
{
    android::WebViewCore* core = android::WebViewCore::getWebViewCore(this);
    if (!core) // SVG does not instantiate webviewcore
        return; // and doesn't need to record drawing offscreen
    SkRegion rectRgn = SkRegion(rect);
    rectRgn.op(vis, SkRegion::kDifference_Op);
    SkRegion::Iterator iter(rectRgn);
    for (; !iter.done(); iter.next()) {
        const SkIRect& diff = iter.rect();
        core->offInvalidate(diff);
    }
}
#endif

bool ScrollView::platformIsOffscreen() const
{
    /* other platforms override platformIsOffscreen when the browser
       window is no longer on screen. We override it to prevent gif
       animations from queuing up subsequent frames during dragging. */
    return android::WebViewCore::getWebViewCore(this)->drawIsPaused();
}

} // namespace WebCore
