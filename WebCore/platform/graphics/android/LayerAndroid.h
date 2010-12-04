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

#include "RefPtr.h"
#include "SkColor.h"
#include "SkLayer.h"

#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

#ifndef BZERO_DEFINED
#define BZERO_DEFINED
// http://www.opengroup.org/onlinepubs/000095399/functions/bzero.html
// For maximum portability, it is recommended to replace the function call to bzero() as follows:
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)
#endif

class SkBitmapRef;
class SkCanvas;
class SkMatrix;
class SkPicture;

namespace android {
class DrawExtra;
}

using namespace android;

struct SkLength {
    enum SkLengthType { Undefined, Auto, Relative, Percent, Fixed, Static, Intrinsic, MinIntrinsic };
    SkLengthType type;
    SkScalar value;
    SkLength() {
        type = Undefined;
        value = 0;
    }
    bool defined() const {
        if (type == Undefined)
            return false;
        return true;
    }
    float calcFloatValue(float max) const {
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

class LayerAndroid : public SkLayer {

public:
    LayerAndroid(bool isRootLayer);
    LayerAndroid(const LayerAndroid& layer);
    LayerAndroid(SkPicture*);
    virtual ~LayerAndroid();

    static int instancesCount();

    void setTranslation(SkScalar x, SkScalar y) { m_translation.set(x, y); }
    void setRotation(SkScalar a) { m_angleTransform = a; m_doRotation = true; }
    void setScale(SkScalar x, SkScalar y) { m_scale.set(x, y); }
    SkPoint translation() const { return m_translation; }
    SkRect  bounds() const {
        const SkPoint& pos = this->getPosition();
        const SkSize& size = this->getSize();
        SkRect rect;
        rect.set(pos.fX, pos.fY,
                 pos.fX + size.width(),
                 pos.fY + size.height());
        rect.offset(m_translation.fX, m_translation.fY);
        return rect;
    }
    void setFixedPosition(SkLength left,   // CSS left property
                          SkLength top,    // CSS top property
                          SkLength right,  // CSS right property
                          SkLength bottom, // CSS bottom property
                          SkLength marginLeft,   // CSS margin-left property
                          SkLength marginTop,    // CSS margin-top property
                          SkLength marginRight,  // CSS margin-right property
                          SkLength marginBottom, // CSS margin-bottom property
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
        setInheritFromRootTransform(true);
    }

    void setBackgroundColor(SkColor color);
    void setMaskLayer(LayerAndroid*);
    void setMasksToBounds(bool masksToBounds) {
        m_haveClip = masksToBounds;
    }
    bool masksToBounds() const { return m_haveClip; }

    void setIsRootLayer(bool isRootLayer) { m_isRootLayer = isRootLayer; }
    bool isRootLayer() const { return m_isRootLayer; }

    SkPicture* recordContext();

    // Returns true if the content position has changed.
    bool scrollBy(int dx, int dy);

    void addAnimation(PassRefPtr<AndroidAnimation> anim);
    void removeAnimation(const String& name);
    bool evaluateAnimations() const;
    bool evaluateAnimations(double time) const;
    bool hasAnimations() const;

    SkPicture* picture() const { return m_recordingPicture; }

    // remove layers bounds from visible rectangle to show what can be
    // scrolled into view; returns original minus layer bounds in global space.
    SkRect subtractLayers(const SkRect& visibleRect) const;

    void dumpLayers(FILE*, int indentLevel) const;
    void dumpToLog() const;

    /** Call this with the current viewport (scrolling, zoom) to update
        the position of the fixed layers.

        This call is recursive, so it should be called on the root of the
        hierarchy.
    */
    void updateFixedLayersPositions(const SkRect& viewPort);

    /** Call this to update the position attribute, so that later calls
        like bounds() will report the corrected position.

        This call is recursive, so it should be called on the root of the
        hierarchy.
     */
    void updatePositions();

    void clipArea(SkTDArray<SkRect>* region) const;
    const LayerAndroid* find(int x, int y, SkPicture* root) const;
    const LayerAndroid* findById(int uniqueID) const {
        return const_cast<LayerAndroid*>(this)->findById(uniqueID);
    }
    LayerAndroid* findById(int uniqueID);
    LayerAndroid* getChild(int index) const {
        return static_cast<LayerAndroid*>(this->INHERITED::getChild(index));
    }
    void setExtra(DrawExtra* extra);  // does not assign ownership
    int uniqueId() const { return m_uniqueId; }
    bool isFixed() { return m_isFixed; }

    /** This sets a content image -- calling it means we will use
        the image directly when drawing the layer instead of using
        the content painted by WebKit. See comments below for
        m_recordingPicture and m_contentsImage.
    */
    void setContentsImage(SkBitmapRef* img);

    void bounds(SkRect* ) const;

    bool contentIsScrollable() const { return m_contentScrollable; }

    // Set when building the layer hierarchy for scrollable elements.
    void setContentScrollable(bool scrollable) {
        m_contentScrollable = scrollable;
    }

protected:
    virtual void onDraw(SkCanvas*, SkScalar opacity);

private:
    class FindState;
#if DUMP_NAV_CACHE
    friend class CachedLayer::Debug; // debugging access only
#endif

    void findInner(FindState& ) const;
    bool prepareContext(bool force = false);
    void clipInner(SkTDArray<SkRect>* region, const SkRect& local) const;

    bool m_isRootLayer;
    bool m_drawsContent;
    bool m_haveClip;
    bool m_doRotation;
    bool m_isFixed;
    bool m_backgroundColorSet;
    bool m_contentScrollable;

    SkLength m_fixedLeft;
    SkLength m_fixedTop;
    SkLength m_fixedRight;
    SkLength m_fixedBottom;
    SkLength m_fixedMarginLeft;
    SkLength m_fixedMarginTop;
    SkLength m_fixedMarginRight;
    SkLength m_fixedMarginBottom;
    SkRect   m_fixedRect;

    SkPoint m_translation;
    SkPoint m_scale;
    SkScalar m_angleTransform;
    SkColor m_backgroundColor;

    // Note that m_recordingPicture and m_contentsImage are mutually exclusive;
    // m_recordingPicture is used when WebKit is asked to paint the layer's
    // content, while m_contentsImage contains an image that we directly
    // composite, using the layer's dimensions as a destination rect.
    // We do this as if the layer only contains an image, directly compositing
    // it is a much faster method than using m_recordingPicture.
    SkPicture* m_recordingPicture;

    SkBitmapRef* m_contentsImage;

    typedef HashMap<String, RefPtr<AndroidAnimation> > KeyframesMap;
    KeyframesMap m_animations;
    DrawExtra* m_extra;
    int m_uniqueId;

    typedef SkLayer INHERITED;
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

#endif  // LayerAndroid_h
