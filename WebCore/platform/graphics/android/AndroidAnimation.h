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
#include "GraphicsLayer.h"
#include "HashMap.h"
#include "LayerAndroid.h"
#include "RefPtr.h"
#include "Timer.h"
#include "Vector.h"

namespace WebCore {

class TimingFunction;

class AndroidAnimation : public RefCounted<AndroidAnimation> {
  public:
    AndroidAnimation(const Animation* animation,
                     double beginTime);
    AndroidAnimation(AndroidAnimation* anim);

    virtual ~AndroidAnimation();
    virtual PassRefPtr<AndroidAnimation> copy() = 0;
    float currentProgress(double time);
    bool checkIterationsAndProgress(double time, float* finalProgress);
    virtual void swapDirection() = 0;
    virtual bool evaluate(LayerAndroid* layer, double time) = 0;
    static long instancesCount();
    void setName(const String& name) { m_name = name; }
    String name() { return m_name; }

  protected:
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
    static PassRefPtr<AndroidOpacityAnimation> create(float fromValue,
                                                     float toValue,
                                                     const Animation* animation,
                                                     double beginTime);
    AndroidOpacityAnimation(float fromValue, float toValue,
                            const Animation* animation,
                            double beginTime);
    AndroidOpacityAnimation(AndroidOpacityAnimation* anim);
    virtual PassRefPtr<AndroidAnimation> copy();

    virtual void swapDirection();
    virtual bool evaluate(LayerAndroid* layer, double time);

  private:
    float m_fromValue;
    float m_toValue;
};

class AndroidTransformAnimation : public AndroidAnimation {
  public:
    static PassRefPtr<AndroidTransformAnimation> create(
                                                     const Animation* animation,
                                                     double beginTime);
    AndroidTransformAnimation(const Animation* animation, double beginTime);

    AndroidTransformAnimation(AndroidTransformAnimation* anim);
    virtual PassRefPtr<AndroidAnimation> copy();

    void setOriginalPosition(FloatPoint position) { m_position = position; }
    void setRotation(float fA, float tA);
    void setTranslation(float fX, float fY, float fZ,
                        float tX, float tY, float tZ);
    void setScale(float fX, float fY, float fZ,
                  float tX, float tY, float tZ);
    virtual void swapDirection();
    virtual bool evaluate(LayerAndroid* layer, double time);

  private:
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
