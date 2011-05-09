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

#ifndef PaintLayerOperation_h
#define PaintLayerOperation_h

#include "QueuedOperation.h"

class SkLayer;

namespace WebCore {

class LayerAndroid;
class LayerTexture;

class PaintLayerOperation : public QueuedOperation {
 public:
    PaintLayerOperation(LayerAndroid* layer)
        : QueuedOperation(QueuedOperation::PaintLayer, 0)
        , m_layer(layer) {}
    virtual ~PaintLayerOperation() {}
    virtual bool operator==(const QueuedOperation* operation);
    virtual void run();
    SkLayer* baseLayer();
    LayerAndroid* layer() { return m_layer; }
    LayerTexture* texture();

 private:
    LayerAndroid* m_layer;
};

class PaintLayerBaseFilter : public OperationFilter {
 public:
    PaintLayerBaseFilter(SkLayer* layer) : m_baseLayer(layer) {}
    virtual bool check(QueuedOperation* operation);

 private:
    SkLayer* m_baseLayer;
};

class PaintLayerTextureFilter : public OperationFilter {
 public:
    PaintLayerTextureFilter(LayerTexture* texture) : m_texture(texture) {}
    virtual bool check(QueuedOperation* operation);

 private:
    LayerTexture* m_texture;
};

}

#endif // PaintLayerOperation_h
