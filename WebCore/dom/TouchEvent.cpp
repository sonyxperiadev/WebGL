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

#include "config.h"

#if ENABLE(TOUCH_EVENTS) // Android

#include "TouchEvent.h"

namespace WebCore {

TouchEvent::TouchEvent(TouchList* touches, TouchList* targetTouches,
        TouchList* changedTouches, const AtomicString& type, 
        PassRefPtr<AbstractView> view, int screenX, int screenY, int pageX, int pageY)
    : MouseRelatedEvent(type, true, true, view, 0, screenX, screenY, pageX, pageY,
                        false, false, false, false)
    , m_touches(touches)
    , m_targetTouches(targetTouches)
    , m_changedTouches(changedTouches)
{
}

void TouchEvent::initTouchEvent(TouchList* touches, TouchList* targetTouches,
        TouchList* changedTouches, const AtomicString& type, 
        PassRefPtr<AbstractView> view, int screenX, int screenY, int clientX, int clientY)
{
    if (dispatched())
        return;

    initUIEvent(type, true, true, view, 0);

    m_screenX = screenX;
    m_screenY = screenY;
    initCoordinates(clientX, clientY);
}

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)
