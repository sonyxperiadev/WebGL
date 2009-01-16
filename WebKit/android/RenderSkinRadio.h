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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
