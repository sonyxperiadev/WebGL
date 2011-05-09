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

#include "config.h"
#include "LayerTexture.h"

#include "LayerAndroid.h"

namespace WebCore {

unsigned int LayerTexture::pictureUsed()
{
      consumerLock();
      TextureTileInfo* info = m_texturesInfo.get(getReadableTexture());
      unsigned int pictureUsed = 0;
      if (info)
          pictureUsed = info->m_picture;
      consumerRelease();
      return pictureUsed;
}

void LayerTexture::setTextureInfoFor(LayerAndroid* layer)
{
      TextureTileInfo* textureInfo = m_texturesInfo.get(getWriteableTexture());
      if (!textureInfo) {
          textureInfo = new TextureTileInfo();
      }
      textureInfo->m_layerId = layer->uniqueId();
      textureInfo->m_picture = layer->pictureUsed();
      textureInfo->m_scale = layer->getScale();
      m_texturesInfo.set(getWriteableTexture(), textureInfo);
      m_layerId = layer->uniqueId();
      m_scale = layer->getScale();
      if (!m_ready)
          m_ready = true;
}

bool LayerTexture::readyFor(LayerAndroid* layer)
{
      TextureTileInfo* info = m_texturesInfo.get(getReadableTexture());
      if (info &&
          info->m_layerId == layer->uniqueId() &&
          info->m_scale == layer->getScale() &&
          info->m_picture == layer->pictureUsed())
          return true;
      return false;
}

} // namespace WebCore
