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

extern android::AssetManager* globalAssetManager();

static const char* gFiles[] = {
    "btn_default_disabled_holo.9.png",
    "btn_default_normal_holo.9.png",
    "btn_default_focused_holo.9.png",
    "btn_default_pressed_holo.9.png"
    };

namespace WebCore {

RenderSkinButton::RenderSkinButton(String drawableDirectory)
    : m_decoded(false)
    , m_decodingAttempted(false)
    , m_drawableDirectory(drawableDirectory)
{
    // Ensure our enums properly line up with our arrays.
    android::CompileTimeAssert<(RenderSkinAndroid::kDisabled == 0)> a1;
    android::CompileTimeAssert<(RenderSkinAndroid::kNormal == 1)> a2;
    android::CompileTimeAssert<(RenderSkinAndroid::kFocused == 2)> a3;
    android::CompileTimeAssert<(RenderSkinAndroid::kPressed == 3)> a4;
}

void RenderSkinButton::decode()
{
    m_decodingAttempted = true;

    android::AssetManager* am = globalAssetManager();

    for (size_t i = 0; i < 4; i++) {
        String path = m_drawableDirectory;
        path.append(String(gFiles[i]));
        if (!RenderSkinNinePatch::decodeAsset(am, path.utf8().data(), &m_buttons[i])) {
            m_decoded = false;
            LOGE("RenderSkinButton::decode: button assets failed to decode\n\tWebView buttons will not draw");
            return;
        }
    }
    m_decoded = true;
}

void RenderSkinButton::draw(SkCanvas* canvas, const IntRect& r,
                            RenderSkinAndroid::State newState)
{
    if (!m_decodingAttempted)
        decode();

    // If we failed to decode, do nothing.  This way the browser still works,
    // and webkit will still draw the label and layout space for us.
    if (!m_decoded) {
        return;
    }
    
    // Ensure that the state is within the valid range of our array.
    SkASSERT(static_cast<unsigned>(newState) < 
            static_cast<unsigned>(RenderSkinAndroid::kNumStates));

    RenderSkinNinePatch::DrawNinePatch(canvas, SkRect(r), m_buttons[newState]);
}

} //WebCore
