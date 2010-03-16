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

#ifndef GraphicsLayerAndroid_h
#define GraphicsLayerAndroid_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatRect.h"
#include "Frame.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerClient.h"
#include "LayerAndroid.h"
#include "RefPtr.h"
#include "Vector.h"

class FloatPoint3D;
class Image;

namespace WebCore {

class GraphicsLayerAndroid : public GraphicsLayer {
public:

    GraphicsLayerAndroid(GraphicsLayerClient*);
    virtual ~GraphicsLayerAndroid();

    virtual void setName(const String&);

    // for hosting this GraphicsLayer in a native layer hierarchy
    virtual NativeLayer nativeLayer() const;

    virtual bool setChildren(const Vector<GraphicsLayer*>&);
    virtual void addChild(GraphicsLayer*);
    virtual void addChildAtIndex(GraphicsLayer*, int index);
    virtual void addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling);
    virtual void addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling);
    virtual bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild);

    virtual void removeFromParent();

    virtual void setPosition(const FloatPoint&);
    virtual void setAnchorPoint(const FloatPoint3D&);
    virtual void setSize(const FloatSize&);

    virtual void setTransform(const TransformationMatrix&);

    virtual void setChildrenTransform(const TransformationMatrix&);

    virtual void setMaskLayer(GraphicsLayer*);
    virtual void setMasksToBounds(bool);
    virtual void setDrawsContent(bool);

    virtual void setBackgroundColor(const Color&);
    virtual void clearBackgroundColor();

    virtual void setContentsOpaque(bool);

    virtual void setOpacity(float);

    virtual void setNeedsDisplay();
    virtual void setNeedsDisplayInRect(const FloatRect&);

    virtual bool addAnimation(const KeyframeValueList& valueList,
                              const IntSize& boxSize,
                              const Animation* anim,
                              const String& keyframesName,
                              double beginTime);
    bool createTransformAnimationsFromKeyframes(const KeyframeValueList&,
                                                const Animation*,
                                                const String& keyframesName,
                                                double beginTime,
                                                const IntSize& boxSize);
    bool createAnimationFromKeyframes(const KeyframeValueList&,
                                      const Animation*,
                                      const String& keyframesName,
                                      double beginTime);

    virtual void removeAnimationsForProperty(AnimatedPropertyID);
    virtual void removeAnimationsForKeyframes(const String& keyframesName);
    virtual void pauseAnimation(const String& keyframesName);

    virtual void suspendAnimations(double time);
    virtual void resumeAnimations();

    virtual void setContentsToImage(Image*);
    bool repaintAll();
    virtual PlatformLayer* platformLayer() const;

    void pauseDisplay(bool state);

#ifndef NDEBUG
    virtual void setDebugBackgroundColor(const Color&);
    virtual void setDebugBorder(const Color&, float borderWidth);
#endif

    virtual void setZPosition(float);

    void askForSync();
    void syncPositionState();
    void needsSyncChildren();
    void syncChildren();
    void syncMask();
    virtual void syncCompositingState();
    void setFrame(Frame*);
    void notifyClientAnimationStarted();

    void sendImmediateRepaint();
    LayerAndroid* contentLayer() { return m_contentLayer; }

    static int instancesCount();

private:

    void updateFixedPosition();

    bool repaint(const FloatRect& rect);
    void needsNotifyClient();

    bool m_needsSyncChildren;
    bool m_needsSyncMask;
    bool m_needsRepaint;
    bool m_needsDisplay;
    bool m_needsNotifyClient;

    bool m_haveContents;
    bool m_haveImage;

    float m_translateX;
    float m_translateY;
    float m_currentTranslateX;
    float m_currentTranslateY;

    FloatPoint m_currentPosition;

    RefPtr<Frame> m_frame;

    Vector<FloatRect> m_invalidatedRects;

    LayerAndroid* m_contentLayer;
};

} // namespace WebCore


#endif // USE(ACCELERATED_COMPOSITING)

#endif // GraphicsLayerAndroid_h
