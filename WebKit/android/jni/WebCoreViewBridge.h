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
    WebCoreViewBridge() { }
    virtual ~WebCoreViewBridge() { }

    virtual void draw(WebCore::GraphicsContext* ctx, 
        const WebCore::IntRect& rect) = 0;

    const WebCore::IntRect& getBounds() const 
    {
        return m_bounds;
    }

    const WebCore::IntRect& getVisibleBounds() const
    {
        return m_visibleBounds;
    }

    const WebCore::IntRect& getWindowBounds() const
    {
        return m_windowBounds;
    }

    void setSize(int w, int h)
    {
        m_bounds.setWidth(w);
        m_bounds.setHeight(h);
    }

    void setVisibleSize(int w, int h)
    {
        m_visibleBounds.setWidth(w);
        m_visibleBounds.setHeight(h);
    }

    void setLocation(int x, int y)
    {
        m_bounds.setX(x);
        m_bounds.setY(y);
        m_visibleBounds.setX(x);
        m_visibleBounds.setY(y);
    }

    void setWindowBounds(int x, int y, int h, int v)
    {
        m_windowBounds = WebCore::IntRect(x, y, h, v);
    }

    int width() const     { return m_bounds.width(); }
    int height() const    { return m_bounds.height(); }
    int locX() const      { return m_bounds.x(); }
    int locY() const      { return m_bounds.y(); }

    int visibleWidth() const    { return m_visibleBounds.width(); }
    int visibleHeight() const   { return m_visibleBounds.height(); }
    int visibleX() const        { return m_visibleBounds.x(); }
    int visibleY() const        { return m_visibleBounds.y(); }

    virtual bool forFrameView() const { return false; }
    virtual bool forPluginView() const { return false; }

private:
    WebCore::IntRect    m_bounds;
    WebCore::IntRect    m_windowBounds;
    WebCore::IntRect    m_visibleBounds;
};

#endif // WEBCORE_VIEW_BRIDGE_H
