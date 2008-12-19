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
