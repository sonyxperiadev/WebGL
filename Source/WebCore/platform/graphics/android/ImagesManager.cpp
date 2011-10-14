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
#include "ImagesManager.h"

#include "ImageTexture.h"

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "ImagesManager", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "ImagesManager", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

ImagesManager* ImagesManager::instance()
{
    if (!gInstance)
        gInstance = new ImagesManager();

    return gInstance;
}

ImagesManager* ImagesManager::gInstance = 0;

void ImagesManager::addImage(SkBitmapRef* imgRef)
{
    if (!imgRef)
        return;

    android::Mutex::Autolock lock(m_imagesLock);
    if (!m_images.contains(imgRef))
        m_images.set(imgRef, new ImageTexture(imgRef));
}

void ImagesManager::removeImage(SkBitmapRef* imgRef)
{
    android::Mutex::Autolock lock(m_imagesLock);
    if (!m_images.contains(imgRef))
        return;

    ImageTexture* image = m_images.get(imgRef);
    image->release();

    if (!image->refCount()) {
        m_images.remove(imgRef);
        delete image;
    }
}

void ImagesManager::showImages()
{
    XLOGC("We have %d images", m_images.size());
    HashMap<SkBitmapRef*, ImageTexture*>::iterator end = m_images.end();
    int i = 0;
    for (HashMap<SkBitmapRef*, ImageTexture*>::iterator it = m_images.begin(); it != end; ++it) {
        XLOGC("Image %x (%d/%d) has %d references", it->first, i,
              m_images.size(), it->second->refCount());
        i++;
    }
}

ImageTexture* ImagesManager::getTextureForImage(SkBitmapRef* img, bool retain)
{
    android::Mutex::Autolock lock(m_imagesLock);
    ImageTexture* image = m_images.get(img);
    if (retain && image)
        image->retain();
    return image;
}

void ImagesManager::scheduleTextureUpload(ImageTexture* texture)
{
    if (m_imagesToUpload.contains(texture))
        return;

    texture->retain();
    m_imagesToUpload.append(texture);
}

bool ImagesManager::uploadTextures()
{
    // scheduleUpload and uploadTextures are called on the same thread
    if (!m_imagesToUpload.size())
        return false;
    ImageTexture* texture = m_imagesToUpload.last();
    texture->uploadGLTexture();
    m_imagesToUpload.removeLast();
    removeImage(texture->imageRef());
    return m_imagesToUpload.size();
}

} // namespace WebCore
