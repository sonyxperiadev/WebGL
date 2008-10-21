/* 
 *
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

#ifndef platform_graphics_context_h
#define platform_graphics_context_h

#include "IntRect.h"
#include "RenderSkinAndroid.h"
#include "RenderSkinButton.h"
#include "SkCanvas.h"
#include "SkPicture.h"
#include "SkTDArray.h"

class SkCanvas;
class WebCore::Node;

class Container {
public:
    Container(WebCore::Node* node, const WebCore::IntRect& r, 
            WebCore::RenderSkinAndroid::State is) 
        : m_node(node), m_rect(r), m_state(is)
    { 
        m_picture = new SkPicture; 
    }
    
    ~Container() 
    { 
        m_picture->unref(); 
    }
    
    bool matches(WebCore::Node* match) { return m_node == match; }
    
    // Provide a pointer to our SkPicture.
    SkPicture* picture() { return m_picture; }
    
    // Update the focus state of this button, depending on whether it 
    // corresponds to the focused node passed in.  If its state has changed,
    // re-record to the subpicture, so the master picture will reflect the
    // change.
    void updateFocusState(WebCore::Node* focus)
    {
        WebCore::RenderSkinAndroid::State state = m_node == focus ? 
                WebCore::RenderSkinAndroid::kFocused : WebCore::RenderSkinAndroid::kNormal;
        if (state == m_state)
            return;
        m_state = state;
        SkCanvas* canvas = m_picture->beginRecording(m_rect.width(), m_rect.height());
        WebCore::RenderSkinButton::Draw(canvas, m_rect, state);
        m_picture->endRecording();
    }
private:
    // Mark copy and assignment private so noone can use them.
    Container& operator=(const Container& src) { return *this; }
    Container(const Container& src) { }
    // Only used for comparison, since after it is stored it will be transferred
    // to the UI thread.
    WebCore::Node*                      m_node;
    // The rectangle representing the bounds of the button.
    WebCore::IntRect                    m_rect;
    // An SkPicture that, thanks to storeButtonInfo, is pointed to by the master
    // picture, so that we can rerecord this button without rerecording the 
    // world.
    SkPicture*                          m_picture;
    // The state of the button - Currently kFocused or kNormal (and kDisabled
    // as an initial value), but could be expanded to include other states.
    WebCore::RenderSkinAndroid::State   m_state;
};

namespace WebCore {

class PlatformGraphicsContext {
public:
    PlatformGraphicsContext();
    PlatformGraphicsContext(SkCanvas* canvas);
    ~PlatformGraphicsContext();

    SkCanvas*                   mCanvas;
    
    bool deleteUs() const { return m_deleteCanvas; }
    // If our graphicscontext has a button list, add a new container for the
    // nod/rect, and record a new subpicture for this node/button in the current
    // mCanvas
    void storeButtonInfo(Node* node, const IntRect& r);
    // Detaches button array (if any), returning it to the caller and setting our
    // internal ptr to NULL
    SkTDArray<Container*>* getAndClearButtonInfo();
private:
    bool                     m_deleteCanvas;
    SkTDArray<Container*>*   m_buttons;
};

}
#endif

