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
#include "TexturesGenerator.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GLUtils.h"
#include "TilesManager.h"

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "TexturesGenerator", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

void TexturesGenerator::schedulePaintForTilesSet(TilesSet* set)
{
    android::Mutex::Autolock lock(mRequestedPixmapsLock);
    for (unsigned int i = 0; i < mRequestedPixmaps.size(); i++) {
        TilesSet* s = mRequestedPixmaps[i];
        if (s && *s == *set) {
            // Similar set already in the queue
            delete set;
            return;
        }
    }

    XLOG("%x schedulePaintForTilesSet (%x) %d, %d, %d, %d", this, set,
        set->firstTileX(), set->firstTileY(), set->nbRows(), set->nbCols());
    mRequestedPixmaps.append(set);
    m_newRequestLock.lock();
    m_newRequestCond.signal();
    m_newRequestLock.unlock();
}

status_t TexturesGenerator::readyToRun()
{
    TilesManager::instance()->enableTextures();
    XLOG("Textures enabled (context acquired...)");
    TilesManager::instance()->paintTexturesDefault();
    XLOG("Textures painted");
    TilesManager::instance()->markGeneratorAsReady();
    XLOG("Thread ready to run");
    return NO_ERROR;
}

bool TexturesGenerator::threadLoop()
{
    XLOG("threadLoop, waiting for signal");
    m_newRequestLock.lock();
    m_newRequestCond.wait(m_newRequestLock);
    m_newRequestLock.unlock();
    XLOG("threadLoop, got signal");

    bool stop = false;
    while (!stop) {
        mRequestedPixmapsLock.lock();
        TilesSet* set = 0;
        if (mRequestedPixmaps.size())
            set = mRequestedPixmaps.first();
        mRequestedPixmapsLock.unlock();

        if (set) {
            set->paint();
            mRequestedPixmapsLock.lock();
            mRequestedPixmaps.remove(0);
            mRequestedPixmapsLock.unlock();
            delete set;
        }

        mRequestedPixmapsLock.lock();
        if (!mRequestedPixmaps.size())
            stop = true;
        mRequestedPixmapsLock.unlock();
    }

    return true;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
