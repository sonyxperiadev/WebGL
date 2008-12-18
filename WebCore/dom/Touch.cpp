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

#include "Touch.h"

#include "FrameView.h"

namespace WebCore {

static int contentsX(Frame* frame)
{
    if (!frame)
        return 0;
    FrameView* frameView = frame->view();
    if (!frameView)
        return 0;
    return frameView->scrollX();
}

static int contentsY(Frame* frame)
{
    if (!frame)
        return 0;
    FrameView* frameView = frame->view();
    if (!frameView)
        return 0;
    return frameView->scrollY();
}

Touch::Touch(Frame* frame, EventTarget* target, unsigned identifier, 
        int screenX, int screenY, int pageX, int pageY)
    : m_frame(frame)
    , m_target(target)
    , m_identifier(identifier)
    , m_clientX(pageX - contentsX(m_frame.get()))
    , m_clientY(pageY - contentsY(m_frame.get()))
    , m_screenX(screenX)
    , m_screenY(screenY)
    , m_pageX(pageX)
    , m_pageY(pageY)
{
}

void Touch::updateLocation(int screenX, int screenY, int pageX, int pageY)
{
    m_clientX = pageX - contentsX(m_frame.get());
    m_clientY = pageY - contentsY(m_frame.get());
    m_screenX = screenX;
    m_screenY = screenY;
    m_pageX = pageX;
    m_pageY = pageY;
}

} // namespace WebCore

#endif

