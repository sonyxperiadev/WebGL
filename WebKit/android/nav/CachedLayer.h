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

#ifndef CachedLayer_H
#define CachedLayer_H

#include "CachedDebug.h"
#include "IntRect.h"

class SkPicture;

namespace WebCore {
    class LayerAndroid;
}

using namespace WebCore;

namespace android {

class CachedLayer {
public:
#if USE(ACCELERATED_COMPOSITING)
    bool operator<(const CachedLayer& l) const {
        return mCachedNodeIndex < l.mCachedNodeIndex;
    }
    // FIXME: adjustBounds should be renamed globalBounds or toGlobal
    IntRect adjustBounds(const LayerAndroid* root, const IntRect& bounds) const;
    // Moves the bounds by the layer's scroll position.  Assumes the incoming
    // bounds have been adjusted by adjustBounds.
    IntRect unadjustBounds(const LayerAndroid* root,
                           const IntRect& bounds) const;
    int cachedNodeIndex() const { return mCachedNodeIndex; }
    const LayerAndroid* layer(const LayerAndroid* root) const;
    IntRect localBounds(const LayerAndroid* root, const IntRect& bounds) const;
    SkPicture* picture(const LayerAndroid* root) const;
    void toLocal(const LayerAndroid* root, int* xPtr, int* yPtr) const;
    void reset() { mLayer = 0; }
    void setCachedNodeIndex(int index) { mCachedNodeIndex = index; }
    void setOffset(const IntPoint& offset) { mOffset = offset; }
    void setScrollOffset(const IntPoint& scrollOffset) {
        mScrollOffset = scrollOffset;
    }
    void setUniqueId(int uniqueId) { mUniqueId = uniqueId; }
    int uniqueId() const { return mUniqueId; }
private:
    int mCachedNodeIndex;
    mutable const LayerAndroid* mLayer;
    // mOffset and mScrollOffset are the position and scroll offset of the
    // layer when recorded by the nav cache.
    IntPoint mOffset;
    IntPoint mScrollOffset;
    int mUniqueId;

#if DUMP_NAV_CACHE
public:
    class Debug {
public:
        CachedLayer* base() const;
        void print() const;
        static void printLayerAndroid(const LayerAndroid* );
        static void printRootLayerAndroid(const LayerAndroid* );
        static int spaces;
    } mDebug;
#endif
#endif // USE(ACCELERATED_COMPOSITING)
};

}

#endif
