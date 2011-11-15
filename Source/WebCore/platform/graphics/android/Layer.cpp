#include "config.h"
#include "Layer.h"
#include "SkCanvas.h"

//#define DEBUG_DRAW_LAYER_BOUNDS
//#define DEBUG_TRACK_NEW_DELETE

#ifdef DEBUG_TRACK_NEW_DELETE
    static int gLayerAllocCount;
#endif

///////////////////////////////////////////////////////////////////////////////

Layer::Layer() {
    fParent = NULL;
    m_opacity = SK_Scalar1;
    m_size.set(0, 0);
    m_position.set(0, 0);
    m_anchorPoint.set(SK_ScalarHalf, SK_ScalarHalf);

    m_matrix.reset();
    m_childrenMatrix.reset();
    m_shouldInheritFromRootTransform = false;

    m_hasOverflowChildren = false;
    m_state = 0;

#ifdef DEBUG_TRACK_NEW_DELETE
    gLayerAllocCount += 1;
    SkDebugf("Layer new:    %d\n", gLayerAllocCount);
#endif
}

Layer::Layer(const Layer& src) : INHERITED() {
    fParent = NULL;
    m_opacity = src.m_opacity;
    m_size = src.m_size;
    m_position = src.m_position;
    m_anchorPoint = src.m_anchorPoint;

    m_matrix = src.m_matrix;
    m_childrenMatrix = src.m_childrenMatrix;
    m_shouldInheritFromRootTransform = src.m_shouldInheritFromRootTransform;

    m_hasOverflowChildren = src.m_hasOverflowChildren;
    m_state = 0;

#ifdef DEBUG_TRACK_NEW_DELETE
    gLayerAllocCount += 1;
    SkDebugf("Layer copy:   %d\n", gLayerAllocCount);
#endif
}

Layer::~Layer() {
    removeChildren();

#ifdef DEBUG_TRACK_NEW_DELETE
    gLayerAllocCount -= 1;
    SkDebugf("Layer delete: %d\n", gLayerAllocCount);
#endif
}

///////////////////////////////////////////////////////////////////////////////

int Layer::countChildren() const {
    return m_children.count();
}

Layer* Layer::getChild(int index) const {
    if ((unsigned)index < (unsigned)m_children.count()) {
        SkASSERT(m_children[index]->fParent == this);
        return m_children[index];
    }
    return NULL;
}

Layer* Layer::addChild(Layer* child) {
    SkASSERT(this != child);
    child->ref();
    child->detachFromParent();
    SkASSERT(child->fParent == NULL);
    child->fParent = this;

    *m_children.append() = child;
    return child;
}

void Layer::detachFromParent() {
    if (fParent) {
        int index = fParent->m_children.find(this);
        SkASSERT(index >= 0);
        fParent->m_children.remove(index);
        fParent = NULL;
        unref();  // this call might delete us
    }
}

void Layer::removeChildren() {
    int count = m_children.count();
    for (int i = 0; i < count; i++) {
        Layer* child = m_children[i];
        SkASSERT(child->fParent == this);
        child->fParent = NULL;  // in case it has more than one owner
        child->unref();
    }
    m_children.reset();
}

Layer* Layer::getRootLayer() const {
    const Layer* root = this;
    while (root->fParent != NULL) {
        root = root->fParent;
    }
    return const_cast<Layer*>(root);
}

///////////////////////////////////////////////////////////////////////////////

void Layer::getLocalTransform(SkMatrix* matrix) const {
    matrix->setTranslate(m_position.fX, m_position.fY);

    SkScalar tx = SkScalarMul(m_anchorPoint.fX, m_size.width());
    SkScalar ty = SkScalarMul(m_anchorPoint.fY, m_size.height());
    matrix->preTranslate(tx, ty);
    matrix->preConcat(getMatrix());
    matrix->preTranslate(-tx, -ty);
}

void Layer::localToAncestor(const Layer* ancestor, SkMatrix* matrix) const {
    if (this == ancestor) {
        matrix->setIdentity();
        return;
    }

    getLocalTransform(matrix);

    // Fixed position layers simply use the root layer's transform.
    if (shouldInheritFromRootTransform()) {
        ASSERT(!ancestor);
        matrix->postConcat(getRootLayer()->getMatrix());
        return;
    }

    // Apply the local and child transforms for every layer between this layer
    // and ancestor.
    ASSERT(isAncestor(ancestor));
    for (const Layer* layer = this->fParent; layer != ancestor; layer = layer->fParent) {
        SkMatrix tmp;
        layer->getLocalTransform(&tmp);
        tmp.preConcat(layer->getChildrenMatrix());
        matrix->postConcat(tmp);
    }

    // If ancestor is not the root layer, apply its child transformation too.
    if (ancestor)
        matrix->postConcat(ancestor->getChildrenMatrix());
}

///////////////////////////////////////////////////////////////////////////////

void Layer::onDraw(SkCanvas*, SkScalar opacity) {
//    SkDebugf("----- no onDraw for %p\n", this);
}

#include "SkString.h"

void Layer::draw(SkCanvas* canvas, SkScalar opacity) {
#if 0
    SkString str1, str2;
 //   getMatrix().toDumpString(&str1);
 //   getChildrenMatrix().toDumpString(&str2);
    SkDebugf("--- drawlayer %p opacity %g size [%g %g] pos [%g %g] matrix %s children %s\n",
             this, opacity * getOpacity(), m_size.width(), m_size.height(),
             m_position.fX, m_position.fY, str1.c_str(), str2.c_str());
#endif

    opacity = SkScalarMul(opacity, getOpacity());
    if (opacity <= 0) {
//        SkDebugf("---- abort drawing %p opacity %g\n", this, opacity);
        return;
    }

    SkAutoCanvasRestore acr(canvas, true);

    // apply our local transform
    {
        SkMatrix tmp;
        getLocalTransform(&tmp);
        if (shouldInheritFromRootTransform()) {
            // should we also apply the root's childrenMatrix?
            canvas->setMatrix(getRootLayer()->getMatrix());
        }
        canvas->concat(tmp);
    }

    onDraw(canvas, opacity);

#ifdef DEBUG_DRAW_LAYER_BOUNDS
    {
        SkRect r = SkRect::MakeSize(getSize());
        SkPaint p;
        p.setAntiAlias(true);
        p.setStyle(SkPaint::kStroke_Style);
        p.setStrokeWidth(SkIntToScalar(2));
        p.setColor(0xFFFF44DD);
        canvas->drawRect(r, p);
        canvas->drawLine(r.fLeft, r.fTop, r.fRight, r.fBottom, p);
        canvas->drawLine(r.fLeft, r.fBottom, r.fRight, r.fTop, p);
    }
#endif

    int count = countChildren();
    if (count > 0) {
        canvas->concat(getChildrenMatrix());
        for (int i = 0; i < count; i++) {
            getChild(i)->draw(canvas, opacity);
        }
    }
}

bool Layer::isAncestor(const Layer* possibleAncestor) const {
    if (!possibleAncestor)
        return true;
    for (const Layer* layer = getParent(); layer; layer = layer->getParent()) {
        if (layer == possibleAncestor)
            return true;
    }
    return false;
}

void Layer::setState(WebCore::GLWebViewState* state) {
    m_state = state;
    int count = countChildren();
    for (int i = 0; i < count; i++)
        m_children[i]->setState(state);
}
