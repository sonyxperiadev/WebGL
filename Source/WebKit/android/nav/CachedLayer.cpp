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
    FloatPoint position = getGlobalPosition(aLayer);
    temp.move(position.x(), position.y());

    // Add in any layer translation.
    // FIXME: Should use bounds() and apply the entire transformation matrix.
    const FloatPoint& translation = aLayer->translation();
    temp.move(translation.x(), translation.y());

    SkRect clip;
    aLayer->bounds(&clip);

    // Do not try to traverse the parent chain if this is the root as the parent
    // will not be a LayerAndroid.
    if (aLayer != root) {
        LayerAndroid* parent = static_cast<LayerAndroid*>(aLayer->getParent());
        while (parent) {
            SkRect pClip;
            parent->bounds(&pClip);

            // Move our position into our parent's coordinate space.
            clip.offset(pClip.fLeft, pClip.fTop);
            // Clip our visible rectangle to the parent.
            clip.intersect(pClip);

            // Stop at the root.
            if (parent == root)
                break;
            parent = static_cast<LayerAndroid*>(parent->getParent());
        }
    }

    // Intersect the result with the visible clip.
    temp.intersect(clip);

    IntRect result = enclosingIntRect(temp);

    DBG_NAV_LOGV("root=%p aLayer=%p [%d]"
        " bounds=(%d,%d,w=%d,h=%d) trans=(%g,%g) pos=(%f,%f)"
        " offset=(%d,%d)"
        " result=(%d,%d,w=%d,h=%d)",
        root, aLayer, aLayer->uniqueId(),
        bounds.x(), bounds.y(), bounds.width(), bounds.height(),
        translation.x(), translation.y(), position.x(), position.y(),
        mOffset.x(), mOffset.y(),
        result.x(), result.y(), result.width(), result.height());
    return result;
}

FloatPoint CachedLayer::getGlobalPosition(const LayerAndroid* aLayer) const
{
    SkPoint result = aLayer->getPosition();
    const SkLayer* parent = aLayer->getParent();
    while (parent) {
        result += parent->getPosition();
        DBG_NAV_LOGV("result=(%g,%g) parent=%p [%d]", result.fX, result.fY,
            parent, ((LayerAndroid*) parent)->uniqueId());
        parent = parent->getParent();
    }
    return result;
}

const LayerAndroid* CachedLayer::layer(const LayerAndroid* root) const
{
    if (!root)
        return 0;
    return root->findById(mUniqueId);
}

// return bounds relative to the layer as recorded when walking the dom
IntRect CachedLayer::localBounds(const LayerAndroid* root,
    const IntRect& bounds) const
{
    IntRect temp = bounds;
    // Remove the original offset from the bounds.
    temp.move(-mOffset.x(), -mOffset.y());

#if DEBUG_NAV_UI
    const LayerAndroid* aLayer = layer(root);
    DBG_NAV_LOGD("aLayer=%p [%d] bounds=(%d,%d,w=%d,h=%d) offset=(%d,%d)"
        " result=(%d,%d,w=%d,h=%d)",
        aLayer, aLayer ? aLayer->uniqueId() : 0,
        bounds.x(), bounds.y(), bounds.width(), bounds.height(), 
        mOffset.x(), mOffset.y(),
        temp.x(), temp.y(), temp.width(), temp.height());
#endif

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

void CachedLayer::toLocal(const LayerAndroid* root, int* xPtr, int* yPtr) const
{
    const LayerAndroid* aLayer = layer(root);
    if (!aLayer)
        return;
    DBG_NAV_LOGD("root=%p aLayer=%p [%d]", root, aLayer, aLayer->uniqueId());
    SkRect localBounds;
    aLayer->bounds(&localBounds);
    *xPtr -= localBounds.fLeft;
    *yPtr -= localBounds.fTop;
}

#if DUMP_NAV_CACHE

CachedLayer* CachedLayer::Debug::base() const {
    return (CachedLayer*) ((char*) this - OFFSETOF(CachedLayer, mDebug));
}

void CachedLayer::Debug::print() const
{
    CachedLayer* b = base();
    DUMP_NAV_LOGD("    // int mCachedNodeIndex=%d;\n", b->mCachedNodeIndex);
    DUMP_NAV_LOGD("    // int mOffset=(%d, %d);\n",
        b->mOffset.x(), b->mOffset.y());
    DUMP_NAV_LOGD("    // int mUniqueId=%p;\n", b->mUniqueId);
    DUMP_NAV_LOGD("%s\n", "");
}

#endif

#if DUMP_NAV_CACHE

int CachedLayer::Debug::spaces;

void CachedLayer::Debug::printLayerAndroid(const LayerAndroid* layer)
{
    ++spaces;
    SkRect bounds;
    layer->bounds(&bounds);
    DBG_NAV_LOGD("%.*s layer=%p [%d] (%g,%g,%g,%g)"
        " position=(%g,%g) translation=(%g,%g) anchor=(%g,%g)"
        " matrix=(%g,%g) childMatrix=(%g,%g) picture=%p clipped=%s"
        " scrollable=%s\n",
        spaces, "                   ", layer, layer->uniqueId(),
        bounds.fLeft, bounds.fTop, bounds.width(), bounds.height(),
        layer->getPosition().fX, layer->getPosition().fY,
        layer->translation().x(), layer->translation().y(),
        layer->getAnchorPoint().fX, layer->getAnchorPoint().fY,
        layer->getMatrix().getTranslateX(), layer->getMatrix().getTranslateY(),
        layer->getChildrenMatrix().getTranslateX(),
        layer->getChildrenMatrix().getTranslateY(),
        layer->picture(), layer->m_haveClip ? "true" : "false",
        layer->contentIsScrollable() ? "true" : "false");
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

