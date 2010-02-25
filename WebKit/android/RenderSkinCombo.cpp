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
#include "RenderSkinCombo.h"

#include "Document.h"
#include "Element.h"
#include "Node.h"
#include "NodeRenderStyle.h"
#include "RenderStyle.h"
#include "SkCanvas.h"
#include "SkNinePatch.h"

namespace WebCore {

static SkBitmap         s_bitmap[2];    // Collection of assets for a combo box
static bool             s_decoded;      // True if all assets were decoded
static const int        s_margin = 2;
static const SkIRect    s_mar = { s_margin, s_margin,
                                RenderSkinCombo::extraWidth(), s_margin };
static SkIRect          s_subset;


RenderSkinCombo::RenderSkinCombo()
{
}

void RenderSkinCombo::Init(android::AssetManager* am)
{
    if (s_decoded)
        return;
    // Maybe short circuiting is fine, since I don't even draw if one state is not decoded properly
    // but is that necessary in the final version?
    s_decoded = RenderSkinAndroid::DecodeBitmap(am, "images/combobox-noHighlight.png", &s_bitmap[kNormal]);
    s_decoded = RenderSkinAndroid::DecodeBitmap(am, "images/combobox-disabled.png", &s_bitmap[kDisabled]) && s_decoded;

    int width = s_bitmap[kNormal].width();
    int height = s_bitmap[kNormal].height();
    s_subset.set(width - RenderSkinCombo::extraWidth()  + s_margin, 0, width, height);
}


bool RenderSkinCombo::Draw(SkCanvas* canvas, Node* element, int x, int y, int width, int height)
{
    if (!s_decoded)
        return true;

    State state = (element->isElementNode() && static_cast<Element*>(element)->isEnabledFormControl()) ? kNormal : kDisabled;
    if (height < (s_margin<<1) + 1) {
        height = (s_margin<<1) + 1;
    }
    SkRect bounds;

    bounds.set(SkIntToScalar(x+1), SkIntToScalar(y+1), SkIntToScalar(x + width-1), SkIntToScalar(y + height-1));
    RenderStyle* style = element->renderStyle();
    SkPaint paint;
    paint.setColor(style->backgroundColor().rgb());
    canvas->drawRect(bounds, paint);

    bounds.set(SkIntToScalar(x), SkIntToScalar(y), SkIntToScalar(x + width), SkIntToScalar(y + height));

    if (style->borderLeftColor().isValid() ||
        style->borderRightColor().isValid() ||
        style->borderTopColor().isValid() ||
        style->borderBottomColor().isValid()) {
        bounds.fLeft += SkIntToScalar(width - RenderSkinCombo::extraWidth());
        bounds.fRight -= SkIntToScalar(style->borderRightWidth());
        bounds.fTop += SkIntToScalar(style->borderTopWidth());
        bounds.fBottom -= SkIntToScalar(style->borderBottomWidth());
        canvas->drawBitmapRect(s_bitmap[state], &s_subset, bounds);
    } else {
        SkNinePatch::DrawNine(canvas, bounds, s_bitmap[state], s_mar);
    }
    return false;
}

}   //WebCore
