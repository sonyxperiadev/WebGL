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

#include "Color.h"
#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "FloatSize.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "RefPtr.h"
#include "StringHash.h"
#include "Vector.h"
#include <wtf/HashSet.h>

class SkCanvas;
class SkPicture;
class SkRect;

namespace WebCore {

class AndroidAnimation;
class AndroidAnimationValue;

class LayerAndroid : public RefCounted<LayerAndroid> {

public:
    static PassRefPtr<LayerAndroid> create(bool isRootLayer);
    LayerAndroid(bool isRootLayer);
    LayerAndroid(LayerAndroid* layer);
    ~LayerAndroid();

    static int instancesCount();

    void setSize(FloatSize size) { m_size = size; }
    void setOpacity(float opacity) { m_opacity = opacity; }
    void setTranslation(FloatPoint translation) { m_translation = translation; }
    void setRotation(float a) { m_angleTransform = a; m_doRotation = true; }
    void setScale(FloatPoint3D scale) { m_scale = scale; }
    void setPosition(FloatPoint position) { m_position = position; }
    void setAnchorPoint(FloatPoint3D point) { m_anchorPoint = point; }
    void setHaveContents(bool haveContents) { m_haveContents = haveContents; }
    void setHaveImage(bool haveImage) { m_haveImage = haveImage; }
    void setDrawsContent(bool drawsContent);
    void setMaskLayer(LayerAndroid*);
    void setMasksToBounds(bool);
    void setBackgroundColor(const Color& color);
    void setIsRootLayer(bool isRootLayer) { m_isRootLayer = isRootLayer; }

    void paintOn(float scrollX, float scrollY, float scale, SkCanvas*);
    GraphicsContext* paintContext();
    void removeAllChildren() { m_children.clear(); }
    void addChildren(LayerAndroid* layer) { m_children.append(layer); }
    bool prepareContext(bool force = false);
    void startRecording();
    void stopRecording();
    SkPicture* recordContext();
    void setClip(SkCanvas* clip);
    FloatPoint position() { return m_position; }
    FloatPoint translation() { return m_translation; }
    FloatSize size() { return m_size; }

    void setFixedPosition(FloatPoint position);
    void addAnimation(PassRefPtr<AndroidAnimation> anim);
    void removeAnimation(const String& name);
    Vector<RefPtr<AndroidAnimationValue> >* evaluateAnimations() const;
    bool evaluateAnimations(double time,
             Vector<RefPtr<AndroidAnimationValue> >* result) const;
    bool hasAnimations() const;

private:

    void paintChildren(float scrollX, float scrollY,
                       float scale, SkCanvas* canvas,
                       float opacity);

    void paintMe(float scrollX, float scrollY,
                 float scale, SkCanvas* canvas,
                 float opacity);

    bool m_doRotation;
    bool m_isRootLayer;
    bool m_isFixed;
    bool m_haveContents;
    bool m_drawsContent;
    bool m_haveImage;
    bool m_haveClip;
    bool m_backgroundColorSet;

    float m_angleTransform;
    float m_opacity;

    FloatSize m_size;
    FloatPoint m_position;
    FloatPoint m_translation;
    FloatPoint m_fixedPosition;
    FloatPoint3D m_anchorPoint;
    FloatPoint3D m_scale;

    SkPicture* m_recordingPicture;
    Color m_backgroundColor;

    Vector<RefPtr<LayerAndroid> > m_children;
    typedef HashMap<String, RefPtr<AndroidAnimation> > KeyframesMap;
    KeyframesMap m_animations;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif  // LayerAndroid_h
