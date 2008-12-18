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

#ifndef TOUCHLIST_H_
#define TOUCHLIST_H_

#if ENABLE(TOUCH_EVENTS) // Android

#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include "Touch.h"

namespace WebCore {

    class TouchList : public RefCounted<TouchList> {
    public:
        static PassRefPtr<TouchList> create()
        {
            return adoptRef(new TouchList);
        }

        unsigned length() const { return m_values.size(); }

        Touch* item (unsigned);

        void append(const PassRefPtr<Touch> touch) { m_values.append(touch); }

    private:
        TouchList() {}

        Vector<RefPtr<Touch> > m_values;
    };

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)

#endif /* TOUCHLIST_H_ */
