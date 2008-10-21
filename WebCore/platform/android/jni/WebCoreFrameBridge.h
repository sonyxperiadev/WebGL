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

#ifndef WEBCOREFRAMEBRIDGE_H
#define WEBCOREFRAMEBRIDGE_H

#include "FrameLoaderClient.h"
#include "PlatformString.h"
#include "WebCoreRefObject.h"
#include <jni.h>

namespace WebCore {
    class FrameAndroid;
    class HistoryItem;
    class Image;
    class RenderPart;
    class ResourceHandle;
    class ResourceRequest;
}

class WebCoreViewBridge;

namespace android {

class WebCoreResourceLoader;

class WebCoreFrameBridge : public WebCoreRefObject {
  public:
    WebCoreFrameBridge(JNIEnv* env, jobject obj, jobject historyList);
    ~WebCoreFrameBridge();

    WebCoreResourceLoader* startLoadingResource(WebCore::ResourceHandle*,
            const WebCore::ResourceRequest& request,
            bool isHighPriority,
            bool synchronous);

    void reportError(int errorCode, const WebCore::String& description,
            const WebCore::String& failingUrl);
    
    void loadStarted(WebCore::FrameAndroid* frame);

    void transitionToCommitted(WebCore::Frame* frame);

    void didFinishLoad(WebCore::Frame* frame);

    void addHistoryItem(WebCore::HistoryItem* item);

    void removeHistoryItem(int index);

    void updateHistoryIndex(int newIndex);

    void setTitle(const WebCore::String& title);

    void windowObjectCleared(WebCore::FrameAndroid* frame);
	
	void setProgress(float newProgress);

    const WebCore::String userAgentForURL(const WebCore::KURL* url);

    void didReceiveIcon(WebCore::Image* icon);
    
    void updateVisitedHistory(const WebCore::KURL& url, bool reload);

    bool canHandleRequest(const WebCore::ResourceRequest& request);

    WebCore::Frame* createWindow(bool dialog, bool userGesture);

    void requestFocus() const;

    void closeWindow(WebCoreViewBridge* viewBridge);

    void decidePolicyForFormResubmission(WebCore::FramePolicyFunction func);

    void setUserAgent(WebCore::String userAgent) { mUserAgent = userAgent; }

    /**
     * This function is called during a key event so that canHandleRequest can
     * avoid asking the application to override the url loading. If a load is
     * due to a key event, then we ask the application if it wants to override
     * the load. Otherwise, we attempt to load the resource internally.
     */
    void setInKeyHandler(bool inKeyHandler) { mInKeyHandler = inKeyHandler; }
    
  private:
    struct JavaBrowserFrame;
    JavaBrowserFrame* mJavaFrame;
    WebCore::String mUserAgent;
    bool mInKeyHandler;
};

}   // namespace android

#endif // WEBCOREFRAMEBRIDGE_H
