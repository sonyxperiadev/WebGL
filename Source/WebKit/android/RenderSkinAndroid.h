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

#ifndef RenderSkinAndroid_h
#define RenderSkinAndroid_h

#include "PlatformString.h"

namespace android {
    class AssetManager;
}

class SkBitmap;

namespace WebCore {
class Node;
class RenderSkinButton;

class RenderSkinAndroid
{
public:
    enum State {
        kDisabled,
        kNormal,
        kFocused,
        kPressed,
    
        kNumStates
    };

    enum Resolution {
        MedRes,
        HighRes,
        ExtraHighRes,
        ResolutionCount // Keep at the end
    };

    RenderSkinAndroid(String drawableDirectory);
    ~RenderSkinAndroid();
    
    /* DecodeBitmap determines which file to use, with the given fileName of the form 
     * "images/bitmap.png", and uses the asset manager to select the exact one.  It
     * returns true if it successfully decoded the bitmap, false otherwise.
     */
    static bool DecodeBitmap(android::AssetManager* am, const char* fileName, SkBitmap* bitmap);

    static String DrawableDirectory() { return s_drawableDirectory; }
    static Resolution DrawableResolution() { return s_drawableResolution; }

    RenderSkinButton* renderSkinButton() const { return m_button; }

private:
    static String s_drawableDirectory;
    static Resolution s_drawableResolution;
    RenderSkinButton* m_button;
};

} // WebCore

#endif
