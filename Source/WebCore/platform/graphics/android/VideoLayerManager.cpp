/*
 * Copyright 2011 The Android Open Source Project
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
#include "VideoLayerManager.h"

#if USE(ACCELERATED_COMPOSITING)

#ifdef DEBUG
#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "VideoLayerManager", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

// Define the max sum of all the video's sizes.
// Note that video_size = width * height. If there is no compression, then the
// maximum memory consumption could be 4 * video_size.
// Setting this to 2M, means that maximum memory consumption of all the
// screenshots would not be above 8M.
#define MAX_VIDEOSIZE_SUM 2097152

namespace WebCore {

VideoLayerManager::VideoLayerManager()
{
    m_currentTimeStamp = 0;
}

// Getting TextureId for GL draw call, in the UI thread.
GLuint VideoLayerManager::getTextureId(const int layerId)
{
    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    GLuint result = 0;
    if (m_videoLayerInfoMap.contains(layerId))
        result = m_videoLayerInfoMap.get(layerId)->textureId;
    return result;
}

// Getting matrix for GL draw call, in the UI thread.
GLfloat* VideoLayerManager::getMatrix(const int layerId)
{
    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    GLfloat* result = 0;
    if (m_videoLayerInfoMap.contains(layerId))
        result = m_videoLayerInfoMap.get(layerId)->surfaceMatrix;
    return result;
}

int VideoLayerManager::getTotalMemUsage()
{
    int sum = 0;
    InfoIterator end = m_videoLayerInfoMap.end();
    for (InfoIterator it = m_videoLayerInfoMap.begin(); it != end; ++it)
        sum += it->second->videoSize;
    return sum;
}

// When the video start, we know its texture info, so we register when we
// recieve the setSurfaceTexture call, this happens on UI thread.
void VideoLayerManager::registerTexture(const int layerId, const GLuint textureId)
{
    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    // If the texture has been registered, then early return.
    if (m_videoLayerInfoMap.get(layerId)) {
        GLuint oldTextureId = m_videoLayerInfoMap.get(layerId)->textureId;
        if (oldTextureId != textureId)
            removeLayerInternal(layerId);
        else
            return;
    }
    // The old info is deleted and now complete the new info and store it.
    VideoLayerInfo* pInfo = new VideoLayerInfo();
    pInfo->textureId = textureId;
    memset(pInfo->surfaceMatrix, 0, sizeof(pInfo->surfaceMatrix));
    pInfo->videoSize = 0;
    m_currentTimeStamp++;
    pInfo->timeStamp = m_currentTimeStamp;

    m_videoLayerInfoMap.add(layerId, pInfo);
    XLOG("GL texture %d regisered for layerId %d", textureId, layerId);

    return;
}

// Only when the video is prepared, we got the video size. So we should update
// the size for the video accordingly.
// This is called from webcore thread, from MediaPlayerPrivateAndroid.
void VideoLayerManager::updateVideoLayerSize(const int layerId, const int size )
{
    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    if (m_videoLayerInfoMap.contains(layerId)) {
        VideoLayerInfo* pInfo = m_videoLayerInfoMap.get(layerId);
        if (pInfo)
            pInfo->videoSize = size;
    }

    // If the memory usage is out of bound, then just delete the oldest ones.
    // Because we only recycle the texture before the current timestamp, the
    // current video's texture will not be deleted.
    while (getTotalMemUsage() > MAX_VIDEOSIZE_SUM)
        if (!recycleTextureMem())
            break;
    return;
}

// This is called only from UI thread, at drawGL time.
void VideoLayerManager::updateMatrix(const int layerId, const GLfloat* matrix)
{
    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    if (m_videoLayerInfoMap.contains(layerId)) {
        // If the existing video layer's matrix is matching the incoming one,
        // then skip the update.
        VideoLayerInfo* pInfo = m_videoLayerInfoMap.get(layerId);
        ASSERT(matrix);
        if (pInfo && !memcmp(matrix, pInfo->surfaceMatrix, sizeof(pInfo->surfaceMatrix)))
            return;
        memcpy(pInfo->surfaceMatrix, matrix, sizeof(pInfo->surfaceMatrix));
    } else {
        XLOG("Error: should not reach here, the layerId %d should exist!", layerId);
        ASSERT(false);
    }
    return;
}

// This is called on the webcore thread, save the GL texture for recycle in
// the retired queue. They will be deleted in deleteUnusedTextures() in the UI
// thread.
// Return true when we found one texture to retire.
bool VideoLayerManager::recycleTextureMem()
{
    // Find the oldest texture int the m_videoLayerInfoMap, put it in m_retiredTextures
    int oldestTimeStamp = m_currentTimeStamp;
    int oldestLayerId = -1;

    InfoIterator end = m_videoLayerInfoMap.end();
#ifdef DEBUG
    XLOG("VideoLayerManager::recycleTextureMem m_videoLayerInfoMap contains");
    for (InfoIterator it = m_videoLayerInfoMap.begin(); it != end; ++it)
        XLOG("  layerId %d, textureId %d, videoSize %d, timeStamp %d ",
             it->first, it->second->textureId, it->second->videoSize, it->second->timeStamp);
#endif
    for (InfoIterator it = m_videoLayerInfoMap.begin(); it != end; ++it) {
        if (it->second->timeStamp < oldestTimeStamp) {
            oldestTimeStamp = it->second->timeStamp;
            oldestLayerId = it->first;
        }
    }

    bool foundTextureToRetire = (oldestLayerId != -1);
    if (foundTextureToRetire)
        removeLayerInternal(oldestLayerId);

    return foundTextureToRetire;
}

// This is only called in the UI thread, b/c glDeleteTextures need to be called
// on the right context.
void VideoLayerManager::deleteUnusedTextures()
{
    m_retiredTexturesLock.lock();
    int size = m_retiredTextures.size();
    if (size > 0) {
        GLuint* textureNames = new GLuint[size];
        int index = 0;
        Vector<GLuint>::const_iterator end = m_retiredTextures.end();
        for (Vector<GLuint>::const_iterator it = m_retiredTextures.begin();
             it != end; ++it) {
            GLuint textureName = *it;
            if (textureName) {
                textureNames[index] = textureName;
                index++;
                XLOG("GL texture %d will be deleted", textureName);
            }
        }
        glDeleteTextures(size, textureNames);
        delete textureNames;
        m_retiredTextures.clear();
    }
    m_retiredTexturesLock.unlock();
    return;
}

// This can be called in the webcore thread in the media player's dtor.
void VideoLayerManager::removeLayer(const int layerId)
{
    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    removeLayerInternal(layerId);
}

// This can be called on both UI and webcore thread. Since this is a private
// function, it is up to the public function to handle the lock for
// m_videoLayerInfoMap.
void VideoLayerManager::removeLayerInternal(const int layerId)
{
    // Delete the layerInfo corresponding to this layerId and remove from the map.
    if (m_videoLayerInfoMap.contains(layerId)) {
        GLuint textureId = m_videoLayerInfoMap.get(layerId)->textureId;
        if (textureId) {
            // Buffer up the retired textures in either UI or webcore thread,
            // will be purged at deleteUnusedTextures in the UI thread.
            m_retiredTexturesLock.lock();
            m_retiredTextures.append(textureId);
            m_retiredTexturesLock.unlock();
        }
        delete m_videoLayerInfoMap.get(layerId);
        m_videoLayerInfoMap.remove(layerId);
    }
    return;
}

}
#endif // USE(ACCELERATED_COMPOSITING)
