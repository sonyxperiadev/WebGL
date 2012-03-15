/*
 * Copyright (C) 2011, 2012 Sony Ericsson Mobile Communications AB
 * Copyright (C) 2012 Sony Mobile Communications AB
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

#if ENABLE(WEBGL) && USE(ACCELERATED_COMPOSITING)

#include "WebGLLayer.h"

#include "GraphicsContext3DInternal.h"
#include "SkRect.h"
#include "TilesManager.h"
#include "TransformationMatrix.h"

namespace WebCore {

WebGLLayer::WebGLLayer(GraphicsContext3DProxy* proxy)
    : LayerAndroid((RenderLayer*)0)
    , m_proxy(proxy)
{
    m_proxy->incr();
}

WebGLLayer::WebGLLayer(const WebGLLayer& layer)
    : LayerAndroid(layer)
    , m_proxy(layer.m_proxy)
{
    m_proxy->incr();
}

WebGLLayer::~WebGLLayer()
{
    m_proxy->decr();
}

bool WebGLLayer::drawGL()
{
    bool askScreenUpdate = false;

    askScreenUpdate |= LayerAndroid::drawGL();

    if (m_proxy.get()) {
        GLuint texture;
        SkRect localBounds;
        bool locked = m_proxy->lockFrontBuffer(texture, localBounds);
        if (locked) {
            // Flip the y-coordinate
            TransformationMatrix transform = m_drawTransform;
            transform = transform.translate(0, 2 * localBounds.top() + localBounds.height());
            transform = transform.scale3d(1.0, -1.0, 1.0);
            TilesManager::instance()->shader()->drawLayerQuad(transform, localBounds,
                                                              texture, 1.0f, true, GL_TEXTURE_EXTERNAL_OES);
            m_proxy->releaseFrontBuffer();
        }
    }

    return askScreenUpdate;
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(ACCELERATED_COMPOSITING)
