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
#include "android_graphics.h"
#include "Document.h"
#include "IntRect.h"
#include "Node.h"
#include "RenderObject.h"
#include "RenderSkinMediaButton.h"
#include "RenderSlider.h"
#include "SkCanvas.h"
#include "SkNinePatch.h"
#include "SkRect.h"
#include <utils/AssetManager.h>
#include <utils/Debug.h>
#include <utils/Log.h>
#include <wtf/text/CString.h>

extern android::AssetManager* globalAssetManager();

struct PatchData {
    const char* name;
    int8_t outset, margin;
};

static const PatchData gFiles[] =
    {
        { "scrubber_primary_holo.9.png", 0, 0 }, // SLIDER_TRACK, left of the SLIDER_THUMB
        { "ic_media_pause.png", 0, 0}, // PAUSE
        { "ic_media_play.png", 0, 0 }, // PLAY
        { "ic_media_pause.png", 0, 0 }, // MUTE
        { "ic_media_rew.png", 0, 0 }, // REWIND
        { "ic_media_ff.png", 0, 0 }, // FORWARD
        { "ic_media_fullscreen.png", 0, 0 }, // FULLSCREEN
        { "spinner_76_outer_holo.png", 0, 0 }, // SPINNER_OUTER
        { "spinner_76_inner_holo.png", 0, 0 }, // SPINNER_INNER
        { "ic_media_video_poster.png", 0, 0 }, // VIDEO
        { "btn_media_player_disabled.9.png", 0, 0 }, // BACKGROUND_SLIDER
        { "scrubber_track_holo_dark.9.png", 0, 0 },  // SLIDER_TRACK
        { "scrubber_control_normal_holo.png", 0, 0 }      // SLIDER_THUMB
    };

static SkBitmap gButton[sizeof(gFiles)/sizeof(gFiles[0])];
static bool gDecoded;
static bool gDecodingFailed;

namespace WebCore {

void RenderSkinMediaButton::Decode()
{
    String drawableDirectory = RenderSkinAndroid::DrawableDirectory();

    gDecoded = true;
    gDecodingFailed = false;
    android::AssetManager* am = globalAssetManager();
    for (size_t i = 0; i < sizeof(gFiles)/sizeof(gFiles[0]); i++) {
        String path = drawableDirectory + gFiles[i].name;
        if (!RenderSkinAndroid::DecodeBitmap(am, path.utf8().data(), &gButton[i])) {
            gDecodingFailed = true;
            LOGD("RenderSkinButton::Init: button assets failed to decode\n\tBrowser buttons will not draw");
            break;
        }
    }
}

void RenderSkinMediaButton::Draw(SkCanvas* canvas, const IntRect& r, int buttonType,
                                 bool translucent, RenderObject* o)
{
    if (!gDecoded) {
        Decode();
    }

    // If we failed to decode, do nothing.  This way the browser still works,
    // and webkit will still draw the label and layout space for us.
    if (gDecodingFailed)
        return;

    bool drawsNinePatch = false;
    bool drawsImage = true;
    bool drawsBackgroundColor = true;

    int ninePatchIndex = 0;
    int imageIndex = 0;

    SkRect bounds(r);
    SkScalar imageMargin = 8;
    SkPaint paint;

    int alpha = 255;
    if (translucent)
        alpha = 190;

    SkColor backgroundColor = SkColorSetARGB(alpha, 34, 34, 34);
    SkColor trackBackgroundColor = SkColorSetARGB(255, 100, 100, 100);
    paint.setColor(backgroundColor);
    paint.setFlags(SkPaint::kFilterBitmap_Flag);

    switch (buttonType) {
    case PAUSE:
    case PLAY:
    case MUTE:
    case REWIND:
    case FORWARD:
    case FULLSCREEN:
    {
         imageIndex = buttonType + 1;
         paint.setColor(backgroundColor);
         break;
    }
    case SPINNER_OUTER:
    case SPINNER_INNER:
    case VIDEO:
    {
         drawsBackgroundColor = false;
         imageIndex = buttonType + 1;
         break;
    }
    case BACKGROUND_SLIDER:
    {
         drawsBackgroundColor = false;
         drawsImage = false;
         break;
    }
    case SLIDER_TRACK:
    {
         drawsNinePatch = true;
         drawsImage = false;
         ninePatchIndex = buttonType + 1;
         break;
    }
    case SLIDER_THUMB:
    {
         drawsBackgroundColor = false;
         imageMargin = 0;
         imageIndex = buttonType + 1;
         break;
    }
    default:
         return;
    }

    if (drawsBackgroundColor) {
        canvas->drawRect(r, paint);
    }

    if (drawsNinePatch) {
        const PatchData& pd = gFiles[ninePatchIndex];
        int marginValue = pd.margin + pd.outset;

        SkIRect margin;
        margin.set(marginValue, marginValue, marginValue, marginValue);
        if (buttonType == SLIDER_TRACK) {
            // Cut the height in half (with some extra slop determined by trial
            // and error to get the placement just right.
            SkScalar quarterHeight = SkScalarHalf(SkScalarHalf(bounds.height()));
            bounds.fTop += quarterHeight + SkScalarHalf(3);
            bounds.fBottom += -quarterHeight + SK_ScalarHalf;
            if (o && o->isSlider()) {
                RenderSlider* slider = toRenderSlider(o);
                IntRect thumb = slider->thumbRect();
                // Inset the track by half the width of the thumb, so the track
                // does not appear to go beyond the space where the thumb can
                // be.
                SkScalar thumbHalfWidth = SkIntToScalar(thumb.width()/2);
                bounds.fLeft += thumbHalfWidth;
                bounds.fRight -= thumbHalfWidth;
                if (thumb.x() > 0) {
                    // The video is past the starting point.  Show the area to
                    // left of the thumb as having been played.
                    SkScalar alreadyPlayed = SkIntToScalar(thumb.center().x() + r.x());
                    SkRect playedRect(bounds);
                    playedRect.fRight = alreadyPlayed;
                    SkNinePatch::DrawNine(canvas, playedRect, gButton[0], margin);
                    bounds.fLeft = alreadyPlayed;
                }

            }
        }
        SkNinePatch::DrawNine(canvas, bounds, gButton[ninePatchIndex], margin);
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

} // WebCore
