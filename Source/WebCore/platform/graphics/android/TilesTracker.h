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

#ifndef TilesTracker_h
#define TilesTracker_h

#if USE(ACCELERATED_COMPOSITING)

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "TilesTracker", __VA_ARGS__)

namespace WebCore {

class TilesTracker {
public:
    TilesTracker() {
        clear();
    }

    void track(bool ready, bool haveTexture) {
        m_nbTextures++;
        if (ready)
            m_nbTexturesReady++;
        else
            m_nbTexturesNotReady++;
        if (haveTexture)
            m_nbTexturesUsed++;
    }

    void clear() {
        m_nbLayers = 0;
        m_nbVisibleLayers = 0;
        m_nbTextures = 0;
        m_nbTexturesReady = 0;
        m_nbTexturesNotReady = 0;
        m_nbTexturesUsed = 0;
    }

    void trackLayer() { m_nbLayers++; }
    void trackVisibleLayer() { m_nbVisibleLayers++; }

    void showTrackTextures() {
        XLOG("We had %d/%d layers needing %d textures, we had %d, %d were ready, %d were not",
              m_nbLayers, m_nbVisibleLayers, m_nbTextures, m_nbTexturesUsed, m_nbTexturesReady, m_nbTexturesNotReady);
    }

private:
    int m_nbLayers;
    int m_nbVisibleLayers;
    int m_nbTextures;
    int m_nbTexturesReady;
    int m_nbTexturesNotReady;
    int m_nbTexturesUsed;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TilesTracker_h
