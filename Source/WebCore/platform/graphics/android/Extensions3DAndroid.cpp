/*
 * Copyright (C) 2011, Sony Ericsson Mobile Communications AB
 * Copyright (C) 2012 Sony Mobile Communications AB
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Sony Ericsson Mobile Communications AB nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SONY ERICSSON MOBILE COMMUNICATIONS AB BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBGL)

#include "Extensions3DAndroid.h"
#include "GraphicsContext3D.h"

#include <GLES2/gl2.h>

namespace WebCore {

Extensions3DAndroid::Extensions3DAndroid(const String& extensions)
    : m_extensions(extensions)
{
}

Extensions3DAndroid::~Extensions3DAndroid()
{
}

bool Extensions3DAndroid::supports(const String& ext)
{
    return m_extensions.contains(ext);
}

void Extensions3DAndroid::ensureEnabled(const String& name)
{
}

bool Extensions3DAndroid::isEnabled(const String& name)
{
    return supports(name);
}

int Extensions3DAndroid::getGraphicsResetStatusARB()
{
    return GraphicsContext3D::NO_ERROR;
}

void Extensions3DAndroid::blitFramebuffer(long srcX0, long srcY0, long srcX1, long srcY1,
                                          long dstX0, long dstY0, long dstX1, long dstY1,
                                          unsigned long mask, unsigned long filter)
{
}

void Extensions3DAndroid::renderbufferStorageMultisample(unsigned long target,
                                                         unsigned long samples,
                                                         unsigned long internalformat,
                                                         unsigned long width,
                                                         unsigned long height)
{
}

Platform3DObject Extensions3DAndroid::createVertexArrayOES()
{
    return 0;
}

void Extensions3DAndroid::deleteVertexArrayOES(Platform3DObject)
{
}

GC3Dboolean Extensions3DAndroid::isVertexArrayOES(Platform3DObject)
{
    return GL_FALSE;
}

void Extensions3DAndroid::bindVertexArrayOES(Platform3DObject)
{
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
