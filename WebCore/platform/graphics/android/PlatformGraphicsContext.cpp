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

PlatformGraphicsContext::PlatformGraphicsContext(SkCanvas* canvas, WTF::Vector<Container>* buttons)
        : mCanvas(canvas), m_deleteCanvas(false), m_buttons(buttons)
{
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
}

void PlatformGraphicsContext::storeButtonInfo(Node* node, const IntRect& r)
{
    if (m_buttons == NULL)
        return;
    // Check to see if we already have a Container for this node.  If so, update
    // it with the new rectangle and make the new recording canvas reference
    // its picture.
    Container* end = m_buttons->end();
    for (Container* ptr = m_buttons->begin(); ptr != end; ptr++) {
        if (ptr->matches(node)) {
            mCanvas->drawPicture(*(ptr->picture()));
            ptr->setRect(r);
            return;
        }
    }
    // We did not have a Container representing this node, so create a new one.
    Container container(node, r);
    // Place a reference to our subpicture in the Picture.
    mCanvas->drawPicture(*(container.picture()));
    // Keep track of the information about the button.
    m_buttons->append(container);
}

}   // WebCore
