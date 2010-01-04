#include "config.h"
#include "LayerAndroid.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidAnimation.h"
#include "CString.h"
#include "GraphicsLayerAndroid.h"
#include "PlatformGraphicsContext.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderView.h"
#include "SkDevice.h"
#include "SkDrawFilter.h"
#include <wtf/CurrentTime.h>

#define LAYER_DEBUG // Add diagonals for debugging
#undef LAYER_DEBUG

namespace WebCore {

static int gDebugLayerAndroidInstances;
inline int LayerAndroid::instancesCount()
{
    return gDebugLayerAndroidInstances;
}

class OpacityDrawFilter : public SkDrawFilter {
 public:
    OpacityDrawFilter(int opacity) : m_opacity(opacity) { }
    virtual bool filter(SkCanvas* canvas, SkPaint* paint, Type)
    {
        m_previousOpacity = paint->getAlpha();
        paint->setAlpha(m_opacity);
        return true;
    }
    virtual void restore(SkCanvas* canvas, SkPaint* paint, Type)
    {
        paint->setAlpha(m_previousOpacity);
    }
 private:
    int m_opacity;
    int m_previousOpacity;
};

PassRefPtr<LayerAndroid> LayerAndroid::create(bool isRootLayer)
{
    return adoptRef(new LayerAndroid(isRootLayer));
}

LayerAndroid::LayerAndroid(bool isRootLayer) :
    m_doRotation(false),
    m_isRootLayer(isRootLayer),
    m_isFixed(false),
    m_haveContents(false),
    m_drawsContent(true),
    m_haveImage(false),
    m_haveClip(false),
    m_backgroundColorSet(false),
    m_angleTransform(0),
    m_opacity(1),
    m_size(0, 0),
    m_position(0, 0),
    m_translation(0, 0),
    m_fixedPosition(0, 0),
    m_anchorPoint(0, 0, 0),
    m_scale(1, 1, 1),
    m_recordingPicture(0)
{
    gDebugLayerAndroidInstances++;
}

LayerAndroid::LayerAndroid(LayerAndroid* layer) :
    m_doRotation(layer->m_doRotation),
    m_isRootLayer(layer->m_isRootLayer),
    m_isFixed(layer->m_isFixed),
    m_haveContents(layer->m_haveContents),
    m_drawsContent(layer->m_drawsContent),
    m_haveImage(layer->m_haveImage),
    m_haveClip(layer->m_haveClip),
    m_backgroundColorSet(layer->m_backgroundColorSet),
    m_angleTransform(layer->m_angleTransform),
    m_opacity(layer->m_opacity),
    m_size(layer->m_size),
    m_position(layer->m_position),
    m_translation(layer->m_translation),
    m_fixedPosition(layer->m_fixedPosition),
    m_anchorPoint(layer->m_anchorPoint),
    m_scale(layer->m_scale)
{
    if (layer->m_recordingPicture) {
        layer->m_recordingPicture->ref();
        m_recordingPicture = layer->m_recordingPicture;
    } else
        m_recordingPicture = 0;

    for (unsigned int i = 0; i < layer->m_children.size(); i++)
        m_children.append(adoptRef(new LayerAndroid(layer->m_children[i].get())));

    KeyframesMap::const_iterator end = layer->m_animations.end();
    for (KeyframesMap::const_iterator it = layer->m_animations.begin(); it != end; ++it)
        m_animations.add((it->second)->name(), adoptRef((it->second)->copy()));

    end = m_animations.end();
    for (KeyframesMap::const_iterator it = m_animations.begin(); it != end; ++it)
        (it->second)->setLayer(this);

    gDebugLayerAndroidInstances++;
}

LayerAndroid::~LayerAndroid()
{
    m_recordingPicture->safeUnref();
    m_children.clear();
    m_animations.clear();
    gDebugLayerAndroidInstances--;
}

static int gDebugNbAnims = 0;

Vector<RefPtr<AndroidAnimationValue> >* LayerAndroid::evaluateAnimations() const
{
    double time = WTF::currentTime();
    Vector<RefPtr<AndroidAnimationValue> >* result = new Vector<RefPtr<AndroidAnimationValue> >();
    gDebugNbAnims = 0;
    if (evaluateAnimations(time, result))
      return result;
    return 0;
}

bool LayerAndroid::hasAnimations() const
{
    for (unsigned int i = 0; i < m_children.size(); i++) {
        if (m_children[i]->hasAnimations())
            return true;
    }
    return !!m_animations.size();
}

bool LayerAndroid::evaluateAnimations(double time,
                                      Vector<RefPtr<AndroidAnimationValue> >* result) const
{
    bool hasRunningAnimations = false;
    for (unsigned int i = 0; i < m_children.size(); i++) {
        if (m_children[i]->evaluateAnimations(time, result))
            hasRunningAnimations = true;
    }
    KeyframesMap::const_iterator end = m_animations.end();
    for (KeyframesMap::const_iterator it = m_animations.begin(); it != end; ++it) {
        gDebugNbAnims++;
        if ((it->second)->evaluate(time)) {
            result->append((it->second)->result());
            hasRunningAnimations = true;
        }
    }
    
    return hasRunningAnimations;
}

void LayerAndroid::addAnimation(PassRefPtr<AndroidAnimation> anim)
{
    m_animations.add(anim->name(), anim);
}

void LayerAndroid::removeAnimation(const String& name)
{
    m_animations.remove(name);
}

void LayerAndroid::setFixedPosition(FloatPoint position)
{
    m_fixedPosition = position;
    m_isFixed = true;
}

void LayerAndroid::setDrawsContent(bool drawsContent)
{
    m_drawsContent = drawsContent;
    for (unsigned int i = 0; i < m_children.size(); i++) {
        LayerAndroid* layer = m_children[i].get();
        layer->setDrawsContent(drawsContent);
    }
}

// We only use the bounding rect of the layer as mask...
// TODO: use a real mask?
void LayerAndroid::setMaskLayer(LayerAndroid* layer)
{
    if (layer)
        m_haveClip = true;
}

void LayerAndroid::setMasksToBounds(bool masksToBounds)
{
    m_haveClip = masksToBounds;
}

void LayerAndroid::setBackgroundColor(const Color& color)
{
    m_backgroundColor = color;
    m_backgroundColorSet = true;
    setHaveContents(true);
    setDrawsContent(true);
}

static int gDebugChildLevel;

void LayerAndroid::paintOn(float scrollX, float scrollY, float scale, SkCanvas* canvas)
{
    gDebugChildLevel = 0;
    paintChildren(scrollX, scrollY, scale, canvas, 1);
}

void LayerAndroid::setClip(SkCanvas* canvas)
{
    SkRect clip;
    clip.fLeft = m_position.x() + m_translation.x();
    clip.fTop = m_position.y() + m_translation.y();
    clip.fRight = clip.fLeft + m_size.width();
    clip.fBottom = clip.fTop + m_size.height();
    canvas->clipRect(clip);
}

void LayerAndroid::paintChildren(float scrollX, float scrollY,
                                 float scale, SkCanvas* canvas,
                                 float opacity)
{
    canvas->save();

    if (m_haveClip)
        setClip(canvas);

    paintMe(scrollX, scrollY, scale, canvas, opacity);
    canvas->translate(m_position.x() + m_translation.x(),
                      m_position.y() + m_translation.y());

    for (unsigned int i = 0; i < m_children.size(); i++) {
        LayerAndroid* layer = m_children[i].get();
        if (layer) {
            gDebugChildLevel++;
            layer->paintChildren(scrollX, scrollY, scale, canvas, opacity * m_opacity);
            gDebugChildLevel--;
        }
    }

    canvas->restore();
}

void LayerAndroid::paintMe(float scrollX,
                           float scrollY,
                           float scale,
                           SkCanvas* canvas,
                           float opacity)
{
    if (!prepareContext())
        return;

    if (!m_haveImage && !m_drawsContent && !m_isRootLayer)
        return;

    SkAutoCanvasRestore restore(canvas, true);

    int canvasOpacity = opacity * m_opacity * 255;
    if (canvasOpacity != 255)
        canvas->setDrawFilter(new OpacityDrawFilter(canvasOpacity));

    SkPaint paintMode;
    if (m_backgroundColorSet) {
        paintMode.setARGB(m_backgroundColor.alpha(),
            m_backgroundColor.red(),
            m_backgroundColor.green(),
            m_backgroundColor.blue());
    } else
        paintMode.setARGB(0, 0, 0, 0);

    paintMode.setXfermodeMode(SkXfermode::kSrc_Mode);

    float x, y;
    if (m_isFixed) {
        x = m_fixedPosition.x() + (scrollX / scale);
        y = m_fixedPosition.y() + (scrollY / scale);
    } else {
        x = m_translation.x() + m_position.x();
        y = m_translation.y() + m_position.y();
    }

    canvas->translate(x, y);

    if (m_doRotation) {
        float anchorX = m_anchorPoint.x() * m_size.width();
        float anchorY = m_anchorPoint.y() * m_size.height();
        canvas->translate(anchorX, anchorY);
        canvas->rotate(m_angleTransform);
        canvas->translate(-anchorX, -anchorY);
    }

    float sx = m_scale.x();
    float sy = m_scale.y();
    if (sx > 1.0f || sy > 1.0f) {
        float dx = (sx * m_size.width()) - m_size.width();
        float dy = (sy * m_size.height()) - m_size.height();
        canvas->translate(-dx / 2.0f, -dy / 2.0f);
        canvas->scale(sx, sy);
    }

    m_recordingPicture->draw(canvas);

#ifdef LAYER_DEBUG
    float w = m_size.width();
    float h = m_size.height();
    SkPaint paint;
    paint.setARGB(128, 255, 0, 0);
    canvas->drawLine(0, 0, w, h, paint);
    canvas->drawLine(0, h, w, 0, paint);
    paint.setARGB(128, 0, 255, 0);
    canvas->drawLine(0, 0, 0, h, paint);
    canvas->drawLine(0, h, w, h, paint);
    canvas->drawLine(w, h, w, 0, paint);
    canvas->drawLine(w, 0, 0, 0, paint);
#endif
}

SkPicture* LayerAndroid::recordContext()
{
    if (prepareContext(true))
        return m_recordingPicture;
    return 0;
}

bool LayerAndroid::prepareContext(bool force)
{
    if (!m_haveContents)
        return false;

    if (!m_isRootLayer) {
        if (force || !m_recordingPicture
            || (m_recordingPicture
                && ((m_recordingPicture->width() != (int) m_size.width())
                   || (m_recordingPicture->height() != (int) m_size.height())))) {
            m_recordingPicture->safeUnref();
            m_recordingPicture = new SkPicture();
        }
    } else if (m_recordingPicture) {
        m_recordingPicture->safeUnref();
        m_recordingPicture = 0;
    }

    return m_recordingPicture;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
