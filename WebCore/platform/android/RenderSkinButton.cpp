/* 
 * Copyright 2006, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *
 *     http://www.apache.org/licenses/LICENSE-2.0 
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License.
 */

#include "config.h"
#include "AndroidLog.h"
#include "android_graphics.h"
#include "Document.h"
#include "IntRect.h"
#include "Node.h"
#include "RenderSkinButton.h"
#include "SkCanvas.h"
#include "SkNinePatch.h"
#include "SkRect.h"
#include "utils/Debug.h"

struct PatchData {
    const char* name;
    int8_t outset, margin;
};

static const PatchData gFiles[] =
    {
        { "res/drawable/btn_default_normal_disable.9.png", 2, 7 },
        { "res/drawable/btn_default_normal.9.png", 2, 7 },
        { "res/drawable/btn_default_selected.9.png", 2, 7 }
    };

static SkBitmap gButton[sizeof(gFiles)/sizeof(gFiles[0])];
static bool     gDecoded;

namespace WebCore {

void RenderSkinButton::Init(android::AssetManager* am)
{
    static bool gInited;
    if (gInited)
        return;

    gInited = true;
    gDecoded = true;
    for (size_t i = 0; i < sizeof(gFiles)/sizeof(gFiles[0]); i++) {
        if (!RenderSkinAndroid::DecodeBitmap(am, gFiles[i].name, &gButton[i])) {
            gDecoded = false;
            ANDROID_LOGD("RenderSkinButton::Init: button assets failed to decode\n\tBrowser buttons will not draw");
            break;
        }
    }

    // Ensure our enums properly line up with our arrays.
    android::CompileTimeAssert<(RenderSkinAndroid::kDisabled == 0)> a1;
    android::CompileTimeAssert<(RenderSkinAndroid::kNormal == 1)> a2;
    android::CompileTimeAssert<(RenderSkinAndroid::kFocused == 2)> a3;
}

void RenderSkinButton::Draw(SkCanvas* canvas, const IntRect& r, RenderSkinAndroid::State newState)
{
    // If we failed to decode, do nothing.  This way the browser still works,
    // and webkit will still draw the label and layout space for us.
    if (!gDecoded) {
        return;
    }
    
    // Ensure that the state is within the valid range of our array.
    SkASSERT(newState < RenderSkinAndroid::kNumStates && newState >= 0);

    // Set up the ninepatch information for drawing.
    SkRect bounds;
    android_setrect(&bounds, r);
    const PatchData& pd = gFiles[newState];
    int marginValue = pd.margin + pd.outset;

    SkIRect margin;

    margin.set(marginValue, marginValue, marginValue, marginValue);
    
    // Draw to the canvas.
    SkNinePatch::DrawNine(canvas, bounds, gButton[newState], margin);
}

} //WebCore

