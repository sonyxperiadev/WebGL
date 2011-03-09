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

#ifndef TileSet_h
#define TileSet_h

#if USE(ACCELERATED_COMPOSITING)

#include "BaseTile.h"
#include "Vector.h"

namespace WebCore {

/**
 * This purpose of this class is to act as a container for BaseTiles that need
 * to upload their contents to the GPU.  A TiledPage creates a new TileSet and
 * provides the set with identifying characteristics of the TiledPage's current
 * state (see constructor). This information allows the consumer of the TileSet
 * to determine if an equivalent TileSet already exists in the upload pipeline.
 */
class TileSet {
public:
    TileSet(TiledPage* tiledPage, int nbRows, int nbCols);
    ~TileSet();

    bool operator==(const TileSet& set);
    void paint();

    void add(BaseTile* texture)
    {
        m_tiles.append(texture);
    }

    TiledPage* page()
    {
        return m_tiledPage;
    }

    unsigned int size()
    {
        return m_tiles.size();
    }

private:
    Vector<BaseTile*> m_tiles;

    TiledPage* m_tiledPage;
    int m_nbRows;
    int m_nbCols;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TileSet_h
