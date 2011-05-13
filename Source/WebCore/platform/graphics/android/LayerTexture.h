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
#include "ClassTracker.h"
#include "IntRect.h"

namespace WebCore {

class LayerTexture : public BackedDoubleBufferedTexture {
 public:
    LayerTexture(uint32_t w, uint32_t h,
                 SkBitmap::Config config = SkBitmap::kARGB_8888_Config)
        : BackedDoubleBufferedTexture(w, h, 0, config)
        , m_layerId(0)
        , m_scale(1)
        , m_ready(false)
    {
#ifdef DEBUG_COUNT
        ClassTracker::instance()->increment("LayerTexture");
#endif
    }
    virtual ~LayerTexture()
    {
#ifdef DEBUG_COUNT
        ClassTracker::instance()->decrement("LayerTexture");
#endif
    };

    void setTextureInfoFor(LayerAndroid* layer);
    bool readyFor(LayerAndroid* layer);
    void setRect(const IntRect& r) { m_rect = r; }
    IntRect& rect() { return m_rect; }
    int id() { return m_layerId; }
    float scale() { return m_scale; }
    void setId(int id) { m_layerId = id; }
    void setScale(float scale) { m_scale = scale; }
    bool ready() { return m_ready; }
    unsigned int pictureUsed();

 private:

    IntRect m_rect;
    int m_layerId;
    float m_scale;
    bool m_ready;
};

} // namespace WebCore

#endif // LayerTexture_h
