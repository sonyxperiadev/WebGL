/*
 * Copyright (C) 2011, Sony Ericsson Mobile Communications AB
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

#ifndef WebGLLayer_h
#define WebGLLayer_h

#include "config.h"

#if ENABLE(WEBGL) && USE(ACCELERATED_COMPOSITING)

#include "GLUtils.h"
#include "LayerAndroid.h"
#include <jni.h>

namespace WebCore {

class GraphicsContext3DInternal;

class WebGLLayer : public LayerAndroid {
public:
    WebGLLayer();
    WebGLLayer(const WebGLLayer& layer);

    virtual LayerAndroid* copy() const { return new WebGLLayer(*this); }
    virtual bool drawGL();

    void setGraphicsContext(GraphicsContext3DInternal* context) { m_context = context; }
    
private:
    GraphicsContext3DInternal* m_context;
};

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(ACCELERATED_COMPOSITING)
#endif // WebGLLayer_h
