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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

PlatformGraphicsContext::PlatformGraphicsContext()
        : mCanvas(new SkCanvas), m_deleteCanvas(true), m_buttons(0)
{
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
