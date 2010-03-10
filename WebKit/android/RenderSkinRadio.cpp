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

#include "CString.h"
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

static const char* checks[] = { "btn_check_off.png",
                                "btn_check_on.png",
                                "btn_radio_off.png",
                                "btn_radio_on.png"};
// Matches the width of the bitmap
static SkScalar SIZE;

namespace WebCore {

static SkBitmap s_bitmap[4];
static bool     s_decoded;

void RenderSkinRadio::Init(android::AssetManager* am, String drawableDirectory)
{
    if (s_decoded)
        return;
    String path = drawableDirectory + checks[0];
    s_decoded = RenderSkinAndroid::DecodeBitmap(am, path.utf8().data(), &s_bitmap[0]);
    path = drawableDirectory + checks[1];
    s_decoded = RenderSkinAndroid::DecodeBitmap(am, path.utf8().data(), &s_bitmap[1]) && s_decoded;
    path = drawableDirectory + checks[2];
    s_decoded = RenderSkinAndroid::DecodeBitmap(am, path.utf8().data(), &s_bitmap[2]) && s_decoded;
    path = drawableDirectory + checks[3];
    s_decoded = RenderSkinAndroid::DecodeBitmap(am, path.utf8().data(), &s_bitmap[3]) && s_decoded;
    SIZE = SkIntToScalar(s_bitmap[0].width());
}

void RenderSkinRadio::Draw(SkCanvas* canvas, Node* element, const IntRect& ir,
        bool isCheckBox)
{
    if (!s_decoded || !element) {
        return;
    }
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
    SkScalar scale = SkScalarDiv(width, SIZE);
    saveScaleCount = canvas->save();
    canvas->translate(r.fLeft, r.fTop);
    canvas->scale(scale, scale);

    bool checked = false;
    if (InputElement* inputElement = toInputElement(static_cast<Element*>(element))) {
        checked = inputElement->isChecked();
    }

    canvas->drawBitmap(s_bitmap[checked + 2*(!isCheckBox)],
            0, 0, &paint);
    canvas->restoreToCount(saveScaleCount);
}

} //WebCore
