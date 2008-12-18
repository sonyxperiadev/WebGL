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

#ifndef TouchEvent_h
#define TouchEvent_h

#if ENABLE(TOUCH_EVENTS) // Android

#include "MouseRelatedEvent.h"
#include "TouchList.h"

namespace WebCore {

    class TouchEvent : public MouseRelatedEvent {
    public:
        static PassRefPtr<TouchEvent> create()
        {
            return adoptRef(new TouchEvent);
        }
        static PassRefPtr<TouchEvent> create(TouchList* touches, 
                TouchList* targetTouches, TouchList* changedTouches, 
                const AtomicString& type, PassRefPtr<AbstractView> view,
                int screenX, int screenY, int pageX, int pageY)
        {
            return adoptRef(new TouchEvent(touches, targetTouches, changedTouches,
                    type, view, screenX, screenY, pageX, pageY));
        }

        void initTouchEvent(TouchList* touches, TouchList* targetTouches,
                TouchList* changedTouches, const AtomicString& type, 
                PassRefPtr<AbstractView> view, int screenX, int screenY, 
                int clientX, int clientY);

        TouchList* touches() const {return m_touches.get();}
        TouchList* targetTouches() const {return m_targetTouches.get();}
        TouchList* changedTouches() const {return m_changedTouches.get();}

    private:
        TouchEvent() {}
        TouchEvent(TouchList* touches, TouchList* targetTouches,
                TouchList* changedTouches, const AtomicString& type,
                PassRefPtr<AbstractView>, int screenX, int screenY, int pageX,
                int pageY);

        virtual bool isTouchEvent() const {return true;}

        RefPtr<TouchList> m_touches;
        RefPtr<TouchList> m_targetTouches;
        RefPtr<TouchList> m_changedTouches;
    };

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)

#endif // TouchEvent_h
