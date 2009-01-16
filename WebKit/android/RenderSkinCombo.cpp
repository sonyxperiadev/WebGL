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
#include "RenderSkinCombo.h"
#include "SkCanvas.h"
#include "SkNinePatch.h"

namespace WebCore {

static const int margin = 2;
static const SkIRect mar = { margin, margin, RenderSkinCombo::extraWidth(), margin };

SkBitmap RenderSkinCombo::m_bitmap[2];
bool     RenderSkinCombo::m_decoded;
    
RenderSkinCombo::RenderSkinCombo()
{
    m_height = 20;
    m_width = 75;
    m_state = kNormal;
    m_bounds.set(0, 0, SkIntToScalar(m_width), SkIntToScalar(m_height));
}

void RenderSkinCombo::Init(android::AssetManager* am)
{
    if (m_decoded)
        return;
    // Maybe short circuiting is fine, since I don't even draw if one state is not decoded properly
    // but is that necessary in the final version?
    m_decoded = RenderSkinAndroid::DecodeBitmap(am, "images/combobox-noHighlight.png", &m_bitmap[kNormal]);
    m_decoded = RenderSkinAndroid::DecodeBitmap(am, "images/combobox-disabled.png", &m_bitmap[kDisabled]) && m_decoded;
}


bool RenderSkinCombo::draw(PlatformGraphicsContext *p)
{
    // The text is drawn right next to the left side - should I draw the box slightly to the left?
    if (!m_decoded)
        return false;
    SkCanvas* canvas = p->mCanvas;
    SkNinePatch::DrawNine(canvas, m_bounds, m_bitmap[m_state], mar);
    return false;
}

void RenderSkinCombo::notifyState(Node* element)
{
    m_state = kDisabled;
    if (!element)
        return;
    if (element->isEnabled())
        m_state = kNormal;
}

void RenderSkinCombo::setDim(int width, int height)
{
    if (width != m_width || height != m_height) {
        m_width = width;
        if (height < (margin<<1) + 1)
            height = (margin<<1) + 1;
        else
           m_height = height;
        m_bounds.set(0, 0, SkIntToScalar(m_width), SkIntToScalar(m_height));
    }
}

}   //WebCore
