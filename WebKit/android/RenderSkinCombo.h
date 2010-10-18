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

#ifndef RenderSkinCombo_h
#define RenderSkinCombo_h

#include "RenderSkinAndroid.h"
#include "SkRect.h"

class SkCanvas;

namespace WebCore {

// This is very similar to RenderSkinButton - maybe they should be the same class?
class RenderSkinCombo : public RenderSkinAndroid
{
public:
    /**
     * Initialize the class before use. Uses the AssetManager to initialize any bitmaps the class may use.
     */
    static void Init(android::AssetManager*, String drawableDirectory);

    /**
     * Draw the provided Node on the SkCanvas, using the dimensions provided by
     * x,y,w,h.  Return true if we did not draw, and WebKit needs to draw it,
     * false otherwise.
     */
    static bool Draw(SkCanvas* , Node* , int x, int y, int w, int h);

    // The image is wider than the RenderObject, so this accounts for that.
    static int extraWidth() { return arrowMargin[resolution]; }
    static int padding() { return padMargin[resolution]; }

    enum Resolution {
        MedRes,
        HighRes
    };
private:
    static Resolution resolution;
    const static int arrowMargin[2];
    const static int padMargin[2];
    const static SkIRect margin[2][2];
}; 

} // WebCore

#endif
