/* libs/WebKitLib/WebKit/WebCore/platform/android/RenderSkinCombo.cpp
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
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
