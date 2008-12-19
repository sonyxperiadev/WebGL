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

#ifndef WEB_FRAMEVIEW_H
#define WEB_FRAMEVIEW_H

#include "WebCoreViewBridge.h"

namespace WebCore {
    class FrameView;
}

namespace android {
    class WebViewCore;

    class WebFrameView: public WebCoreViewBridge {
    public:
        WebFrameView(WebCore::FrameView* frameView, WebViewCore* webViewCore);
        virtual ~WebFrameView();

        virtual void draw(WebCore::GraphicsContext* ctx,
                const WebCore::IntRect& rect);

        WebViewCore* webViewCore() const {
            return mWebViewCore;
        }

        void setView(WebCore::FrameView* frameView);

        virtual bool forFrameView() const { return true; }

    private:
        WebCore::FrameView* mFrameView;
        WebViewCore* mWebViewCore;
    };

} // namespace android

#endif // WEB_FRAMEVIEW_H
