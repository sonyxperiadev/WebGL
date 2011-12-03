#include "config.h"
#include "LayerAndroid.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidAnimation.h"
#include "ClassTracker.h"
#include "DrawExtra.h"
#include "GLUtils.h"
#include "ImagesManager.h"
#include "MediaLayer.h"
#include "PaintedSurface.h"
#include "ParseCanvas.h"
#include "SkBitmapRef.h"
#include "SkBounder.h"
#include "SkDrawFilter.h"
#include "SkPaint.h"
#include "SkPicture.h"
#include "TilesManager.h"

#include <wtf/CurrentTime.h>
#include <math.h>

#define LAYER_DEBUG // Add diagonals for debugging
#undef LAYER_DEBUG

#include <cutils/log.h>
#include <wtf/text/CString.h>
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "LayerAndroid", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "LayerAndroid", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

static int gUniqueId;

class OpacityDrawFilter : public SkDrawFilter {
public:
    OpacityDrawFilter(int opacity) : m_opacity(opacity) { }
    virtual void filter(SkPaint* paint, Type)
    {
        paint->setAlpha(m_opacity);
    }
private:
    int m_opacity;
};

class HasTextBounder : public SkBounder {
    virtual bool onIRect(const SkIRect& rect)
    {
        return false;
    }
};

class HasTextCanvas : public SkCanvas {
public:
    HasTextCanvas(SkBounder* bounder, SkPicture* picture)
        : m_picture(picture)
        , m_hasText(false)
    {
        setBounder(bounder);
    }

    void setHasText()
    {
        m_hasText = true;
        m_picture->abortPlayback();
    }

    bool hasText()
    {
        return m_hasText;
    }

    virtual bool clipPath(const SkPath&, SkRegion::Op) {
        return true;
    }

    virtual void commonDrawBitmap(const SkBitmap& bitmap,
                                  const SkIRect* rect,
                                  const SkMatrix&,
                                  const SkPaint&) {}

    virtual void drawPaint(const SkPaint& paint) {}
    virtual void drawPath(const SkPath&, const SkPaint& paint) {}
    virtual void drawPoints(PointMode, size_t,
                            const SkPoint [], const SkPaint& paint) {}

    virtual void drawRect(const SkRect& , const SkPaint& paint) {}
    virtual void drawSprite(const SkBitmap& , int , int ,
                            const SkPaint* paint = NULL) {}

    virtual void drawText(const void*, size_t byteLength, SkScalar,
                          SkScalar, const SkPaint& paint)
    {
        setHasText();
    }

    virtual void drawPosText(const void* , size_t byteLength,
                             const SkPoint [], const SkPaint& paint)
    {
        setHasText();
    }

    virtual void drawPosTextH(const void*, size_t byteLength,
                              const SkScalar [], SkScalar,
                              const SkPaint& paint)
    {
        setHasText();
    }

    virtual void drawTextOnPath(const void*, size_t byteLength,
                                const SkPath&, const SkMatrix*,
                                const SkPaint& paint)
    {
        setHasText();
    }

    virtual void drawPicture(SkPicture& picture) {
        SkCanvas::drawPicture(picture);
    }

private:

    SkPicture* m_picture;
    bool m_hasText;
};

///////////////////////////////////////////////////////////////////////////////

LayerAndroid::LayerAndroid(RenderLayer* owner) : Layer(),
    m_haveClip(false),
    m_isFixed(false),
    m_isIframe(false),
    m_backfaceVisibility(true),
    m_visible(true),
    m_preserves3D(false),
    m_anchorPointZ(0),
    m_recordingPicture(0),
    m_uniqueId(++gUniqueId),
    m_texture(0),
    m_imageCRC(0),
    m_pictureUsed(0),
    m_scale(1),
    m_lastComputeTextureSize(0),
    m_owningLayer(owner),
    m_type(LayerAndroid::WebCoreLayer),
    m_hasText(true)
{
    m_backgroundColor = 0;

    m_preserves3D = false;
    m_iframeOffset.set(0,0);
    m_dirtyRegion.setEmpty();
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("LayerAndroid");
    ClassTracker::instance()->add(this);
#endif
}

LayerAndroid::LayerAndroid(const LayerAndroid& layer) : Layer(layer),
    m_haveClip(layer.m_haveClip),
    m_isIframe(layer.m_isIframe),
    m_uniqueId(layer.m_uniqueId),
    m_texture(0),
    m_owningLayer(layer.m_owningLayer),
    m_type(LayerAndroid::UILayer),
    m_hasText(true)
{
    m_isFixed = layer.m_isFixed;
    m_imageCRC = layer.m_imageCRC;
    if (m_imageCRC)
        ImagesManager::instance()->retainImage(m_imageCRC);

    m_renderLayerPos = layer.m_renderLayerPos;
    m_transform = layer.m_transform;
    m_backfaceVisibility = layer.m_backfaceVisibility;
    m_visible = layer.m_visible;
    m_backgroundColor = layer.m_backgroundColor;

    m_fixedLeft = layer.m_fixedLeft;
    m_fixedTop = layer.m_fixedTop;
    m_fixedRight = layer.m_fixedRight;
    m_fixedBottom = layer.m_fixedBottom;
    m_fixedMarginLeft = layer.m_fixedMarginLeft;
    m_fixedMarginTop = layer.m_fixedMarginTop;
    m_fixedMarginRight = layer.m_fixedMarginRight;
    m_fixedMarginBottom = layer.m_fixedMarginBottom;
    m_fixedRect = layer.m_fixedRect;
    m_iframeOffset = layer.m_iframeOffset;
    m_recordingPicture = layer.m_recordingPicture;
    SkSafeRef(m_recordingPicture);

    m_preserves3D = layer.m_preserves3D;
    m_anchorPointZ = layer.m_anchorPointZ;
    m_drawTransform = layer.m_drawTransform;
    m_childrenTransform = layer.m_childrenTransform;
    m_pictureUsed = layer.m_pictureUsed;
    m_dirtyRegion = layer.m_dirtyRegion;
    m_scale = layer.m_scale;
    m_lastComputeTextureSize = 0;

    for (int i = 0; i < layer.countChildren(); i++)
        addChild(layer.getChild(i)->copy())->unref();

    KeyframesMap::const_iterator end = layer.m_animations.end();
    for (KeyframesMap::const_iterator it = layer.m_animations.begin(); it != end; ++it) {
        m_animations.add(it->first, it->second);
    }

    m_hasText = layer.m_hasText;

#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("LayerAndroid - recopy (UI)");
    ClassTracker::instance()->add(this);
#endif
}

void LayerAndroid::checkTextPresence()
{
    if (m_recordingPicture) {
        // Let's check if we have text or not. If we don't, we can limit
        // ourselves to scale 1!
        HasTextBounder hasTextBounder;
        HasTextCanvas checker(&hasTextBounder, m_recordingPicture);
        SkBitmap bitmap;
        bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                         m_recordingPicture->width(),
                         m_recordingPicture->height());
        checker.setBitmapDevice(bitmap);
        checker.drawPicture(*m_recordingPicture);
        m_hasText = checker.hasText();
    }
}

LayerAndroid::LayerAndroid(SkPicture* picture) : Layer(),
    m_haveClip(false),
    m_isFixed(false),
    m_isIframe(false),
    m_recordingPicture(picture),
    m_uniqueId(++gUniqueId),
    m_texture(0),
    m_imageCRC(0),
    m_scale(1),
    m_lastComputeTextureSize(0),
    m_owningLayer(0),
    m_type(LayerAndroid::NavCacheLayer),
    m_hasText(true)
{
    m_backgroundColor = 0;
    SkSafeRef(m_recordingPicture);
    m_iframeOffset.set(0,0);
    m_dirtyRegion.setEmpty();
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("LayerAndroid - from picture");
    ClassTracker::instance()->add(this);
#endif
}

LayerAndroid::~LayerAndroid()
{
    if (m_imageCRC)
        ImagesManager::instance()->releaseImage(m_imageCRC);

    SkSafeUnref(m_recordingPicture);
    m_animations.clear();
#ifdef DEBUG_COUNT
    ClassTracker::instance()->remove(this);
    if (m_type == LayerAndroid::WebCoreLayer)
        ClassTracker::instance()->decrement("LayerAndroid");
    else if (m_type == LayerAndroid::UILayer)
        ClassTracker::instance()->decrement("LayerAndroid - recopy (UI)");
    else if (m_type == LayerAndroid::NavCacheLayer)
        ClassTracker::instance()->decrement("LayerAndroid - from picture");
#endif
}

static int gDebugNbAnims = 0;

bool LayerAndroid::evaluateAnimations()
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

bool LayerAndroid::evaluateAnimations(double time)
{
    bool hasRunningAnimations = false;
    for (int i = 0; i < countChildren(); i++) {
        if (getChild(i)->evaluateAnimations(time))
            hasRunningAnimations = true;
    }

    m_hasRunningAnimations = false;
    int nbAnims = 0;
    KeyframesMap::const_iterator end = m_animations.end();
    for (KeyframesMap::const_iterator it = m_animations.begin(); it != end; ++it) {
        gDebugNbAnims++;
        nbAnims++;
        LayerAndroid* currentLayer = const_cast<LayerAndroid*>(this);
        m_hasRunningAnimations |= (it->second)->evaluate(currentLayer, time);
    }

    return hasRunningAnimations || m_hasRunningAnimations;
}

void LayerAndroid::initAnimations() {
    // tell auto-initializing animations to start now
    for (int i = 0; i < countChildren(); i++)
        getChild(i)->initAnimations();

    KeyframesMap::const_iterator localBegin = m_animations.begin();
    KeyframesMap::const_iterator localEnd = m_animations.end();
    for (KeyframesMap::const_iterator localIt = localBegin; localIt != localEnd; ++localIt)
        (localIt->second)->suggestBeginTime(WTF::currentTime());
}

void LayerAndroid::addDirtyArea()
{
    IntSize layerSize(getSize().width(), getSize().height());

    FloatRect area = TilesManager::instance()->shader()->rectInInvScreenCoord(m_drawTransform, layerSize);
    FloatRect clippingRect = TilesManager::instance()->shader()->rectInScreenCoord(m_clippingRect);
    FloatRect clip = TilesManager::instance()->shader()->convertScreenCoordToInvScreenCoord(clippingRect);

    area.intersect(clip);
    IntRect dirtyArea(area.x(), area.y(), area.width(), area.height());
    m_state->addDirtyArea(dirtyArea);
}

void LayerAndroid::addAnimation(PassRefPtr<AndroidAnimation> prpAnim)
{
    RefPtr<AndroidAnimation> anim = prpAnim;
    pair<String, int> key(anim->name(), anim->type());
    removeAnimationsForProperty(anim->type());
    m_animations.add(key, anim);
}

void LayerAndroid::removeAnimationsForProperty(AnimatedPropertyID property)
{
    KeyframesMap::const_iterator end = m_animations.end();
    Vector<pair<String, int> > toDelete;
    for (KeyframesMap::const_iterator it = m_animations.begin(); it != end; ++it) {
        if ((it->second)->type() == property)
            toDelete.append(it->first);
    }

    for (unsigned int i = 0; i < toDelete.size(); i++)
        m_animations.remove(toDelete[i]);
}

void LayerAndroid::removeAnimationsForKeyframes(const String& name)
{
    KeyframesMap::const_iterator end = m_animations.end();
    Vector<pair<String, int> > toDelete;
    for (KeyframesMap::const_iterator it = m_animations.begin(); it != end; ++it) {
        if ((it->second)->name() == name)
            toDelete.append(it->first);
    }

    for (unsigned int i = 0; i < toDelete.size(); i++)
        m_animations.remove(toDelete[i]);
}

// We only use the bounding rect of the layer as mask...
// FIXME: use a real mask?
void LayerAndroid::setMaskLayer(LayerAndroid* layer)
{
    if (layer)
        m_haveClip = true;
}

void LayerAndroid::setBackgroundColor(SkColor color)
{
    m_backgroundColor = color;
}

static int gDebugChildLevel;

FloatPoint LayerAndroid::translation() const
{
    TransformationMatrix::DecomposedType tDecomp;
    m_transform.decompose(tDecomp);
    FloatPoint p(tDecomp.translateX, tDecomp.translateY);
    return p;
}

SkRect LayerAndroid::bounds() const
{
    SkRect rect;
    bounds(&rect);
    return rect;
}

void LayerAndroid::bounds(SkRect* rect) const
{
    const SkPoint& pos = this->getPosition();
    const SkSize& size = this->getSize();

    // The returned rect has the translation applied
    // FIXME: apply the full transform to the rect,
    // and fix the text selection accordingly
    FloatPoint p(pos.fX, pos.fY);
    p = m_transform.mapPoint(p);
    rect->fLeft = p.x();
    rect->fTop = p.y();
    rect->fRight = p.x() + size.width();
    rect->fBottom = p.y() + size.height();
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

class FindCheck : public SkBounder {
public:
    FindCheck()
        : m_drew(false)
        , m_drewText(false)
    {
    }

    bool drew() const { return m_drew; }
    bool drewText() const { return m_drewText; }
    void reset() { m_drew = m_drewText = false; }

protected:
    virtual bool onIRect(const SkIRect& )
    {
        m_drew = true;
        return false;
    }

    virtual bool onIRectGlyph(const SkIRect& , const SkBounder::GlyphRec& )
    {
        m_drew = m_drewText = true;
        return false;
    }

    bool m_drew;
    bool m_drewText;
};

class FindCanvas : public ParseCanvas {
public:
    void draw(SkPicture* picture, SkScalar offsetX, SkScalar offsetY)
    {
        save();
        translate(-offsetX, -offsetY);
        picture->draw(this);
        restore();
    }
};

class LayerAndroid::FindState {
public:
    static const int TOUCH_SLOP = 10;

    FindState(int x, int y)
        : m_x(x)
        , m_y(y)
        , m_bestX(x)
        , m_bestY(y)
        , m_best(0)
    {
        m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, TOUCH_SLOP * 2,
             TOUCH_SLOP * 2);
        m_checker.setBounder(&m_findCheck);
        m_checker.setBitmapDevice(m_bitmap);
    }

    const LayerAndroid* best() const { return m_best; }
    int bestX() const { return m_bestX; }
    int bestY() const { return m_bestY; }

    bool drew(SkPicture* picture, const SkRect& localBounds)
    {
        m_findCheck.reset();
        SkScalar localX = SkIntToScalar(m_x - TOUCH_SLOP) - localBounds.fLeft;
        SkScalar localY = SkIntToScalar(m_y - TOUCH_SLOP) - localBounds.fTop;
        m_checker.draw(picture, localX, localY);
        return m_findCheck.drew();
    }

    bool drewText() { return m_findCheck.drewText(); }

    void setBest(const LayerAndroid* best, int x, int y)
    {
        m_best = best;
        m_bestX = x;
        m_bestY = y;
    }
    int x() const { return m_x; }
    int y() const { return m_y; }

    void setLocation(int x, int y)
    {
        m_x = x;
        m_y = y;
    }

protected:
    int m_x;
    int m_y;
    int m_bestX;
    int m_bestY;
    const LayerAndroid* m_best;
    FindCheck m_findCheck;
    SkBitmap m_bitmap;
    FindCanvas m_checker;
};

void LayerAndroid::findInner(LayerAndroid::FindState& state) const
{
    int x = state.x();
    int y = state.y();
    SkRect localBounds;
    bounds(&localBounds);
    if (!localBounds.contains(x, y))
        return;
    // Move into local coordinates.
    state.setLocation(x - localBounds.fLeft, y - localBounds.fTop);
    for (int i = 0; i < countChildren(); i++)
        getChild(i)->findInner(state);
    // Move back into the parent coordinates.
    int testX = state.x();
    int testY = state.y();
    state.setLocation(x + localBounds.fLeft, y + localBounds.fTop);
    if (!m_recordingPicture)
        return;
    if (!contentIsScrollable() && !state.drew(m_recordingPicture, localBounds))
        return;
    state.setBest(this, testX, testY); // set last match (presumably on top)
}

const LayerAndroid* LayerAndroid::find(int* xPtr, int* yPtr, SkPicture* root) const
{
    FindState state(*xPtr, *yPtr);
    SkRect rootBounds;
    rootBounds.setEmpty();
    if (root && state.drew(root, rootBounds) && state.drewText())
        return 0; // use the root picture only if it contains the text
    findInner(state);
    *xPtr = state.bestX();
    *yPtr = state.bestY();
    return state.best();
}

///////////////////////////////////////////////////////////////////////////////

bool LayerAndroid::updateFixedLayersPositions(SkRect viewport, LayerAndroid* parentIframeLayer)
{
    bool hasFixedElements = false;
    XLOG("updating fixed positions, using viewport %fx%f - %fx%f",
         viewport.fLeft, viewport.fTop,
         viewport.width(), viewport.height());
    // If this is an iframe, accumulate the offset from the parent with
    // current position, and change the parent pointer.
    if (m_isIframe) {
        // If this is the top level, take the current position
        SkPoint parentOffset;
        parentOffset.set(0,0);
        if (parentIframeLayer)
            parentOffset = parentIframeLayer->getPosition();

        m_iframeOffset = parentOffset + getPosition();

        parentIframeLayer = this;
    }

    if (m_isFixed) {
        hasFixedElements = true;
        // So if this is a fixed layer inside a iframe, use the iframe offset
        // and the iframe's size as the viewport and pass to the children
        if (parentIframeLayer) {
            viewport = SkRect::MakeXYWH(parentIframeLayer->m_iframeOffset.fX,
                                 parentIframeLayer->m_iframeOffset.fY,
                                 parentIframeLayer->getSize().width(),
                                 parentIframeLayer->getSize().height());
        }
        float w = viewport.width();
        float h = viewport.height();
        float dx = viewport.fLeft;
        float dy = viewport.fTop;
        float x = dx;
        float y = dy;

        // It turns out that when it is 'auto', we should use the webkit value
        // from the original render layer's X,Y, that will take care of alignment
        // with the parent's layer and fix Margin etc.
        if (!(m_fixedLeft.defined() || m_fixedRight.defined()))
            x += m_renderLayerPos.x();
        else if (m_fixedLeft.defined() || !m_fixedRight.defined())
            x += m_fixedMarginLeft.calcFloatValue(w) + m_fixedLeft.calcFloatValue(w) - m_fixedRect.fLeft;
        else
            x += w - m_fixedMarginRight.calcFloatValue(w) - m_fixedRight.calcFloatValue(w) - m_fixedRect.fRight;

        if (!(m_fixedTop.defined() || m_fixedBottom.defined()))
            y += m_renderLayerPos.y();
        else if (m_fixedTop.defined() || !m_fixedBottom.defined())
            y += m_fixedMarginTop.calcFloatValue(h) + m_fixedTop.calcFloatValue(h) - m_fixedRect.fTop;
        else
            y += h - m_fixedMarginBottom.calcFloatValue(h) - m_fixedBottom.calcFloatValue(h) - m_fixedRect.fBottom;

        this->setPosition(x, y);
    }

    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        hasFixedElements |= this->getChild(i)->updateFixedLayersPositions(viewport, parentIframeLayer);

    return hasFixedElements;
}

void LayerAndroid::updatePositions()
{
    // apply the viewport to us
    if (!m_isFixed) {
        // turn our fields into a matrix.
        //
        // FIXME: this should happen in the caller, and we should remove these
        // fields from our subclass
        SkMatrix matrix;
        GLUtils::toSkMatrix(matrix, m_transform);
        this->setMatrix(matrix);
    }

    // now apply it to our children
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->updatePositions();
}

void LayerAndroid::updateGLPositionsAndScale(const TransformationMatrix& parentMatrix,
                                             const FloatRect& clipping, float opacity, float scale)
{
    m_atomicSync.lock();
    IntSize layerSize(getSize().width(), getSize().height());
    FloatPoint anchorPoint(getAnchorPoint().fX, getAnchorPoint().fY);
    FloatPoint position(getPosition().fX, getPosition().fY);
    float centerOffsetX = (0.5f - anchorPoint.x()) * layerSize.width();
    float centerOffsetY = (0.5f - anchorPoint.y()) * layerSize.height();
    float originX = anchorPoint.x() * layerSize.width();
    float originY = anchorPoint.y() * layerSize.height();
    TransformationMatrix localMatrix;
    if (!m_isFixed)
        localMatrix = parentMatrix;
    localMatrix.translate3d(originX + position.x(),
                            originY + position.y(),
                            anchorPointZ());
    localMatrix.multiply(m_transform);
    localMatrix.translate3d(-originX,
                            -originY,
                            -anchorPointZ());

    m_atomicSync.unlock();
    setDrawTransform(localMatrix);
    if (m_drawTransform.isIdentityOrTranslation()) {
        // adjust the translation coordinates of the draw transform matrix so
        // that layers (defined in content coordinates) will align to display/view pixels
        float desiredContentX = round(m_drawTransform.m41() * scale) / scale;
        float desiredContentY = round(m_drawTransform.m42() * scale) / scale;
        XLOG("fudging translation from %f, %f to %f, %f",
             m_drawTransform.m41(), m_drawTransform.m42(),
             desiredContentX, desiredContentY);
        m_drawTransform.setM41(desiredContentX);
        m_drawTransform.setM42(desiredContentY);
    }

    m_zValue = TilesManager::instance()->shader()->zValue(m_drawTransform, getSize().width(), getSize().height());

    m_atomicSync.lock();
    m_scale = scale;
    m_atomicSync.unlock();

    opacity *= getOpacity();
    setDrawOpacity(opacity);

    if (m_haveClip) {
        // The clipping rect calculation and intersetion will be done in documents coordinates.
        FloatRect rect(0, 0, layerSize.width(), layerSize.height());
        FloatRect clip = m_drawTransform.mapRect(rect);
        clip.intersect(clipping);
        setDrawClip(clip);
    } else {
        setDrawClip(clipping);
    }

    if (!m_backfaceVisibility
         && m_drawTransform.inverse().m33() < 0) {
         setVisible(false);
         return;
    } else {
         setVisible(true);
    }

    int count = this->countChildren();
    if (!count)
        return;

    // Flatten to 2D if the layer doesn't preserve 3D.
    if (!preserves3D()) {
        localMatrix.setM13(0);
        localMatrix.setM23(0);
        localMatrix.setM31(0);
        localMatrix.setM32(0);
        localMatrix.setM33(1);
        localMatrix.setM34(0);
        localMatrix.setM43(0);
    }

    // now apply it to our children

    if (!m_childrenTransform.isIdentity()) {
        localMatrix.translate(getSize().width() * 0.5f, getSize().height() * 0.5f);
        localMatrix.multiply(m_childrenTransform);
        localMatrix.translate(-getSize().width() * 0.5f, -getSize().height() * 0.5f);
    }
    for (int i = 0; i < count; i++)
        this->getChild(i)->updateGLPositionsAndScale(localMatrix, drawClip(), opacity, scale);
}

void LayerAndroid::setContentsImage(SkBitmapRef* img)
{
    ImageTexture* image = ImagesManager::instance()->setImage(img);
    ImagesManager::instance()->releaseImage(m_imageCRC);
    m_imageCRC = image ? image->imageCRC() : 0;
}

bool LayerAndroid::needsTexture()
{
    return m_imageCRC || (m_recordingPicture
        && m_recordingPicture->width() && m_recordingPicture->height());
}

void LayerAndroid::removeTexture(PaintedSurface* texture)
{
    if (texture == m_texture)
        m_texture = 0;
}

IntRect LayerAndroid::clippedRect() const
{
    IntRect r(0, 0, getWidth(), getHeight());
    IntRect tr = m_drawTransform.mapRect(r);
    IntRect cr = TilesManager::instance()->shader()->clippedRectWithViewport(tr);
    IntRect rect = m_drawTransform.inverse().mapRect(cr);
    return rect;
}

int LayerAndroid::nbLayers()
{
    int nb = 0;
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        nb += this->getChild(i)->nbLayers();
    return nb+1;
}

int LayerAndroid::nbTexturedLayers()
{
    int nb = 0;
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        nb += this->getChild(i)->nbTexturedLayers();
    if (needsTexture())
        nb++;
    return nb;
}

void LayerAndroid::computeTexturesAmount(TexturesResult* result)
{
    if (!result)
        return;

    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->computeTexturesAmount(result);
    if (m_texture && m_visible)
        m_texture->computeTexturesAmount(result);
}

void LayerAndroid::showLayer(int indent)
{
    char spaces[256];
    memset(spaces, 0, 256);
    for (int i = 0; i < indent; i++)
        spaces[i] = ' ';

    if (!indent) {
        XLOGC("\n\n--- LAYERS TREE ---");
        IntRect documentViewport(TilesManager::instance()->shader()->documentViewport());
        XLOGC("documentViewport(%d, %d, %d, %d)",
              documentViewport.x(), documentViewport.y(),
              documentViewport.width(), documentViewport.height());
    }

    IntRect r(0, 0, getWidth(), getHeight());
    IntRect tr = m_drawTransform.mapRect(r);
    IntRect visible = visibleArea();
    IntRect clip(m_clippingRect.x(), m_clippingRect.y(),
                 m_clippingRect.width(), m_clippingRect.height());
    XLOGC("%s [%d:0x%x] - %s %s - area (%d, %d, %d, %d) - visible (%d, %d, %d, %d) "
          "clip (%d, %d, %d, %d) %s %s prepareContext(%x), pic w: %d h: %d",
          spaces, uniqueId(), m_owningLayer,
          needsTexture() ? "needs a texture" : "no texture",
          m_imageCRC ? "has an image" : "no image",
          tr.x(), tr.y(), tr.width(), tr.height(),
          visible.x(), visible.y(), visible.width(), visible.height(),
          clip.x(), clip.y(), clip.width(), clip.height(),
          contentIsScrollable() ? "SCROLLABLE" : "",
          isFixed() ? "FIXED" : "",
          m_recordingPicture,
          m_recordingPicture ? m_recordingPicture->width() : -1,
          m_recordingPicture ? m_recordingPicture->height() : -1);

    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->showLayer(indent + 1);
}

void LayerAndroid::swapTiles()
{
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->swapTiles();

    if (m_texture)
        m_texture->swapTiles();
}

void LayerAndroid::setIsDrawing(bool isDrawing)
{
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->setIsDrawing(isDrawing);

    if (m_texture) {
        m_texture->setDrawingLayer(isDrawing ? this : 0);
        m_texture->clearPaintingLayer();
    }
}

void LayerAndroid::setIsPainting(Layer* drawingTree)
{
    XLOG("setting layer %p as painting, needs texture %d, drawing tree %p",
         this, needsTexture(), drawingTree);
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->setIsPainting(drawingTree);


    LayerAndroid* drawingLayer = 0;
    if (drawingTree)
        drawingLayer = static_cast<LayerAndroid*>(drawingTree)->findById(uniqueId());

    obtainTextureForPainting(drawingLayer);
}

void LayerAndroid::mergeInvalsInto(Layer* replacementTree)
{
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->mergeInvalsInto(replacementTree);

    LayerAndroid* replacementLayer = static_cast<LayerAndroid*>(replacementTree)->findById(uniqueId());
    if (replacementLayer)
        replacementLayer->markAsDirty(m_dirtyRegion);
}

bool LayerAndroid::isReady()
{
    int count = countChildren();
    for (int i = 0; i < count; i++)
        if (!getChild(i)->isReady())
            return false;

    if (m_texture)
        return m_texture->isReady();
    // TODO: image, check if uploaded?
    return true;
}

bool LayerAndroid::updateWithTree(LayerAndroid* newTree)
{
// Disable fast update for now
#if (0)
    bool needsRepaint = false;
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        needsRepaint |= this->getChild(i)->updateWithTree(newTree);

    if (newTree) {
        LayerAndroid* newLayer = newTree->findById(uniqueId());
        needsRepaint |= updateWithLayer(newLayer);
    }
    return needsRepaint;
#else
    return true;
#endif
}

// Return true to indicate to WebViewCore that the updates
// are too complicated to be fully handled and we need a full
// call to webkit (e.g. handle repaints)
bool LayerAndroid::updateWithLayer(LayerAndroid* layer)
{
    if (!layer)
        return true;

    android::AutoMutex lock(m_atomicSync);
    m_position = layer->m_position;
    m_anchorPoint = layer->m_anchorPoint;
    m_size = layer->m_size;
    m_opacity = layer->m_opacity;
    m_transform = layer->m_transform;

    if (m_imageCRC != layer->m_imageCRC)
        m_visible = false;

    if ((m_recordingPicture != layer->m_recordingPicture)
        || (m_imageCRC != layer->m_imageCRC))
        return true;

    return false;
}

void LayerAndroid::obtainTextureForPainting(LayerAndroid* drawingLayer)
{
    if (!needsTexture())
        return;

    if (m_imageCRC) {
        if (m_texture) {
            m_texture->setDrawingLayer(0);
            m_texture->clearPaintingLayer();
            m_texture = 0;
        }
    } else {
        if (drawingLayer) {
            // if a previous tree had the same layer, paint with that painted surface
            m_texture = drawingLayer->m_texture;
        }

        if (!m_texture)
            m_texture = new PaintedSurface();

        // pass the invalidated regions to the PaintedSurface
        m_texture->setPaintingLayer(this, m_dirtyRegion);
    }
    m_dirtyRegion.setEmpty();
}


static inline bool compareLayerZ(const LayerAndroid* a, const LayerAndroid* b)
{
    return a->zValue() > b->zValue();
}

// We call this in WebViewCore, when copying the tree of layers.
// As we construct a new tree that will be passed on the UI,
// we mark the webkit-side tree as having no more dirty region
// (otherwise we would continuously have those dirty region UI-side)
void LayerAndroid::clearDirtyRegion()
{
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->clearDirtyRegion();

    m_dirtyRegion.setEmpty();
}

void LayerAndroid::prepare()
{
    XLOG("LA %p preparing, m_texture %p", this, m_texture);

    int count = this->countChildren();
    if (count > 0) {
        Vector <LayerAndroid*> sublayers;
        for (int i = 0; i < count; i++)
            sublayers.append(this->getChild(i));

        // now we sort for the transparency
        std::stable_sort(sublayers.begin(), sublayers.end(), compareLayerZ);

        // iterate in reverse so top layers get textures first
        for (int i = count-1; i >= 0; i--)
            sublayers[i]->prepare();
    }

    if (m_texture)
        m_texture->prepare(m_state);
}

IntRect LayerAndroid::unclippedArea()
{
    IntRect area;
    area.setX(0);
    area.setY(0);
    area.setWidth(getSize().width());
    area.setHeight(getSize().height());
    return area;
}

IntRect LayerAndroid::visibleArea()
{
    IntRect area = unclippedArea();
    // First, we get the transformed area of the layer,
    // in document coordinates
    IntRect rect = m_drawTransform.mapRect(area);
    int dx = rect.x();
    int dy = rect.y();

    // Then we apply the clipping
    IntRect clip(m_clippingRect);
    rect.intersect(clip);

    // Now clip with the viewport in documents coordinate
    IntRect documentViewport(TilesManager::instance()->shader()->documentViewport());
    rect.intersect(documentViewport);

    // Finally, let's return the visible area, in layers coordinate
    rect.move(-dx, -dy);
    return rect;
}

bool LayerAndroid::drawCanvas(SkCanvas* canvas)
{
    if (!m_visible)
        return false;

    bool askScreenUpdate = false;

    {
        SkAutoCanvasRestore acr(canvas, true);
        SkRect r;
        r.set(m_clippingRect.x(), m_clippingRect.y(),
              m_clippingRect.x() + m_clippingRect.width(),
              m_clippingRect.y() + m_clippingRect.height());
        canvas->clipRect(r);
        SkMatrix matrix;
        GLUtils::toSkMatrix(matrix, m_drawTransform);
        SkMatrix canvasMatrix = canvas->getTotalMatrix();
        matrix.postConcat(canvasMatrix);
        canvas->setMatrix(matrix);
        SkRect layerRect;
        layerRect.fLeft = 0;
        layerRect.fTop = 0;
        layerRect.fRight = getWidth();
        layerRect.fBottom = getHeight();
        onDraw(canvas, m_drawOpacity);
    }

    // When the layer is dirty, the UI thread should be notified to redraw.
    askScreenUpdate |= drawChildrenCanvas(canvas);
    m_atomicSync.lock();
    if (askScreenUpdate || m_hasRunningAnimations || m_drawTransform.hasPerspective())
        addDirtyArea();

    m_atomicSync.unlock();
    return askScreenUpdate;
}

bool LayerAndroid::drawGL()
{
    FloatRect clippingRect = TilesManager::instance()->shader()->rectInScreenCoord(m_clippingRect);
    TilesManager::instance()->shader()->clip(clippingRect);
    if (!m_visible)
        return false;

    bool askScreenUpdate = false;

    if (m_state->layersRenderingMode() < GLWebViewState::kScrollableAndFixedLayers) {
        if (m_texture)
            askScreenUpdate |= m_texture->draw();
        if (m_imageCRC) {
            ImageTexture* imageTexture = ImagesManager::instance()->retainImage(m_imageCRC);
            if (imageTexture)
                imageTexture->drawGL(this);
            ImagesManager::instance()->releaseImage(m_imageCRC);
        }
    }

    // When the layer is dirty, the UI thread should be notified to redraw.
    askScreenUpdate |= drawChildrenGL();
    m_atomicSync.lock();
    if (askScreenUpdate || m_hasRunningAnimations || m_drawTransform.hasPerspective())
        addDirtyArea();

    m_atomicSync.unlock();
    return askScreenUpdate;
}

bool LayerAndroid::drawChildrenCanvas(SkCanvas* canvas)
{
    bool askScreenUpdate = false;
    int count = this->countChildren();
    if (count > 0) {
        Vector <LayerAndroid*> sublayers;
        for (int i = 0; i < count; i++)
            sublayers.append(this->getChild(i));

        // now we sort for the transparency
        std::stable_sort(sublayers.begin(), sublayers.end(), compareLayerZ);
        for (int i = 0; i < count; i++) {
            LayerAndroid* layer = sublayers[i];
            askScreenUpdate |= layer->drawCanvas(canvas);
        }
    }

    return askScreenUpdate;
}

bool LayerAndroid::drawChildrenGL()
{
    bool askScreenUpdate = false;
    int count = this->countChildren();
    if (count > 0) {
        Vector <LayerAndroid*> sublayers;
        for (int i = 0; i < count; i++)
            sublayers.append(this->getChild(i));

        // now we sort for the transparency
        std::stable_sort(sublayers.begin(), sublayers.end(), compareLayerZ);
        for (int i = 0; i < count; i++) {
            LayerAndroid* layer = sublayers[i];
            askScreenUpdate |= layer->drawGL();
        }
    }

    return askScreenUpdate;
}

void LayerAndroid::contentDraw(SkCanvas* canvas)
{
    if (m_recordingPicture)
      canvas->drawPicture(*m_recordingPicture);

    if (TilesManager::instance()->getShowVisualIndicator()) {
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
          SkPaint paint;
          paint.setARGB(80, 255, 0, 0);
          canvas->drawRect(m_fixedRect, paint);
        }
    }
}

void LayerAndroid::onDraw(SkCanvas* canvas, SkScalar opacity)
{
    if (m_haveClip) {
        SkRect r;
        r.set(0, 0, getSize().width(), getSize().height());
        canvas->clipRect(r);
        return;
    }

    if (!prepareContext())
        return;

    // we just have this save/restore for opacity...
    SkAutoCanvasRestore restore(canvas, true);

    int canvasOpacity = SkScalarRound(opacity * 255);
    if (canvasOpacity < 255)
        canvas->setDrawFilter(new OpacityDrawFilter(canvasOpacity));

    if (m_imageCRC) {
        ImageTexture* imageTexture = ImagesManager::instance()->retainImage(m_imageCRC);
        m_dirtyRegion.setEmpty();
        if (imageTexture) {
            SkRect dest;
            dest.set(0, 0, getSize().width(), getSize().height());
            imageTexture->drawCanvas(canvas, dest);
        }
        ImagesManager::instance()->releaseImage(m_imageCRC);
    }
    contentDraw(canvas);
}

SkPicture* LayerAndroid::recordContext()
{
    if (prepareContext(true))
        return m_recordingPicture;
    return 0;
}

bool LayerAndroid::prepareContext(bool force)
{
    if (masksToBounds())
        return false;

    if (force || !m_recordingPicture ||
        (m_recordingPicture &&
         ((m_recordingPicture->width() != (int) getSize().width()) ||
          (m_recordingPicture->height() != (int) getSize().height())))) {
        SkSafeUnref(m_recordingPicture);
        m_recordingPicture = new SkPicture();
    }

    return m_recordingPicture;
}

SkRect LayerAndroid::subtractLayers(const SkRect& visibleRect) const
{
    SkRect result;
    if (m_recordingPicture) {
        // FIXME: This seems wrong. localToGlobal() applies the full local transform,
        // se surely we should operate globalMatrix on size(), not bounds() with
        // the position removed? Perhaps we never noticed the bug because most
        // layers don't use a local transform?
        // See http://b/5338388
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

void writeRect(FILE* file, int indentLevel, const char* str, SkRect rect)
{
    writeIndent(file, indentLevel);
    fprintf(file, "%s = { x = %.3f; y = %.3f; w = %.3f; h = %.3f; };\n",
            str, rect.fLeft, rect.fTop, rect.width(), rect.height());
}

void writeLength(FILE* file, int indentLevel, const char* str, SkLength length)
{
    if (!length.defined())
        return;
    writeIndent(file, indentLevel);
    fprintf(file, "%s = { type = %d; value = %.2f; };\n", str, length.type, length.value);
}

void writeMatrix(FILE* file, int indentLevel, const char* str, const TransformationMatrix& matrix)
{
    writeIndent(file, indentLevel);
    fprintf(file, "%s = { (%.2f,%.2f,%.2f,%.2f),(%.2f,%.2f,%.2f,%.2f),"
            "(%.2f,%.2f,%.2f,%.2f),(%.2f,%.2f,%.2f,%.2f) };\n",
            str,
            matrix.m11(), matrix.m12(), matrix.m13(), matrix.m14(),
            matrix.m21(), matrix.m22(), matrix.m23(), matrix.m24(),
            matrix.m31(), matrix.m32(), matrix.m33(), matrix.m34(),
            matrix.m41(), matrix.m42(), matrix.m43(), matrix.m44());
}

void LayerAndroid::dumpLayers(FILE* file, int indentLevel) const
{
    writeln(file, indentLevel, "{");

    writeHexVal(file, indentLevel + 1, "layer", (int)this);
    writeIntVal(file, indentLevel + 1, "layerId", m_uniqueId);
    writeIntVal(file, indentLevel + 1, "haveClip", m_haveClip);
    writeIntVal(file, indentLevel + 1, "isFixed", m_isFixed);
    writeIntVal(file, indentLevel + 1, "m_isIframe", m_isIframe);
    writePoint(file, indentLevel + 1, "m_iframeOffset", m_iframeOffset);

    writeFloatVal(file, indentLevel + 1, "opacity", getOpacity());
    writeSize(file, indentLevel + 1, "size", getSize());
    writePoint(file, indentLevel + 1, "position", getPosition());
    writePoint(file, indentLevel + 1, "anchor", getAnchorPoint());

    writeMatrix(file, indentLevel + 1, "drawMatrix", m_drawTransform);
    writeMatrix(file, indentLevel + 1, "transformMatrix", m_transform);
    writeRect(file, indentLevel + 1, "clippingRect", SkRect(m_clippingRect));

    if (m_isFixed) {
        writeLength(file, indentLevel + 1, "fixedLeft", m_fixedLeft);
        writeLength(file, indentLevel + 1, "fixedTop", m_fixedTop);
        writeLength(file, indentLevel + 1, "fixedRight", m_fixedRight);
        writeLength(file, indentLevel + 1, "fixedBottom", m_fixedBottom);
        writeLength(file, indentLevel + 1, "fixedMarginLeft", m_fixedMarginLeft);
        writeLength(file, indentLevel + 1, "fixedMarginTop", m_fixedMarginTop);
        writeLength(file, indentLevel + 1, "fixedMarginRight", m_fixedMarginRight);
        writeLength(file, indentLevel + 1, "fixedMarginBottom", m_fixedMarginBottom);
        writeRect(file, indentLevel + 1, "fixedRect", m_fixedRect);
    }

    if (m_recordingPicture) {
        writeIntVal(file, indentLevel + 1, "m_recordingPicture.width", m_recordingPicture->width());
        writeIntVal(file, indentLevel + 1, "m_recordingPicture.height", m_recordingPicture->height());
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

LayerAndroid* LayerAndroid::findById(int match)
{
    if (m_uniqueId == match)
        return this;
    for (int i = 0; i < countChildren(); i++) {
        LayerAndroid* result = getChild(i)->findById(match);
        if (result)
            return result;
    }
    return 0;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
