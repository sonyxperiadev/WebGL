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

#include "CachedPrefix.h"

#include "CachedLayer.h"
#include "FloatRect.h"
#include "LayerAndroid.h"

namespace android {

#if USE(ACCELERATED_COMPOSITING)

IntRect CachedLayer::adjustBounds(const LayerAndroid* root,
    const IntRect& bounds) const
{
    const LayerAndroid* aLayer = layer(root);
    if (!aLayer) {
        DBG_NAV_LOGD("no layer in root=%p uniqueId=%d", root, mUniqueId);
#if DUMP_NAV_CACHE
        if (root)
            mDebug.printRootLayerAndroid(root);
#endif
        return bounds;
    }
    FloatRect temp = bounds;
    const FloatPoint& position = aLayer->getPosition();
    temp.move(position.x(), position.y());
    const FloatPoint& translation = aLayer->translation();
    temp.move(translation.x(), translation.y());
    IntRect result = enclosingIntRect(temp);
    DBG_NAV_LOGD("root=%p aLayer=%p [%d]"
        " bounds=(%d,%d,w=%d,h=%d) pos=(%g,%g) trans=(%g,%g)"
        " result=(%d,%d,w=%d,h=%d) offset=(%d,%d)",
        root, aLayer, aLayer->uniqueId(),
        bounds.x(), bounds.y(), bounds.width(), bounds.height(),
        position.x(), position.y(), translation.x(), translation.y(),
        result.x(), result.y(), result.width(), result.height(),
        mOffset.x(), mOffset.y());
    result.move(-mOffset.x(), -mOffset.y());
    return result;
}

const LayerAndroid* CachedLayer::layer(const LayerAndroid* root) const
{
    if (!root || mLayer)
        return mLayer;
    return mLayer = root->findById(mUniqueId);
}

// return bounds relative to enclosing layer as recorded when walking the dom
IntRect CachedLayer::localBounds(const IntRect& bounds) const
{
    IntRect temp = bounds;
    temp.move(-mOffset.x(), -mOffset.y());
    return temp;
}

SkPicture* CachedLayer::picture(const LayerAndroid* root) const
{
    const LayerAndroid* aLayer = layer(root);
    if (!aLayer)
        return 0;
    DBG_NAV_LOGD("root=%p aLayer=%p [%d] picture=%p",
        root, aLayer, aLayer->uniqueId(), aLayer->picture());
    return aLayer->picture();
}

#if DUMP_NAV_CACHE

CachedLayer* CachedLayer::Debug::base() const {
    return (CachedLayer*) ((char*) this - OFFSETOF(CachedLayer, mDebug));
}

void CachedLayer::Debug::print() const
{
    CachedLayer* b = base();
    DUMP_NAV_LOGD("    // int mCachedNodeIndex=%d;", b->mCachedNodeIndex);
    DUMP_NAV_LOGD(" LayerAndroid* mLayer=%p;", b->mLayer);
    DUMP_NAV_LOGD(" int mOffset=(%d, %d);", b->mOffset.x(), b->mOffset.y());
    DUMP_NAV_LOGD(" int mUniqueId=%p;\n", b->mUniqueId);
}

#endif

#if DUMP_NAV_CACHE

int CachedLayer::Debug::spaces;

void CachedLayer::Debug::printLayerAndroid(const LayerAndroid* layer)
{
    ++spaces;
    SkRect bounds;
    layer->bounds(&bounds);
    DUMP_NAV_LOGX("%.*s layer=%p [%d] (%g,%g,%g,%g) picture=%p clipped=%s",
        spaces, "                   ", layer, layer->uniqueId(),
        bounds.fLeft, bounds.fTop, bounds.width(), bounds.height(),
        layer->picture(), layer->m_haveClip ? "true" : "false");
    for (int i = 0; i < layer->countChildren(); i++)
        printLayerAndroid(layer->getChild(i));
    --spaces;
}

void CachedLayer::Debug::printRootLayerAndroid(const LayerAndroid* layer)
{
    spaces = 0;
    printLayerAndroid(layer);
}
#endif

#endif // USE(ACCELERATED_COMPOSITING)

}

