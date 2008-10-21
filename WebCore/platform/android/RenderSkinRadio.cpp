/* libs/WebKitLib/WebKit/WebCore/platform/android/RenderSkinRadio.cpp
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

