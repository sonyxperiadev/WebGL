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
#include "PaintedSurface.h"

#include "LayerAndroid.h"
#include "TilesManager.h"
#include "SkCanvas.h"
#include "SkPicture.h"

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "UpdateManager", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "UpdateManager", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

UpdateManager::UpdateManager()
    : m_paintingPicture(0)
    , m_deferredPicture(0)
{
}

UpdateManager::~UpdateManager()
{
    SkSafeUnref(m_paintingPicture);
    SkSafeUnref(m_deferredPicture);
}

void UpdateManager::swap()
{
    m_paintingInval.op(m_deferredInval, SkRegion::kUnion_Op);
    m_deferredInval.setEmpty();

    android::Mutex::Autolock lock(m_paintingPictureSync);
    if (m_deferredPicture) {
        XLOG("unlock of updatemanager %p, was painting %p, now painting %p",
             this, m_paintingPicture, m_deferredPicture);
        SkSafeUnref(m_paintingPicture);
        m_paintingPicture = m_deferredPicture;
        m_deferredPicture = 0;
    }
}

void UpdateManager::updateInval(const SkRegion& invalRegion)
{
    m_deferredInval.op(invalRegion, SkRegion::kUnion_Op);
}

void UpdateManager::updatePicture(SkPicture* picture)
{
    SkSafeRef(picture);
    SkSafeUnref(m_deferredPicture);
    m_deferredPicture = picture;
}

bool UpdateManager::paint(BaseTile* tile, SkCanvas* canvas, unsigned int* pictureUsed)
{
    m_paintingPictureSync.lock();
    SkPicture* picture = m_paintingPicture;
    SkSafeRef(picture);
    m_paintingPictureSync.unlock();

    XLOG("UpdateManager %p painting with picture %p", this, picture);

    if (!picture)
        return false;

    canvas->drawPicture(*picture);

    // TODO: visualization layer diagonals

    SkSafeUnref(picture);
    return true;
}


void UpdateManager::clearPaintingInval()
{
    m_paintingInval.setEmpty();
}

} // namespace WebCore
