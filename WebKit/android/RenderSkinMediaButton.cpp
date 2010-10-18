/*
 * Copyright 2010, The Android Open Source Project
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

#define LOG_TAG "WebCore"

#include "config.h"
#include "CString.h"
#include "android_graphics.h"
#include "Document.h"
#include "IntRect.h"
#include "Node.h"
#include "RenderSkinMediaButton.h"
#include "SkCanvas.h"
#include "SkNinePatch.h"
#include "SkRect.h"
#include <utils/Debug.h>
#include <utils/Log.h>

struct PatchData {
    const char* name;
    int8_t outset, margin;
};

static const PatchData gFiles[] =
    {
        { "btn_media_player.9.png", 0, 0 }, // DEFAULT BGD BUTTON
        { "ic_media_pause.png", 0, 0}, // PAUSE
        { "ic_media_play.png", 0, 0 }, // PLAY
        { "ic_media_pause.png", 0, 0 }, // MUTE
        { "ic_media_rew.png", 0, 0 }, // REWIND
        { "ic_media_ff.png", 0, 0 }, // FORWARD
        { "btn_media_player_disabled.9.png", 0, 0 }, // BACKGROUND_SLIDER
        { "btn_media_player_pressed.9.png", 0, 0 },  // SLIDER_TRACK
        { "btn_media_player.9.png", 0, 0 }           // SLIDER_THUMB
    };

static SkBitmap gButton[sizeof(gFiles)/sizeof(gFiles[0])];
static bool gDecoded;
static bool gHighRes;

namespace WebCore {

void RenderSkinMediaButton::Init(android::AssetManager* am, String drawableDirectory)
{
    static bool gInited;
    if (gInited)
        return;

    gInited = true;
    gDecoded = true;
    gHighRes = drawableDirectory[drawableDirectory.length() - 5] == 'h';
    for (size_t i = 0; i < sizeof(gFiles)/sizeof(gFiles[0]); i++) {
        String path = drawableDirectory + gFiles[i].name;
        if (!RenderSkinAndroid::DecodeBitmap(am, path.utf8().data(), &gButton[i])) {
            gDecoded = false;
            LOGD("RenderSkinButton::Init: button assets failed to decode\n\tBrowser buttons will not draw");
            break;
        }
    }
}

void RenderSkinMediaButton::Draw(SkCanvas* canvas, const IntRect& r, int buttonType)
{
    // If we failed to decode, do nothing.  This way the browser still works,
    // and webkit will still draw the label and layout space for us.
    if (!gDecoded) {
        return;
    }

    bool drawsNinePatch = true;
    bool drawsImage = true;
    bool drawsBackgroundColor = false;

    int ninePatchIndex = 0;
    int imageIndex = 0;

    SkRect bounds(r);
    SkScalar imageMargin = 8;
    SkPaint paint;
    SkColor backgroundColor = SkColorSetARGB(255, 200, 200, 200);
    SkColor trackBackgroundColor = SkColorSetARGB(255, 100, 100, 100);

    switch (buttonType) {
    case PAUSE:
    case PLAY:
    case MUTE:
    case REWIND:
    case FORWARD:
    {
         imageIndex = buttonType + 1;
         drawsBackgroundColor = true;
         paint.setColor(backgroundColor);
         break;
    }
    case BACKGROUND_SLIDER:
    {
         drawsImage = false;
         drawsNinePatch = false;
         drawsBackgroundColor = true;
         paint.setColor(backgroundColor);
         break;
    }
    case SLIDER_TRACK:
    {
         drawsImage = false;
         drawsNinePatch = false;
         drawsBackgroundColor = true;
         paint.setColor(trackBackgroundColor);
         bounds.fTop += 8;
         bounds.fBottom -= 8;
         break;
    }
    case SLIDER_THUMB:
    {
         drawsImage = false;
         ninePatchIndex = buttonType + 1;
         break;
    }
    default:
         drawsImage = false;
         drawsNinePatch = false;
    }

    if (drawsBackgroundColor) {
        canvas->drawRect(r, paint);
    }

    if (drawsNinePatch) {
        const PatchData& pd = gFiles[ninePatchIndex];
        int marginValue = pd.margin + pd.outset;

        SkIRect margin;
        margin.set(marginValue, marginValue, marginValue, marginValue);
        SkNinePatch::DrawNine(canvas, bounds, gButton[0], margin);
    }

    if (drawsImage) {
        SkScalar SIZE = gButton[imageIndex].width();
        SkScalar width = r.width();
        SkScalar scale = SkScalarDiv(width - 2*imageMargin, SIZE);
        int saveScaleCount = canvas->save();
        canvas->translate(bounds.fLeft + imageMargin, bounds.fTop + imageMargin);
        canvas->scale(scale, scale);
        canvas->drawBitmap(gButton[imageIndex], 0, 0, &paint);
        canvas->restoreToCount(saveScaleCount);
    }
}

} //WebCore
