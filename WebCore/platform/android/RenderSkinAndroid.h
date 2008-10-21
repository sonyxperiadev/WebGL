/* libs/WebKitLib/WebKit/WebCore/rendering/RenderSkinAndroid.h
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

#ifndef RenderSkinAndroid_h
#define RenderSkinAndroid_h

namespace android {
    class AssetManager;
}

class SkBitmap;

namespace WebCore {
class Node;
class PlatformGraphicsContext;

/*  RenderSkinAndroid is the base class for all RenderSkins.  Form elements each have a
 *  subclass for drawing themselves.
 */
class RenderSkinAndroid
{
public:
    RenderSkinAndroid();
    virtual ~RenderSkinAndroid() {}
    
    enum State {
        kDisabled,
        kNormal,
        kFocused,
    
        kNumStates
    };

    /**
     * Initialize the Android skinning system. The AssetManager may be used to find resources used
     * in rendering.
     */
    static void Init(android::AssetManager*);
    
    /* DecodeBitmap determines which file to use, with the given fileName of the form 
     * "images/bitmap.png", and uses the asset manager to select the exact one.  It
     * returns true if it successfully decoded the bitmap, false otherwise.
     */
    static bool DecodeBitmap(android::AssetManager* am, const char* fileName, SkBitmap* bitmap);

    /*  draw() tells the skin to draw itself, and returns true if the skin needs
     *  a redraw to animations, false otherwise
     */
    virtual bool draw(PlatformGraphicsContext*) { return false; }
    
    /*  notifyState() checks to see if the element is checked, focused, and enabled
     *  it must be implemented in the subclass
     */
    virtual void notifyState(Node* element) { }
    
    /*  setDim() tells the skin its width and height
     */
    virtual void setDim(int width, int height) { m_width = width; m_height = height; }

protected:
    int                     m_height;
    int                     m_width;

};

} // WebCore

#endif

