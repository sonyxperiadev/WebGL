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

class FindOnPage;
class SkCanvas;
class SkMatrix;
class SkPicture;

namespace WebCore {

class AndroidAnimation;

class LayerAndroid : public SkLayer {

public:
    LayerAndroid(bool isRootLayer);
    LayerAndroid(const LayerAndroid& layer);
    virtual ~LayerAndroid();

    static int instancesCount();

    void setHaveContents(bool haveContents) { m_haveContents = haveContents; }
    void setHaveImage(bool haveImage) { m_haveImage = haveImage; }
    void setDrawsContent(bool drawsContent);
    void setFindOnPage(FindOnPage* findOnPage);
    void setMaskLayer(LayerAndroid*);
    void setMasksToBounds(bool);
    virtual void setBackgroundColor(SkColor color);
    void setIsRootLayer(bool isRootLayer) { m_isRootLayer = isRootLayer; }

    virtual void draw(SkCanvas*, const SkRect* viewPort);
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

    void bounds(SkRect* ) const;
    bool calcPosition(const SkRect* viewPort, SkMatrix*);
    void clipArea(SkTDArray<SkRect>* region) const;
    const LayerAndroid* find(int x, int y) const;
    const LayerAndroid* findById(int uniqueID) const;
    LayerAndroid* getChild(int index) const { return
        static_cast<LayerAndroid*>(m_children[index]); }
    bool haveClip() const { return m_haveClip; }
    int uniqueId() const { return m_uniqueId; }
private:
    bool boundsIsUnique(SkTDArray<SkRect>* region, const SkRect& local) const;
    void clipInner(SkTDArray<SkRect>* region, const SkRect& local) const;
    void paintChildren(const SkRect* viewPort, SkCanvas* canvas, float opacity);
    void paintMe(const SkRect* viewPort, SkCanvas* canvas,
                 float opacity);

    bool m_isRootLayer;
    bool m_haveContents;
    bool m_drawsContent;
    bool m_haveImage;
    bool m_haveClip;

    SkPicture* m_recordingPicture;

    typedef HashMap<String, RefPtr<AndroidAnimation> > KeyframesMap;
    KeyframesMap m_animations;
    FindOnPage* m_findOnPage;
    int m_uniqueId;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif  // LayerAndroid_h
