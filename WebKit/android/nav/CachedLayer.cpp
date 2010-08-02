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
    // First, remove the original offset from the bounds.
    temp.move(-mOffset.x(), -mOffset.y());

    // Next, add in the new position of the layer (could be different due to a
    // fixed position layer).
    const FloatPoint& position = aLayer->getPosition();
    temp.move(position.x(), position.y());

    // Add in any layer translation.
    const FloatPoint& translation = aLayer->translation();
    temp.move(translation.x(), translation.y());

    // Move the bounds by the layer's internal scroll position.
    const SkPoint& scroll = aLayer->scrollPosition();
    temp.move(SkScalarToFloat(-scroll.fX), SkScalarToFloat(-scroll.fY));

    IntRect result = enclosingIntRect(temp);

    // Finally, clip the result to the foreground (this includes the object's
    // border which does not scroll).
    IntRect clip(aLayer->foregroundClip());
    clip.move(position.x(), position.y());
    result.intersect(clip);

    DBG_NAV_LOGD("root=%p aLayer=%p [%d]"
        " bounds=(%d,%d,w=%d,h=%d) trans=(%g,%g)"
        " offset=(%d,%d) clip=(%d,%d,w=%d,h=%d)"
        " result=(%d,%d,w=%d,h=%d)",
        root, aLayer, aLayer->uniqueId(),
        bounds.x(), bounds.y(), bounds.width(), bounds.height(),
        translation.x(), translation.y(), mOffset.x(), mOffset.y(),
        clip.x(), clip.y(), clip.width(), clip.height(),
        result.x(), result.y(), result.width(), result.height());
    return result;
}

IntRect CachedLayer::unadjustBounds(const LayerAndroid* root,
    const IntRect& bounds) const
{
    const LayerAndroid* aLayer = layer(root);
    if (!aLayer)
        return bounds;

    IntRect temp = bounds;
    // Remove the new position (i.e. fixed position elements).
    const FloatPoint& position = aLayer->getPosition();
    temp.move(-position.x(), -position.y());

    // Remove any layer translation.
    const FloatPoint& translation = aLayer->translation();
    temp.move(-translation.x(), -translation.y());

    // Move the bounds by the internal scroll position.
    const SkPoint& scroll = aLayer->scrollPosition();
    temp.move(SkScalarRound(scroll.fX), SkScalarRound(scroll.fY));

    // Move it back to the original offset.
    temp.move(mOffset.x(), mOffset.y());
    return temp;
}

const LayerAndroid* CachedLayer::layer(const LayerAndroid* root) const
{
    if (!root || mLayer)
        return mLayer;
    return mLayer = root->findById(mUniqueId);
}

// return bounds relative to enclosing layer as recorded when walking the dom
IntRect CachedLayer::localBounds(const LayerAndroid* root,
    const IntRect& bounds) const
{
    IntRect temp = bounds;
    // Remove the original offset from the bounds.
    temp.move(-mOffset.x(), -mOffset.y());

    const LayerAndroid* aLayer = layer(root);
    if (aLayer) {
        // Move the bounds by the scroll position of the layer.
        const SkPoint& scroll = aLayer->scrollPosition();
        temp.move(SkScalarToFloat(-scroll.fX), SkScalarToFloat(-scroll.fY));

        // Clip by the layer's foreground bounds.  Since the bounds have
        // already be moved local to the layer, no need to move the foreground
        // clip.
        temp.intersect(IntRect(aLayer->foregroundClip()));
    }

    return enclosingIntRect(temp);
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

