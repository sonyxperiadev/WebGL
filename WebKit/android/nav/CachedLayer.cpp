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

    // Now, add in the original scroll position.  This moves the node to the
    // original location within the layer.
    temp.move(mScrollOffset.x(), mScrollOffset.y());

    // Next, add in the new position of the layer (could be different due to a
    // fixed position layer).
    FloatPoint position = getGlobalPosition(aLayer);
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
    if (!clip.isEmpty()) {
        clip.move(position.x(), position.y());
        result.intersect(clip);
    }

    DBG_NAV_LOGV("root=%p aLayer=%p [%d]"
        " bounds=(%d,%d,w=%d,h=%d) trans=(%g,%g) pos=(%f,%f)"
        " offset=(%d,%d) clip=(%d,%d,w=%d,h=%d)"
        " scroll=(%d,%d) origScroll=(%d,%d)"
        " result=(%d,%d,w=%d,h=%d)",
        root, aLayer, aLayer->uniqueId(),
        bounds.x(), bounds.y(), bounds.width(), bounds.height(),
        translation.x(), translation.y(), position.x(), position.y(),
        mOffset.x(), mOffset.y(),
        clip.x(), clip.y(), clip.width(), clip.height(),
        SkScalarRound(scroll.fX), SkScalarRound(scroll.fY),
        mScrollOffset.x(), mScrollOffset.y(),
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
    FloatPoint position = getGlobalPosition(aLayer);

    temp.move(-position.x(), -position.y());

    // Remove any layer translation.
    const FloatPoint& translation = aLayer->translation();
    temp.move(-translation.x(), -translation.y());

    // Move the bounds by the internal scroll position.
    const SkPoint& scroll = aLayer->scrollPosition();
    temp.move(SkScalarRound(scroll.fX), SkScalarRound(scroll.fY));

    // Move it back to the original offset.
    temp.move(mOffset.x(), mOffset.y());

    // Move the bounds by the original scroll.
    temp.move(-mScrollOffset.x(), -mScrollOffset.y());
    DBG_NAV_LOGD("root=%p aLayer=%p [%d]"
        " bounds=(%d,%d,w=%d,h=%d) trans=(%g,%g) pos=(%f,%f)"
        " offset=(%d,%d)"
        " scroll=(%d,%d) origScroll=(%d,%d)"
        " result=(%d,%d,w=%d,h=%d)",
        root, aLayer, aLayer->uniqueId(),
        bounds.x(), bounds.y(), bounds.width(), bounds.height(),
        translation.x(), translation.y(), position.x(), position.y(),
        mOffset.x(), mOffset.y(),
        SkScalarRound(scroll.fX), SkScalarRound(scroll.fY),
        mScrollOffset.x(), mScrollOffset.y(),
        temp.x(), temp.y(), temp.width(), temp.height());
    return temp;
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

    // We add in the original scroll position in order to position the node
    // relative to the current internal scroll position.
    temp.move(mScrollOffset.x(), mScrollOffset.y());

    const LayerAndroid* aLayer = layer(root);
    FloatPoint position;
    if (aLayer) {
        const LayerAndroid* parent = static_cast<const LayerAndroid*>
            (aLayer->getParent());
        if (parent) {
            position = getGlobalPosition(parent);
            temp.move(-position.x(), -position.y());
        }
        // Move the bounds by the scroll position of the layer.
        const SkPoint& scroll = aLayer->scrollPosition();
        temp.move(SkScalarToFloat(-scroll.fX), SkScalarToFloat(-scroll.fY));

        // Clip by the layer's foreground bounds.  Since the bounds have
        // already be moved local to the layer, no need to move the foreground
        // clip.
        IntRect foregroundClip(aLayer->foregroundClip());
        if (!foregroundClip.isEmpty())
            temp.intersect(foregroundClip);
    }

#if DEBUG_NAV_UI
    const FloatPoint& translation = aLayer->translation();
    SkPoint scroll = SkPoint::Make(0,0);
    if (aLayer) scroll = aLayer->scrollPosition();
    DBG_NAV_LOGD("aLayer=%p [%d] bounds=(%d,%d,w=%d,h=%d) offset=(%d,%d)"
        " scrollOffset=(%d,%d) position=(%g,%g) result=(%d,%d,w=%d,h=%d)"
        " scroll=(%d,%d) trans=(%g,%g)",
        aLayer, aLayer ? aLayer->uniqueId() : 0,
        bounds.x(), bounds.y(), bounds.width(), bounds.height(), 
        mOffset.x(), mOffset.y(),
        mScrollOffset.x(), mScrollOffset.y(), position.x(), position.y(),
        temp.x(), temp.y(), temp.width(), temp.height(),
        scroll.fX, scroll.fY,
        translation.x(), translation.y());
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
    DUMP_NAV_LOGD("    // LayerAndroid* mLayer=%p;\n", b->mLayer);
    DUMP_NAV_LOGD("    // int mOffset=(%d, %d);\n",
        b->mOffset.x(), b->mOffset.y());
    DUMP_NAV_LOGD("    // int mScrollOffset=(%d, %d);\n",
        b->mScrollOffset.x(), b->mScrollOffset.y());
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
    DUMP_NAV_LOGD("%.*s layer=%p [%d] (%g,%g,%g,%g)"
        " position=(%g,%g) translation=(%g,%g)  anchor=(%g,%g)"
        " matrix=(%g,%g) childMatrix=(%g,%g)"
        " foregroundLocation=(%g,%g)"
        " picture=%p clipped=%s\n",
        spaces, "                   ", layer, layer->uniqueId(),
        bounds.fLeft, bounds.fTop, bounds.width(), bounds.height(),
        layer->getPosition().fX, layer->getPosition().fY,
        layer->translation().fX, layer->translation().fY,
        layer->getAnchorPoint().fX, layer->getAnchorPoint().fY,
        layer->getMatrix().getTranslateX(), layer->getMatrix().getTranslateY(),
        layer->getChildrenMatrix().getTranslateX(),
        layer->getChildrenMatrix().getTranslateY(),
        layer->scrollPosition().fX, layer->scrollPosition().fY,
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

