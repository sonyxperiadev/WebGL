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

#include "GLUtils.h"
#include "SkBitmap.h"
#include "SkBitmapRef.h"
#include "SkPicture.h"
#include "SkRefCnt.h"
#include "LayerAndroid.h"

namespace WebCore {

class LayerAndroid;
class TexturesResult;
class TiledTexture;

/////////////////////////////////////////////////////////////////////////////////
// Image sharing codepath for layers
/////////////////////////////////////////////////////////////////////////////////
//
// Layers containing only an image take a slightly different codepath;
// GraphicsLayer::setContentsToImage() is called on the webcore thread,
// passing an Image instance. We get the native image (an SkBitmap) and create
// an ImageTexture instance with it (or increment the refcount of an existing
// instance if the SkBitmap is similar to one already stored in ImagesManager,
// i.e. if two GraphicsLayer share the same image).
//
// To detect if an image is similar, we compute and use a CRC. Each ImageTexture
// is stored in ImagesManager using its CRC as a hash key.
// Simply comparing the address is not enough -- different image could end up
// at the same address (i.e. the image is deallocated then a new one is
// reallocated at the old address)
//
// Each ImageTexture's CRC being unique, LayerAndroid instances simply store that
// and retain/release the corresponding ImageTexture (so that
// queued painting request will work correctly and not crash...).
// LayerAndroid running on the UI thread will get the corresponding
// ImageTexture at draw time.
//
// ImageTexture recopy the original SkBitmap so that they can safely be used
// on a different thread; it uses TiledTexture to allocate and paint the image,
// so that we can share the same textures and limits as the rest of the layers.
//
/////////////////////////////////////////////////////////////////////////////////
class ImageTexture : public SurfacePainter {
public:
    ImageTexture(SkBitmap* bmp, unsigned crc);
    virtual ~ImageTexture();

    bool prepareGL(GLWebViewState*);
    void drawGL(LayerAndroid* painter);
    void drawCanvas(SkCanvas*, SkRect&);
    bool hasContentToShow();
    SkBitmap* bitmap() { return m_image; }
    unsigned imageCRC() { return m_crc; }

    static SkBitmap* convertBitmap(SkBitmap* bitmap);

    static unsigned computeCRC(const SkBitmap* bitmap);
    bool equalsCRC(unsigned crc);

    // methods used by TiledTexture
    virtual const TransformationMatrix* transform();
    virtual float opacity();

    int nbTextures();

    virtual SurfaceType type() { return SurfacePainter::ImageSurface; }

private:

    SkBitmapRef* m_imageRef;
    SkBitmap* m_image;
    TiledTexture* m_texture;
    LayerAndroid* m_layer;
    SkPicture* m_picture;
    TransformationMatrix m_layerMatrix;
    unsigned m_crc;
};

} // namespace WebCore

#endif // ImageTexture
