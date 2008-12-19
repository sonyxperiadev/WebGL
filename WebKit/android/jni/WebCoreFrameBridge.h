/*
** Copyright 2006-2008, The Android Open Source Project
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

// TODO: change name to WebFrame.h

#ifndef WEBFRAME_H
#define WEBFRAME_H

#include "FrameLoaderClient.h"
#include "PlatformString.h"
#include "WebCoreRefObject.h"
#include <jni.h>

namespace WebCore {
    class HistoryItem;
    class Image;
    class Page;
    class RenderPart;
    class ResourceHandle;
    class ResourceRequest;
}

namespace android {

class WebCoreResourceLoader;
class WebViewCore;

// one instance of WebFrame per Page for calling into Java's BrowserFrame
class WebFrame : public WebCoreRefObject {
  public:
    // these ids need to be in sync with the constants in BrowserFrame.java
    enum RAW_RES_ID {
        NODOMAIN = 1,
        LOADERROR,
    };
    WebFrame(JNIEnv* env, jobject obj, jobject historyList, WebCore::Page* page);
    ~WebFrame();

    // helper function
    static WebFrame* getWebFrame(const WebCore::Frame* frame);

    WebCoreResourceLoader* startLoadingResource(WebCore::ResourceHandle*,
            const WebCore::ResourceRequest& request,
            bool isHighPriority,
            bool synchronous);

    void reportError(int errorCode, const WebCore::String& description,
            const WebCore::String& failingUrl);

    void loadStarted(WebCore::Frame* frame);

    void transitionToCommitted(WebCore::Frame* frame);

    void didFinishLoad(WebCore::Frame* frame);

    void addHistoryItem(WebCore::HistoryItem* item);

    void removeHistoryItem(int index);

    void updateHistoryIndex(int newIndex);

    void setTitle(const WebCore::String& title);

    void windowObjectCleared(WebCore::Frame* frame);

	void setProgress(float newProgress);

    const WebCore::String userAgentForURL(const WebCore::KURL* url);

    void didReceiveIcon(WebCore::Image* icon);
    
    void updateVisitedHistory(const WebCore::KURL& url, bool reload);

    bool canHandleRequest(const WebCore::ResourceRequest& request);

    WebCore::Frame* createWindow(bool dialog, bool userGesture);

    void requestFocus() const;

    void closeWindow(WebViewCore* webViewCore);

    void decidePolicyForFormResubmission(WebCore::FramePolicyFunction func);

    void setUserAgent(WebCore::String userAgent) { mUserAgent = userAgent; }

    WebCore::String getRawResourceFilename(RAW_RES_ID) const;

    /**
     * When the user initiates a click (via trackball, enter-press, or touch),
     * we set mUserInitiatedClick to true.  If a load happens due to this click,
     * then we ask the application if it wants to override
     * the load. Otherwise, we attempt to load the resource internally.
     * We also check it to determine whether or not to allow webkit to request
     * a scroll.  If it was user initated, the scroll is allowed.
     */
    void setUserInitiatedClick(bool userInitiatedClick) { mUserInitiatedClick = userInitiatedClick; }

    bool userInitiatedClick() { return mUserInitiatedClick; }
    
    WebCore::Page* page() const { return mPage; }

  private:
    struct JavaBrowserFrame;
    JavaBrowserFrame* mJavaFrame;
    WebCore::Page* mPage;
    WebCore::String mUserAgent;
    bool mUserInitiatedClick;
};

}   // namespace android

#endif // WEBFRAME_H
