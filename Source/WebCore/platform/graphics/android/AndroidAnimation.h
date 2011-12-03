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
#include "TransformOperation.h"
#include "Vector.h"

namespace WebCore {

class TimingFunction;

class AndroidAnimation : public ThreadSafeRefCounted<AndroidAnimation> {
public:
    AndroidAnimation(AnimatedPropertyID type,
                     const Animation* animation,
                     KeyframeValueList* operations,
                     double beginTime);

    virtual ~AndroidAnimation();
    void suggestBeginTime(double time);
    double elapsedTime(double time);
    void pickValues(double progress, int* start, int* end);
    bool checkIterationsAndProgress(double time, float* finalProgress);
    double applyTimingFunction(float from, float to, double progress,
                               const TimingFunction* timingFunction);
    bool evaluate(LayerAndroid* layer, double time);
    virtual void applyForProgress(LayerAndroid* layer, float progress) = 0;
    static long instancesCount();
    void setName(const String& name) { m_name = name; }
    String name() { return m_name; }
    AnimatedPropertyID type() { return m_type; }
    bool fillsBackwards() { return m_fillsBackwards; }
    bool fillsForwards() { return m_fillsForwards; }
    int uniqueId() { return m_uniqueId; }

protected:
    double m_beginTime;
    double m_duration;
    bool m_fillsBackwards;
    bool m_fillsForwards;
    int m_iterationCount;
    int m_direction;
    RefPtr<TimingFunction> m_timingFunction;
    String m_name;
    AnimatedPropertyID m_type;
    KeyframeValueList* m_operations;
    int m_uniqueId;
    bool m_hasFinished;
};

class AndroidOpacityAnimation : public AndroidAnimation {
public:
    static PassRefPtr<AndroidOpacityAnimation> create(const Animation* animation,
                                                      KeyframeValueList* operations,
                                                      double beginTime);
    AndroidOpacityAnimation(const Animation* animation,
                            KeyframeValueList* operations,
                            double beginTime);

    virtual void applyForProgress(LayerAndroid* layer, float progress);
};

class AndroidTransformAnimation : public AndroidAnimation {
public:
    static PassRefPtr<AndroidTransformAnimation> create(
                                                     const Animation* animation,
                                                     KeyframeValueList* operations,
                                                     double beginTime);
    AndroidTransformAnimation(const Animation* animation,
                              KeyframeValueList* operations,
                              double beginTime);

    virtual void applyForProgress(LayerAndroid* layer, float progress);
};

} // namespace WebCore


#endif // USE(ACCELERATED_COMPOSITING)

#endif // AndroidAnimation_h
