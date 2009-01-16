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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

