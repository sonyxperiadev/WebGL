#include "config.h"
#include "LayerAndroid.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidAnimation.h"
#include "DrawExtra.h"
#include "GLUtils.h"
#include "MediaLayer.h"
#include "PaintLayerOperation.h"
#include "ParseCanvas.h"
#include "SkBitmapRef.h"
#include "SkBounder.h"
#include "SkDrawFilter.h"
#include "SkPaint.h"
#include "SkPicture.h"
#include "TilesManager.h"
#include <wtf/CurrentTime.h>

#define LAYER_DEBUG // Add diagonals for debugging
#undef LAYER_DEBUG

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "LayerAndroid", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

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
    m_isFixed(false),
    m_preserves3D(false),
    m_anchorPointZ(0),
    m_recordingPicture(0),
    m_contentsImage(0),
    m_extra(0),
    m_uniqueId(++gUniqueId),
    m_drawingTexture(0),
    m_reservedTexture(0),
    m_pictureUsed(-1),
    m_requestSent(false),
    m_scale(1)
{
    m_backgroundColor = 0;

    m_preserves3D = false;
    m_dirty = false;

    gDebugLayerAndroidInstances++;
}

LayerAndroid::LayerAndroid(const LayerAndroid& layer) : SkLayer(layer),
    m_isRootLayer(layer.m_isRootLayer),
    m_haveClip(layer.m_haveClip),
    m_extra(0), // deliberately not copied
    m_uniqueId(layer.m_uniqueId),
    m_drawingTexture(0),
    m_reservedTexture(0),
    m_requestSent(false)
{
    m_isFixed = layer.m_isFixed;
    m_contentsImage = layer.m_contentsImage;
    m_contentsImage->safeRef();

    m_transform = layer.m_transform;
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

    m_recordingPicture = layer.m_recordingPicture;
    SkSafeRef(m_recordingPicture);

    m_preserves3D = layer.m_preserves3D;
    m_anchorPointZ = layer.m_anchorPointZ;
    m_drawTransform = layer.m_drawTransform;
    m_childrenTransform = layer.m_childrenTransform;
    m_dirty = layer.m_dirty;
    m_pictureUsed = layer.m_pictureUsed;
    m_scale = layer.m_scale;

    for (int i = 0; i < layer.countChildren(); i++)
        addChild(layer.getChild(i)->copy())->unref();

    KeyframesMap::const_iterator end = layer.m_animations.end();
    for (KeyframesMap::const_iterator it = layer.m_animations.begin(); it != end; ++it)
        m_animations.add((it->second)->name(), (it->second)->copy());

    gDebugLayerAndroidInstances++;
}

LayerAndroid::LayerAndroid(SkPicture* picture) : SkLayer(),
    m_isRootLayer(true),
    m_haveClip(false),
    m_isFixed(false),
    m_recordingPicture(picture),
    m_contentsImage(0),
    m_extra(0),
    m_uniqueId(-1),
    m_drawingTexture(0),
    m_reservedTexture(0),
    m_requestSent(false),
    m_scale(1)
{
    m_backgroundColor = 0;
    m_dirty = false;
    SkSafeRef(m_recordingPicture);
    gDebugLayerAndroidInstances++;
}

void LayerAndroid::removeTexture(BackedDoubleBufferedTexture* aTexture)
{
    LayerTexture* texture = static_cast<LayerTexture*>(aTexture);
    android::AutoMutex lock(m_atomicSync);
    if (!texture) { // remove ourself from both textures
        if (m_drawingTexture)
            m_drawingTexture->release(this);
        if (m_reservedTexture &&
            m_reservedTexture != m_drawingTexture)
            m_reservedTexture->release(this);
    } else {
        if (m_drawingTexture && m_drawingTexture == texture)
            m_drawingTexture->release(this);
        if (m_reservedTexture &&
            m_reservedTexture == texture &&
            m_reservedTexture != m_drawingTexture)
            m_reservedTexture->release(this);
    }
    if (m_drawingTexture && m_drawingTexture->owner() != this)
        m_drawingTexture = 0;
    if (m_reservedTexture && m_reservedTexture->owner() != this)
        m_reservedTexture = 0;
}

LayerAndroid::~LayerAndroid()
{
    removeTexture(0);
    removeChildren();
    m_contentsImage->safeUnref();
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
        if ((it->second)->evaluate(currentLayer, time))
            hasRunningAnimations = true;
    }

    return hasRunningAnimations;
}

void LayerAndroid::addAnimation(PassRefPtr<AndroidAnimation> anim)
{
    if (m_animations.get(anim->name()))
        removeAnimation(anim->name());
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

void LayerAndroid::updateFixedLayersPositions(const SkRect& viewport)
{
    if (m_isFixed) {
        float w = viewport.width();
        float h = viewport.height();
        float dx = viewport.fLeft;
        float dy = viewport.fTop;
        float x = dx;
        float y = dy;

        // Not defined corresponds to 'auto';
        // If both left and right are auto, the w3c says we should set left
        // to zero (in left-to-right layout). So we use left if it's defined
        // or if right isn't defined.
        if (m_fixedLeft.defined() || !m_fixedRight.defined())
            x += m_fixedMarginLeft.calcFloatValue(w) + m_fixedLeft.calcFloatValue(w) - m_fixedRect.fLeft;
        else
            x += w - m_fixedMarginRight.calcFloatValue(w) - m_fixedRight.calcFloatValue(w) - m_fixedRect.fRight;

        // Following the same reason as above, if bottom isn't defined, we apply
        // top regardless of it being defined or not.
        if (m_fixedTop.defined() || !m_fixedBottom.defined())
            y += m_fixedMarginTop.calcFloatValue(h) + m_fixedTop.calcFloatValue(h) - m_fixedRect.fTop;
        else
            y += h - m_fixedMarginBottom.calcFloatValue(h) - m_fixedBottom.calcFloatValue(h) - m_fixedRect.fBottom;

        this->setPosition(x, y);
    }

    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->updateFixedLayersPositions(viewport);
}

void LayerAndroid::updatePositions()
{
    // apply the viewport to us
    if (!m_isFixed) {
        // turn our fields into a matrix.
        //
        // TODO: this should happen in the caller, and we should remove these
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

void LayerAndroid::updateGLPositions(const TransformationMatrix& parentMatrix,
                                     const FloatRect& clipping, float opacity)
{
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
    localMatrix.multLeft(m_transform);
    localMatrix.translate3d(-originX,
                            -originY,
                            -anchorPointZ());

    setDrawTransform(localMatrix);
    opacity *= getOpacity();
    setDrawOpacity(opacity);

    if (m_haveClip) {
        // The clipping rect calculation and intersetion will be done in Screen Coord now.
        FloatRect clip =
            TilesManager::instance()->shader()->clipRectInScreenCoord(drawTransform(), layerSize);
        clip.intersect(clipping);
        setDrawClip(clip);
    } else {
        setDrawClip(clipping);
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
        localMatrix.multLeft(m_childrenTransform);
        localMatrix.translate(-getSize().width() * 0.5f, -getSize().height() * 0.5f);
    }
    for (int i = 0; i < count; i++)
        this->getChild(i)->updateGLPositions(localMatrix, drawClip(), opacity);
}

void LayerAndroid::setContentsImage(SkBitmapRef* img)
{
    SkRefCnt_SafeAssign(m_contentsImage, img);
}

bool LayerAndroid::needsTexture()
{
    return m_contentsImage || (!m_isRootLayer && prepareContext()
        && m_recordingPicture->width() && m_recordingPicture->height());
}

void LayerAndroid::reserveGLTextures()
{
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->reserveGLTextures();

    LayerTexture* reservedTexture = 0;
    if (needsTexture()) {
        // Compute the layer size & position we need (clipped to the viewport)
        IntRect r(0, 0, getWidth(), getHeight());
        IntRect tr = drawTransform().mapRect(r);
        IntRect cr = TilesManager::instance()->shader()->clippedRectWithViewport(tr);
        m_layerTextureRect = drawTransform().inverse().mapRect(cr);

        reservedTexture = TilesManager::instance()->getExistingTextureForLayer(this, m_layerTextureRect);

        // If we do not have a drawing texture (i.e. new LayerAndroid tree),
        // we get any one available.
        if (!m_drawingTexture)
            m_drawingTexture = TilesManager::instance()->getExistingTextureForLayer(this, m_layerTextureRect, true);
    }

    // SMP flush
    android::AutoMutex lock(m_atomicSync);
    // we set the reservedTexture if it's different from the drawing texture
    if (m_reservedTexture != reservedTexture &&
        ((m_reservedTexture != m_drawingTexture) ||
         (m_reservedTexture == 0 && m_drawingTexture == 0))) {
        if (m_reservedTexture)
            m_reservedTexture->release(this);
        m_reservedTexture = reservedTexture;
    }
}

void LayerAndroid::createGLTextures()
{
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->createGLTextures();

    if (!needsTexture())
        return;

    LayerTexture* reservedTexture = m_reservedTexture;
    if (!reservedTexture)
        reservedTexture = TilesManager::instance()->createTextureForLayer(this, m_layerTextureRect);

    if (!reservedTexture)
        return;

    // SMP flush
    m_atomicSync.lock();
    m_reservedTexture = reservedTexture;
    m_atomicSync.unlock();

    if (reservedTexture &&
        (reservedTexture != m_drawingTexture) &&
         reservedTexture->isReady()) {
        if (m_drawingTexture) {
            TilesManager::instance()->removeOperationsForTexture(m_drawingTexture);
            m_drawingTexture->release(this);
        }
        m_drawingTexture = reservedTexture;
    }

    if (!needsScheduleRepaint(reservedTexture))
        return;

    m_atomicSync.lock();
    if (!m_requestSent) {
        m_requestSent = true;
        m_atomicSync.unlock();
        XLOG("We schedule a paint for layer %d (%x), because isReady %d or m_dirty %d, using texture %x (%d, %d)",
             uniqueId(), this, m_reservedTexture->isReady(), m_dirty, m_reservedTexture,
             m_reservedTexture->rect().width(), m_reservedTexture->rect().height());
        PaintLayerOperation* operation = new PaintLayerOperation(this);
        TilesManager::instance()->scheduleOperation(operation);
    } else {
        XLOG("We don't schedule a paint for layer %d (%x), because we already sent a request",
             uniqueId(), this);
        m_atomicSync.unlock();
    }
}

bool LayerAndroid::needsScheduleRepaint(LayerTexture* texture)
{
    if (!texture)
        return false;

    if (!m_pictureUsed == -1 || texture->pictureUsed() != m_pictureUsed) {
        XLOG("We mark layer %d (%x) as dirty because: m_pictureUsed(%d == 0?), texture picture used %x",
             uniqueId(), this, m_pictureUsed, texture->pictureUsed());
        if (m_pictureUsed == -1)
            m_pictureUsed = 0;
        texture->setPictureUsed(m_pictureUsed);
        m_dirty = true;
    }

    if (!texture->isReady())
        m_dirty = true;

    return m_dirty;
}

static inline bool compareLayerZ(const LayerAndroid* a, const LayerAndroid* b)
{
    const TransformationMatrix& transformA = a->drawTransform();
    const TransformationMatrix& transformB = b->drawTransform();

    return transformA.m43() < transformB.m43();
}

bool LayerAndroid::drawGL(SkMatrix& matrix)
{
    SkRect rect;
    rect.set(0, 0, getSize().width(), getSize().height());

    TilesManager::instance()->shader()->clip(m_clippingRect);

    if (prepareContext() && m_drawingTexture) {
        TextureInfo* textureInfo = m_drawingTexture->consumerLock();
        if (textureInfo && m_drawingTexture->isReady()) {
            SkRect bounds;
            IntRect textureRect = m_drawingTexture->rect();
            bounds.set(0, 0, textureRect.width(), textureRect.height());
            // move the drawing depending on where the texture is on the layer
            TransformationMatrix m = drawTransform();
            m.translate(textureRect.x(), textureRect.y());
            XLOG("LayerAndroid %d %x (%.2f, %.2f) drawGL (texture %x, %d, %d, %d, %d)", uniqueId(), this, getWidth(), getHeight(),
                 m_drawingTexture, textureRect.x(), textureRect.y(), textureRect.width(), textureRect.height());
            TilesManager::instance()->shader()->drawLayerQuad(m, bounds,
                                                              textureInfo->m_textureId,
                                                              m_drawOpacity);
        }
        m_drawingTexture->consumerRelease();
    }

    // When the layer is dirty, the UI thread should be notified to redraw.
    bool askPaint = drawChildrenGL(matrix);
    m_atomicSync.lock();
    askPaint |= m_dirty;
    m_atomicSync.unlock();
    return askPaint;
}


bool LayerAndroid::drawChildrenGL(SkMatrix& matrix)
{
    bool askPaint = false;
    int count = this->countChildren();
    if (count > 0) {
        Vector <LayerAndroid*> sublayers;
        for (int i = 0; i < count; i++)
            sublayers.append(this->getChild(i));

        // now we sort for the transparency
        std::stable_sort(sublayers.begin(), sublayers.end(), compareLayerZ);
        for (int i = 0; i < count; i++) {
            LayerAndroid* layer = sublayers[i];
            askPaint |= layer->drawGL(matrix);
        }
    }

    return askPaint;
}

void LayerAndroid::setScale(float scale)
{
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->setScale(scale);

    android::AutoMutex lock(m_atomicSync);
    m_scale = scale;
}

// This is called from the texture generation thread
void LayerAndroid::paintBitmapGL()
{
    // We acquire the values below atomically. This ensures that we are reading
    // values correctly across cores. Further, once we have these values they
    // can be updated by other threads without consequence.
    m_atomicSync.lock();
    LayerTexture* texture = m_reservedTexture;

    if (!texture) {
        m_atomicSync.unlock();
        XLOG("Layer %d doesn't have a texture!", uniqueId());
        return;
    }

    XLOG("LayerAndroid paintBitmapGL (layer %d), texture used %x (%d, %d)", uniqueId(), texture,
         texture->rect().width(), texture->rect().height());

    // We need to mark the texture as busy before relinquishing the lock
    // -- so that TilesManager::cleanupLayersTextures() can check if the texture
    // is used before trying to destroy it
    // If LayerAndroid::removeTexture() is called before us, we'd have bailed
    // out early as texture would have been null; if it is called after us, we'd
    // have marked the texture has being busy, and the texture will not be
    // destroy immediately.
    texture->producerAcquireContext();
    TextureInfo* textureInfo = texture->producerLock();
    m_atomicSync.unlock();

    // at this point we can safely check the ownership (if the texture got
    // transferred to another BaseTile under us)
    if (texture->owner() != this) {
        texture->producerRelease();
        return;
    }

    XLOG("LayerAndroid %d %x (%.2f, %.2f) paintBitmapGL WE ARE PAINTING", uniqueId(), this, getWidth(), getHeight());
    SkCanvas* canvas = texture->canvas();
    float scale = texture->scale();

    IntRect textureRect = texture->rect();

    canvas->save();
    canvas->drawARGB(0, 0, 0, 0, SkXfermode::kClear_Mode);
    canvas->scale(scale, scale);
    canvas->translate(-textureRect.x(), -textureRect.y());
    contentDraw(canvas);
    canvas->restore();

    XLOG("LayerAndroid %d paintBitmapGL PAINTING DONE, updating the texture", uniqueId());
    texture->producerUpdate(textureInfo);

    while (!texture->isReady()) {
        TextureInfo* textureInfo = texture->producerLock();
        texture->producerUpdate(textureInfo);
    }

    m_atomicSync.lock();
    m_dirty = false;
    m_requestSent = false;
    m_atomicSync.unlock();
    XLOG("LayerAndroid %d paintBitmapGL UPDATING DONE", uniqueId());
}

void LayerAndroid::contentDraw(SkCanvas* canvas)
{
    if (m_contentsImage) {
      SkRect dest;
      dest.set(0, 0, getSize().width(), getSize().height());
      canvas->drawBitmapRect(m_contentsImage->bitmap(), 0, dest);
    } else {
      canvas->drawPicture(*m_recordingPicture);
    }
    if (m_extra) {
        IntRect dummy; // inval area, unused for now
        m_extra->draw(canvas, this, &dummy);
    }

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
      SkPaint paint;
      paint.setARGB(80, 255, 0, 0);
      canvas->drawRect(m_fixedRect, paint);
    }
#endif
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
    writeIntVal(file, indentLevel + 1, "isRootLayer", m_isRootLayer);
    writeIntVal(file, indentLevel + 1, "isFixed", m_isFixed);

    writeFloatVal(file, indentLevel + 1, "opacity", getOpacity());
    writeSize(file, indentLevel + 1, "size", getSize());
    writePoint(file, indentLevel + 1, "position", getPosition());
    writePoint(file, indentLevel + 1, "anchor", getAnchorPoint());

    writeMatrix(file, indentLevel + 1, "drawMatrix", drawTransform());
    writeMatrix(file, indentLevel + 1, "transformMatrix", m_transform);

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

void LayerAndroid::setExtra(DrawExtra* extra)
{
    m_extra = extra;
    for (int i = 0; i < countChildren(); i++)
        getChild(i)->setExtra(extra);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
