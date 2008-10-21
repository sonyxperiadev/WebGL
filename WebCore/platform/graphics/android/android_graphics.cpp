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

#include "config.h"
#include "android_graphics.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkCornerPathEffect.h"
#include "SkGradientShader.h"
#include "SkPath.h"
#include "SkRegion.h"

SkPoint* android_setpt(SkPoint* dst, const WebCore::IntPoint& src)
{
    dst->set(SkIntToScalar(src.x()), SkIntToScalar(src.y()));
    return dst;
}

SkPoint* android_setpt(SkPoint* dst, const WebCore::FloatPoint& src)
{
    dst->set(SkFloatToScalar(src.x()), SkFloatToScalar(src.y()));
    return dst;
}

SkRect* android_setrect(SkRect* dst, const WebCore::IntRect& src)
{
    dst->set(SkIntToScalar(src.x()),
             SkIntToScalar(src.y()),
             SkIntToScalar(src.x() + src.width()),
             SkIntToScalar(src.y() + src.height()));
    return dst;
}

SkIRect* android_setrect(SkIRect* dst, const WebCore::IntRect& src)
{
    dst->set(src.x(), src.y(),
             src.x() + src.width(),
             src.y() + src.height());
    return dst;
}

SkRect* android_setrect(SkRect* dst, const WebCore::FloatRect& src)
{
    dst->set(SkFloatToScalar(src.x()),
             SkFloatToScalar(src.y()),
             SkFloatToScalar(src.x() + src.width()),
             SkFloatToScalar(src.y() + src.height()));
    return dst;
}

SkIRect* android_setrect(SkIRect* dst, const WebCore::FloatRect& src)
{
    dst->set(SkScalarRound(SkFloatToScalar(src.x())),
             SkScalarRound(SkFloatToScalar(src.y())),
             SkScalarRound(SkFloatToScalar(src.x() + src.width())),
             SkScalarRound(SkFloatToScalar(src.y() + src.height())));
    return dst;
}

SkIRect* android_setrect_scaled(SkIRect* dst, const WebCore::FloatRect& src,
                                float sx, float sy)
{
    dst->set(SkScalarRound(SkFloatToScalar(src.x() * sx)),
             SkScalarRound(SkFloatToScalar(src.y() * sy)),
             SkScalarRound(SkFloatToScalar((src.x() + src.width()) * sx)),
             SkScalarRound(SkFloatToScalar((src.y() + src.height()) * sy)));
    return dst;
}

static const struct CompositOpToPorterDuffMode {
    uint8_t mCompositOp;
    uint8_t mPorterDuffMode;
} gMapCompositOpsToPorterDuffModes[] = {
    { WebCore::CompositeClear,           SkPorterDuff::kClear_Mode },
    { WebCore::CompositeCopy,            SkPorterDuff::kSrcOver_Mode },  // TODO
    { WebCore::CompositeSourceOver,      SkPorterDuff::kSrcOver_Mode },
    { WebCore::CompositeSourceIn,        SkPorterDuff::kSrcIn_Mode },
    { WebCore::CompositeSourceOut,       SkPorterDuff::kSrcOut_Mode },
    { WebCore::CompositeSourceAtop,      SkPorterDuff::kSrcATop_Mode },
    { WebCore::CompositeDestinationOver, SkPorterDuff::kDstOver_Mode },
    { WebCore::CompositeDestinationIn,   SkPorterDuff::kDstIn_Mode },
    { WebCore::CompositeDestinationOut,  SkPorterDuff::kDstOut_Mode },
    { WebCore::CompositeDestinationAtop, SkPorterDuff::kDstATop_Mode },
    { WebCore::CompositeXOR,             SkPorterDuff::kXor_Mode },
    { WebCore::CompositePlusDarker,      SkPorterDuff::kDarken_Mode },
    { WebCore::CompositeHighlight,       SkPorterDuff::kSrcOver_Mode },  // TODO
    { WebCore::CompositePlusLighter,     SkPorterDuff::kLighten_Mode }
};

SkPorterDuff::Mode android_convert_compositeOp(WebCore::CompositeOperator op)
{
    const CompositOpToPorterDuffMode* table = gMapCompositOpsToPorterDuffModes;
    
    for (unsigned i = 0; i < SK_ARRAY_COUNT(gMapCompositOpsToPorterDuffModes); i++) {
        if (table[i].mCompositOp == op) {
            return (SkPorterDuff::Mode)table[i].mPorterDuffMode;
        }
    }

    SkDEBUGF(("GraphicsContext::setCompositeOperation uknown CompositOperator %d\n", op));
    return SkPorterDuff::kSrcOver_Mode; // fall-back
}

SkShader::TileMode android_convert_TileRule(WebCore::Image::TileRule rule)
{
    // stretch == clamp
    // repeat == repeat
    // RoundTile???

    return WebCore::Image::RepeatTile == rule ? SkShader::kRepeat_TileMode : SkShader::kClamp_TileMode;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

static U8CPU InvScaleByte(U8CPU component, uint32_t scale)
{
    SkASSERT(component == (uint8_t)component);
    return (component * scale + 0x8000) >> 16;
}

// move this guy into SkColor.h
static SkColor SkPMColorToColor(SkPMColor pm)
{
    if (0 == pm)
        return 0;
    
    unsigned a = SkGetPackedA32(pm);
    uint32_t scale = (255 << 16) / a;
    
    return SkColorSetARGB(a,
                          InvScaleByte(SkGetPackedR32(pm), scale),
                          InvScaleByte(SkGetPackedG32(pm), scale),
                          InvScaleByte(SkGetPackedB32(pm), scale));
}

WebCore::Color android_SkPMColorToWebCoreColor(SkPMColor pm)
{
    SkColor c = SkPMColorToColor(pm);
    
    return WebCore::Color(SkColorGetR(c), SkColorGetG(c), SkColorGetB(c), SkColorGetA(c));
}

const static SkColor focusOuterColors[] = {
    SkColorSetARGB(0xff, 0xB3, 0x3F, 0x08), // normal focus ring select
    SkColorSetARGB(0xff, 0x46, 0xb0, 0x00), // fake focus ring select, for phone, email, text
    SkColorSetARGB(0x00, 0x00, 0x00, 0x00), // no ring, for buttons
    SkColorSetARGB(0xff, 0xb0, 0x16, 0x00), // invalid focus ring color
    SkColorSetARGB(0xff, 0xAD, 0x5C, 0x0A), // normal focus ring pressed
    SkColorSetARGB(0xff, 0x36, 0xc0, 0x00)  // fake focus ring pressed
};

const static SkColor focusInnerColors[] = {
    SkColorSetARGB(0xff, 0xFE, 0x92, 0x30), // normal focus ring select
    SkColorSetARGB(0xff, 0x8c, 0xd9, 0x00), // fake focus ring select, for phone, email, text
    SkColorSetARGB(0x00, 0x00, 0x00, 0x00), // no ring, for buttons
    SkColorSetARGB(0xff, 0xd9, 0x2c, 0x00), // invalid focus ring color
    SkColorSetARGB(0xff, 0xFE, 0xBD, 0x3A), // normal focus ring pressed
    SkColorSetARGB(0xff, 0x7c, 0xe9, 0x00)  // fake focus ring pressed
};

const static SkColor focusPressedColors[] = {
    SkColorSetARGB(0x80, 0xFF, 0xC6, 0x4B), // normal focus ring pressed
    SkColorSetARGB(0x80, 0x7c, 0xe9, 0x00), // fake focus ring pressed
    SkColorSetARGB(0x80, 0xFF, 0xC6, 0x4B)  // button focus ring pressed
};

#define FOCUS_RING_ROUNDEDNESS SkIntToScalar(5) // used to draw corners
#define FOCUS_RING_INNER_DIAMETER SkFixedToScalar(SkIntToFixed(3)>>1) // 3/2 == 1.5
#define FOCUS_RING_OUTER_OUTSET 2 // used to inflate rects added to region

void FocusRing::DrawRing(SkCanvas* canvas,
    const Vector<WebCore::IntRect>& rects, Flavor flavor)
{
    unsigned                rectCount = rects.size();
    SkRegion                rgn;
    SkPath                  path;
    for (unsigned i = 0; i < rectCount; i++)
    {
        SkIRect    r;

        android_setrect(&r, rects[i]);
        r.inset(-FOCUS_RING_OUTER_OUTSET, -FOCUS_RING_OUTER_OUTSET);
        rgn.op(r, SkRegion::kUnion_Op);
    }
    rgn.getBoundaryPath(&path);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setPathEffect(new SkCornerPathEffect(FOCUS_RING_ROUNDEDNESS))->unref();
    if (flavor >= NORMAL_ANIMATING) { // pressed
        paint.setColor(focusPressedColors[flavor - NORMAL_ANIMATING]);
        canvas->drawPath(path, paint);
    }
    if (flavor == BUTTON_ANIMATING)
        return;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(FOCUS_RING_OUTER_DIAMETER);
    paint.setColor(focusOuterColors[flavor]);
    canvas->drawPath(path, paint);
    paint.setStrokeWidth(FOCUS_RING_INNER_DIAMETER);
    paint.setColor(focusInnerColors[flavor]);
    canvas->drawPath(path, paint);
}


