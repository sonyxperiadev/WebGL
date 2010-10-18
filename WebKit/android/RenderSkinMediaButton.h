/*
 * Copyright 2010, The Android Open Source Project
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

#ifndef RenderSkinMediaButton_h
#define RenderSkinMediaButton_h

#include "RenderSkinAndroid.h"

class SkCanvas;

namespace WebCore {
class IntRect;
class RenderSkinMediaButton
{
public:
    /**
     * Initialize the class before use. Uses the AssetManager to initialize any
     * bitmaps the class may use.
     */
    static void Init(android::AssetManager*, String drawableDirectory);
    /**
     * Draw the skin to the canvas, using the rectangle for its bounds and the
     * State to determine which skin to use, i.e. focused or not focused.
     */
    static void Draw(SkCanvas* , const IntRect& , int buttonType);
    /**
     * Button types
     */
    enum { PAUSE, PLAY, MUTE, REWIND, FORWARD, BACKGROUND_SLIDER, SLIDER_TRACK, SLIDER_THUMB };
    /**
     * Slider dimensions
     */
    static int sliderThumbWidth() { return 10; }
    static int sliderThumbHeight() { return 30; }

};

} // WebCore
#endif // RenderSkinMediaButton_h
