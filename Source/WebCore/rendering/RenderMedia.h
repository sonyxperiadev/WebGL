/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef RenderMedia_h
#define RenderMedia_h

#if ENABLE(VIDEO)

#include "RenderImage.h"

namespace WebCore {

class HTMLMediaElement;
class MediaControls;

class RenderMedia : public RenderImage {
public:
    RenderMedia(HTMLMediaElement*);
    RenderMedia(HTMLMediaElement*, const IntSize& intrinsicSize);
    virtual ~RenderMedia();

    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

    HTMLMediaElement* mediaElement() const;
    MediaControls* controls() const;

<<<<<<< HEAD
    void updateFromElement();
    void updatePlayer();
    void updateControls();
    void updateTimeDisplay();
    
    void forwardEvent(Event*);
#if PLATFORM(ANDROID)
    void updateLastTouch();
#endif
=======
    virtual void updateFromElement();
>>>>>>> webkit.org at r78450

protected:
    virtual void layout();

private:
    virtual RenderObjectChildList* virtualChildren() { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const { return children(); }

    virtual void destroy();
    
    virtual const char* renderName() const { return "RenderMedia"; }
    virtual bool isMedia() const { return true; }
    virtual bool isImage() const { return false; }


    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    virtual bool requiresForcedStyleRecalcPropagation() const { return true; }

    OwnPtr<MediaControls> m_controls;
    RenderObjectChildList m_children;
<<<<<<< HEAD
    Node* m_lastUnderNode;
    Node* m_nodeUnderMouse;
    
    Timer<RenderMedia> m_timeUpdateTimer;
    Timer<RenderMedia> m_opacityAnimationTimer;
    bool m_mouseOver;
    double m_opacityAnimationStartTime;
    double m_opacityAnimationDuration;
    float m_opacityAnimationFrom;
    float m_opacityAnimationTo;
#if PLATFORM(ANDROID)
    double m_lastTouch;
#endif
=======
>>>>>>> webkit.org at r78450
};

inline RenderMedia* toRenderMedia(RenderObject* object)
{
    ASSERT(!object || object->isMedia());
    return static_cast<RenderMedia*>(object);
}

inline MediaControls* RenderMedia::controls() const
{
    return m_controls.get();
}

// This will catch anyone doing an unnecessary cast.
void toRenderMedia(const RenderMedia*);

} // namespace WebCore

#endif
#endif // RenderMedia_h
