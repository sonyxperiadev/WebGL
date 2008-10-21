/* libs/WebKitLib/WebKit/WebCore/platform/android/RenderSkinButton.h
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

#ifndef RenderSkinButton_h
#define RenderSkinButton_h

#include "RenderSkinAndroid.h"

class SkCanvas;

namespace WebCore {
class IntRect;
class RenderSkinButton
{
public:
    /**
     * Initialize the class before use. Uses the AssetManager to initialize any 
     * bitmaps the class may use.
     */
    static void Init(android::AssetManager*);
    /**
     * Draw the skin to the canvas, using the rectangle for its bounds and the 
     * State to determine which skin to use, i.e. focused or not focused.
     */
    static void Draw(SkCanvas* , const IntRect& , RenderSkinAndroid::State);
};

} // WebCore
#endif

