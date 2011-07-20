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

#ifndef TilesProfiler_h
#define TilesProfiler_h

#if USE(ACCELERATED_COMPOSITING)

#include "BaseTile.h"
#include "IntRect.h"
#include "Vector.h"

namespace WebCore {

struct TileProfileRecord {
    TileProfileRecord(int left, int top, int right, int bottom, float scale, int isReady, int level) {
        this->left = left;
        this->top = top;
        this->right = right;
        this->bottom = bottom;
        this->scale = scale;
        this->isReady = isReady;
        this->level = level;
    }
    int left, top, right, bottom;
    bool isReady;
    int level;
    float scale;
};

class TilesProfiler {
public:
    TilesProfiler();

    void start();
    float stop();
    void clear();
    void nextFrame(int left, int top, int right, int bottom, float scale);
    void nextTile(BaseTile& tile, float scale, bool inView);
    void nextInval(const IntRect& rect, float scale);
    int numFrames() {
        return m_records.size();
    };

    int numTilesInFrame(int frame) {
        return m_records[frame].size();
    }

    TileProfileRecord* getTile(int frame, int tile) {
        return &m_records[frame][tile];
    }

private:
    bool m_enabled;
    unsigned int m_goodTiles;
    unsigned int m_badTiles;
    Vector<Vector<TileProfileRecord> > m_records;
    double m_time;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TilesProfiler_h
