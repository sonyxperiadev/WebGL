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

#include <cutils/log.h>
#include <wtf/CurrentTime.h>

#undef LOG
#define LOG(...) android_printLog(ANDROID_LOG_DEBUG, "LayerAndroid", __VA_ARGS__)

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

LayerAndroid::LayerAndroid(bool isRootLayer) : SkLayer(),
    m_isRootLayer(isRootLayer),
    m_haveContents(false),
    m_drawsContent(true),
    m_haveImage(false),
    m_haveClip(false),
    m_recordingPicture(0)
{
    gDebugLayerAndroidInstances++;
}

LayerAndroid::LayerAndroid(LayerAndroid* layer) : SkLayer(layer),
    m_isRootLayer(layer->m_isRootLayer),
    m_haveContents(layer->m_haveContents),
    m_drawsContent(layer->m_drawsContent),
    m_haveImage(layer->m_haveImage),
    m_haveClip(layer->m_haveClip)
{
    m_recordingPicture = layer->m_recordingPicture;
    SkSafeRef(m_recordingPicture);

    for (unsigned int i = 0; i < layer->m_children.size(); i++)
        m_children.append(adoptRef(new LayerAndroid(layer->m_children[i].get())));

    KeyframesMap::const_iterator end = layer->m_animations.end();
    for (KeyframesMap::const_iterator it = layer->m_animations.begin(); it != end; ++it)
        m_animations.add((it->second)->name(), (it->second)->copy());

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
    Vector<RefPtr<AndroidAnimationValue> >* results = new Vector<RefPtr<AndroidAnimationValue> >();
    gDebugNbAnims = 0;
    if (evaluateAnimations(time, results))
      return results;
    delete results;
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
                                      Vector<RefPtr<AndroidAnimationValue> >* results) const
{
    bool hasRunningAnimations = false;
    for (unsigned int i = 0; i < m_children.size(); i++) {
        if (m_children[i]->evaluateAnimations(time, results))
            hasRunningAnimations = true;
    }
    KeyframesMap::const_iterator end = m_animations.end();
    for (KeyframesMap::const_iterator it = m_animations.begin(); it != end; ++it) {
        gDebugNbAnims++;
        LayerAndroid* currentLayer = const_cast<LayerAndroid*>(this);
        if ((it->second)->evaluate(currentLayer, time)) {
            RefPtr<AndroidAnimationValue> result = (it->second)->result();
            if (result)
                results->append(result);
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

void LayerAndroid::setBackgroundColor(SkColor color)
{
    m_backgroundColor = color;
    m_backgroundColorSet = true;
    setHaveContents(true);
    setDrawsContent(true);
}

static int gDebugChildLevel;

void LayerAndroid::paintOn(int scrollX, int scrollY,
                           int width, int height,
                           float scale, SkCanvas* canvas)
{
  SkSize size;
  size.set(width, height);
  paintOn(SkPoint::Make(scrollX, scrollY), size, scale, canvas);
}

void LayerAndroid::paintOn(SkPoint offset, SkSize size, SkScalar scale, SkCanvas* canvas)
{
    gDebugChildLevel = 0;
    int scrollX = offset.fX;
    int scrollY = offset.fY;
    int width = size.width();
    int height = size.height();
    LOG("(%x) PaintOn (scroll(%d,%d) width(%d) height(%d)", this, scrollX, scrollY, width, height);
    paintChildren(scrollX, scrollY, width, height, scale, canvas, 1);
}

void LayerAndroid::setClip(SkCanvas* canvas)
{
    SkRect clip;
    clip.fLeft = m_position.fX + m_translation.fX;
    clip.fTop = m_position.fY + m_translation.fY;
    clip.fRight = clip.fLeft + m_size.width();
    clip.fBottom = clip.fTop + m_size.height();
    canvas->clipRect(clip);
}

void LayerAndroid::paintChildren(int scrollX, int scrollY,
                                 int width, int height,
                                 float scale, SkCanvas* canvas,
                                 float opacity)
{
    canvas->save();

    if (m_haveClip)
        setClip(canvas);

    paintMe(scrollX, scrollY, width, height, scale, canvas, opacity);
    canvas->translate(m_position.fX + m_translation.fX,
                      m_position.fY + m_translation.fY);

    for (unsigned int i = 0; i < m_children.size(); i++) {
        LayerAndroid* layer = m_children[i].get();
        if (layer) {
            gDebugChildLevel++;
            layer->paintChildren(scrollX, scrollY, width, height, scale,
                                 canvas, opacity * m_opacity);
            gDebugChildLevel--;
        }
    }

    canvas->restore();
}

void LayerAndroid::paintMe(int scrollX,
                           int scrollY,
                           int viewWidth,
                           int viewHeight,
                           float scale,
                           SkCanvas* canvas,
                           float opacity)
{
    LOG("(%x) A - paint me (width: %.2f height: %.2f), anchor(%.2f, %.2f), translate(%.2f, %.2f), position(%.2f, %.2f) angle(%.2f) fixed(%d) rotation(%d)",
        this, m_size.width(), m_size.height(), m_anchorPoint.fX, m_anchorPoint.fY, m_translation.fX, m_translation.fY, m_position.fX, m_position.fY, m_angleTransform, m_isFixed, m_doRotation);
    if (!prepareContext())
        return;

    if (!m_haveImage && !m_drawsContent && !m_isRootLayer)
        return;

    SkAutoCanvasRestore restore(canvas, true);

    int canvasOpacity = opacity * m_opacity * 255;
    if (canvasOpacity != 255)
        canvas->setDrawFilter(new OpacityDrawFilter(canvasOpacity));

    /* FIXME
    SkPaint paintMode;
    if (m_backgroundColorSet) {
        paintMode.setColor(m_backgroundColor);
    } else
        paintMode.setARGB(0, 0, 0, 0);

    paintMode.setXfermodeMode(SkXfermode::kSrc_Mode);
    */

    float x = 0;
    float y = 0;
    if (m_isFixed) {
        float w = viewWidth / scale;
        float h = viewHeight / scale;
        float dx = scrollX / scale;
        float dy = scrollY / scale;

        if (m_fixedLeft.defined())
            x = dx + m_fixedLeft.calcFloatValue(w);
        else if (m_fixedRight.defined())
            x = dx + w - m_fixedRight.calcFloatValue(w) - m_size.width();

        if (m_fixedTop.defined())
            y = dy + m_fixedTop.calcFloatValue(h);
        else if (m_fixedBottom.defined())
            y = dy + h - m_fixedBottom.calcFloatValue(h) - m_size.height();

    } else {
        x = m_translation.fX + m_position.fX;
        y = m_translation.fY + m_position.fY;
    }

    canvas->translate(x, y);

    if (m_doRotation) {
        float anchorX = m_anchorPoint.fX * m_size.width();
        float anchorY = m_anchorPoint.fY * m_size.height();
        canvas->translate(anchorX, anchorY);
        canvas->rotate(m_angleTransform);
        canvas->translate(-anchorX, -anchorY);
    }

    float sx = m_scale.fX;
    float sy = m_scale.fY;
    if (sx > 1.0f || sy > 1.0f) {
        float dx = (sx * m_size.width()) - m_size.width();
        float dy = (sy * m_size.height()) - m_size.height();
        canvas->translate(-dx / 2.0f, -dy / 2.0f);
        canvas->scale(sx, sy);
    }

    LOG("(%x) B - paint me (width: %.2f height: %.2f) with x(%.2f) y(%.2f), scale (%.2f, %.2f), anchor(%.2f, %.2f), translate(%.2f, %.2f), position(%.2f, %.2f) angle(%.2f) fixed(%d) rotation(%d)",
        this, m_size.width(), m_size.height(), x, y, sx, sy, m_anchorPoint.fX, m_anchorPoint.fY, m_translation.fX, m_translation.fY, m_position.fX, m_position.fY, m_angleTransform, m_isFixed, m_doRotation);

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
