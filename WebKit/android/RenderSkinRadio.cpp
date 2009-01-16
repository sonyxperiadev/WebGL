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
#include "Document.h"
#include "Node.h"
#include "PlatformGraphicsContext.h"
#include "RenderSkinRadio.h"
#include "SkCanvas.h"

static const char* checks[] = { "res/drawable/checkbox_off_background.png", "res/drawable/checkbox_on_background.png",
                                "res/drawable/radiobutton_off_background.png", "res/drawable/radiobutton_on_background.png"};
static const SkScalar SIZE = SkIntToScalar(19); // Default height and width - corresponds with the bitmap - perhaps we should query the bitmap.

namespace WebCore {

SkBitmap RenderSkinRadio::m_bitmap[4];
bool     RenderSkinRadio::m_decoded;

RenderSkinRadio::RenderSkinRadio(bool isCheckBox)
{
    m_checked = false;
    m_enabled = true;
    m_isCheckBox = isCheckBox;
}

void RenderSkinRadio::Init(android::AssetManager* am)
{
    if (m_decoded)
        return;
    m_decoded = RenderSkinAndroid::DecodeBitmap(am, checks[0], &m_bitmap[0]);
    m_decoded = RenderSkinAndroid::DecodeBitmap(am, checks[1], &m_bitmap[1]) && m_decoded;
    m_decoded = RenderSkinAndroid::DecodeBitmap(am, checks[2], &m_bitmap[2]) && m_decoded;
    m_decoded = RenderSkinAndroid::DecodeBitmap(am, checks[3], &m_bitmap[3]) && m_decoded;
}
    

bool RenderSkinRadio::draw(PlatformGraphicsContext* pgc)
{
    if (!m_decoded) // Seems like an unnecessary slowdown, since it should always decode
        return false;
    SkCanvas* canvas = pgc->mCanvas;
    if (!m_enabled) {
        SkRect r;
        r.set(0, 0, m_size, m_size);
        canvas->saveLayerAlpha(&r, 0x80);
    } else {
        canvas->save();
    }
    if (SIZE != m_size) {
        SkScalar scale = SkScalarDiv(m_size, SIZE);
        canvas->scale(scale, scale);
    }
    canvas->drawBitmap(m_bitmap[m_checked + 2*(!m_isCheckBox)], 0, 0, &m_paint);
    canvas->restore();
    return false; // True if we need to redraw
}

void RenderSkinRadio::notifyState(Node* element)
{
    if (!element) {
        return;
    }
    m_checked = element->isChecked();
    m_enabled = element->isEnabled();
}

} //WebCore

