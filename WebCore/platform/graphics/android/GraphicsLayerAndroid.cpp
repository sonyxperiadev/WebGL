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
#include "GraphicsLayerAndroid.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidAnimation.h"
#include "Animation.h"
#include "CString.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "Length.h"
#include "SkLayer.h"
#include "PlatformBridge.h"
#include "PlatformGraphicsContext.h"
#include "RenderLayerBacking.h"
#include "RenderView.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "SkCanvas.h"
#include "TransformationMatrix.h"
#include "TranslateTransformOperation.h"

#include <cutils/log.h>
#include <wtf/CurrentTime.h>

#undef LOG
#define LOG(...) android_printLog(ANDROID_LOG_DEBUG, "GraphicsLayer", __VA_ARGS__)
#define MLOG(...) android_printLog(ANDROID_LOG_DEBUG, "GraphicsLayer", __VA_ARGS__)
#define TLOG(...) android_printLog(ANDROID_LOG_DEBUG, "GraphicsLayer", __VA_ARGS__)

#undef LOG
#define LOG(...)
#undef MLOG
#define MLOG(...)
#undef TLOG
#define TLOG(...)
#undef LAYER_DEBUG

using namespace std;

static bool gPaused;
static double gPausedDelay;

namespace WebCore {

static int gDebugGraphicsLayerAndroidInstances = 0;
inline int GraphicsLayerAndroid::instancesCount()
{
    return gDebugGraphicsLayerAndroidInstances;
}

static String propertyIdToString(AnimatedPropertyID property)
{
    switch (property) {
    case AnimatedPropertyWebkitTransform:
        return "transform";
    case AnimatedPropertyOpacity:
        return "opacity";
    case AnimatedPropertyBackgroundColor:
        return "backgroundColor";
    case AnimatedPropertyInvalid:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return "";
}

GraphicsLayer::CompositingCoordinatesOrientation GraphicsLayer::compositingCoordinatesOrientation()
{
    return CompositingCoordinatesBottomUp;
}

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    return new GraphicsLayerAndroid(client);
}

SkLength convertLength(Length l) {
  SkLength length;
  length.type = SkLength::Undefined;
  length.value = 0;
  if (l.type() == WebCore::Percent) {
    length.type = SkLength::Percent;
    length.value = l.percent();
  } if (l.type() == WebCore::Fixed) {
    length.type = SkLength::Fixed;
    length.value = l.value();
  }
  return length;
}

GraphicsLayerAndroid::GraphicsLayerAndroid(GraphicsLayerClient* client) :
    GraphicsLayer(client),
    m_needsSyncChildren(false),
    m_needsSyncMask(false),
    m_needsRepaint(false),
    m_needsDisplay(false),
    m_needsNotifyClient(false),
    m_haveContents(false),
    m_haveImage(false),
    m_translateX(0),
    m_translateY(0),
    m_currentTranslateX(0),
    m_currentTranslateY(0),
    m_currentPosition(0, 0)
{
    m_contentLayer = new LayerAndroid(true);
    if (m_client) {
        RenderLayerBacking* backing = static_cast<RenderLayerBacking*>(m_client);
        RenderLayer* renderLayer = backing->owningLayer();
        m_contentLayer->setIsRootLayer(renderLayer->isRootLayer());
    }
    gDebugGraphicsLayerAndroidInstances++;
}

GraphicsLayerAndroid::~GraphicsLayerAndroid()
{
    m_contentLayer->unref();
    gDebugGraphicsLayerAndroidInstances--;
}

void GraphicsLayerAndroid::setName(const String& name)
{
    GraphicsLayer::setName(name);
}

NativeLayer GraphicsLayerAndroid::nativeLayer() const
{
    LOG("(%x) nativeLayer", this);
    return 0;
}

bool GraphicsLayerAndroid::setChildren(const Vector<GraphicsLayer*>& children)
{
    bool childrenChanged = GraphicsLayer::setChildren(children);
    if (childrenChanged) {
        m_needsSyncChildren = true;
        askForSync();
    }

    return childrenChanged;
}

void GraphicsLayerAndroid::addChild(GraphicsLayer* childLayer)
{
#ifndef NDEBUG
    const char* n = (static_cast<GraphicsLayerAndroid*>(childLayer))->m_name.latin1().data();
    LOG("(%x) addChild: %x (%s)", this, childLayer, n);
#endif
    GraphicsLayer::addChild(childLayer);
    m_needsSyncChildren = true;
    askForSync();
}

void GraphicsLayerAndroid::addChildAtIndex(GraphicsLayer* childLayer, int index)
{
    LOG("(%x) addChild %x AtIndex %d", this, childLayer, index);
    GraphicsLayer::addChildAtIndex(childLayer, index);
    m_needsSyncChildren = true;
    askForSync();
}

void GraphicsLayerAndroid::addChildBelow(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    LOG("(%x) addChild %x Below %x", this, childLayer, sibling);
    GraphicsLayer::addChildBelow(childLayer, sibling);
    m_needsSyncChildren = true;
    askForSync();
}

void GraphicsLayerAndroid::addChildAbove(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    LOG("(%x) addChild %x Above %x", this, childLayer, sibling);
    GraphicsLayer::addChildAbove(childLayer, sibling);
    m_needsSyncChildren = true;
    askForSync();
}

bool GraphicsLayerAndroid::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    LOG("(%x) replaceChild %x by %x", this, oldChild, newChild);
    bool ret = GraphicsLayer::replaceChild(oldChild, newChild);
    m_needsSyncChildren = true;
    askForSync();
    return ret;
}

void GraphicsLayerAndroid::removeFromParent()
{
    LOG("(%x) removeFromParent()", this);
    if (m_parent)
        static_cast<GraphicsLayerAndroid*>(m_parent)->needsSyncChildren();
    GraphicsLayer::removeFromParent();
    m_needsSyncChildren = true;
    askForSync();
}

void GraphicsLayerAndroid::needsSyncChildren()
{
    m_needsSyncChildren = true;
    askForSync();
}

void GraphicsLayerAndroid::updateFixedPosition()
{
    if (!m_client)
        return;

    RenderLayerBacking* backing = static_cast<RenderLayerBacking*>(m_client);
    RenderLayer* renderLayer = backing->owningLayer();
    RenderView* view = static_cast<RenderView*>(renderLayer->renderer());

    // If we are a fixed position layer, just set it
    if (view->isPositioned() && view->style()->position() == FixedPosition) {
        SkLength left, top, right, bottom;
        left = convertLength(view->style()->left());
        top = convertLength(view->style()->top());
        right = convertLength(view->style()->right());
        bottom = convertLength(view->style()->bottom());
        // We need to pass the size of the element to compute the final fixed
        // position -- we can't use the layer's size as it could possibly differs.
        // We also have to use the visible overflow and not just the size,
        // as some child elements could be overflowing.
        int w = view->rightVisibleOverflow() - view->leftVisibleOverflow();
        int h = view->bottomVisibleOverflow() - view->topVisibleOverflow();

        SkLength marginLeft, marginTop, marginRight, marginBottom;
        marginLeft = convertLength(view->style()->marginLeft());
        marginTop = convertLength(view->style()->marginTop());
        marginRight = convertLength(view->style()->marginRight());
        marginBottom = convertLength(view->style()->marginBottom());

        m_contentLayer->setFixedPosition(left, top, right, bottom,
                                         marginLeft, marginTop,
                                         marginRight, marginBottom,
                                         offsetFromRenderer().width(),
                                         offsetFromRenderer().height(),
                                         w, h);
    }
}

void GraphicsLayerAndroid::setPosition(const FloatPoint& point)
{
    m_currentPosition = point;
    m_needsDisplay = true;
#ifdef LAYER_DEBUG_2
    LOG("(%x) setPosition(%.2f,%.2f) pos(%.2f, %.2f) anchor(%.2f,%.2f) size(%.2f, %.2f)",
        this, point.x(), point.y(), m_currentPosition.x(), m_currentPosition.y(),
        m_anchorPoint.x(), m_anchorPoint.y(), m_size.width(), m_size.height());
#endif
    updateFixedPosition();
    askForSync();
}

void GraphicsLayerAndroid::setAnchorPoint(const FloatPoint3D& point)
{
    GraphicsLayer::setAnchorPoint(point);
    m_contentLayer->setAnchorPoint(point.x(), point.y());
    askForSync();
}

void GraphicsLayerAndroid::setSize(const FloatSize& size)
{
    if ((size.width() != m_size.width())
          || (size.height() != m_size.height())) {
        MLOG("(%x) setSize (%.2f,%.2f)", this, size.width(), size.height());
        GraphicsLayer::setSize(size);
        m_contentLayer->setSize(size.width(), size.height());
        askForSync();
    }
}

void GraphicsLayerAndroid::setTransform(const TransformationMatrix& t)
{
    TransformationMatrix::DecomposedType tDecomp;
    t.decompose(tDecomp);
    LOG("(%x) setTransform, translate (%.2f, %.2f), mpos(%.2f,%.2f)",
        this, tDecomp.translateX, tDecomp.translateY,
        m_position.x(), m_position.y());

    if ((m_currentTranslateX != tDecomp.translateX)
          || (m_currentTranslateY != tDecomp.translateY)) {
        m_currentTranslateX = tDecomp.translateX;
        m_currentTranslateY = tDecomp.translateY;
        m_needsDisplay = true;
        askForSync();
    }
}

void GraphicsLayerAndroid::setChildrenTransform(const TransformationMatrix& t)
{
    if (t == m_childrenTransform)
       return;
    LOG("(%x) setChildrenTransform", this);

    GraphicsLayer::setChildrenTransform(t);
    for (unsigned int i = 0; i < m_children.size(); i++) {
        GraphicsLayer* layer = m_children[i];
        layer->setTransform(t);
        if (layer->children().size())
            layer->setChildrenTransform(t);
    }
    askForSync();
}

void GraphicsLayerAndroid::setMaskLayer(GraphicsLayer* layer)
{
    if (layer == m_maskLayer)
      return;

    GraphicsLayer::setMaskLayer(layer);
    m_needsSyncMask = true;
    askForSync();
}

void GraphicsLayerAndroid::setMasksToBounds(bool masksToBounds)
{
    GraphicsLayer::setMasksToBounds(masksToBounds);
    m_needsSyncMask = true;
    askForSync();
}

void GraphicsLayerAndroid::setDrawsContent(bool drawsContent)
{
    GraphicsLayer::setDrawsContent(drawsContent);

    if (m_drawsContent) {
        m_haveContents = true;
        setNeedsDisplay();
    }
    askForSync();
}

void GraphicsLayerAndroid::setBackgroundColor(const Color& color)
{
    LOG("(%x) setBackgroundColor", this);
    GraphicsLayer::setBackgroundColor(color);
    SkColor c = SkColorSetARGB(color.alpha(), color.red(), color.green(), color.blue());
    m_contentLayer->setBackgroundColor(c);
    m_haveContents = true;
    askForSync();
}

void GraphicsLayerAndroid::clearBackgroundColor()
{
    LOG("(%x) clearBackgroundColor", this);
    GraphicsLayer::clearBackgroundColor();
    askForSync();
}

void GraphicsLayerAndroid::setContentsOpaque(bool opaque)
{
    LOG("(%x) setContentsOpaque (%d)", this, opaque);
    GraphicsLayer::setContentsOpaque(opaque);
    m_haveContents = true;
    askForSync();
}

void GraphicsLayerAndroid::setOpacity(float opacity)
{
    LOG("(%x) setOpacity: %.2f", this, opacity);
    float clampedOpacity = max(0.0f, min(opacity, 1.0f));

    if (clampedOpacity == m_opacity)
        return;

    MLOG("(%x) setFinalOpacity: %.2f=>%.2f (%.2f)", this,
        opacity, clampedOpacity, m_opacity);
    GraphicsLayer::setOpacity(clampedOpacity);
    m_contentLayer->setOpacity(clampedOpacity);
    askForSync();
}

bool GraphicsLayerAndroid::repaintAll()
{
     LOG("(%x) repaintAll", this);
     bool ret = false;
     for (unsigned int i = 0; i < m_children.size(); i++) {
         GraphicsLayerAndroid* layer = static_cast<GraphicsLayerAndroid*>(m_children[i]);
         if (layer && layer->repaintAll())
             ret = true;
     }
     int nbRects = m_invalidatedRects.size();

     for (int i = 0; !gPaused && i < nbRects; i++) {
         FloatRect rect = m_invalidatedRects[i];
         if (repaint(rect))
             ret = true;
     }
     if (!gPaused) {
         m_needsRepaint = false;
         m_invalidatedRects.clear();
     }
     return ret;
}

void GraphicsLayerAndroid::setNeedsDisplay()
{
    LOG("(%x) setNeedsDisplay()", this);
    FloatRect rect(0, 0, m_size.width(), m_size.height());
    setNeedsDisplayInRect(rect);
}

void GraphicsLayerAndroid::setFrame(Frame* f)
{
    m_frame = f;
}

void GraphicsLayerAndroid::sendImmediateRepaint()
{
    LOG("(%x) sendImmediateRepaint()", this);
    GraphicsLayerAndroid* rootGraphicsLayer = this;

    while (rootGraphicsLayer->parent())
        rootGraphicsLayer = static_cast<GraphicsLayerAndroid*>(rootGraphicsLayer->parent());

    if (rootGraphicsLayer->m_frame
        && rootGraphicsLayer->m_frame->view()) {
        LayerAndroid* rootLayer = new LayerAndroid(true);
        LayerAndroid* copyLayer = new LayerAndroid(*m_contentLayer);
        rootLayer->addChild(copyLayer);
        copyLayer->unref();
        TLOG("(%x) sendImmediateRepaint, copy the layer, (%.2f,%.2f => %.2f,%.2f)",
            this, m_contentLayer->getSize().width(), m_contentLayer->getSize().height(),
            copyLayer->getSize().width(), copyLayer->getSize().height());
        PlatformBridge::setUIRootLayer(m_frame->view(), rootLayer);
        PlatformBridge::immediateRepaint(m_frame->view());
    }
}

bool GraphicsLayerAndroid::repaint(const FloatRect& rect)
{
    LOG("(%x) repaint(%.2f,%.2f,%.2f,%.2f), gPaused(%d) m_needsRepaint(%d) m_haveContents(%d) ",
        this, rect.x(), rect.y(), rect.width(), rect.height(),
        gPaused, m_needsRepaint, m_haveContents);

    if (!gPaused && m_haveContents && m_needsRepaint) {
        SkAutoPictureRecord arp(m_contentLayer->recordContext(), m_size.width(), m_size.height());
        SkCanvas* recordingCanvas = arp.getRecordingCanvas();

        if (!recordingCanvas)
            return false;

        if ((rect.width() > 0.5) && (rect.height() > 0.5)) {
            IntRect r((int)rect.x(), (int)rect.y(),
                  (int)rect.width(), (int)rect.height());

            PlatformGraphicsContext pgc(recordingCanvas, 0);
            GraphicsContext gc(&pgc);

            // with SkPicture, we request the entire layer's content.
            r.setX(0);
            r.setY(0);
            r.setWidth(m_contentLayer->getWidth());
            r.setHeight(m_contentLayer->getHeight());
            paintGraphicsLayerContents(gc, r);

            TLOG("(%x) repaint(%.2f,%.2f,%.2f,%.2f) on (%.2f,%.2f) contentlayer(%.2f,%.2f,%.2f,%.2f)paintGraphicsLayer called!",
                this, rect.x(), rect.y(), rect.width(),
                rect.height(), m_size.width(), m_size.height(),
                m_contentLayer->getPosition().fX,
                m_contentLayer->getPosition().fY,
                m_contentLayer->getSize().width(),
                m_contentLayer->getSize().height());
        }
        return true;
    }
    return false;
}

void GraphicsLayerAndroid::setNeedsDisplayInRect(const FloatRect& rect)
{
    for (unsigned int i = 0; i < m_children.size(); i++) {
        GraphicsLayerAndroid* layer = static_cast<GraphicsLayerAndroid*>(m_children[i]);
        if (layer) {
            FloatRect childrenRect(m_position.x() + m_translateX + rect.x(),
                                   m_position.y() + m_translateY + rect.y(),
                                   rect.width(), rect.height());
            layer->setNeedsDisplayInRect(childrenRect);
        }
    }
    if (!m_haveImage && !drawsContent()) {
        LOG("(%x) setNeedsDisplay(%.2f,%.2f,%.2f,%.2f) doesn't have content, bypass...",
            this, rect.x(), rect.y(), rect.width(), rect.height());
        return;
    }

    const size_t maxDirtyRects = 8;
    for (size_t i = 0; i < m_invalidatedRects.size(); ++i) {
        if (m_invalidatedRects[i].contains(rect))
            return;
    }

#ifdef LAYER_DEBUG
    LOG("(%x) setNeedsDisplayInRect(%d) - (%.2f, %.2f, %.2f, %.2f)", this,
        m_needsRepaint, rect.x(), rect.y(), rect.width(), rect.height());
#endif

    if (m_invalidatedRects.size() < maxDirtyRects)
        m_invalidatedRects.append(rect);
    else
        m_invalidatedRects[0].unite(rect);

    m_needsRepaint = true;
    askForSync();
}

void GraphicsLayerAndroid::pauseDisplay(bool state)
{
    gPaused = state;
    if (gPaused)
        gPausedDelay = WTF::currentTime() + 1;
}

bool GraphicsLayerAndroid::addAnimation(const KeyframeValueList& valueList,
                                        const IntSize& boxSize,
                                        const Animation* anim,
                                        const String& keyframesName,
                                        double beginTime)
{
    if (!anim || anim->isEmptyOrZeroDuration() || valueList.size() != 2)
    return false;

    bool createdAnimations = false;
    if (valueList.property() == AnimatedPropertyWebkitTransform) {
        createdAnimations = createTransformAnimationsFromKeyframes(valueList,
                                                                   anim,
                                                                   keyframesName,
                                                                   beginTime,
                                                                   boxSize);
    } else {
        createdAnimations = createAnimationFromKeyframes(valueList,
                                                         anim,
                                                         keyframesName,
                                                         beginTime);
    }
    askForSync();
    return createdAnimations;
}

bool GraphicsLayerAndroid::createAnimationFromKeyframes(const KeyframeValueList& valueList,
     const Animation* animation, const String& keyframesName, double beginTime)
{
    bool isKeyframe = valueList.size() > 2;
    TLOG("createAnimationFromKeyframes(%d), name(%s) beginTime(%.2f)",
        isKeyframe, keyframesName.latin1().data(), beginTime);
    // TODO: handles keyframe animations correctly

    switch (valueList.property()) {
    case AnimatedPropertyInvalid: break;
    case AnimatedPropertyWebkitTransform: break;
    case AnimatedPropertyBackgroundColor: break;
    case AnimatedPropertyOpacity: {
        MLOG("ANIMATEDPROPERTYOPACITY");
        const FloatAnimationValue* startVal =
            static_cast<const FloatAnimationValue*>(valueList.at(0));
        const FloatAnimationValue* endVal =
            static_cast<const FloatAnimationValue*>(valueList.at(1));
        RefPtr<AndroidOpacityAnimation> anim = AndroidOpacityAnimation::create(startVal->value(),
                                                                               endVal->value(),
                                                                               animation,
                                                                               beginTime);
        if (keyframesName.isEmpty())
            anim->setName(propertyIdToString(valueList.property()));
        else
            anim->setName(keyframesName);

        m_contentLayer->addAnimation(anim.release());
        needsNotifyClient();
        return true;
    } break;
    }
    return false;
}

void GraphicsLayerAndroid::needsNotifyClient()
{
    m_needsNotifyClient = true;
    askForSync();
}

bool GraphicsLayerAndroid::createTransformAnimationsFromKeyframes(const KeyframeValueList& valueList,
                                                                  const Animation* animation,
                                                                  const String& keyframesName,
                                                                  double beginTime,
                                                                  const IntSize& boxSize)
{
    ASSERT(valueList.property() == AnimatedPropertyWebkitTransform);
    TLOG("createTransformAnimationFromKeyframes, name(%s) beginTime(%.2f)",
        keyframesName.latin1().data(), beginTime);

    TransformOperationList functionList;
    bool listsMatch, hasBigRotation;
    fetchTransformOperationList(valueList, functionList, listsMatch, hasBigRotation);

    // If functionLists don't match we do a matrix animation, otherwise we do a component hardware animation.
    // Also, we can't do component animation unless we have valueFunction, so we need to do matrix animation
    // if that's not true as well.

    bool isMatrixAnimation = !listsMatch;
    size_t numAnimations = isMatrixAnimation ? 1 : functionList.size();
    bool isKeyframe = valueList.size() > 2;

    float fromTranslateX = 0;
    float fromTranslateY = 0;
    float fromTranslateZ = 0;
    float toTranslateX   = 0;
    float toTranslateY   = 0;
    float toTranslateZ   = 0;
    float fromAngle      = 0;
    float toAngle        = 0;
    float fromScaleX     = 1;
    float fromScaleY     = 1;
    float fromScaleZ     = 1;
    float toScaleX       = 1;
    float toScaleY       = 1;
    float toScaleZ       = 1;

    bool doTranslation = false;
    bool doRotation = false;
    bool doScaling = false;

    TLOG("(%x) animateTransform, valueList(%d) functionList(%d) duration(%.2f)", this,
        valueList.size(), functionList.size(), animation->duration());

    // FIXME: add support for the translate 3d operations (when
    // we'll have an OpenGL backend)

    for (unsigned int i = 0; i < valueList.size(); i++) {
        const TransformOperations* operation = ((TransformAnimationValue*)valueList.at(i))->value();
        Vector<RefPtr<TransformOperation> > ops = operation->operations();
        TLOG("(%x) animateTransform, dealing with the %d operation, with %d ops", this, i, ops.size());
        for (unsigned int j = 0; j < ops.size(); j++) {
            TransformOperation* op = ops[j].get();
            TLOG("(%x) animateTransform, dealing with the %d:%d operation, current op: %d (translate is %d, rotate %d, scale %d)",
                this, i, j, op->getOperationType(), TransformOperation::TRANSLATE, TransformOperation::ROTATE, TransformOperation::SCALE);
            if ((op->getOperationType() == TransformOperation::TRANSLATE) ||
                (op->getOperationType() == TransformOperation::TRANSLATE_3D)) {
                TranslateTransformOperation* translateOperation = (TranslateTransformOperation*) op;
                IntSize bounds(m_size.width(), m_size.height());
                float x = translateOperation->x(bounds);
                float y = translateOperation->y(bounds);
                float z = translateOperation->z(bounds);
                if (!i) {
                    fromTranslateX = x;
                    fromTranslateY = y;
                    fromTranslateZ = z;
                } else {
                    toTranslateX = x;
                    toTranslateY = y;
                    toTranslateZ = z;
                }
                TLOG("(%x) animateTransform, the %d operation is a translation(%.2f,%.2f,%.2f)",
                    this, j, x, y, z);
                doTranslation = true;
            } else if (op->getOperationType() == TransformOperation::TRANSLATE_X) {
                TranslateTransformOperation* translateOperation = (TranslateTransformOperation*) op;
                IntSize bounds(m_size.width(), m_size.height());
                float x = translateOperation->x(bounds);
                if (!i)
                    fromTranslateX = x;
                else
                    toTranslateX = x;
                TLOG("(%x) animateTransform, the %d operation is a translation_x(%.2f)",
                    this, j, x);
                doTranslation = true;
            } else if (op->getOperationType() == TransformOperation::TRANSLATE_Y) {
                TranslateTransformOperation* translateOperation = (TranslateTransformOperation*) op;
                IntSize bounds(m_size.width(), m_size.height());
                float y = translateOperation->y(bounds);
                if (!i)
                    fromTranslateY = y;
                else
                    toTranslateY = y;
                TLOG("(%x) animateTransform, the %d operation is a translation_y(%.2f)",
                    this, j, y);
                doTranslation = true;
            } else if (op->getOperationType() == TransformOperation::TRANSLATE_Z) {
                TranslateTransformOperation* translateOperation = (TranslateTransformOperation*) op;
                IntSize bounds(m_size.width(), m_size.height());
                float z = translateOperation->z(bounds);
                if (!i)
                    fromTranslateZ = z;
                else
                    toTranslateZ = z;
                TLOG("(%x) animateTransform, the %d operation is a translation_z(%.2f)",
                    this, j, z);
                doTranslation = true;
            } else if ((op->getOperationType() == TransformOperation::ROTATE)
                          || (op->getOperationType() == TransformOperation::ROTATE_X)
                          || (op->getOperationType() == TransformOperation::ROTATE_Y)) {
                LOG("(%x) animateTransform, the %d operation is a rotation", this, j);
                RotateTransformOperation* rotateOperation = (RotateTransformOperation*) op;
                float angle = rotateOperation->angle();
                TLOG("(%x) animateTransform, the %d operation is a rotation (%d), of angle %.2f",
                    this, j, op->getOperationType(), angle);

                if (!i)
                    fromAngle = angle;
                else
                    toAngle = angle;
                doRotation = true;
            } else if (op->getOperationType() == TransformOperation::SCALE_X) {
                ScaleTransformOperation* scaleOperation = (ScaleTransformOperation*) op;
                if (!i)
                    fromScaleX = scaleOperation->x();
                else
                    toScaleX = scaleOperation->x();
                doScaling = true;
            } else if (op->getOperationType() == TransformOperation::SCALE_Y) {
                ScaleTransformOperation* scaleOperation = (ScaleTransformOperation*) op;
                if (!i)
                    fromScaleY = scaleOperation->y();
                else
                    toScaleY = scaleOperation->y();
                doScaling = true;
            } else if (op->getOperationType() == TransformOperation::SCALE_Z) {
                ScaleTransformOperation* scaleOperation = (ScaleTransformOperation*) op;
                if (!i)
                    fromScaleZ = scaleOperation->z();
                else
                    toScaleZ = scaleOperation->z();
                doScaling = true;
            } else if (op->getOperationType() == TransformOperation::SCALE) {
                ScaleTransformOperation* scaleOperation = (ScaleTransformOperation*) op;
                if (!i) {
                    fromScaleX = scaleOperation->x();
                    fromScaleY = scaleOperation->y();
                    fromScaleZ = scaleOperation->z();
                } else {
                    toScaleX = scaleOperation->x();
                    toScaleY = scaleOperation->y();
                    toScaleZ = scaleOperation->z();
                }
                doScaling = true;
            } else {
                TLOG("(%x) animateTransform, the %d operation is not a rotation (%d)",
                    this, j, op->getOperationType());
            }
        }
    }

    RefPtr<AndroidTransformAnimation> anim = AndroidTransformAnimation::create(animation, beginTime);

    if (keyframesName.isEmpty())
        anim->setName(propertyIdToString(valueList.property()));
    else
        anim->setName(keyframesName);

    anim->setOriginalPosition(m_position);

    if (doTranslation)
        anim->setTranslation(fromTranslateX, fromTranslateY, fromTranslateZ,
                         toTranslateX, toTranslateY, toTranslateZ);
    if (doRotation)
        anim->setRotation(fromAngle, toAngle);
    if (doScaling)
        anim->setScale(fromScaleX, fromScaleY, fromScaleZ,
                       toScaleX, toScaleY, toScaleZ);
    m_contentLayer->addAnimation(anim.release());

    needsNotifyClient();
    return true;
}

void GraphicsLayerAndroid::removeAnimationsForProperty(AnimatedPropertyID anID)
{
    TLOG("NRO removeAnimationsForProperty(%d)", anID);
    m_contentLayer->removeAnimation(propertyIdToString(anID));
    askForSync();
}

void GraphicsLayerAndroid::removeAnimationsForKeyframes(const String& keyframesName)
{
    TLOG("NRO removeAnimationsForKeyframes(%s)", keyframesName.latin1().data());
    m_contentLayer->removeAnimation(keyframesName);
    askForSync();
}

void GraphicsLayerAndroid::pauseAnimation(const String& keyframesName)
{
    TLOG("NRO pauseAnimation(%s)", keyframesName.latin1().data());
}

void GraphicsLayerAndroid::suspendAnimations(double time)
{
    TLOG("NRO suspendAnimations(%.2f)", time);
}

void GraphicsLayerAndroid::resumeAnimations()
{
    TLOG("NRO resumeAnimations()");
}

void GraphicsLayerAndroid::setContentsToImage(Image* image)
{
    TLOG("(%x) setContentsToImage", this, image);
    if (image) {
        m_haveContents = true;
        if (!m_haveImage) {
            m_haveImage = true;
            setNeedsDisplay();
            askForSync();
        }
    }
}

PlatformLayer* GraphicsLayerAndroid::platformLayer() const
{
    LOG("platformLayer");
    return (PlatformLayer*) m_contentLayer;
}

#ifndef NDEBUG
void GraphicsLayerAndroid::setDebugBackgroundColor(const Color& color)
{
}

void GraphicsLayerAndroid::setDebugBorder(const Color& color, float borderWidth)
{
}
#endif

void GraphicsLayerAndroid::setZPosition(float position)
{
    LOG("(%x) setZPosition: %.2f", this, position);
    GraphicsLayer::setZPosition(position);
    askForSync();
}

void GraphicsLayerAndroid::askForSync()
{
    if (m_client)
        m_client->notifySyncRequired(this);
}

void GraphicsLayerAndroid::syncChildren()
{
    if (m_needsSyncChildren) {
        m_contentLayer->removeChildren();
        for (unsigned int i = 0; i < m_children.size(); i++) {
            m_contentLayer->addChild(
                (static_cast<GraphicsLayerAndroid*>(m_children[i]))->contentLayer());
        }
        m_needsSyncChildren = false;
    }
}

void GraphicsLayerAndroid::syncMask()
{
    if (m_needsSyncMask) {
        if (m_maskLayer) {
            GraphicsLayerAndroid* layer = static_cast<GraphicsLayerAndroid*>(m_maskLayer);
            LayerAndroid* mask = reinterpret_cast<LayerAndroid*>(layer->platformLayer());
            m_contentLayer->setMaskLayer(mask);
        } else
            m_contentLayer->setMaskLayer(0);

        m_contentLayer->setMasksToBounds(m_masksToBounds);
        m_needsSyncMask = false;
    }
}

void GraphicsLayerAndroid::syncPositionState()
{
     if (m_needsDisplay) {
         m_translateX = m_currentTranslateX;
         m_translateY = m_currentTranslateY;
         m_position = m_currentPosition;
         m_contentLayer->setTranslation(m_currentTranslateX, m_currentTranslateY);
         m_contentLayer->setPosition(m_currentPosition.x(), m_currentPosition.y());
         m_needsDisplay = false;
     }
}

void GraphicsLayerAndroid::syncCompositingState()
{
    for (unsigned int i = 0; i < m_children.size(); i++) {
        GraphicsLayerAndroid* layer = static_cast<GraphicsLayerAndroid*>(m_children[i]);
        layer->syncCompositingState();
    }

    syncChildren();
    syncMask();
    syncPositionState();

    if (!gPaused || WTF::currentTime() >= gPausedDelay)
        repaintAll();
}

void GraphicsLayerAndroid::notifyClientAnimationStarted()
{
    for (unsigned int i = 0; i < m_children.size(); i++) {
        GraphicsLayerAndroid* layer = static_cast<GraphicsLayerAndroid*>(m_children[i]);
        layer->notifyClientAnimationStarted();
    }

    if (m_needsNotifyClient) {
        if (client())
            client()->notifyAnimationStarted(this, WTF::currentTime());
        m_needsNotifyClient = false;
    }
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
