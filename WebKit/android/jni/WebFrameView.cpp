/*
 *
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "webcoreglue"

#include <config.h>
#include "WebFrameView.h"

#include "android_graphics.h"
#include "GraphicsContext.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HostWindow.h"
#include "PlatformGraphicsContext.h"
#include "WebViewCore.h"

#include <SkCanvas.h>

namespace android {

WebFrameView::WebFrameView(WebCore::FrameView* frameView, WebViewCore* webViewCore)
    : WebCoreViewBridge()
    , mFrameView(frameView)
    , mWebViewCore(webViewCore) {
    // attach itself to mFrameView
    mFrameView->setPlatformWidget(this);
    Retain(mWebViewCore);
}

WebFrameView::~WebFrameView() {
    Release(mWebViewCore);
}

void WebFrameView::draw(WebCore::GraphicsContext* ctx, const WebCore::IntRect& rect) {
    WebCore::Frame* frame = mFrameView->frame();

    if (NULL == frame->contentRenderer()) {
        // We only do this if there is nothing else to draw.
        // If there is a renderer, it will fill the bg itself, so we don't want to
        // double-draw (slow)
        SkCanvas* canvas = ctx->platformContext()->mCanvas;
        canvas->drawColor(SK_ColorWHITE);
    } else if (frame->tree()->parent()) {
        // Note: this code was moved from FrameLoaderClientAndroid
        //
        // For subframe, create a new translated rect from the given rectangle.
        WebCore::IntRect transRect(rect);
        // In Frame::markAllMatchesForText(), it does a fake paint.  So we need
        // to handle the case where platformContext() is null.  However, we still
        // want to call paint, since WebKit must have called the paint for a reason.
        SkCanvas* canvas = ctx->platformContext() ? ctx->platformContext()->mCanvas : NULL;
        if (canvas) {
            const WebCore::IntRect& bounds = getBounds();

            // Grab the intersection of transRect and the frame's bounds.
            transRect.intersect(bounds);
            if (transRect.isEmpty())
                return;

            // Move the transRect into the frame's local coordinates.
            transRect.move(-bounds.x(), -bounds.y());

            // Translate the canvas, add a clip.
            SkRect r;
            android_setrect(&r, transRect);

            canvas->save();
            canvas->translate(SkIntToScalar(bounds.x()), SkIntToScalar(bounds.y()));
            canvas->clipRect(r);
        }
        mFrameView->paintContents(ctx, transRect);
        if (canvas)
            canvas->restore();
    } else {
        mFrameView->paintContents(ctx, rect);
    }
}

void WebFrameView::setView(WebCore::FrameView* frameView) {
    mFrameView = frameView;
    mFrameView->setPlatformWidget(this);
}

}   // namespace android

