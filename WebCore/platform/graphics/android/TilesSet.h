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

#ifndef TilesSet_h
#define TilesSet_h

#if USE(ACCELERATED_COMPOSITING)

#include "BaseTile.h"
#include "Vector.h"

namespace WebCore {

class TilesSet {
public:
#ifdef DEBUG
    static int count();
#endif
    TilesSet(int id, float scale, int firstTileX, int firstTileY, int rows, int cols);
    ~TilesSet();

    int id() const { return m_id; }
    float scale() const { return m_scale; }
    int firstTileX() const { return m_firstTileX; }
    int firstTileY() const { return m_firstTileY; }
    int nbRows() const { return m_nbRows; }
    int nbCols() const { return m_nbCols; }
    bool operator==(const TilesSet& set);
    void reserveTextures();

    void paint();
    void setPainting(bool state) { m_painting = state; }

    void add(BaseTile* texture)
    {
        mTiles.append(texture);
    }

private:
    Vector<BaseTile*> mTiles;

    int m_id;
    float m_scale;
    int m_firstTileX;
    int m_firstTileY;
    int m_nbRows;
    int m_nbCols;
    bool m_painting;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TilesSet_h
