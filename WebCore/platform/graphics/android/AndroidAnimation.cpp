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

#include "config.h"
#include "AndroidAnimation.h"

#if USE(ACCELERATED_COMPOSITING)

#include "Animation.h"
#include "GraphicsLayerAndroid.h"

#include "Timer.h"
#include "TimingFunction.h"
#include "UnitBezier.h"

#include <wtf/CurrentTime.h>

namespace WebCore {

static long gDebugAndroidAnimationInstances;

long AndroidAnimation::instancesCount()
{
    return gDebugAndroidAnimationInstances;
}

AndroidAnimation::AndroidAnimation(const Animation* animation,
                                   double beginTime) :
    m_beginTime(beginTime),
    m_duration(animation->duration()),
    m_iterationCount(animation->iterationCount()),
    m_currentIteration(0),
    m_direction(animation->direction()),
    m_timingFunction(animation->timingFunction())
{
    if (!static_cast<int>(beginTime)) // time not set
        m_beginTime = WTF::currentTime();

    gDebugAndroidAnimationInstances++;
}

AndroidAnimation::AndroidAnimation(AndroidAnimation* anim) :
    m_beginTime(anim->m_beginTime),
    m_duration(anim->m_duration),
    m_iterationCount(anim->m_iterationCount),
    m_currentIteration(0),
    m_direction(anim->m_direction),
    m_timingFunction(anim->m_timingFunction)
{
    gDebugAndroidAnimationInstances++;
}

AndroidAnimation::~AndroidAnimation()
{
    gDebugAndroidAnimationInstances--;
}

float AndroidAnimation::currentProgress(double time)
{
    if (m_beginTime <= 0.000001) // overflow or not correctly set
        m_beginTime = time;

    m_elapsedTime = time - m_beginTime;

    if (m_duration <= 0)
      m_duration = 0.000001;

    if (m_elapsedTime < 0) // animation not yet started.
        return 0;

    return m_elapsedTime / m_duration;
}

bool AndroidAnimation::checkIterationsAndProgress(double time, float* finalProgress)
{
    float progress = currentProgress(time);

    int currentIteration = static_cast<int>(progress);
    if (currentIteration != m_currentIteration)
        if (m_direction == Animation::AnimationDirectionAlternate)
            swapDirection();

    m_currentIteration = currentIteration;
    progress -= m_currentIteration;

    if ((m_currentIteration >= m_iterationCount)
          && (m_iterationCount != Animation::IterationCountInfinite))
        return false;

    if (m_timingFunction.type() != LinearTimingFunction) {
        UnitBezier bezier(m_timingFunction.x1(),
                          m_timingFunction.y1(),
                          m_timingFunction.x2(),
                          m_timingFunction.y2());
        if (m_duration > 0)
            progress = bezier.solve(progress, 1.0f / (200.0f * m_duration));
    }

    *finalProgress = progress;
    return true;
}

PassRefPtr<AndroidOpacityAnimation> AndroidOpacityAnimation::create(
                                                float fromValue,
                                                float toValue,
                                                const Animation* animation,
                                                double beginTime)
{
    return adoptRef(new AndroidOpacityAnimation(fromValue, toValue,
                                                animation, beginTime));
}

AndroidOpacityAnimation::AndroidOpacityAnimation(float fromValue, float toValue,
                                                 const Animation* animation,
                                                 double beginTime)
    : AndroidAnimation(animation, beginTime),
      m_fromValue(fromValue), m_toValue(toValue)
{
}

AndroidOpacityAnimation::AndroidOpacityAnimation(AndroidOpacityAnimation* anim)
    : AndroidAnimation(anim),
    m_fromValue(anim->m_fromValue),
    m_toValue(anim->m_toValue)
{
}

PassRefPtr<AndroidAnimation> AndroidOpacityAnimation::copy()
{
    return adoptRef(new AndroidOpacityAnimation(this));
}

void AndroidOpacityAnimation::swapDirection()
{
    float v = m_toValue;
    m_toValue = m_fromValue;
    m_fromValue = m_toValue;
}

bool AndroidOpacityAnimation::evaluate(LayerAndroid* layer, double time)
{
    float progress;
    if (!checkIterationsAndProgress(time, &progress))
        return false;

    if (progress < 0) // we still want to be evaluated until we get progress > 0
        return true;

    float value = m_fromValue + ((m_toValue - m_fromValue) * progress);
    layer->setOpacity(value);
    return true;
}

PassRefPtr<AndroidTransformAnimation> AndroidTransformAnimation::create(
                                                     const Animation* animation,
                                                     double beginTime)
{
    return adoptRef(new AndroidTransformAnimation(animation, beginTime));
}

AndroidTransformAnimation::AndroidTransformAnimation(const Animation* animation,
                                                     double beginTime)
    : AndroidAnimation(animation, beginTime),
    m_doTranslation(false),
    m_doScaling(false),
    m_doRotation(false)
{
}

AndroidTransformAnimation::AndroidTransformAnimation(AndroidTransformAnimation* anim)
    : AndroidAnimation(anim),
    m_doTranslation(anim->m_doTranslation),
    m_doScaling(anim->m_doScaling),
    m_doRotation(anim->m_doRotation),
    m_position(anim->m_position),
    m_fromX(anim->m_fromX), m_fromY(anim->m_fromY), m_fromZ(anim->m_fromZ),
    m_toX(anim->m_toX), m_toY(anim->m_toY), m_toZ(anim->m_toZ),
    m_fromAngle(anim->m_fromAngle), m_toAngle(anim->m_toAngle),
    m_fromScaleX(anim->m_fromScaleX), m_fromScaleY(anim->m_fromScaleY), m_fromScaleZ(anim->m_fromScaleZ),
    m_toScaleX(anim->m_toScaleX), m_toScaleY(anim->m_toScaleY), m_toScaleZ(anim->m_toScaleZ)
{
}

PassRefPtr<AndroidAnimation> AndroidTransformAnimation::copy()
{
    return adoptRef(new AndroidTransformAnimation(this));
}

void AndroidTransformAnimation::setRotation(float fA, float tA)
{
    m_fromAngle = fA;
    m_toAngle = tA;
    m_doRotation = true;
}

void AndroidTransformAnimation::setTranslation(float fX, float fY, float fZ,
    float tX, float tY, float tZ)
{
    m_fromX = fX;
    m_fromY = fY;
    m_fromZ = fZ;
    m_toX = tX;
    m_toY = tY;
    m_toZ = tZ;
    m_doTranslation = true;
}

void AndroidTransformAnimation::setScale(float fX, float fY, float fZ,
                                         float tX, float tY, float tZ)
{
    m_fromScaleX = fX;
    m_fromScaleY = fY;
    m_fromScaleZ = fZ;
    m_toScaleX   = tX;
    m_toScaleY   = tY;
    m_toScaleZ   = tZ;
    m_doScaling = true;
}

void AndroidTransformAnimation::swapDirection()
{
    if (m_doTranslation) {
        float tx = m_toX;
        m_toX = m_fromX;
        m_fromX = tx;
        float ty = m_toY;
        m_toY = m_fromY;
        m_fromY = ty;
        float tz = m_toZ;
        m_toZ = m_fromZ;
        m_fromZ = tz;
    }
    if (m_doScaling) {
        float sx = m_toScaleX;
        m_toScaleX = m_fromScaleX;
        m_fromScaleX = sx;
        float sy = m_toScaleY;
        m_toScaleY = m_fromScaleY;
        m_fromScaleY = sy;
    }
    if (m_doRotation) {
        float a = m_toAngle;
        m_toAngle = m_fromAngle;
        m_fromAngle = a;
    }
}

bool AndroidTransformAnimation::evaluate(LayerAndroid* layer, double time)
{
    float progress;
    if (!checkIterationsAndProgress(time, &progress))
        return false;

    if (progress < 0) // we still want to be evaluated until we get progress > 0
        return true;

    float x = m_fromX + (m_toX - m_fromX) * progress;
    float y = m_fromY + (m_toY - m_fromY) * progress;
    float z = m_fromZ + (m_toZ - m_fromZ) * progress;
    float sx = m_fromScaleX + (m_toScaleX - m_fromScaleX) * progress;
    float sy = m_fromScaleY + (m_toScaleY - m_fromScaleY) * progress;
    float sz = m_fromScaleZ + (m_toScaleZ - m_fromScaleZ) * progress;
    float a = m_fromAngle + (m_toAngle - m_fromAngle) * progress;

    if (m_doTranslation)
        layer->setTranslation(x, y);

    if (m_doScaling)
        layer->setScale(sx, sy);

    if (m_doRotation)
        layer->setRotation(a);

    return true;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
