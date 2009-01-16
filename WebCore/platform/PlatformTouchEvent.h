/*
 * Copyright 2008, The Android Open Source Project
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PlatformTouchEvent_h
#define PlatformTouchEvent_h

#if ENABLE(TOUCH_EVENTS) // Android

#include "IntPoint.h"

namespace WebCore {

    enum TouchEventType {TouchEventStart, TouchEventMove, TouchEventEnd, TouchEventCancel};

    class PlatformTouchEvent {
    public:
        PlatformTouchEvent()
            : m_eventType(TouchEventCancel)
        {
        }

        PlatformTouchEvent(const IntPoint& pos, const IntPoint& globalPos, TouchEventType eventType)
            : m_position(pos)
            , m_globalPosition(globalPos)
            , m_eventType(eventType)
        {
        }

        const IntPoint& pos() const { return m_position; }
        int x() const { return m_position.x(); }
        int y() const { return m_position.y(); }
        int globalX() const { return m_globalPosition.x(); }
        int globalY() const { return m_globalPosition.y(); }
        TouchEventType eventType() const { return m_eventType; }

    private:
        IntPoint m_position;
        IntPoint m_globalPosition;
        TouchEventType m_eventType;
    };

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)

#endif // PlatformTouchEvent_h
