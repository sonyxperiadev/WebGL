/*
 * Copyright (C) 2008 The Android Open Source Project
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

// must include config.h first for webkit to fiddle with new/delete
#include "config.h"
#include "SkANP.h"

static ANPCanvas* anp_newCanvas(const ANPBitmap* bitmap) {
    SkBitmap bm;
    return new ANPCanvas(*SkANP::SetBitmap(&bm, *bitmap));
}

static void anp_deleteCanvas(ANPCanvas* canvas) {
    delete canvas;
}

static void anp_save(ANPCanvas* canvas) {
    canvas->skcanvas->save();
}

static void anp_restore(ANPCanvas* canvas) {
    canvas->skcanvas->restore();
}

static void anp_translate(ANPCanvas* canvas, float tx, float ty) {
    canvas->skcanvas->translate(SkFloatToScalar(tx), SkFloatToScalar(ty));
}

static void anp_scale(ANPCanvas* canvas, float sx, float sy) {
    canvas->skcanvas->scale(SkFloatToScalar(sx), SkFloatToScalar(sy));
}

static void anp_rotate(ANPCanvas* canvas, float degrees) {
    canvas->skcanvas->rotate(SkFloatToScalar(degrees));
}

static void anp_skew(ANPCanvas* canvas, float kx, float ky) {
    canvas->skcanvas->skew(SkFloatToScalar(kx), SkFloatToScalar(ky));
}

static void anp_clipRect(ANPCanvas* canvas, const ANPRectF* rect) {
    SkRect r;
    canvas->skcanvas->clipRect(*SkANP::SetRect(&r, *rect));
}

static void anp_clipPath(ANPCanvas* canvas, const ANPPath* path) {
    canvas->skcanvas->clipPath(*path);
}

static void anp_drawColor(ANPCanvas* canvas, ANPColor color) {
    canvas->skcanvas->drawColor(color);
}

static void anp_drawPaint(ANPCanvas* canvas, const ANPPaint* paint) {
    canvas->skcanvas->drawPaint(*paint);
}

static void anp_drawRect(ANPCanvas* canvas, const ANPRectF* rect,
                         const ANPPaint* paint) {
    SkRect  r;
    canvas->skcanvas->drawRect(*SkANP::SetRect(&r, *rect), *paint);
}

static void anp_drawOval(ANPCanvas* canvas, const ANPRectF* rect,
                         const ANPPaint* paint) {
    SkRect  r;
    canvas->skcanvas->drawOval(*SkANP::SetRect(&r, *rect), *paint);
}

static void anp_drawPath(ANPCanvas* canvas, const ANPPath* path,
                         const ANPPaint* paint) {
    canvas->skcanvas->drawPath(*path, *paint);
}

static void anp_drawText(ANPCanvas* canvas, const void* text, uint32_t length,
                         float x, float y, const ANPPaint* paint) {
    canvas->skcanvas->drawText(text, length,
                               SkFloatToScalar(x), SkFloatToScalar(y),
                               *paint);
}

static void anp_drawPosText(ANPCanvas* canvas, const void* text,
                uint32_t byteLength, const float xy[], const ANPPaint* paint) {
    canvas->skcanvas->drawPosText(text, byteLength,
                                  reinterpret_cast<const SkPoint*>(xy), *paint);
}

static void anp_drawBitmap(ANPCanvas* canvas, const ANPBitmap* bitmap,
                           float x, float y, const ANPPaint* paint) {
    SkBitmap    bm;
    canvas->skcanvas->drawBitmap(*SkANP::SetBitmap(&bm, *bitmap),
                                 SkFloatToScalar(x), SkFloatToScalar(y),
                                 paint);
}

static void anp_drawBitmapRect(ANPCanvas* canvas, const ANPBitmap* bitmap,
                              const ANPRectI* src, const ANPRectF* dst,
                               const ANPPaint* paint) {
    SkBitmap    bm;
    SkRect      dstR;
    SkIRect     srcR, *srcPtr = NULL;
    
    if (src) {
        srcPtr = SkANP::SetRect(&srcR, *src);
    }
    canvas->skcanvas->drawBitmapRect(*SkANP::SetBitmap(&bm, *bitmap), srcPtr,
                           *SkANP::SetRect(&dstR, *dst), paint);
}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void ANPCanvasInterfaceV0_Init(ANPInterface* value) {
    ANPCanvasInterfaceV0* i = reinterpret_cast<ANPCanvasInterfaceV0*>(value);
    
    ASSIGN(i, newCanvas);
    ASSIGN(i, deleteCanvas);
    ASSIGN(i, save);
    ASSIGN(i, restore);
    ASSIGN(i, translate);
    ASSIGN(i, scale);
    ASSIGN(i, rotate);
    ASSIGN(i, skew);
    ASSIGN(i, clipRect);
    ASSIGN(i, clipPath);
    ASSIGN(i, drawColor);
    ASSIGN(i, drawPaint);
    ASSIGN(i, drawRect);
    ASSIGN(i, drawOval);
    ASSIGN(i, drawPath);
    ASSIGN(i, drawText);
    ASSIGN(i, drawPosText);
    ASSIGN(i, drawBitmap);
    ASSIGN(i, drawBitmapRect);
}

