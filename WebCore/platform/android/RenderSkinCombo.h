/* libs/WebKitLib/WebKit/WebCore/platform/android/RenderSkinCombo.h
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

#ifndef RenderSkinCombo_h
#define RenderSkinCombo_h

#include "RenderSkinAndroid.h"
#include "SkBitmap.h"
#include "SkPaint.h"
#include "SkRect.h"

namespace WebCore {

// This is very similar to RenderSkinButton - maybe they should be the same class?
class RenderSkinCombo : public RenderSkinAndroid
{
public:
    RenderSkinCombo();
    virtual ~RenderSkinCombo() {}

    /**
     * Initialize the class before use. Uses the AssetManager to initialize any bitmaps the class may use.
     */
    static void Init(android::AssetManager*);

    virtual bool draw(PlatformGraphicsContext*);
    virtual void notifyState(Node* element);
    virtual void setDim(int width, int height);

    // The image is an extra 30 pixels wider than the RenderObject, so this accounts for that.
    static int extraWidth() { return arrowMargin; }
    
private:
    SkRect m_bounds;    // Maybe this should become a protected member of RenderSkinAndroid...
    static SkBitmap m_bitmap[2];    // Collection of assets for a combo box
    static bool     m_decoded;      // True if all assets were decoded
    SkPaint m_paint;
    // Could probably move m_state into RenderSkinAndroid...
    // Although notice that the state for RenderSkinRadio is just an integer, and it behaves differently
    State m_state;
    
    static const int arrowMargin = 30;
}; 

} // WebCore

#endif
