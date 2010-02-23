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
#include "StringHash.h"
#include <wtf/HashMap.h>

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
    LayerAndroid(SkPicture* );
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
    void setFixedPosition(SkLength left, SkLength top, SkLength right, SkLength bottom) {
        m_fixedLeft = left;
        m_fixedTop = top;
        m_fixedRight = right;
        m_fixedBottom = bottom;
        m_isFixed = true;
    }

    void setBackgroundColor(SkColor color);
    void setHaveContents(bool haveContents) { m_haveContents = haveContents; }
    void setHaveImage(bool haveImage) { m_haveImage = haveImage; }
    void setDrawsContent(bool drawsContent);
    void setMaskLayer(LayerAndroid*);
    void setMasksToBounds(bool);
    void setIsRootLayer(bool isRootLayer) { m_isRootLayer = isRootLayer; }

    bool prepareContext(bool force = false);
    void startRecording();
    void stopRecording();
    SkPicture* recordContext();
    void setClip(SkCanvas* clip);

    void addAnimation(PassRefPtr<AndroidAnimation> anim);
    void removeAnimation(const String& name);
    bool evaluateAnimations() const;
    bool evaluateAnimations(double time) const;
    bool hasAnimations() const;

    SkPicture* picture() const { return m_recordingPicture; }

    void dumpLayers(FILE*, int indentLevel) const;

    /** Call this with the current viewport (scrolling, zoom) to update its
        position attribute, so that later calls like bounds() will report the
        corrected position (assuming the layer had fixed-positioning).

        This call is recursive, so it should be called on the root of the
        hierarchy.
     */
    void updatePositions(const SkRect& viewPort);

    void bounds(SkRect* ) const;
    void clipArea(SkTDArray<SkRect>* region) const;
    const LayerAndroid* find(int x, int y) const;
    const LayerAndroid* findById(int uniqueID) const;
    LayerAndroid* getChild(int index) const {
        return static_cast<LayerAndroid*>(this->INHERITED::getChild(index));
    }
    bool haveClip() const { return m_haveClip; }
    void setExtra(DrawExtra* extra);  // does not assign ownership
    int uniqueId() const { return m_uniqueId; }

protected:
    virtual void onDraw(SkCanvas*, SkScalar opacity);

private:
    bool boundsIsUnique(SkTDArray<SkRect>* region, const SkRect& local) const;
    void clipInner(SkTDArray<SkRect>* region, const SkRect& local) const;

    bool m_isRootLayer;
    bool m_haveContents;
    bool m_drawsContent;
    bool m_haveImage;
    bool m_haveClip;
    bool m_doRotation;
    bool m_isFixed;
    bool m_backgroundColorSet;

    SkLength m_fixedLeft;
    SkLength m_fixedTop;
    SkLength m_fixedRight;
    SkLength m_fixedBottom;
    SkPoint m_translation;
    SkPoint m_scale;
    SkScalar m_angleTransform;
    SkColor m_backgroundColor;

    SkPicture* m_recordingPicture;

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
