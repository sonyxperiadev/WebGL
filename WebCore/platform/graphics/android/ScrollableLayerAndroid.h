/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ScrollableLayerAndroid_h
#define ScrollableLayerAndroid_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerAndroid.h"

namespace WebCore {

class ScrollableLayerAndroid : public LayerAndroid {

public:
    ScrollableLayerAndroid(RenderLayer* owner)
        : LayerAndroid(owner) {}
    ScrollableLayerAndroid(const ScrollableLayerAndroid& layer)
        : LayerAndroid(layer)
        , m_scrollLimits(layer.m_scrollLimits) {}
    ScrollableLayerAndroid(const LayerAndroid& layer)
        : LayerAndroid(layer)
    {
        m_scrollLimits.setEmpty();
    }
    virtual ~ScrollableLayerAndroid() {};

    virtual bool contentIsScrollable() const { return true; }

    virtual LayerAndroid* copy() const { return new ScrollableLayerAndroid(*this); }

    // Returns true if the content position has changed.
    bool scrollTo(int dx, int dy);

    // Fills the rect with the current scroll offset and the maximum scroll.
    // fLeft   = scrollX
    // fTop    = scrollY
    // fRight  = maxX
    // fBottom = maxY
    void getScrollRect(SkIRect* out) const;

    void setScrollLimits(float x, float y, float width, float height)
    {
        m_scrollLimits.set(x, y, x + width, y + height);
    }

private:
    SkRect m_scrollLimits;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // LayerAndroid_h
