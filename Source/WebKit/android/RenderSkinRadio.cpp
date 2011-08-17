/*
 * Copyright 2006, The Android Open Source Project
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderSkinRadio.h"

#include "android_graphics.h"
#include "Document.h"
#include "Element.h"
#include "InputElement.h"
#include "IntRect.h"
#include "Node.h"
#include "RenderSkinAndroid.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkRect.h"
#include <utils/AssetManager.h>
#include <wtf/text/CString.h>

extern android::AssetManager* globalAssetManager();

static const char* checks[] = { "btn_check_off_holo.png",
                                "btn_check_on_holo.png",
                                "btn_radio_off_holo.png",
                                "btn_radio_on_holo.png"};
// Matches the width of the bitmap
static SkScalar s_bitmapWidth;

namespace WebCore {

static SkBitmap s_bitmap[4];
static bool s_decodingAttempted = false;
static bool s_decoded = false;

void RenderSkinRadio::Decode() {
    if (s_decodingAttempted)
        return;

    s_decodingAttempted = true;
    s_decoded = false;

    android::AssetManager* am = globalAssetManager();
    String drawableDir = RenderSkinAndroid::DrawableDirectory();
    for (int i = 0; i < 4; i++) {
        String path = drawableDir + checks[i];
        if (!RenderSkinAndroid::DecodeBitmap(am, path.utf8().data(), &s_bitmap[i]))
            return;
    }
    s_decoded = true;
    s_bitmapWidth = SkIntToScalar(s_bitmap[0].width());
}

void RenderSkinRadio::Draw(SkCanvas* canvas, Node* element, const IntRect& ir,
        bool isCheckBox)
{
    if (!element)
        return;

    if (!s_decodingAttempted)
        Decode();

    if (!s_decoded)
        return;

    SkRect r(ir);
    // Set up a paint to with filtering to look better.
    SkPaint paint;
    paint.setFlags(SkPaint::kFilterBitmap_Flag);
    int saveScaleCount = 0;

    if (!element->isElementNode() ||
        !static_cast<Element*>(element)->isEnabledFormControl()) {
        paint.setAlpha(0x80);
    }
    SkScalar width = r.width();
    SkScalar scale = SkScalarDiv(width, s_bitmapWidth);
    saveScaleCount = canvas->save();
    canvas->translate(r.fLeft, r.fTop);
    canvas->scale(scale, scale);

    bool checked = false;
    if (InputElement* inputElement = element->toInputElement()) {
        checked = inputElement->isChecked();
    }

    canvas->drawBitmap(s_bitmap[checked + 2*(!isCheckBox)],
            0, 0, &paint);
    canvas->restoreToCount(saveScaleCount);
}

} //WebCore
