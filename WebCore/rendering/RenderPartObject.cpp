/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderPartObject.h"

#include "Frame.h"
#include "FrameLoaderClient.h"
#include "HTMLEmbedElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLParamElement.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "RenderView.h"
#include "RenderWidgetProtector.h"
#include "Text.h"

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "HTMLVideoElement.h"
#endif

namespace WebCore {

using namespace HTMLNames;

RenderPartObject::RenderPartObject(Element* element)
    : RenderPart(element)
{
}

void RenderPartObject::layout()
{
    ASSERT(needsLayout());

#ifdef FLATTEN_IFRAME
    RenderPart::calcWidth();
    RenderPart::calcHeight();
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
        // will disrupt layout if we force it to expand. Plus on a desktop,
        // the extra content is not accessible.
        if (scrolling || !widthIsFixed || !heightIsFixed) {
            FrameView* view = static_cast<FrameView*>(widget());
            RenderView* root = view ? view->frame()->contentRenderer() : NULL;
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
                // if the style is not a fixed value. Use the maximum value so
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
#else
    calcWidth();
    calcHeight();
#endif

    RenderPart::layout();

    m_overflow.clear();
    addShadowOverflow();

    setNeedsLayout(false);
}

#ifdef FLATTEN_IFRAME
void RenderPartObject::calcWidth() {
    RenderPart::calcWidth();
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
}

void RenderPartObject::calcHeight() {
    RenderPart::calcHeight();
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
}
#endif

void RenderPartObject::viewCleared()
{
    if (node() && widget() && widget()->isFrameView()) {
        FrameView* view = static_cast<FrameView*>(widget());
        int marginw = -1;
        int marginh = -1;
        if (node()->hasTagName(iframeTag)) {
            HTMLIFrameElement* frame = static_cast<HTMLIFrameElement*>(node());
            marginw = frame->getMarginWidth();
            marginh = frame->getMarginHeight();
        }
        if (marginw != -1)
            view->setMarginWidth(marginw);
        if (marginh != -1)
            view->setMarginHeight(marginh);
    }
}

}
