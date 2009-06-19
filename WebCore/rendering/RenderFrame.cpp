/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
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
#include "RenderFrame.h"

#include "FrameView.h"
#include "HTMLFrameElement.h"

#ifdef FLATTEN_FRAMESET
#include "Frame.h"
#include "Document.h"
#include "RenderView.h"
#endif

namespace WebCore {

RenderFrame::RenderFrame(HTMLFrameElement* frame)
    : RenderPart(frame)
{
    setInline(false);
}

FrameEdgeInfo RenderFrame::edgeInfo() const
{
    HTMLFrameElement* element = static_cast<HTMLFrameElement*>(node());
    return FrameEdgeInfo(element->noResize(), element->hasFrameBorder());
}

void RenderFrame::viewCleared()
{
    HTMLFrameElement* element = static_cast<HTMLFrameElement*>(node());
    if (!element || !widget() || !widget()->isFrameView())
        return;

    FrameView* view = static_cast<FrameView*>(widget());

    int marginw = element->getMarginWidth();
    int marginh = element->getMarginHeight();

    if (marginw != -1)
        view->setMarginWidth(marginw);
    if (marginh != -1)
        view->setMarginHeight(marginh);
}

#ifdef FLATTEN_FRAMESET
void RenderFrame::layout()
{
    if (widget() && widget()->isFrameView()) {
        FrameView* view = static_cast<FrameView*>(widget());
        RenderView* root = NULL;
        if (view->frame() && view->frame()->document() && 
            view->frame()->document()->renderer() &&
            view->frame()->document()->renderer()->isRenderView())
            root = static_cast<RenderView*>(view->frame()->document()->renderer());
        if (root) {
            // Resize the widget so that the RenderView will layout according to those dimensions.
            view->resize(width(), height());
            view->layout();
            // We can only grow in width and height because if positionFrames gives us a width and we become smaller,
            // then the fixup process of forcing the frame to fill extra space will fail.
            if (width() > root->docWidth()) {
                view->resize(root->docWidth(), 0);
                view->layout();
            }
            // Honor the height set by RenderFrameSet::positionFrames unless our document height is larger.
            setHeight(max(root->docHeight(), height()));
            setWidth(max(root->docWidth(), width()));
        }
    }
    setNeedsLayout(false);
}
#endif

} // namespace WebCore
