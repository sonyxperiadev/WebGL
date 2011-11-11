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
#include <utils/AssetManager.h>
#include <wtf/text/CString.h>

extern android::AssetManager* globalAssetManager();

namespace WebCore {

// Indicates if the entire asset is being drawn, or if the border is being
// excluded and just the arrow drawn.
enum BorderStyle {
    FullAsset,
    NoBorder,
    BorderStyleCount // Keep at the end.
};

// There are 2.5 different concepts of a 'border' here, which results
// in rather a lot of magic constants.

// Firstly, we have the extra padding that webkit needs to know about,
// which defines how much bigger this element is made by the
// asset. This is actually a bit broader than the actual border on the
// asset, to make things look less cramped. The border is the same
// width on all sides, except on the right when it's significantly
// wider to allow for the arrow.
const int RenderSkinCombo::arrowMargin[ResolutionCount] = {
    16, // Medium resolution
    25, // High resolution
    34 // Extra high resolution
};
const int RenderSkinCombo::padMargin[ResolutionCount] = {
    1, // Medium resolution
    1, // High resolution
    1 // Extra high resolution
};

namespace {
// Then we have the borders used for the 9-patch stretch. The
// rectangle at the centre of these borders is entirely below and to
// the left of the arrow in the asset. Hence the border widths are the
// same for the bottom and left, but are different for the top. The
// right hand border width happens to be the same as arrowMargin
// defined above.
const int stretchMargin[RenderSkinAndroid::ResolutionCount] = { // border width for the bottom and left of the 9-patch
    2, // Medium resolution
    2, // High resolution
    3 // Extra high resolution

};
const int stretchTop[RenderSkinAndroid::ResolutionCount] = { // border width for the top of the 9-patch
    16, // Medium resolution
    23, // High resolution
    32 // Extra high resolution
};

// Finally, if the border is defined by the CSS, we only draw the
// arrow and not the border. We do this by drawing the relevant subset
// of the bitmap, which must now be precisely determined by what's in
// the asset with no extra padding to make things look properly
// spaced. The border to remove at the top, right and bottom of the
// image is the same as stretchMargin above, but we need to know the width
// of the arrow.
const int arrowWidth[RenderSkinAndroid::ResolutionCount] = {
    18, // Medium resolution
    27, // High resolution
    36 // Extra high resolution
};

// scale factors for various resolutions
const float scaleFactor[RenderSkinAndroid::ResolutionCount] = {
    1.0f, // medium res
    1.5f, // high res
    2.0f  // extra high res
};

// Store the calculated 9 patch margins for each border style.
SkIRect margin[BorderStyleCount];

SkBitmap bitmaps[2][BorderStyleCount]; // Collection of assets for a combo box - 2 states (enabled/disabled)
bool isDecodingAttempted = false; // True if we've tried to decode the assets
bool isDecoded = false;           // True if all assets were decoded

} // namespace

int RenderSkinCombo::minHeight() {
    return SkScalarRound(stretchTop[RenderSkinAndroid::DrawableResolution()]
            / scaleFactor[RenderSkinAndroid::DrawableResolution()]);
}

void RenderSkinCombo::Decode()
{
    if (isDecodingAttempted)
        return;

    isDecodingAttempted = true;
    isDecoded = false;

    android::AssetManager* am = globalAssetManager();

    String drawableDirectory = RenderSkinAndroid::DrawableDirectory();
    Resolution res = RenderSkinAndroid::DrawableResolution();

    isDecoded = RenderSkinAndroid::DecodeBitmap(am, (drawableDirectory + "combobox_nohighlight.png").utf8().data(), &bitmaps[kNormal][FullAsset]);
    isDecoded &= RenderSkinAndroid::DecodeBitmap(am, (drawableDirectory + "combobox_disabled.png").utf8().data(), &bitmaps[kDisabled][FullAsset]);

    int width = bitmaps[kNormal][FullAsset].width();
    int height = bitmaps[kNormal][FullAsset].height();
    SkIRect subset;
    subset.set(width - arrowWidth[res], 0, width, height);
    bitmaps[kNormal][FullAsset].extractSubset(&bitmaps[kNormal][NoBorder], subset);
    bitmaps[kDisabled][FullAsset].extractSubset(&bitmaps[kDisabled][NoBorder], subset);

    // Calculate 9 patch margins.
    SkIRect fullAssetMargin;
    fullAssetMargin.fLeft = stretchMargin[res];
    fullAssetMargin.fTop = stretchMargin[res];
    fullAssetMargin.fRight = arrowMargin[res] + stretchMargin[res];
    fullAssetMargin.fBottom = stretchTop[res];

    SkIRect noBorderMargin;
    noBorderMargin.fLeft = 0;
    noBorderMargin.fTop = stretchTop[res];
    noBorderMargin.fRight = 0;
    noBorderMargin.fBottom = stretchMargin[res];

    margin[FullAsset] = fullAssetMargin;
    margin[NoBorder] = noBorderMargin;
}

bool RenderSkinCombo::Draw(SkCanvas* canvas, Node* element, int x, int y, int width, int height)
{
    if (!isDecodingAttempted)
        Decode();

    if (!isDecoded)
        return true;

    int resolution = RenderSkinAndroid::DrawableResolution();
    State state = (element->isElementNode() && static_cast<Element*>(element)->isEnabledFormControl()) ? kNormal : kDisabled;
    height = std::max(height, (stretchMargin[resolution] * 2));

    SkRect bounds;
    BorderStyle drawBorder = FullAsset;

    bounds.set(SkIntToScalar(x+1), SkIntToScalar(y+1), SkIntToScalar(x + width-1), SkIntToScalar(y + height-1));
    RenderStyle* style = element->renderStyle();
    SkPaint paint;
    paint.setColor(style->visitedDependentColor(CSSPropertyBackgroundColor).rgb());
    canvas->drawRect(bounds, paint);

    bounds.set(SkIntToScalar(x), SkIntToScalar(y), SkIntToScalar(x + width), SkIntToScalar(y + height));

    // If this is an appearance where RenderTheme::paint returns true
    // without doing anything, this means that
    // RenderBox::PaintBoxDecorationWithSize will end up painting the
    // border, so we shouldn't paint a border here.
    if (style->appearance() == MenulistButtonPart ||
        style->appearance() == ListboxPart ||
        style->appearance() == TextFieldPart ||
        style->appearance() == TextAreaPart) {
        bounds.fLeft += SkIntToScalar(width - RenderSkinCombo::extraWidth());
        bounds.fRight -= SkIntToScalar(style->borderRightWidth());
        bounds.fTop += SkIntToScalar(style->borderTopWidth());
        bounds.fBottom -= SkIntToScalar(style->borderBottomWidth());
        drawBorder = NoBorder;
    }
    float scale = scaleFactor[resolution];
    bounds.fLeft = bounds.fLeft * scale;
    bounds.fRight = bounds.fRight * scale;
    bounds.fTop = bounds.fTop * scale;
    bounds.fBottom = bounds.fBottom * scale;
    int count = canvas->save();
    canvas->scale(1.0f / scale, 1.0f / scale);
    SkNinePatch::DrawNine(canvas, bounds, bitmaps[state][drawBorder], margin[drawBorder]);
    canvas->restoreToCount(count);
    return false;
}

}   // namspace WebCore
