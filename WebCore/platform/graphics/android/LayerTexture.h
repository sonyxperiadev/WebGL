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

#ifndef LayerTexture_h
#define LayerTexture_h

#include "BackedDoubleBufferedTexture.h"

namespace WebCore {

class LayerTexture : public BackedDoubleBufferedTexture {
 public:
    LayerTexture(uint32_t w, uint32_t h,
                 SkBitmap::Config config = SkBitmap::kARGB_8888_Config)
        : BackedDoubleBufferedTexture(w, h, config)
        , m_id(0)
        , m_pictureUsed(0)
        , m_textureUpdates(0)
    {}

    int id() { return m_id; }
    void setId(int id) { m_id = id; }

    unsigned int pictureUsed() { return m_pictureUsed; }
    void setPictureUsed(unsigned pictureUsed) { m_pictureUsed = pictureUsed; }
    bool isReady();
    virtual void producerUpdate(TextureInfo* textureInfo);

 private:
    void update();

    int m_id;
    unsigned int m_pictureUsed;
    unsigned int m_textureUpdates;
};

} // namespace WebCore

#endif // LayerTexture_h
