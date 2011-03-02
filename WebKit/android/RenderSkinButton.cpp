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

#define LOG_TAG "WebCore"

#include "config.h"
#include "android_graphics.h"
#include "Document.h"
#include "IntRect.h"
#include "Node.h"
#include "RenderSkinButton.h"
#include "RenderSkinNinePatch.h"
#include "SkCanvas.h"
#include "SkNinePatch.h"
#include "SkRect.h"
#include <utils/Asset.h>
#include <utils/AssetManager.h>
#include <utils/Debug.h>
#include <utils/Log.h>
#include <utils/ResourceTypes.h>
#include <wtf/text/CString.h>

static const char* gFiles[] = {
    "btn_default_disabled_holo.9.png",
    "btn_default_normal_holo.9.png",
    "btn_default_focused_holo.9.png",
    "btn_default_pressed_holo.9.png"
    };

static bool gDecoded;

struct NinePatch {
    SkBitmap m_bitmap;
    void* m_serializedPatchData;
};

static NinePatch gButtons[4];
class SkRegion;

using namespace android;

extern void NinePatch_Draw(SkCanvas* canvas, const SkRect& bounds,
        const SkBitmap& bitmap, const Res_png_9patch& chunk,
        const SkPaint* paint, SkRegion** outRegion);

namespace WebCore {

void RenderSkinButton::Init(android::AssetManager* am, String drawableDirectory)
{
    static bool gInited;
    if (gInited)
        return;

    gInited = true;
    gDecoded = true;
    for (size_t i = 0; i < 4; i++) {
        String path = String(drawableDirectory.impl());
        path.append(String(gFiles[i]));
        android::Asset* asset = am->open(path.utf8().data(), android::Asset::ACCESS_BUFFER);
        if (!asset) {
            asset = am->openNonAsset(path.utf8().data(), android::Asset::ACCESS_BUFFER);
            if (!asset) {
                gDecoded = false;
                LOGE("RenderSkinButton::Init: button assets failed to decode\n\tBrowser buttons will not draw");
                return;
            }
        }
        RenderSkinNinePatch::decodeAsset(asset, &gButtons[i].m_bitmap, &gButtons[i].m_serializedPatchData);
        asset->close();
    }

    // Ensure our enums properly line up with our arrays.
    android::CompileTimeAssert<(RenderSkinAndroid::kDisabled == 0)>     a1;
    android::CompileTimeAssert<(RenderSkinAndroid::kNormal == 1)>       a2;
    android::CompileTimeAssert<(RenderSkinAndroid::kFocused == 2)>      a3;
    android::CompileTimeAssert<(RenderSkinAndroid::kPressed == 3)>      a4;
}

void RenderSkinButton::Draw(SkCanvas* canvas, const IntRect& r, RenderSkinAndroid::State newState)
{
    // If we failed to decode, do nothing.  This way the browser still works,
    // and webkit will still draw the label and layout space for us.
    if (!gDecoded) {
        return;
    }
    
    // Ensure that the state is within the valid range of our array.
    SkASSERT(static_cast<unsigned>(newState) < 
            static_cast<unsigned>(RenderSkinAndroid::kNumStates));

    SkRect bounds(r);
    NinePatch* patch = &gButtons[newState];
    Res_png_9patch* data = Res_png_9patch::deserialize(patch->m_serializedPatchData);
    NinePatch_Draw(canvas, bounds, patch->m_bitmap, *data, 0, 0);
}

} //WebCore
