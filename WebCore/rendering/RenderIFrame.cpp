/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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
#include "RenderIFrame.h"

#include "FrameView.h"
#include "HTMLIFrameElement.h"
#include "RenderView.h"
#include "Settings.h"

namespace WebCore {

using namespace HTMLNames;
    
RenderIFrame::RenderIFrame(Element* element)
    : RenderFrameBase(element)
{
}

void RenderIFrame::calcHeight()
{
    RenderPart::calcHeight();
#ifdef ANDROID_FLATTEN_IFRAME
    if (!node()->hasTagName(iframeTag) || !widget() || !widget()->isFrameView())
        return;
    FrameView* view = static_cast<FrameView*>(widget());
    RenderView* root = static_cast<RenderView*>(view->frame()->contentRenderer());
    if (!root)
        return;
    // Do not expand if the scrollbars are suppressed and the height is fixed.
    bool scrolling = static_cast<HTMLIFrameElement*>(node())->scrollingMode() != ScrollbarAlwaysOff;
    if (!scrolling && style()->height().isFixed())
        return;
    // Update the widget
    updateWidgetPosition();

    // Layout to get the content height
    do {
        view->layout();
    } while (view->layoutPending() || root->needsLayout());

    int extraHeight = paddingTop() + paddingBottom() + borderTop() + borderBottom();
    setHeight(max(width(), view->contentsHeight() + extraHeight));

    // Update one last time to ensure the dimensions.
    updateWidgetPosition();
    return;
#endif
    if (!flattenFrame())
         return;

    HTMLIFrameElement* frame = static_cast<HTMLIFrameElement*>(node());
    bool isScrollable = frame->scrollingMode() != ScrollbarAlwaysOff;

    if (isScrollable || !style()->height().isFixed()) {
        FrameView* view = static_cast<FrameView*>(widget());
        int border = borderTop() + borderBottom();
        setHeight(max(height(), view->contentsHeight() + border));
    }
}

void RenderIFrame::calcWidth()
{
    RenderPart::calcWidth();
#ifdef ANDROID_FLATTEN_IFRAME
    if (!node()->hasTagName(iframeTag) || !widget() || !widget()->isFrameView())
        return;
    FrameView* view = static_cast<FrameView*>(widget());
    RenderView* root = static_cast<RenderView*>(view->frame()->contentRenderer());
    if (!root)
        return;
    // Do not expand if the scrollbars are suppressed and the width is fixed.
    bool scrolling = static_cast<HTMLIFrameElement*>(node())->scrollingMode() != ScrollbarAlwaysOff;
    if (!scrolling && style()->width().isFixed())
        return;
    // Update the dimensions to get the correct minimum preferred
    // width
    updateWidgetPosition();

    int extraWidth = paddingLeft() + paddingRight() + borderLeft() + borderRight();
    // Set the width
    setWidth(max(width(), root->minPrefWidth()) + extraWidth);

    // Update based on the new width
    updateWidgetPosition();

    // Layout to get the content width
    do {
        view->layout();
    } while (view->layoutPending() || root->needsLayout());

    setWidth(max(width(), view->contentsWidth() + extraWidth));

    // Update one last time to ensure the dimensions.
    updateWidgetPosition();
    return;
#endif
    if (!flattenFrame())
        return;

    HTMLIFrameElement* frame = static_cast<HTMLIFrameElement*>(node());
    bool isScrollable = frame->scrollingMode() != ScrollbarAlwaysOff;

    if (isScrollable || !style()->width().isFixed()) {
        FrameView* view = static_cast<FrameView*>(widget());
        int border = borderLeft() + borderRight();
        setWidth(max(width(), view->contentsWidth() + border));
    }
}

bool RenderIFrame::flattenFrame()
{
    if (!node() || !node()->hasTagName(iframeTag))
        return false;

    HTMLIFrameElement* element = static_cast<HTMLIFrameElement*>(node());
    bool isScrollable = element->scrollingMode() != ScrollbarAlwaysOff;

    if (!isScrollable && style()->width().isFixed()
        && style()->height().isFixed())
        return false;

    Frame* frame = element->document()->frame();
    bool enabled = frame && frame->settings()->frameFlatteningEnabled();

    if (!enabled || !frame->page())
        return false;

    FrameView* view = frame->page()->mainFrame()->view();
    if (!view)
        return false;

    // Do not flatten offscreen inner frames during frame flattening.
    return absoluteBoundingBoxRect().intersects(IntRect(IntPoint(0, 0), view->contentsSize()));
}

void RenderIFrame::layout()
{
    ASSERT(needsLayout());

    RenderPart::calcWidth();
    RenderPart::calcHeight();

#ifdef ANDROID_FLATTEN_IFRAME
    // Calculate the styled dimensions by subtracting the border and padding.
    int extraWidth = paddingLeft() + paddingRight() + borderLeft() + borderRight();
    int extraHeight = paddingTop() + paddingBottom() + borderTop() + borderBottom();
    int styleWidth = width() - extraWidth;
    int styleHeight = height() - extraHeight;
    // Some IFrames have a width and/or height of 1 when they are meant to be
    // hidden. If that is the case, do not try to expand.
    if (node()->hasTagName(iframeTag) && widget() && widget()->isFrameView() &&
            styleWidth > 1 && styleHeight > 1) {
        HTMLIFrameElement* element = static_cast<HTMLIFrameElement*>(node());
        bool scrolling = element->scrollingMode() != ScrollbarAlwaysOff;
        bool widthIsFixed = style()->width().isFixed();
        bool heightIsFixed = style()->height().isFixed();
        // If an iframe has a fixed dimension and suppresses scrollbars, it
        // will disrupt layout if we force it to expand.  Plus on a desktop,
        // the extra content is not accessible.
        if (scrolling || !widthIsFixed || !heightIsFixed) {
            FrameView* view = static_cast<FrameView*>(widget());
            RenderView* root = view ?  view->frame()->contentRenderer() : NULL;
            if (root && style()->visibility() != HIDDEN) {
                // Update the dimensions to get the correct minimum preferred
                // width
                updateWidgetPosition();

                // Use the preferred width if it is larger and only if
                // scrollbars are visible or the width style is not fixed.
                if (scrolling || !widthIsFixed)
                    setWidth(max(width(), root->minPrefWidth()) + extraWidth);

                // Resize the view to recalc the height.
                int h = height() - extraHeight;
                int w = width() - extraWidth;
                if (w > view->width())
                    h = 0;
                if (w != view->width() || h != view->height()) {
                    view->resize(w, h);
                }

                // Layout the view.
                do {
                    view->layout();
                } while (view->layoutPending() || root->needsLayout());

                int contentHeight = view->contentsHeight();
                int contentWidth = view->contentsWidth();
                // Only change the width or height if scrollbars are visible or
                // if the style is not a fixed value.  Use the maximum value so
                // that iframes never shrink.
                if (scrolling || !heightIsFixed)
                    setHeight(max(height(), contentHeight + extraHeight));
                if (scrolling || !widthIsFixed)
                    setWidth(max(width(), contentWidth + extraWidth));

                // Update one last time
                updateWidgetPosition();

                // Layout one more time to ensure all objects have the correct
                // height.
                view->layout();

#if !ASSERT_DISABLED
                ASSERT(!view->layoutPending());
                ASSERT(!root->needsLayout());
                // Sanity check when assertions are enabled.
                RenderObject* c = root->nextInPreOrder();
                while (c) {
                    ASSERT(!c->needsLayout());
                    c = c->nextInPreOrder();
                }
#endif
            }
        }
    }
#endif
    if (flattenFrame()) {
        layoutWithFlattening(style()->width().isFixed(), style()->height().isFixed());
        return;
    }

    RenderPart::layout();

    m_overflow.clear();
    addShadowOverflow();

    setNeedsLayout(false);
}

#if USE(ACCELERATED_COMPOSITING)
bool RenderIFrame::requiresLayer() const
{
    if (RenderPart::requiresLayer())
        return true;
    
    return requiresAcceleratedCompositing();
}

bool RenderIFrame::requiresAcceleratedCompositing() const
{
    if (!node() || !node()->hasTagName(iframeTag))
        return false;

    // If the contents of the iframe are composited, then we have to be as well.
    HTMLIFrameElement* element = static_cast<HTMLIFrameElement*>(node());
    if (Document* contentDocument = element->contentDocument()) {
        if (RenderView* view = contentDocument->renderView())
            return view->usesCompositing();
    }

    return false;
}
#endif

}
