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

    fMatrix.reset();
    fChildrenMatrix.reset();
    fFlags = 0;

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

    fMatrix = src.fMatrix;
    fChildrenMatrix = src.fChildrenMatrix;
    fFlags = src.fFlags;

#ifdef DEBUG_TRACK_NEW_DELETE
    gLayerAllocCount += 1;
    SkDebugf("Layer copy:   %d\n", gLayerAllocCount);
#endif
}

Layer::~Layer() {
    this->removeChildren();

#ifdef DEBUG_TRACK_NEW_DELETE
    gLayerAllocCount -= 1;
    SkDebugf("Layer delete: %d\n", gLayerAllocCount);
#endif
}

///////////////////////////////////////////////////////////////////////////////

bool Layer::isInheritFromRootTransform() const {
    return (fFlags & kInheritFromRootTransform_Flag) != 0;
}

void Layer::setInheritFromRootTransform(bool doInherit) {
    if (doInherit) {
        fFlags |= kInheritFromRootTransform_Flag;
    } else {
        fFlags &= ~kInheritFromRootTransform_Flag;
    }
}

void Layer::setMatrix(const SkMatrix& matrix) {
    fMatrix = matrix;
}

void Layer::setChildrenMatrix(const SkMatrix& matrix) {
    fChildrenMatrix = matrix;
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
        this->unref();  // this call might delete us
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
    matrix->preConcat(this->getMatrix());
    matrix->preTranslate(-tx, -ty);
}

void Layer::localToGlobal(SkMatrix* matrix) const {
    this->getLocalTransform(matrix);

    if (this->isInheritFromRootTransform()) {
        matrix->postConcat(this->getRootLayer()->getMatrix());
        return;
    }

    const Layer* layer = this;
    while (layer->fParent != NULL) {
        layer = layer->fParent;

        SkMatrix tmp;
        layer->getLocalTransform(&tmp);
        tmp.preConcat(layer->getChildrenMatrix());
        matrix->postConcat(tmp);
    }
}

///////////////////////////////////////////////////////////////////////////////

void Layer::onDraw(SkCanvas*, SkScalar opacity) {
//    SkDebugf("----- no onDraw for %p\n", this);
}

#include "SkString.h"

void Layer::draw(SkCanvas* canvas, SkScalar opacity) {
#if 0
    SkString str1, str2;
 //   this->getMatrix().toDumpString(&str1);
 //   this->getChildrenMatrix().toDumpString(&str2);
    SkDebugf("--- drawlayer %p opacity %g size [%g %g] pos [%g %g] matrix %s children %s\n",
             this, opacity * this->getOpacity(), m_size.width(), m_size.height(),
             m_position.fX, m_position.fY, str1.c_str(), str2.c_str());
#endif

    opacity = SkScalarMul(opacity, this->getOpacity());
    if (opacity <= 0) {
//        SkDebugf("---- abort drawing %p opacity %g\n", this, opacity);
        return;
    }

    SkAutoCanvasRestore acr(canvas, true);

    // apply our local transform
    {
        SkMatrix tmp;
        this->getLocalTransform(&tmp);
        if (this->isInheritFromRootTransform()) {
            // should we also apply the root's childrenMatrix?
            canvas->setMatrix(getRootLayer()->getMatrix());
        }
        canvas->concat(tmp);
    }

    this->onDraw(canvas, opacity);

#ifdef DEBUG_DRAW_LAYER_BOUNDS
    {
        SkRect r = SkRect::MakeSize(this->getSize());
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

    int count = this->countChildren();
    if (count > 0) {
        canvas->concat(this->getChildrenMatrix());
        for (int i = 0; i < count; i++) {
            this->getChild(i)->draw(canvas, opacity);
        }
    }
}
