/* libs/WebKitLib/WebKit/WebCore/platform/android/RenderSkinRadio.h
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

#ifndef RenderSkinRadio_h
#define RenderSkinRadio_h

#include "RenderSkinAndroid.h"
#include "SkBitmap.h"
#include "SkPaint.h"
#include "SkRect.h"

namespace WebCore {

class Node;

/* RenderSkin for a radio button or a checkbox
 */
class RenderSkinRadio : public RenderSkinAndroid
{
public:
    /* This skin represents a checkbox if isCheckBox is true, otherwise it is a radio button */
    RenderSkinRadio(bool isCheckBox);
    virtual ~RenderSkinRadio() {}
    
    /**
     * Initialize the class before use. Uses the AssetManager to initialize any bitmaps the class may use.
     */
    static void Init(android::AssetManager*);

    virtual bool draw(PlatformGraphicsContext*);
    virtual void notifyState(Node* element);
    virtual void setDim(int width, int height) { RenderSkinAndroid::setDim(width, height); m_size = SkIntToScalar(height); }

protected:
    static SkBitmap m_bitmap[4];  // Bitmaps representing all states
    static bool     m_decoded;    // True if all assets were decoded.
    bool        m_isCheckBox;
    bool        m_checked;
    bool        m_enabled;
    SkPaint     m_paint;    
    SkScalar    m_size;
};

} // WebCore
#endif
