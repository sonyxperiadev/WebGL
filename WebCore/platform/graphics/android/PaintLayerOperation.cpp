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

#include "config.h"
#include "PaintLayerOperation.h"

#include "LayerAndroid.h"

bool PaintLayerOperation::operator==(const QueuedOperation* operation)
{
    if (operation->type() != type())
        return false;
    const PaintLayerOperation* op = static_cast<const PaintLayerOperation*>(operation);
    return op->m_layer->uniqueId() == m_layer->uniqueId();
}

void PaintLayerOperation::run()
{
    if (m_layer)
        m_layer->paintBitmapGL();
}

SkLayer* PaintLayerOperation::baseLayer()
{
    if (!m_layer)
        return 0;

    return m_layer->getRootLayer();
}

LayerTexture* PaintLayerOperation::texture()
{
    if (!m_layer)
        return 0;
    return m_layer->texture();
}

bool PaintLayerBaseFilter::check(QueuedOperation* operation)
{
    if (operation->type() == QueuedOperation::PaintLayer) {
        PaintLayerOperation* op = static_cast<PaintLayerOperation*>(operation);
        if (op->baseLayer() == m_baseLayer)
            return true;
    }
    return false;
}

bool PaintLayerTextureFilter::check(QueuedOperation* operation)
{
    if (operation->type() == QueuedOperation::PaintLayer) {
        PaintLayerOperation* op = static_cast<PaintLayerOperation*>(operation);
        if (op->texture() == m_texture)
            return true;
    }
    return false;
}
