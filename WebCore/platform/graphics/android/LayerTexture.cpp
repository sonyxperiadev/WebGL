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
#include "LayerTexture.h"

#define DOUBLE_BUFFERED_TEXTURE_MINIMUM_ACCESS 3

void LayerTexture::producerUpdate(TextureInfo* textureInfo)
{
    update();
    BackedDoubleBufferedTexture::producerUpdate(textureInfo);
}

void LayerTexture::update()
{
    // FIXME: fix the double buffered texture class instead of doing this
    // this is a stop gap measure and should be removed.
    // Right now we have to update the textures 3 times (one create, two
    // updates) before we can be sure to have a non-corrupted texture
    // to display.
    if (m_textureUpdates < DOUBLE_BUFFERED_TEXTURE_MINIMUM_ACCESS)
        m_textureUpdates++;
}

bool LayerTexture::isReady()
{
    return m_textureUpdates == DOUBLE_BUFFERED_TEXTURE_MINIMUM_ACCESS;
}
