#include "config.h"
#include "LayerAndroid.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidAnimation.h"
#include "DrawExtra.h"
#include "SkCanvas.h"
#include "SkDrawFilter.h"
#include "SkPaint.h"
#include "SkPicture.h"
#include <wtf/CurrentTime.h>

#define LAYER_DEBUG // Add diagonals for debugging
#undef LAYER_DEBUG

namespace WebCore {

static int gDebugLayerAndroidInstances;
static int gUniqueId;

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

///////////////////////////////////////////////////////////////////////////////

LayerAndroid::LayerAndroid(bool isRootLayer) : SkLayer(),
    m_isRootLayer(isRootLayer),
    m_haveClip(false),
    m_doRotation(false),
    m_isFixed(false),
    m_recordingPicture(0),
    m_extra(0),
    m_uniqueId(++gUniqueId)
{
    m_angleTransform = 0;
    m_translation.set(0, 0);
    m_scale.set(1, 1);
    m_backgroundColor = 0;

    gDebugLayerAndroidInstances++;
}

LayerAndroid::LayerAndroid(const LayerAndroid& layer) : SkLayer(layer),
    m_isRootLayer(layer.m_isRootLayer),
    m_haveClip(layer.m_haveClip),
    m_extra(0), // deliberately not copied
    m_uniqueId(layer.m_uniqueId)
{
    m_doRotation = layer.m_doRotation;
    m_isFixed = layer.m_isFixed;

    m_angleTransform = layer.m_angleTransform;
    m_translation = layer.m_translation;
    m_scale = layer.m_scale;
    m_backgroundColor = layer.m_backgroundColor;

    m_fixedLeft = layer.m_fixedLeft;
    m_fixedTop = layer.m_fixedTop;
    m_fixedRight = layer.m_fixedRight;
    m_fixedBottom = layer.m_fixedBottom;
    m_fixedMarginLeft = layer.m_fixedMarginLeft;
    m_fixedMarginTop = layer.m_fixedMarginTop;
    m_fixedMarginRight = layer.m_fixedMarginRight;
    m_fixedMarginBottom = layer.m_fixedMarginBottom;
    m_fixedOffset = layer.m_fixedOffset;
    m_fixedWidth = layer.m_fixedWidth;
    m_fixedHeight = layer.m_fixedHeight;

    m_recordingPicture = layer.m_recordingPicture;
    SkSafeRef(m_recordingPicture);

    for (int i = 0; i < layer.countChildren(); i++)
        addChild(new LayerAndroid(*layer.getChild(i)))->unref();

    KeyframesMap::const_iterator end = layer.m_animations.end();
    for (KeyframesMap::const_iterator it = layer.m_animations.begin(); it != end; ++it)
        m_animations.add((it->second)->name(), (it->second)->copy());

    gDebugLayerAndroidInstances++;
}

LayerAndroid::LayerAndroid(SkPicture* picture) : SkLayer(),
    m_isRootLayer(true),
    m_haveClip(false),
    m_doRotation(false),
    m_isFixed(false),
    m_recordingPicture(picture),
    m_extra(0),
    m_uniqueId(-1)
{
    m_angleTransform = 0;
    m_translation.set(0, 0);
    m_scale.set(1, 1);
    m_backgroundColor = 0;
    SkSafeRef(m_recordingPicture);
    gDebugLayerAndroidInstances++;
}

LayerAndroid::~LayerAndroid()
{
    removeChildren();
    m_recordingPicture->safeUnref();
    m_animations.clear();
    gDebugLayerAndroidInstances--;
}

static int gDebugNbAnims = 0;

bool LayerAndroid::evaluateAnimations() const
{
    double time = WTF::currentTime();
    gDebugNbAnims = 0;
    return evaluateAnimations(time);
}

bool LayerAndroid::hasAnimations() const
{
    for (int i = 0; i < countChildren(); i++) {
        if (getChild(i)->hasAnimations())
            return true;
    }
    return !!m_animations.size();
}

bool LayerAndroid::evaluateAnimations(double time) const
{
    bool hasRunningAnimations = false;
    for (int i = 0; i < countChildren(); i++) {
        if (getChild(i)->evaluateAnimations(time))
            hasRunningAnimations = true;
    }
    KeyframesMap::const_iterator end = m_animations.end();
    for (KeyframesMap::const_iterator it = m_animations.begin(); it != end; ++it) {
        gDebugNbAnims++;
        LayerAndroid* currentLayer = const_cast<LayerAndroid*>(this);
        if ((it->second)->evaluate(currentLayer, time)) {
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
}

static int gDebugChildLevel;

void LayerAndroid::bounds(SkRect* rect) const
{
    const SkPoint& pos = this->getPosition();
    const SkSize& size = this->getSize();
    rect->fLeft = pos.fX + m_translation.fX;
    rect->fTop = pos.fY + m_translation.fY;
    rect->fRight = rect->fLeft + size.width();
    rect->fBottom = rect->fTop + size.height();
}

static bool boundsIsUnique(const SkTDArray<SkRect>& region,
                           const SkRect& local)
{
    for (int i = 0; i < region.count(); i++) {
        if (region[i].contains(local))
            return false;
    }
    return true;
}

void LayerAndroid::clipArea(SkTDArray<SkRect>* region) const
{
    SkRect local;
    local.set(0, 0, std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max());
    clipInner(region, local);
}

void LayerAndroid::clipInner(SkTDArray<SkRect>* region,
                             const SkRect& local) const
{
    SkRect localBounds;
    bounds(&localBounds);
    localBounds.intersect(local);
    if (localBounds.isEmpty())
        return;
    if (m_recordingPicture && boundsIsUnique(*region, localBounds))
        *region->append() = localBounds;
    for (int i = 0; i < countChildren(); i++)
        getChild(i)->clipInner(region, m_haveClip ? localBounds : local);
}

const LayerAndroid* LayerAndroid::find(int x, int y) const
{
    for (int i = 0; i < countChildren(); i++) {
        const LayerAndroid* found = getChild(i)->find(x, y);
        if (found)
            return found;
    }
    SkRect localBounds;
    bounds(&localBounds);
    if (localBounds.contains(x, y))
        return this;
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

// The Layer bounds and the renderview bounds are not always indentical.
// We need to compute the intersection to correctly compute the
// positiong...
static SkRect computeLayerRect(LayerAndroid* layer) {
  SkRect layerRect, viewRect;
  SkScalar fX, fY;
  fX = layer->getOffset().fX;
  fY = layer->getOffset().fY;
  layerRect.set(0, 0, layer->getSize().width(), layer->getSize().height());
  viewRect.set(-fX, -fY, -fX + layer->getFixedWidth(), -fY + layer->getFixedHeight());
  layerRect.intersect(viewRect);
  return layerRect;
}

void LayerAndroid::updateFixedLayersPositions(const SkRect& viewport)
{
    if (m_isFixed) {
        float w = viewport.width();
        float h = viewport.height();
        float dx = viewport.fLeft;
        float dy = viewport.fTop;
        float x = dx;
        float y = dy;

        SkRect layerRect = computeLayerRect(this);

        if (m_fixedLeft.defined())
            x += m_fixedMarginLeft.calcFloatValue(w) + m_fixedLeft.calcFloatValue(w) - layerRect.fLeft;
        else if (m_fixedRight.defined())
            x += w - m_fixedMarginRight.calcFloatValue(w) - m_fixedRight.calcFloatValue(w) - layerRect.width();

        if (m_fixedTop.defined())
            y += m_fixedMarginTop.calcFloatValue(h) + m_fixedTop.calcFloatValue(h) - layerRect.fTop;
        else if (m_fixedBottom.defined())
            y += h - m_fixedMarginBottom.calcFloatValue(h) - m_fixedBottom.calcFloatValue(h) - layerRect.fTop - layerRect.height();

        this->setPosition(x, y);
    }

    int count = this->countChildren();
    for (int i = 0; i < count; i++) {
        this->getChild(i)->updateFixedLayersPositions(viewport);
    }
}

void LayerAndroid::updatePositions() {
    // apply the viewport to us
    SkMatrix matrix;
    if (!m_isFixed) {
        // turn our fields into a matrix.
        //
        // TODO: this should happen in the caller, and we should remove these
        // fields from our subclass
        matrix.setTranslate(m_translation.fX, m_translation.fY);
        if (m_doRotation) {
            matrix.preRotate(m_angleTransform);
        }
        matrix.preScale(m_scale.fX, m_scale.fY);
        this->setMatrix(matrix);
    }

    // now apply it to our children
    int count = this->countChildren();
    for (int i = 0; i < count; i++) {
        this->getChild(i)->updatePositions();
    }
}

void LayerAndroid::onDraw(SkCanvas* canvas, SkScalar opacity) {
    if (m_haveClip) {
        SkRect r;
        r.set(0, 0, getSize().width(), getSize().height());
        canvas->clipRect(r);
    }

    if (!prepareContext())
        return;

    // we just have this save/restore for opacity...
    SkAutoCanvasRestore restore(canvas, true);

    int canvasOpacity = SkScalarRound(opacity * 255);
    if (canvasOpacity < 255)
        canvas->setDrawFilter(new OpacityDrawFilter(canvasOpacity));

    canvas->drawPicture(*m_recordingPicture);
    if (m_extra)
        m_extra->draw(canvas, this);

#ifdef LAYER_DEBUG
    float w = getSize().width();
    float h = getSize().height();
    SkPaint paint;
    paint.setARGB(128, 255, 0, 0);
    canvas->drawLine(0, 0, w, h, paint);
    canvas->drawLine(0, h, w, 0, paint);
    paint.setARGB(128, 0, 255, 0);
    canvas->drawLine(0, 0, 0, h, paint);
    canvas->drawLine(0, h, w, h, paint);
    canvas->drawLine(w, h, w, 0, paint);
    canvas->drawLine(w, 0, 0, 0, paint);

    if (m_isFixed) {
      SkRect layerRect = computeLayerRect(this);
      SkPaint paint;
      paint.setARGB(128, 0, 0, 255);
      canvas->drawRect(layerRect, paint);
    }
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
    if (!m_isRootLayer) {
        if (force || !m_recordingPicture
            || (m_recordingPicture
                && ((m_recordingPicture->width() != (int) getSize().width())
                   || (m_recordingPicture->height() != (int) getSize().height())))) {
            m_recordingPicture->safeUnref();
            m_recordingPicture = new SkPicture();
        }
    } else if (m_recordingPicture) {
        m_recordingPicture->safeUnref();
        m_recordingPicture = 0;
    }

    return m_recordingPicture;
}

SkRect LayerAndroid::subtractLayers(const SkRect& visibleRect) const
{
    SkRect result;
    if (m_recordingPicture) {
        SkRect globalRect = bounds();
        globalRect.offset(-getPosition()); // localToGlobal adds in position
        SkMatrix globalMatrix;
        localToGlobal(&globalMatrix);
        globalMatrix.mapRect(&globalRect);
        SkIRect roundedGlobal;
        globalRect.round(&roundedGlobal);
        SkIRect iVisibleRect;
        visibleRect.round(&iVisibleRect);
        SkRegion visRegion(iVisibleRect);
        visRegion.op(roundedGlobal, SkRegion::kDifference_Op);
        result.set(visRegion.getBounds());
#if DEBUG_NAV_UI
        SkDebugf("%s visibleRect=(%g,%g,r=%g,b=%g) globalRect=(%g,%g,r=%g,b=%g)"
            "result=(%g,%g,r=%g,b=%g)", __FUNCTION__,
            visibleRect.fLeft, visibleRect.fTop,
            visibleRect.fRight, visibleRect.fBottom,
            globalRect.fLeft, globalRect.fTop,
            globalRect.fRight, globalRect.fBottom,
            result.fLeft, result.fTop, result.fRight, result.fBottom);
#endif
    } else
        result = visibleRect;
    for (int i = 0; i < countChildren(); i++)
        result = getChild(i)->subtractLayers(result);
    return result;
}

// Debug tools : dump the layers tree in a file.
// The format is simple:
// properties have the form: key = value;
// all statements are finished with a semi-colon.
// value can be:
// - int
// - float
// - array of elements
// - composed type
// a composed type enclose properties in { and }
// an array enclose composed types in { }, separated with a comma.
// exemple:
// {
//   x = 3;
//   y = 4;
//   value = {
//     x = 3;
//     y = 4;
//   };
//   anarray = [
//     { x = 3; },
//     { y = 4; }
//   ];
// }

void lwrite(FILE* file, const char* str)
{
    fwrite(str, sizeof(char), strlen(str), file);
}

void writeIndent(FILE* file, int indentLevel)
{
    if (indentLevel)
        fprintf(file, "%*s", indentLevel*2, " ");
}

void writeln(FILE* file, int indentLevel, const char* str)
{
    writeIndent(file, indentLevel);
    lwrite(file, str);
    lwrite(file, "\n");
}

void writeIntVal(FILE* file, int indentLevel, const char* str, int value)
{
    writeIndent(file, indentLevel);
    fprintf(file, "%s = %d;\n", str, value);
}

void writeHexVal(FILE* file, int indentLevel, const char* str, int value)
{
    writeIndent(file, indentLevel);
    fprintf(file, "%s = %x;\n", str, value);
}

void writeFloatVal(FILE* file, int indentLevel, const char* str, float value)
{
    writeIndent(file, indentLevel);
    fprintf(file, "%s = %.3f;\n", str, value);
}

void writePoint(FILE* file, int indentLevel, const char* str, SkPoint point)
{
    writeIndent(file, indentLevel);
    fprintf(file, "%s = { x = %.3f; y = %.3f; };\n", str, point.fX, point.fY);
}

void writeSize(FILE* file, int indentLevel, const char* str, SkSize size)
{
    writeIndent(file, indentLevel);
    fprintf(file, "%s = { w = %.3f; h = %.3f; };\n", str, size.width(), size.height());
}

void writeLength(FILE* file, int indentLevel, const char* str, SkLength length)
{
    if (!length.defined()) return;
    writeIndent(file, indentLevel);
    fprintf(file, "%s = { type = %d; value = %.2f; };\n", str, length.type, length.value);
}

void LayerAndroid::dumpLayers(FILE* file, int indentLevel) const
{
    writeln(file, indentLevel, "{");

    writeHexVal(file, indentLevel + 1, "layer", (int)this);
    writeIntVal(file, indentLevel + 1, "layerId", m_uniqueId);
    writeIntVal(file, indentLevel + 1, "haveClip", m_haveClip);
    writeIntVal(file, indentLevel + 1, "isRootLayer", m_isRootLayer);
    writeIntVal(file, indentLevel + 1, "isFixed", m_isFixed);

    writeFloatVal(file, indentLevel + 1, "opacity", getOpacity());
    writeSize(file, indentLevel + 1, "size", getSize());
    writePoint(file, indentLevel + 1, "position", getPosition());
    writePoint(file, indentLevel + 1, "translation", m_translation);
    writePoint(file, indentLevel + 1, "anchor", getAnchorPoint());
    writePoint(file, indentLevel + 1, "scale", m_scale);

    if (m_doRotation)
        writeFloatVal(file, indentLevel + 1, "angle", m_angleTransform);

    if (m_isFixed) {
        writeLength(file, indentLevel + 1, "fixedLeft", m_fixedLeft);
        writeLength(file, indentLevel + 1, "fixedTop", m_fixedTop);
        writeLength(file, indentLevel + 1, "fixedRight", m_fixedRight);
        writeLength(file, indentLevel + 1, "fixedBottom", m_fixedBottom);
        writeLength(file, indentLevel + 1, "fixedMarginLeft", m_fixedMarginLeft);
        writeLength(file, indentLevel + 1, "fixedMarginTop", m_fixedMarginTop);
        writeLength(file, indentLevel + 1, "fixedMarginRight", m_fixedMarginRight);
        writeLength(file, indentLevel + 1, "fixedMarginBottom", m_fixedMarginBottom);
        writePoint(file, indentLevel + 1, "fixedOffset", m_fixedOffset);
        writeIntVal(file, indentLevel + 1, "fixedWidth", m_fixedWidth);
        writeIntVal(file, indentLevel + 1, "fixedHeight", m_fixedHeight);
    }

    if (m_recordingPicture) {
        writeIntVal(file, indentLevel + 1, "picture width", m_recordingPicture->width());
        writeIntVal(file, indentLevel + 1, "picture height", m_recordingPicture->height());
    }

    if (countChildren()) {
        writeln(file, indentLevel + 1, "children = [");
        for (int i = 0; i < countChildren(); i++) {
            if (i > 0)
                writeln(file, indentLevel + 1, ", ");
            getChild(i)->dumpLayers(file, indentLevel + 1);
        }
        writeln(file, indentLevel + 1, "];");
    }
    writeln(file, indentLevel, "}");
}

void LayerAndroid::dumpToLog() const
{
    FILE* file = fopen("/data/data/com.android.browser/layertmp", "w");
    dumpLayers(file, 0);
    fclose(file);
    file = fopen("/data/data/com.android.browser/layertmp", "r");
    char buffer[512];
    bzero(buffer, sizeof(buffer));
    while (fgets(buffer, sizeof(buffer), file))
        SkDebugf("%s", buffer);
    fclose(file);
}

const LayerAndroid* LayerAndroid::findById(int match) const
{
    if (m_uniqueId == match)
        return this;
    for (int i = 0; i < countChildren(); i++) {
        const LayerAndroid* result = getChild(i)->findById(match);
        if (result)
            return result;
    }
    return 0;
}

void LayerAndroid::setExtra(DrawExtra* extra)
{
    m_extra = extra;
    for (int i = 0; i < countChildren(); i++)
        getChild(i)->setExtra(extra);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
