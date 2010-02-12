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
#include "Length.h"
#include "RefPtr.h"
#include "SkLayer.h"
#include "StringHash.h"
#include "Vector.h"
#include <wtf/HashMap.h>

class SkCanvas;
class SkPicture;
class SkRect;

namespace WebCore {

class AndroidAnimation;
class AndroidAnimationValue;

class LayerAndroid : public SkLayer, public RefCounted<LayerAndroid> {

public:
    static PassRefPtr<LayerAndroid> create(bool isRootLayer);
    LayerAndroid(bool isRootLayer);
    LayerAndroid(LayerAndroid* layer);
    virtual ~LayerAndroid();

    static int instancesCount();

    void setHaveContents(bool haveContents) { m_haveContents = haveContents; }
    void setHaveImage(bool haveImage) { m_haveImage = haveImage; }
    void setDrawsContent(bool drawsContent);
    void setMaskLayer(LayerAndroid*);
    void setMasksToBounds(bool);
    virtual void setBackgroundColor(SkColor color);
    void setIsRootLayer(bool isRootLayer) { m_isRootLayer = isRootLayer; }

    void paintOn(int scrollX, int scrollY, int width, int height, float scale, SkCanvas*);
    void paintOn(SkPoint offset, SkSize size, SkScalar scale, SkCanvas*);
    void removeAllChildren() { m_children.clear(); }
    void addChildren(LayerAndroid* layer) { m_children.append(layer); }
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

    void calcPosition(int scrollX, int scrollY, int viewWidth, int viewHeight,
                      float scale, float* xPtr, float* yPtr);

    SkPicture* picture() const { return m_recordingPicture; }
    const LayerAndroid* child(unsigned i) const { return m_children[i].get(); }
    unsigned childCount() const { return m_children.size(); }

private:

    void paintChildren(int scrollX, int scrollY,
                       int width, int height,
                       float scale, SkCanvas* canvas,
                       float opacity);

    void paintMe(int scrollX, int scrollY,
                 int width, int height,
                 float scale, SkCanvas* canvas,
                 float opacity);

    bool m_isRootLayer;
    bool m_haveContents;
    bool m_drawsContent;
    bool m_haveImage;
    bool m_haveClip;

    SkPicture* m_recordingPicture;

    Vector<RefPtr<LayerAndroid> > m_children;
    typedef HashMap<String, RefPtr<AndroidAnimation> > KeyframesMap;
    KeyframesMap m_animations;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif  // LayerAndroid_h
