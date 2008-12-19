/* 
 *
 * Copyright 2007, The Android Open Source Project
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

#ifndef android_graphics_DEFINED
#define android_graphics_DEFINED

#include "Color.h"
#include "Image.h"
#include "wtf/Vector.h"

#include "SkColor.h"
#include "SkPorterDuff.h"
#include "SkScalar.h"
#include "SkShader.h"

class SkCanvas;
struct SkPoint;
struct SKRect;

namespace WebCore {
    class FloatRect;
    class IntPoint;
    class IntRect;
    class GraphicsContext;
}

SkPoint* android_setpt(SkPoint* dst, const WebCore::IntPoint& src);
SkPoint* android_setpt(SkPoint* dst, const WebCore::FloatPoint& src);
SkRect* android_setrect(SkRect* dst, const WebCore::IntRect& src);
SkRect* android_setrect(SkRect* dst, const WebCore::FloatRect& src);
SkIRect* android_setrect(SkIRect* dst, const WebCore::IntRect& src);
SkIRect* android_setrect(SkIRect* dst, const WebCore::FloatRect& src);
SkIRect* android_setrect_scaled(SkIRect* dst, const WebCore::FloatRect& src,
                                float sx, float sy);

SkPorterDuff::Mode android_convert_compositeOp(WebCore::CompositeOperator);
SkShader::TileMode android_convert_TileRule(WebCore::Image::TileRule);

WebCore::Color android_SkPMColorToWebCoreColor(SkPMColor pm);

SkCanvas* android_gc2canvas(WebCore::GraphicsContext* gc);

// Data and methods for focus rings

// used to inflate node cache entry
#define FOCUS_RING_HIT_TEST_RADIUS 5  

// used to inval rectangle enclosing pressed state of focus ring
#define FOCUS_RING_OUTER_DIAMETER SkFixedToScalar(SkIntToFixed(13)>>2) // 13/4 == 3.25

struct FocusRing {
public:
    enum Flavor {
        NORMAL_FLAVOR,
        FAKE_FLAVOR,
        INVALID_FLAVOR,
        NORMAL_ANIMATING,
        FAKE_ANIMATING,
        ANIMATING_COUNT = 2
    };
    
    static void DrawRing(SkCanvas* , 
        const Vector<WebCore::IntRect>& rects, Flavor );
};

#endif

