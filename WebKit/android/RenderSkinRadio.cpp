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
#include "RenderSkinRadio.h"

#include "android_graphics.h"
#include "Document.h"
#include "IntRect.h"
#include "Node.h"
#include "RenderSkinAndroid.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkRect.h"

static const char* checks[] = { "res/drawable/checkbox_off_background.png", "res/drawable/checkbox_on_background.png",
                                "res/drawable/radiobutton_off_background.png", "res/drawable/radiobutton_on_background.png"};
static const SkScalar SIZE = SkIntToScalar(19); // Default height and width - corresponds with the bitmap - perhaps we should query the bitmap.

namespace WebCore {

static SkBitmap s_bitmap[4];
static bool     s_decoded;

void RenderSkinRadio::Init(android::AssetManager* am)
{
    if (s_decoded)
        return;
    s_decoded = RenderSkinAndroid::DecodeBitmap(am, checks[0], &s_bitmap[0]);
    s_decoded = RenderSkinAndroid::DecodeBitmap(am, checks[1], &s_bitmap[1]) && s_decoded;
    s_decoded = RenderSkinAndroid::DecodeBitmap(am, checks[2], &s_bitmap[2]) && s_decoded;
    s_decoded = RenderSkinAndroid::DecodeBitmap(am, checks[3], &s_bitmap[3]) && s_decoded;
}

void RenderSkinRadio::Draw(SkCanvas* canvas, Node* element, const IntRect& ir,
        bool isCheckBox)
{
    if (!s_decoded || !element) {
        return;
    }
    SkRect r;
    android_setrect(&r, ir);
    int saveLayerCount = 0;
    int saveScaleCount = 0;
    if (!element->isEnabled()) {
        saveLayerCount = canvas->saveLayerAlpha(&r, 0x80);
    }
    SkScalar width = r.width();
    if (SIZE != width) {
        SkScalar scale = SkScalarDiv(width, SIZE);
        saveScaleCount =  canvas->scale(scale, scale);
    }
    canvas->drawBitmap(s_bitmap[element->isChecked() + 2*(!isCheckBox)],
            r.fLeft, r.fTop, NULL);
    if (saveLayerCount != 0) {
        canvas->restoreToCount(saveLayerCount);
    } else if (saveScaleCount != 0) {
        canvas->restoreToCount(saveScaleCount);
    }
    return;
}

} //WebCore

