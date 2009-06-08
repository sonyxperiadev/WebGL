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

#include "config.h"
#include "android_graphics.h"
#include "IntRect.h"
#include "SkCanvas.h"
#include "SkCornerPathEffect.h"
#include "SkPath.h"
#include "SkRegion.h"

///////////////////////////////////////////////////////////////////////////////

const static SkColor cursorOuterColors[] = {
    SkColorSetARGB(0xff, 0xB3, 0x3F, 0x08), // normal ring select
    SkColorSetARGB(0xff, 0x46, 0xb0, 0x00), // fake ring select, for phone, email, text
    SkColorSetARGB(0xff, 0xAD, 0x5C, 0x0A), // normal ring pressed
    SkColorSetARGB(0xff, 0x36, 0xc0, 0x00)  // fake ring pressed
};

const static SkColor cursorInnerColors[] = {
    SkColorSetARGB(0xff, 0xFE, 0x92, 0x30), // normal ring select
    SkColorSetARGB(0xff, 0x8c, 0xd9, 0x00), // fake ring select, for phone, email, text
    SkColorSetARGB(0xff, 0xFE, 0xBD, 0x3A), // normal ring pressed
    SkColorSetARGB(0xff, 0x7c, 0xe9, 0x00)  // fake ring pressed
};

const static SkColor cursorPressedColors[] = {
    SkColorSetARGB(0x80, 0xFF, 0xC6, 0x4B), // normal ring pressed
    SkColorSetARGB(0x80, 0x7c, 0xe9, 0x00)  // fake ring pressed
};

#define CURSOR_RING_ROUNDEDNESS SkIntToScalar(5) // used to draw corners
#define CURSOR_RING_INNER_DIAMETER SkFixedToScalar(SkIntToFixed(3)>>1) // 3/2 == 1.5
#define CURSOR_RING_OUTER_OUTSET 2 // used to inflate rects added to region

void CursorRing::DrawRing(SkCanvas* canvas,
    const Vector<WebCore::IntRect>& rects, Flavor flavor)
{
    unsigned                rectCount = rects.size();
    SkRegion                rgn;
    SkPath                  path;
    for (unsigned i = 0; i < rectCount; i++)
    {
        SkRect  r(rects[i]);
        SkIRect ir;

        r.round(&ir);
        ir.inset(-CURSOR_RING_OUTER_OUTSET, -CURSOR_RING_OUTER_OUTSET);
        rgn.op(ir, SkRegion::kUnion_Op);
    }
    rgn.getBoundaryPath(&path);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setPathEffect(new SkCornerPathEffect(CURSOR_RING_ROUNDEDNESS))->unref();
    if (flavor >= NORMAL_ANIMATING) { // pressed
        paint.setColor(cursorPressedColors[flavor - NORMAL_ANIMATING]);
        canvas->drawPath(path, paint);
    }
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(CURSOR_RING_OUTER_DIAMETER);
    paint.setColor(cursorOuterColors[flavor]);
    canvas->drawPath(path, paint);
    paint.setStrokeWidth(CURSOR_RING_INNER_DIAMETER);
    paint.setColor(cursorInnerColors[flavor]);
    canvas->drawPath(path, paint);
}


