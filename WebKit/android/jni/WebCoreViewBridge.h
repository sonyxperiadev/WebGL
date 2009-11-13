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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WEBCORE_VIEW_BRIDGE_H
#define WEBCORE_VIEW_BRIDGE_H

// TODO: move this outside of jni directory

#include "IntRect.h"
#include "WebCoreRefObject.h"

namespace WebCore
{
    class GraphicsContext;
}

class WebCoreViewBridge : public WebCoreRefObject {
public:
    WebCoreViewBridge() : 
        mBounds(0,0,0,0),
        m_windowBounds(0,0,0,0)
    {}
    virtual ~WebCoreViewBridge() {}

    virtual void draw(WebCore::GraphicsContext* ctx, 
        const WebCore::IntRect& rect) = 0;

    const WebCore::IntRect& getBounds() const 
    {
        return mBounds;
    }
    
    const WebCore::IntRect& getWindowBounds() const
    {
        return m_windowBounds;
    }

    void setSize(int w, int h)
    {
        mBounds.setWidth(w);
        mBounds.setHeight(h);
    }

    void setLocation(int x, int y)
    {
        mBounds.setX(x);
        mBounds.setY(y);
    }

    void setWindowBounds(int x, int y, int h, int v)
    {
        m_windowBounds = WebCore::IntRect(x, y, h, v);
    }

    int width() const     { return mBounds.width(); }
    int height() const    { return mBounds.height(); }
    int locX() const      { return mBounds.x(); }
    int locY() const      { return mBounds.y(); }

    virtual bool forFrameView() const { return false; }
    virtual bool forPluginView() const { return false; }

private:
    WebCore::IntRect    mBounds;
    WebCore::IntRect    m_windowBounds;
};

#endif // WEBCORE_VIEW_BRIDGE_H
