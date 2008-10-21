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

#include "config.h"
#include "Node.h"
#include "PlatformGraphicsContext.h"
#include "SkCanvas.h"

namespace WebCore {
        
PlatformGraphicsContext::PlatformGraphicsContext(SkCanvas* canvas) : mCanvas(canvas), m_deleteCanvas(false)
{
    // This is useful only if this Canvas is part of an SkPicture object.
    m_buttons = new SkTDArray<Container*>;
}

PlatformGraphicsContext::PlatformGraphicsContext() : m_deleteCanvas(true)
{
    mCanvas = new SkCanvas;
    // Since this is our own private SkCanvas, and has no relation to a picture
    // storing button references would be meaningless.
    m_buttons = NULL;
}

PlatformGraphicsContext::~PlatformGraphicsContext()
{
    if (m_deleteCanvas) {
//        printf("-------------------- deleting offscreen canvas\n");
        delete mCanvas;
    }
    if (m_buttons != NULL) {
        m_buttons->deleteAll();
        delete m_buttons;
    }
}

SkTDArray<Container*>* PlatformGraphicsContext::getAndClearButtonInfo()
{
    // The caller is now responsible for deleting the array
    SkTDArray<Container*>* buttons = m_buttons;
    m_buttons = NULL;
    return buttons;
}

void PlatformGraphicsContext::storeButtonInfo(Node* node, const IntRect& r)
{
    if (m_buttons == NULL)
        return;
    // Initialize all of the nodes to have disabled state, so that we guarantee 
    // that we paint all of them the first time.
    RenderSkinAndroid::State state = RenderSkinAndroid::kDisabled;
    Container* container = new Container(node, r, state);
    // Place a reference to our subpicture in the Picture.
    mCanvas->drawPicture(*(container->picture()));
    // Keep track of the information about the button.
    *m_buttons->append() = container;
}

}   // WebCore
