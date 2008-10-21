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

#ifndef WEBCORE_VIEW_BRIDGE_H
#define WEBCORE_VIEW_BRIDGE_H

#include "IntRect.h"
#include "SkTDArray.h"
#include "WebCoreRefObject.h"
#include "Widget.h"
#include <ui/KeycodeLabels.h>
#include <stdlib.h>
namespace WebCore
{
    class GraphicsContext;
    class Node;
    class StringImpl;
    class String;
    class RenderText;
    class Frame;
    class FrameView;
}

class WebCoreReply : public WebCoreRefObject {
public:
    virtual ~WebCoreReply() {}
    
    virtual void replyInt(int value) {
        SkDEBUGF(("WebCoreReply::replyInt(%d) not handled\n", value));
    }

    virtual void replyIntArray(SkTDArray<int> array) {
        SkDEBUGF(("WebCoreReply::replyIntArray() not handled\n"));
    }
        // add more replyFoo signatures as needed
};

class WebCoreViewBridge : public WebCoreRefObject {
public:
    WebCoreViewBridge(): mBounds(0,0,0,0), mScreenWidth(0), mScale(100),
        mWidget(NULL), mParent(NULL) {}
    virtual ~WebCoreViewBridge() { Release(mParent); }
    
    virtual void setParent(WebCoreViewBridge* parent) {Release(mParent); mParent = parent; Retain(mParent); }
    virtual WebCoreViewBridge* getParent() { return mParent; }

    // these are needed for WidgetAndroid.cpp
    virtual void draw(WebCore::GraphicsContext* ctx, 
        const WebCore::IntRect& rect, bool invalCache) = 0;
    virtual void layout() {}
    virtual bool isEnabled() const { return true; }
    virtual void setEnabled(bool) {}    
    virtual bool hasFocus() const { return true; }
    virtual void setFocus(bool) {}
    virtual void didFirstLayout() {}
    virtual void restoreScale(int) {}

    // Used by the page cache
    virtual void setView(WebCore::FrameView* view) {}

    const WebCore::IntRect& getBounds() const 
    {
        return mBounds;
    }
    virtual void setBounds(int left, int top, int right, int bottom) 
    {
        this->setLocation(left, top);
        this->setSize(right - left, bottom - top);
    }
    virtual int getMaxXScroll() const { return width() >> 2; }
    virtual int getMaxYScroll() const { return height() >> 2; }
    virtual void notifyFocusSet() {}
    virtual void notifyProgressFinished() {}
    // Subclasses should implement this if they want to do something after being resized
    virtual void onResize() {}
    
    // These are referenced by Scrollview (and others)
    virtual bool scrollIntoView(WebCore::IntRect rect, bool force) { return false; }
    virtual void scrollTo(int x, int y, bool animate=false) {}
    virtual void scrollBy(int x, int y) {}
    virtual void contentInvalidate(const WebCore::IntRect &rect)
    {
        if (mParent)
            mParent->contentInvalidate(rect);
    }
    virtual void contentInvalidate()
    {
        if (mParent)
            mParent->contentInvalidate();
    }
    // invalidate the view/display, NOT the content/DOM
    virtual void viewInvalidate()
    {
        if (mParent)
            mParent->viewInvalidate();
    }

    // these need not be virtual
    //
    void setSize(int w, int h)
    {
        int ow = width();
        int oh = height();
        mBounds.setWidth(w);
        mBounds.setHeight(h);
        // Only call onResize if the new size is different.
        if (w != ow || h != oh)
            onResize();
    }

    // used by layout when it needs to wrap content column around screen
    void setSizeScreenWidthAndScale(int w, int h, int screenWidth, int scale)
    {
        int ow = width();
        int oh = height();
        int osw = mScreenWidth;
        mBounds.setWidth(w);
        mBounds.setHeight(h);
        mScreenWidth = screenWidth;
        mScale = scale;
        // Only call onResize if the new size is different.
        if (w != ow || h != oh || screenWidth != osw)
            onResize();
    }

    void setLocation(int x, int y)
    {
        mBounds.setX(x);
        mBounds.setY(y);
    }

    int width() const     { return mBounds.width(); }
    int height() const    { return mBounds.height(); }
    int locX() const      { return mBounds.x(); }
    int locY() const      { return mBounds.y(); }
    int screenWidth() const { return mParent ? mParent->screenWidth() : mScreenWidth; }
    int scale() const     { return mParent ? mParent->scale() : mScale; }

    // called by RenderPopupMenuAndroid
    virtual void popupRequest(WebCoreReply* reply,
                              int currIndex,
                              const uint16_t** labels,  // the first short is the length (number of following shorts)
                              size_t labelCount,        // the number of label strings
                              const int enabled[],      // bools telling which corresponding labels are selectable
                              size_t enabledCount)      // length of the enabled array, which should equal labelCount
    {
        if (mParent)
            mParent->popupRequest(reply, currIndex, labels, labelCount, enabled, enabledCount);
    }
    
    //implemented in android_widget_htmlwidget
    virtual void removeFrameGeneration(WebCore::Frame* ) {}
    virtual void updateFrameGeneration(WebCore::Frame* ) {}
    virtual void jsAlert(const WebCore::String& url, const WebCore::String& text) { }
    virtual bool jsConfirm(const WebCore::String& url, const WebCore::String& text) { return false; }
    virtual bool jsPrompt(const WebCore::String& url, const WebCore::String& message, const WebCore::String& defaultValue, WebCore::String& result) { return false;}
    virtual bool jsUnload(const WebCore::String& url, const WebCore::String& message) { return false; }

    virtual void updateTextfield(WebCore::Node* pointer, bool changeToPassword, const WebCore::String& text)
    {
        if (mParent)
            mParent->updateTextfield(pointer, changeToPassword, text);
    }

    void setWidget(WebCore::Widget* w) { mWidget = w; }
    WebCore::Widget* widget() { return mWidget; }
    
private:
    WebCore::IntRect    mBounds;
    int                 mScreenWidth;
    int                 mScale;
    WebCore::Widget*    mWidget;
protected:
    WebCoreViewBridge*  mParent;
};

#endif // WEBCORE_VIEW_BRIDGE_H
