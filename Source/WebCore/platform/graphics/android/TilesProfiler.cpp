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
#include "TilesProfiler.h"

#if USE(ACCELERATED_COMPOSITING)

#include "TilesManager.h"
#include <wtf/CurrentTime.h>

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "TilesProfiler", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

// Hard limit on amount of frames (and thus memory) profiling can take
#define MAX_PROF_FRAMES 400
#define INVAL_CODE -2

namespace WebCore {
TilesProfiler::TilesProfiler()
    : m_enabled(false)
{
}

void TilesProfiler::start()
{
    m_enabled = true;
    m_goodTiles = 0;
    m_badTiles = 0;
    m_records.clear();
    m_time = currentTimeMS();
    XLOG("initializing tileprofiling");
}

float TilesProfiler::stop()
{
    m_enabled = false;
    XLOG("completed tile profiling, observed %d frames", m_records.size());
    return (1.0 * m_goodTiles) / (m_goodTiles + m_badTiles);
}

void TilesProfiler::clear()
{
    XLOG("clearing tile profiling of its %d frames", m_records.size());
    m_records.clear();
}

void TilesProfiler::nextFrame(int left, int top, int right, int bottom, float scale)
{
    if (!m_enabled || (m_records.size() > MAX_PROF_FRAMES))
        return;

    double currentTime = currentTimeMS();
    double timeDelta = currentTime - m_time;
    m_time = currentTime;

#ifdef DEBUG
    if (m_records.size() != 0) {
        XLOG("completed tile profiling frame, observed %d tiles. %f ms since last",
             m_records[0].size(), timeDelta);
    }
#endif // DEBUG

    m_records.append(WTF::Vector<TileProfileRecord>());

    //first record designates viewport
    m_records.last().append(TileProfileRecord(
                                left, top, right, bottom,
                                scale, true, (int)(timeDelta * 1000)));
}

void TilesProfiler::nextTile(BaseTile& tile, float scale, bool inView)
{
    if (!m_enabled || (m_records.size() > MAX_PROF_FRAMES) || (m_records.size() == 0))
        return;

    bool isReady = tile.isTileReady();
    int left = tile.x() * TilesManager::tileWidth();
    int top = tile.y() * TilesManager::tileWidth();
    int right = left + TilesManager::tileWidth();
    int bottom = top + TilesManager::tileWidth();

    if (inView) {
        if (isReady)
            m_goodTiles++;
        else
            m_badTiles++;
    }
    m_records.last().append(TileProfileRecord(
                                left, top, right, bottom,
                                scale, isReady, (int)tile.drawCount()));
    XLOG("adding tile %d %d %d %d, scale %f", left, top, right, bottom, scale);
}

void TilesProfiler::nextInval(const IntRect& rect, float scale)
{
    if (!m_enabled || (m_records.size() > MAX_PROF_FRAMES) || (m_records.size() == 0))
        return;

    m_records.last().append(TileProfileRecord(
                                rect.x(), rect.y(),
                                rect.maxX(), rect.maxY(), scale, false, INVAL_CODE));
    XLOG("adding inval region %d %d %d %d, scale %f", rect.x(), rect.y(),
         rect.maxX(), rect.maxY(), scale);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
