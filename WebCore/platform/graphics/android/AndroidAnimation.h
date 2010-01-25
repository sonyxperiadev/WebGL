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

#ifndef AndroidAnimation_h
#define AndroidAnimation_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "HashMap.h"
#include "LayerAndroid.h"
#include "RefPtr.h"
#include "Timer.h"
#include "Vector.h"

namespace WebCore {

class AndroidAnimation;
class GraphicsLayerAndroid;
class TimingFunction;

typedef Vector<RefPtr<AndroidAnimation> > AnimsVector;
typedef HashMap<RefPtr<LayerAndroid>, AnimsVector* > LayersAnimsMap;

class AndroidAnimationValue : public RefCounted<AndroidAnimationValue> {
  public:
    AndroidAnimationValue(LayerAndroid* layer) : m_layer(layer) { }
    virtual ~AndroidAnimationValue() { }
    virtual void apply() = 0;
  protected:
    RefPtr<LayerAndroid> m_layer;
};

class AndroidOpacityAnimationValue : public AndroidAnimationValue {
  public:
    static PassRefPtr<AndroidOpacityAnimationValue> create(
                                   LayerAndroid* layer, float value) {
        return adoptRef(new AndroidOpacityAnimationValue(layer, value));
    }
    AndroidOpacityAnimationValue(LayerAndroid* layer, float value) :
      AndroidAnimationValue(layer), m_value(value) { }
    virtual void apply() { m_layer->setOpacity(m_value); }
  private:
    float m_value;
};

class AndroidTransformAnimationValue : public AndroidAnimationValue {
  public:
    static PassRefPtr<AndroidTransformAnimationValue> create(
                                   LayerAndroid* layer,
                                   FloatPoint translation,
                                   FloatPoint3D scale,
                                   float rotation) {
        return adoptRef(new AndroidTransformAnimationValue(layer, translation, scale, rotation));
    }

    AndroidTransformAnimationValue(LayerAndroid* layer,
                                   FloatPoint translation,
                                   FloatPoint3D scale,
                                   float rotation) :
      AndroidAnimationValue(layer),
       m_doTranslation(false), m_doScaling(false), m_doRotation(false),
       m_translation(translation), m_scale(scale), m_rotation(rotation) { }
    void setDoTranslation(bool doTranslation) { m_doTranslation = doTranslation; }
    void setDoScaling(bool doScaling) { m_doScaling = doScaling; }
    void setDoRotation(bool doRotation) { m_doRotation = doRotation; }

    virtual void apply();

  private:
    bool m_doTranslation;
    bool m_doScaling;
    bool m_doRotation;
    FloatPoint m_translation;
    FloatPoint3D m_scale;
    float m_rotation;
};

class AndroidAnimation : public RefCounted<AndroidAnimation> {
  public:
    AndroidAnimation(LayerAndroid* contentLayer,
                     const Animation* animation,
                     double beginTime);
    AndroidAnimation(AndroidAnimation* anim);

    virtual ~AndroidAnimation();
    virtual AndroidAnimation* copy() = 0;
    float currentProgress(double time);
    bool checkIterationsAndProgress(double time, float* finalProgress);
    virtual void swapDirection() = 0;
    virtual bool evaluate(double time) = 0;
    LayerAndroid* contentLayer() { return m_contentLayer.get(); }
    static long instancesCount();
    void setLayer(LayerAndroid* layer) { m_contentLayer = layer; }
    void setName(const String& name) { m_name = name; }
    String name() { return m_name; }
    virtual PassRefPtr<AndroidAnimationValue> result() = 0;

  protected:
    RefPtr<LayerAndroid> m_contentLayer;
    double m_beginTime;
    double m_elapsedTime;
    double m_duration;
    int m_iterationCount;
    int m_currentIteration;
    int m_direction;
    TimingFunction m_timingFunction;
    String m_name;
};

class AndroidOpacityAnimation : public AndroidAnimation {
  public:
    static PassRefPtr<AndroidOpacityAnimation> create(LayerAndroid* contentLayer,
                                        float fromValue, float toValue,
                                        const Animation* animation,
                                        double beginTime);
    AndroidOpacityAnimation(LayerAndroid* contentLayer,
                            float fromValue, float toValue,
                            const Animation* animation,
                            double beginTime);
    AndroidOpacityAnimation(AndroidOpacityAnimation* anim);
    virtual AndroidAnimation* copy();
    virtual PassRefPtr<AndroidAnimationValue> result() { return m_result.release(); }

    virtual void swapDirection();
    virtual bool evaluate(double time);

  private:
    RefPtr<AndroidOpacityAnimationValue> m_result;
    float m_fromValue;
    float m_toValue;
};

class AndroidTransformAnimation : public AndroidAnimation {
  public:
    static PassRefPtr<AndroidTransformAnimation> create(LayerAndroid* contentLayer,
                                        const Animation* animation,
                                        double beginTime);
    AndroidTransformAnimation(LayerAndroid* contentLayer,
                             const Animation* animation,
                             double beginTime);

    AndroidTransformAnimation(AndroidTransformAnimation* anim);
    virtual AndroidAnimation* copy();

    void setOriginalPosition(FloatPoint position) { m_position = position; }
    void setRotation(float fA, float tA);
    void setTranslation(float fX, float fY, float fZ,
                        float tX, float tY, float tZ);
    void setScale(float fX, float fY, float fZ,
                  float tX, float tY, float tZ);
    virtual void swapDirection();
    virtual bool evaluate(double time);
    virtual PassRefPtr<AndroidAnimationValue> result() { return m_result.release(); }

  private:
    RefPtr<AndroidTransformAnimationValue> m_result;
    bool m_doTranslation;
    bool m_doScaling;
    bool m_doRotation;
    FloatPoint m_position;
    float m_fromX, m_fromY, m_fromZ;
    float m_toX, m_toY, m_toZ;
    float m_fromAngle, m_toAngle;
    float m_fromScaleX, m_fromScaleY, m_fromScaleZ;
    float m_toScaleX, m_toScaleY, m_toScaleZ;
};

} // namespace WebCore


#endif // USE(ACCELERATED_COMPOSITING)

#endif // AndroidAnimation_h
