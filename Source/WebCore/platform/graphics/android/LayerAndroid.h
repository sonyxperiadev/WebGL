/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LayerAndroid_h
#define LayerAndroid_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "FloatRect.h"
#include "GraphicsLayerClient.h"
#include "ImageTexture.h"
#include "Layer.h"
#include "RefPtr.h"
#include "SkBitmap.h"
#include "SkColor.h"
#include "SkRegion.h"
#include "SkStream.h"
#include "TransformationMatrix.h"

#include <wtf/HashMap.h>

#ifndef BZERO_DEFINED
#define BZERO_DEFINED
// http://www.opengroup.org/onlinepubs/000095399/functions/bzero.html
// For maximum portability, it is recommended to replace the function call to bzero() as follows:
#define bzero(b, len) (memset((b), '\0', (len)), (void) 0)
#endif

class SkBitmapRef;
class SkCanvas;
class SkMatrix;
class SkPicture;

namespace WebCore {
class LayerAndroid;
class ImageTexture;
}

namespace android {
class DrawExtra;
void serializeLayer(WebCore::LayerAndroid* layer, SkWStream* stream);
WebCore::LayerAndroid* deserializeLayer(SkStream* stream);
void cleanupImageRefs(WebCore::LayerAndroid* layer);
}

using namespace android;

struct SkLength {
    enum SkLengthType { Undefined, Auto, Relative, Percent, Fixed, Static, Intrinsic, MinIntrinsic };
    SkLengthType type;
    SkScalar value;
    SkLength()
    {
        type = Undefined;
        value = 0;
    }
    bool defined() const
    {
        if (type == Undefined)
            return false;
        return true;
    }
    float calcFloatValue(float max) const
    {
        switch (type) {
        case Percent:
            return (max * value) / 100.0f;
        case Fixed:
            return value;
        default:
            return value;
        }
    }
};

namespace WebCore {

class AndroidAnimation;
class BaseTileTexture;
class GLWebViewState;
class LayerAndroidFindState;
class RenderLayer;
class TiledPage;
class PaintedSurface;

class TexturesResult {
public:
    TexturesResult()
        : fixed(0)
        , scrollable(0)
        , clipped(0)
        , full(0)
    {}

    int fixed;
    int scrollable;
    int clipped;
    int full;
};

class TEST_EXPORT LayerAndroid : public Layer {
public:
    enum LayerType { UndefinedLayer, WebCoreLayer, UILayer, NavCacheLayer };

    LayerAndroid(RenderLayer* owner);
    LayerAndroid(const LayerAndroid& layer);
    LayerAndroid(SkPicture*);
    virtual ~LayerAndroid();

    virtual TiledPage* page() { return 0; }

    void setBackfaceVisibility(bool value) { m_backfaceVisibility = value; }
    void setTransform(const TransformationMatrix& matrix) { m_transform = matrix; }
    FloatPoint translation() const;
    // Returns a rect describing the bounds of the layer with the local
    // transformation applied, expressed relative to the parent layer.
    // FIXME: Currently we use only the translation component of the local
    // transformation.
    SkRect bounds() const;
    IntRect clippedRect() const;
    bool outsideViewport();

    IntRect unclippedArea();
    IntRect visibleArea();

    virtual bool needsTexture();
    void removeTexture(PaintedSurface*);

    // Debug helper methods
    int nbLayers();
    int nbTexturedLayers();
    void showLayer(int indent = 0);

    void computeTexturesAmount(TexturesResult*);

    float getScale() { return m_scale; }

    // draw layer and its children via Z, pre-order traversal
    virtual bool drawGL();
    bool drawChildrenGL();
    virtual bool drawCanvas(SkCanvas*);
    bool drawChildrenCanvas(SkCanvas*);

    // prepare layer and its children via reverse-Z, post-order traversal
    void prepare();

    void updateGLPositionsAndScale(const TransformationMatrix& parentMatrix,
                                   const FloatRect& clip, float opacity, float scale);
    void setDrawOpacity(float opacity) { m_drawOpacity = opacity; }
    float drawOpacity() { return m_drawOpacity; }
    void setVisible(bool value) { m_visible = value; }

    bool preserves3D() { return m_preserves3D; }
    void setPreserves3D(bool value) { m_preserves3D = value; }
    void setAnchorPointZ(float z) { m_anchorPointZ = z; }
    float anchorPointZ() { return m_anchorPointZ; }
    void setDrawTransform(const TransformationMatrix& transform) { m_drawTransform = transform; }
    const TransformationMatrix* drawTransform() const { return &m_drawTransform; }
    void setChildrenTransform(const TransformationMatrix& t) { m_childrenTransform = t; }
    void setDrawClip(const FloatRect& rect) { m_clippingRect = rect; }
    const FloatRect& drawClip() { return m_clippingRect; }

    void setFixedPosition(SkLength left, // CSS left property
                          SkLength top, // CSS top property
                          SkLength right, // CSS right property
                          SkLength bottom, // CSS bottom property
                          SkLength marginLeft, // CSS margin-left property
                          SkLength marginTop, // CSS margin-top property
                          SkLength marginRight, // CSS margin-right property
                          SkLength marginBottom, // CSS margin-bottom property
                          const IntPoint& renderLayerPos, // For undefined fixed position
                          SkRect viewRect) { // view rect, can be smaller than the layer's
        m_fixedLeft = left;
        m_fixedTop = top;
        m_fixedRight = right;
        m_fixedBottom = bottom;
        m_fixedMarginLeft = marginLeft;
        m_fixedMarginTop = marginTop;
        m_fixedMarginRight = marginRight;
        m_fixedMarginBottom = marginBottom;
        m_fixedRect = viewRect;
        m_isFixed = true;
        m_renderLayerPos = renderLayerPos;
        setShouldInheritFromRootTransform(true);
    }

    void setBackgroundColor(SkColor color);
    void setMaskLayer(LayerAndroid*);
    void setMasksToBounds(bool masksToBounds)
    {
        m_haveClip = masksToBounds;
    }
    bool masksToBounds() const { return m_haveClip; }

    SkPicture* recordContext();

    void addAnimation(PassRefPtr<AndroidAnimation> anim);
    void removeAnimationsForProperty(AnimatedPropertyID property);
    void removeAnimationsForKeyframes(const String& name);
    bool evaluateAnimations();
    bool evaluateAnimations(double time);
    void initAnimations();
    bool hasAnimations() const;
    void addDirtyArea();

    SkPicture* picture() const { return m_recordingPicture; }

    // Given a rect in global space, subtracts from it the bounds of this layer
    // and of all of its children. Returns the bounding rectangle of the result,
    // in global space.
    SkRect subtractLayers(const SkRect&) const;

    void dumpLayers(FILE*, int indentLevel) const;
    void dumpToLog() const;

    /** Call this with the current viewport (scrolling, zoom) to update
        the position of the fixed layers.

        This call is recursive, so it should be called on the root of the
        hierarchy.
    */
    bool updateFixedLayersPositions(SkRect viewPort, LayerAndroid* parentIframeLayer = 0);

    /** Call this to update the position attribute, so that later calls
        like bounds() will report the corrected position.

        This call is recursive, so it should be called on the root of the
        hierarchy.
     */
    void updatePositions();

    void clipArea(SkTDArray<SkRect>* region) const;
    const LayerAndroid* find(int* xPtr, int* yPtr, SkPicture* root) const;
    const LayerAndroid* findById(int uniqueID) const
    {
        return const_cast<LayerAndroid*>(this)->findById(uniqueID);
    }
    LayerAndroid* findById(int uniqueID);
    LayerAndroid* getChild(int index) const
    {
        return static_cast<LayerAndroid*>(this->INHERITED::getChild(index));
    }
    int uniqueId() const { return m_uniqueId; }
    bool isFixed() { return m_isFixed; }

    /** This sets a content image -- calling it means we will use
        the image directly when drawing the layer instead of using
        the content painted by WebKit.
        Images are handled in ImagesManager, as they can be shared
        between layers.
    */
    void setContentsImage(SkBitmapRef* img);

    void bounds(SkRect*) const;

    virtual LayerAndroid* copy() const { return new LayerAndroid(*this); }

    void needsRepaint() { m_pictureUsed++; }
    unsigned int pictureUsed() { return m_pictureUsed; }

    void clearDirtyRegion();

    void contentDraw(SkCanvas*);

    virtual bool isMedia() const { return false; }
    virtual bool isVideo() const { return false; }

    RenderLayer* owningLayer() const { return m_owningLayer; }

    void setIsIframe(bool isIframe) { m_isIframe = isIframe; }
    float zValue() const { return m_zValue; }

    // ViewStateSerializer friends
    friend void android::serializeLayer(LayerAndroid* layer, SkWStream* stream);
    friend LayerAndroid* android::deserializeLayer(SkStream* stream);
    friend void android::cleanupImageRefs(LayerAndroid* layer);

    PaintedSurface* texture() { return m_texture; }
    void obtainTextureForPainting(LayerAndroid* drawingLayer);

    // Update layers using another tree. Only works for basic properties
    // such as the position, the transform. Return true if anything more
    // complex is needed.
    bool updateWithTree(LayerAndroid*);
    virtual bool updateWithLayer(LayerAndroid*);

    int type() { return m_type; }

    bool hasText() { return m_hasText; }
    void checkTextPresence();

    void copyAnimationStartTimesRecursive(LayerAndroid* oldTree);

// rendering asset management
    void swapTiles();
    void setIsDrawing(bool isDrawing);
    void setIsPainting(Layer* drawingTree);
    void mergeInvalsInto(Layer* replacementTree);
    bool isReady();

protected:
    virtual void onDraw(SkCanvas*, SkScalar opacity);

    TransformationMatrix m_drawTransform;

private:
    class FindState;
#if DUMP_NAV_CACHE
    friend class CachedLayer::Debug; // debugging access only
#endif

    void copyAnimationStartTimes(LayerAndroid* oldLayer);
    void findInner(FindState&) const;
    bool prepareContext(bool force = false);
    void clipInner(SkTDArray<SkRect>* region, const SkRect& local) const;

    // -------------------------------------------------------------------
    // Fields to be serialized
    // -------------------------------------------------------------------

    bool m_haveClip;
    bool m_isFixed;
    bool m_backgroundColorSet;
    bool m_isIframe;

    SkLength m_fixedLeft;
    SkLength m_fixedTop;
    SkLength m_fixedRight;
    SkLength m_fixedBottom;
    SkLength m_fixedMarginLeft;
    SkLength m_fixedMarginTop;
    SkLength m_fixedMarginRight;
    SkLength m_fixedMarginBottom;
    SkRect m_fixedRect;

    // When fixed element is undefined or auto, the render layer's position
    // is needed for offset computation
    IntPoint m_renderLayerPos;

    bool m_backfaceVisibility;
    bool m_visible;

    SkColor m_backgroundColor;

    bool m_preserves3D;
    float m_anchorPointZ;
    float m_drawOpacity;

    // Note that m_recordingPicture and m_imageRef are mutually exclusive;
    // m_recordingPicture is used when WebKit is asked to paint the layer's
    // content, while m_imageRef contains an image that we directly
    // composite, using the layer's dimensions as a destination rect.
    // We do this as if the layer only contains an image, directly compositing
    // it is a much faster method than using m_recordingPicture.
    SkPicture* m_recordingPicture;

    typedef HashMap<pair<String, int>, RefPtr<AndroidAnimation> > KeyframesMap;
    KeyframesMap m_animations;

    TransformationMatrix m_transform;
    TransformationMatrix m_childrenTransform;

    // -------------------------------------------------------------------
    // Fields that are not serialized (generated, cached, or non-serializable)
    // -------------------------------------------------------------------

    SkPoint m_iframeOffset;

    float m_zValue;

    FloatRect m_clippingRect;

    int m_uniqueId;

    PaintedSurface* m_texture;
    unsigned m_imageCRC;

    unsigned int m_pictureUsed;

    // used to signal the framework we need a repaint
    bool m_hasRunningAnimations;

    float m_scale;

    // We try to not always compute the texture size, as this is quite heavy
    static const double s_computeTextureDelay = 0.2; // 200 ms
    double m_lastComputeTextureSize;

    // This mutex serves two purposes. (1) It ensures that certain operations
    // happen atomically and (2) it makes sure those operations are synchronized
    // across all threads and cores.
    android::Mutex m_atomicSync;

    RenderLayer* m_owningLayer;

    int m_type;

    bool m_hasText;

    typedef Layer INHERITED;
};

}

#else

class SkPicture;

namespace WebCore {

class LayerAndroid {
public:
    LayerAndroid(SkPicture* picture) :
        m_recordingPicture(picture), // does not assign ownership
        m_uniqueId(-1)
    {}
    SkPicture* picture() const { return m_recordingPicture; }
    int uniqueId() const { return m_uniqueId; }
private:
    SkPicture* m_recordingPicture;
    int m_uniqueId;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // LayerAndroid_h
