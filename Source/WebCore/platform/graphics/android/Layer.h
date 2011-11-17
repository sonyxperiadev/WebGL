/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef Layer_DEFINED
#define Layer_DEFINED

#include "TestExport.h"
#include "SkRefCnt.h"
#include "SkTDArray.h"
#include "SkColor.h"
#include "SkMatrix.h"
#include "SkPoint.h"
#include "SkRect.h"
#include "SkRegion.h"
#include "SkSize.h"

namespace WebCore {
class IntRect;
class GLWebViewState;
};

class SkCanvas;

class TEST_EXPORT Layer : public SkRefCnt {

public:
    Layer();
    Layer(const Layer&);
    virtual ~Layer();

    // Whether the layer should apply its tranform directly onto the root
    // layer, rather than using the transforms of all ancestor layers. This is
    // used for fixed position layers.
    bool shouldInheritFromRootTransform() const { return m_shouldInheritFromRootTransform; }
    SkScalar getOpacity() const { return m_opacity; }
    const SkSize& getSize() const { return m_size; }
    const SkPoint& getPosition() const { return m_position; }
    const SkPoint& getAnchorPoint() const { return m_anchorPoint; }
    const SkMatrix& getMatrix() const { return m_matrix; }
    const SkMatrix& getChildrenMatrix() const { return m_childrenMatrix; }

    SkScalar getWidth() const { return m_size.width(); }
    SkScalar getHeight() const { return m_size.height(); }

    void setShouldInheritFromRootTransform(bool inherit) { m_shouldInheritFromRootTransform = inherit; }
    void setOpacity(SkScalar opacity) { m_opacity = opacity; }
    void setSize(SkScalar w, SkScalar h) { m_size.set(w, h); }
    void setPosition(SkScalar x, SkScalar y) { m_position.set(x, y); }
    void setAnchorPoint(SkScalar x, SkScalar y) { m_anchorPoint.set(x, y); }
    void setMatrix(const SkMatrix& matrix) { m_matrix = matrix; }
    void setChildrenMatrix(const SkMatrix& matrix) { m_childrenMatrix = matrix; }

// rendering asset management

    // tell rendering assets to update their tile content with most recent painted data
    virtual void swapTiles() {}

    // tell rendering assets to use this layer tree for drawing
    virtual void setIsDrawing(bool isDrawing) {}

    // take rendering assets from drawing tree, or create if they don't exist
    virtual void setIsPainting(Layer* drawingTree) {}

    // if a similar layer exists in the replacement tree, add invals to it
    virtual void mergeInvalsInto(Layer* replacementTree) {}

    void markAsDirty(const SkRegion& invalRegion) {
        m_dirtyRegion.op(invalRegion, SkRegion::kUnion_Op);
    }

    bool isDirty() {
        return !m_dirtyRegion.isEmpty();
    }

// drawing

    virtual bool isReady() { return false; }

    // TODO: clean out several of these, leave them in GLWebViewState

    virtual bool prepare(double currentTime, WebCore::IntRect& viewRect,
                         SkRect& visibleRect, float scale) { return 0; }
    virtual bool drawGL(WebCore::IntRect& viewRect,
                        SkRect& visibleRect, float scale) { return 0; }
    WebCore::GLWebViewState* state() { return m_state; }
    void setState(WebCore::GLWebViewState* state);

// children

    /** Return the number of layers in our child list.
     */
    int countChildren() const;

    /** Return the child at the specified index (starting at 0). This does not
        affect the reference count of the child.
     */
    Layer* getChild(int index) const;

    /** Add this layer to our child list at the end (top-most), and ref() it.
        If it was already in another hierarchy, remove it from that list.
        Return the new child.
     */
    Layer* addChild(Layer* child);

    /** Remove this layer from its parent's list (or do nothing if it has no
        parent.) If it had a parent, then unref() is called.
     */
    void detachFromParent();

    /** Remove, and unref(), all of the layers in our child list.
     */
    void removeChildren();

    /** Return our parent layer, or NULL if we have none.
     */
    Layer* getParent() const { return fParent; }

    /** Return the root layer in this hiearchy. If this layer is the root
        (i.e. has no parent), then this returns itself.
     */
    Layer* getRootLayer() const;

    // coordinate system transformations

    /** Return, in matrix, the matix transfomations that are applied locally
        when this layer draws (i.e. its position and matrix/anchorPoint).
        This does not include the childrenMatrix, since that is only applied
        after this layer draws (but before its children draw).
     */
    void getLocalTransform(SkMatrix* matrix) const;

    /** Return, in matrix, the concatenation of transforms that are applied
        from this layer's root parent to the layer itself.
        This is the matrix that is applied to the layer during drawing.
     */
    void localToGlobal(SkMatrix* matrix) const { localToAncestor(0, matrix); }

    /** Return, as a matrix, the transform that converts from this layer's local
        space to the space of the given ancestor layer. Use NULL for ancestor to
        represent the root layer. Note that this method must not be called on a
        fixed position layer with ancestor != NULL.

        For non-fixed position layers, the following holds (in pseudo-code for
        brevity) ...
        SkMatrix localToAncestor = layer->localToAncestor(ancestor);
        SkMatrix ancestorToGlobal = ancestor->localToAncestor(NULL);
        SkMatrix localToGlobal = layer->localToGlobal();
        ASSERT(localToAncestor * ancestorToGlobal == localToGlobal);
     */
    void localToAncestor(const Layer* ancestor, SkMatrix* matrix) const;

    // paint method

    virtual bool drawCanvas(SkCanvas*) { return false; }
    void draw(SkCanvas*, SkScalar opacity);
    void draw(SkCanvas* canvas) {
        this->draw(canvas, SK_Scalar1);
    }

    void setHasOverflowChildren(bool value) { m_hasOverflowChildren = value; }

    virtual bool contentIsScrollable() const { return false; }

protected:
    virtual void onDraw(SkCanvas*, SkScalar opacity);

    bool m_hasOverflowChildren;

    bool isAncestor(const Layer*) const;

    Layer* fParent;
    SkScalar m_opacity;
    SkSize m_size;
    // The position of the origin of the layer, relative to the parent layer.
    SkPoint m_position;
    // The point in the layer used as the origin for local transformations,
    // expressed as a fraction of the layer size.
    SkPoint m_anchorPoint;
    SkMatrix m_matrix;
    SkMatrix m_childrenMatrix;
    bool m_shouldInheritFromRootTransform;

    SkTDArray<Layer*> m_children;

    // invalidation region
    SkRegion m_dirtyRegion;

    WebCore::GLWebViewState* m_state;

    typedef SkRefCnt INHERITED;
};

#endif
