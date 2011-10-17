/*
 * Copyright 2011, The Android Open Source Project
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

#ifndef ImageTexture_h
#define ImageTexture_h

#include "ClassTracker.h"
#include "GLUtils.h"
#include "SkBitmap.h"
#include "SkBitmapRef.h"
#include "SkRefCnt.h"
#include "LayerAndroid.h"

namespace WebCore {

class LayerAndroid;

/////////////////////////////////////////////////////////////////////////////////
// Image sharing codepath for layers
/////////////////////////////////////////////////////////////////////////////////
//
// We receive an SkBitmapRef on the webcore thread; from this we create
// an ImageTexture instance and keep it in TilesManager in a hashmap
// (see TilesManager::addImage())
//
// The ImageTexture will recopy the pointed SkBitmap locally (so we can safely
// use it on the texture generation thread), and just use the SkBitmapRef as a
// key.
//
// Layers on the shared image path will ask TilesManager for the corresponding
// ImageTexture, instead of using a PaintedSurface+TiledTexture.
// When the ImageTexture is prepared for the first time, we directly upload
// the bitmap to a texture.
//
// TODO: limit how many ImageTextures can be uploaded in one draw cycle
// TODO: limit the size of ImageTextures (use a TiledTexture when needed)
//
/////////////////////////////////////////////////////////////////////////////////
class ImageTexture {
public:
    ImageTexture(SkBitmapRef* img);
    virtual ~ImageTexture();

    void prepareGL();
    void uploadGLTexture();
    void drawGL(LayerAndroid* painter);
    void drawCanvas(SkCanvas*, SkRect&);
    void retain() { m_refCount++; }
    void release();
    unsigned int refCount() { return m_refCount; }
    SkBitmapRef* imageRef() { return m_imageRef; }
    SkBitmap* bitmap() { return m_image; }

private:

    void deleteTexture();

    SkBitmapRef* m_imageRef;
    SkBitmap* m_image;
    GLuint m_textureId;
    unsigned int m_refCount;
};

} // namespace WebCore

#endif // ImageTexture
