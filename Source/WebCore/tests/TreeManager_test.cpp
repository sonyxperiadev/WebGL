/*
 * Copyright 2011, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <gtest/gtest.h>

#include "SkRefCnt.h"
#include "TransformationMatrix.h"
#include "IntRect.h"
#include "Layer.h"
#include "LayerAndroid.h"
#include "TreeManager.h"
#include "SkPicture.h"

#include <cutils/log.h>
#include <wtf/text/CString.h>
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "TreeManager_test", __VA_ARGS__)

namespace WebCore {

// Used for testing simple cases for tree painting, drawing, swapping
class TestLayer : public Layer {
public:
    TestLayer()
        : m_isDrawing(false)
        , m_isPainting(false)
        , m_isDonePainting(false)
        , m_drawCount(0)
    {}

    bool m_isDrawing;
    bool m_isPainting;
    bool m_isDonePainting;
    double m_drawCount;

    bool drawGL(WebCore::IntRect& viewRect, SkRect& visibleRect, float scale) {
        m_drawCount++;
        return false;
    }

    bool isReady() {
        return m_isDonePainting;
    }

    void setIsDrawing(bool isDrawing) {
        m_isDrawing = isDrawing;
        if (isDrawing)
            m_isPainting = false;
    }

    void setIsPainting(Layer* drawingTree) {
        m_isPainting = true;
        m_isDonePainting = false;
        setIsDrawing(false);
    }
};

// Used for testing complex trees, and painted surfaces
class TestLayerAndroid : public LayerAndroid {
public:
    TestLayerAndroid(SkPicture* picture) : LayerAndroid(picture)
        , m_isDonePainting(false)
        , m_drawCount(0)
    {}

    TestLayerAndroid(const TestLayerAndroid& testLayer) : LayerAndroid(testLayer)
        , m_isDonePainting(testLayer.m_isDonePainting)
        , m_drawCount(testLayer.m_drawCount)
    {
        XLOGC("copying TLA %p as %p", &testLayer, this);
    }

    bool m_isDonePainting;
    double m_drawCount;

    bool drawGL(WebCore::IntRect& viewRect, SkRect& visibleRect, float scale) {
        m_drawCount++;
        return false;
    }

    bool isReady() {
        return m_isDonePainting;
    }

    LayerAndroid* copy() const { return new TestLayerAndroid(*this); }
};

class TreeManagerTest : public testing::Test {
protected:
    IntRect m_iRect;
    SkRect m_sRect;
    double m_scale;

    void allocLayerWithPicture(bool useTestLayer, LayerAndroid** layerHandle, SkPicture** pictureHandle) {
        SkPicture* p = new SkPicture();
        p->beginRecording(16,16, 0);
        p->endRecording();

        LayerAndroid* l;
        if (useTestLayer)
            l = new TestLayerAndroid(p);
        else
            l = new LayerAndroid(p);
        l->setSize(16, 16);
        SkSafeUnref(p); // layer takes sole ownership of picture

        if (layerHandle)
            *layerHandle = l;
        if (pictureHandle)
            *pictureHandle = p;
    }

    bool drawGL(TreeManager& manager, bool* swappedPtr) {
        // call draw gl here in one place, so that when its parameters change,
        // the tests only have to be updated in one place
        return manager.drawGL(0, m_iRect, m_sRect, m_scale, false, swappedPtr, 0);
    }

    virtual void SetUp() {
        m_iRect = IntRect(0, 0, 1, 1);
        m_sRect = SkRect::MakeWH(1, 1);
        m_scale = 1.0;
    }
    virtual void TearDown() { }
};

TEST_F(TreeManagerTest, EmptyTree_DoesntRedraw) {
    TreeManager manager;

    drawGL(manager, false);
}

TEST_F(TreeManagerTest, OneLayerTree_SingleTree_SwapCheck) {
    TreeManager manager;
    TestLayer layer;

    // initialize with tree, should be painting
    manager.updateWithTree(&layer, true);

    ASSERT_TRUE(layer.m_isPainting);
    ASSERT_FALSE(layer.m_isDrawing);

    // should not call swap, and return true since content isn't done
    for (int i = 1; i < 6; i++) {
        bool swapped = false;
        ASSERT_TRUE(drawGL(manager, &swapped));
        ASSERT_FALSE(swapped);
        ASSERT_EQ(layer.m_drawCount, 0);
    }

    layer.m_isDonePainting = true;

    // swap content, should return false since no new picture
    bool swapped = false;
    ASSERT_FALSE(drawGL(manager, &swapped));
    ASSERT_TRUE(swapped);
    ASSERT_EQ(layer.m_drawCount, 1); // verify layer drawn
}

TEST_F(TreeManagerTest, OneLayerTree_SingleTree_RefCountCorrectly) {
    TreeManager manager;
    TestLayer* layer = new TestLayer();
    ASSERT_EQ(layer->getRefCnt(), 1);

    // initialize with tree, should be painting
    manager.updateWithTree(layer, true);
    ASSERT_EQ(layer->getRefCnt(), 2);

    layer->m_isDonePainting = true;
    ASSERT_FALSE(drawGL(manager, 0));

    // should be drawing
    ASSERT_EQ(layer->getRefCnt(), 2);

    manager.updateWithTree(0, false);

    // layer should be removed
    ASSERT_EQ(layer->getRefCnt(), 1);
    SkSafeUnref(layer);
}

TEST_F(TreeManagerTest, OneLayerTree_TwoTreeFlush_PaintDrawRefCheck) {
    TreeManager manager;
    TestLayer* firstLayer = new TestLayer();
    TestLayer* secondLayer = new TestLayer();
    ASSERT_EQ(firstLayer->getRefCnt(), 1);
    ASSERT_EQ(secondLayer->getRefCnt(), 1);

    ///// ENQUEUE 2 TREES

    // first starts painting
    manager.updateWithTree(firstLayer, true);
    ASSERT_TRUE(firstLayer->m_isPainting);
    ASSERT_FALSE(firstLayer->m_isDrawing);

    // second is queued
    manager.updateWithTree(secondLayer, false);
    ASSERT_FALSE(secondLayer->m_isPainting);
    ASSERT_FALSE(secondLayer->m_isDrawing);

    // nothing changes
    ASSERT_TRUE(drawGL(manager, 0));

    ////////// FIRST FINISHES PAINTING, SWAP THE TREES

    firstLayer->m_isDonePainting = true;
    bool swapped = false;
    ASSERT_TRUE(drawGL(manager, &swapped));
    ASSERT_TRUE(swapped);

    // first is drawing
    ASSERT_EQ(firstLayer->m_drawCount, 1);
    ASSERT_FALSE(firstLayer->m_isPainting);
    ASSERT_TRUE(firstLayer->m_isDrawing);
    ASSERT_EQ(firstLayer->getRefCnt(), 2);

    // second is painting (and hasn't drawn)
    ASSERT_EQ(secondLayer->m_drawCount, 0);
    ASSERT_TRUE(secondLayer->m_isPainting);
    ASSERT_FALSE(secondLayer->m_isDrawing);
    ASSERT_EQ(secondLayer->getRefCnt(), 2);

    ////////// SECOND FINISHES PAINTING, SWAP AGAIN

    secondLayer->m_isDonePainting = true;

    // draw again, swap, first should be deleted
    swapped = false;
    ASSERT_FALSE(drawGL(manager, &swapped)); // no painting layer
    ASSERT_TRUE(swapped);

    // first layer gone!
    ASSERT_EQ(firstLayer->getRefCnt(), 1);
    SkSafeUnref(firstLayer);

    // second is drawing
    ASSERT_EQ(secondLayer->m_drawCount, 1);
    ASSERT_FALSE(secondLayer->m_isPainting);
    ASSERT_TRUE(secondLayer->m_isDrawing);
    ASSERT_EQ(secondLayer->getRefCnt(), 2);

     ////////// INSERT NULL, BOTH TREES NOW REMOVED

    // insert null tree, which should deref secondLayer immediately
    manager.updateWithTree(0, false);
    ASSERT_EQ(secondLayer->getRefCnt(), 1);
    SkSafeUnref(secondLayer);

    // nothing to draw or swap
    swapped = false;
    ASSERT_FALSE(drawGL(manager, &swapped));
    ASSERT_FALSE(swapped);
}

TEST_F(TreeManagerTest, LayerAndroidTree_PictureRefCount) {
    RenderLayer* renderLayer = 0;
    LayerAndroid* l;
    SkPicture* p;
    allocLayerWithPicture(false, &l, &p);
    ASSERT_TRUE(l->needsTexture());
    SkSafeRef(p); // ref picture locally so it exists after layer (so we can see
                  // layer derefs it)

    ASSERT_EQ(l->getRefCnt(), 1);
    ASSERT_EQ(p->getRefCnt(), 2);
    SkSafeUnref(l);

    ASSERT_EQ(p->getRefCnt(), 1);
    SkSafeUnref(p);
}

TEST_F(TreeManagerTest, LayerAndroidTree_PaintTreeWithPictures) {
    XLOGC("STARTING PAINT TEST");

    TreeManager manager;
    RenderLayer* renderLayer = 0;
    LayerAndroid root(renderLayer);
    LayerAndroid* noPaintChild = new LayerAndroid(renderLayer);
    root.addChild(noPaintChild);

    root.showLayer(0);

    ASSERT_EQ(noPaintChild->getRefCnt(), 2);


    LayerAndroid* copy = new LayerAndroid(root);
    copy->showLayer(0);
    manager.updateWithTree(copy, true);


    // no painting layer, should swap immediately
    bool swapped = false;
    ASSERT_FALSE(drawGL(manager, &swapped));
    ASSERT_TRUE(swapped);


     ////////// add 2 painting layers, push new tree copy into tree manager

    LayerAndroid* paintChildA;
    allocLayerWithPicture(true, &paintChildA, 0);
    noPaintChild->addChild(paintChildA);
    ASSERT_TRUE(paintChildA->needsTexture());

    LayerAndroid* paintChildB;
    allocLayerWithPicture(true, &paintChildB, 0);
    noPaintChild->addChild(paintChildB);
    ASSERT_TRUE(paintChildB->needsTexture());

    LayerAndroid* copy1 = new LayerAndroid(root);
    copy1->showLayer(0);
    manager.updateWithTree(copy1, false);

    swapped = false;
    ASSERT_TRUE(drawGL(manager, &swapped));
    ASSERT_FALSE(swapped); // painting layers not ready

    ASSERT_EQ(TreeManager::getTotalPaintedSurfaceCount(), 2);

    ////////// remove painting layer, add new painting layer, push new tree copy into tree manager

    LayerAndroid* paintChildC;
    allocLayerWithPicture(true, &paintChildC, 0);
    noPaintChild->addChild(paintChildC);
    ASSERT_TRUE(paintChildC->needsTexture());

    paintChildB->detachFromParent();
    ASSERT_EQ(paintChildB->getRefCnt(), 1);
    SkSafeUnref(paintChildB);

    LayerAndroid* copy2 = new LayerAndroid(root);
    copy2->showLayer(0);
    manager.updateWithTree(copy2, false);

    swapped = false;
    ASSERT_TRUE(drawGL(manager, &swapped));
    ASSERT_FALSE(swapped); // painting layers not ready


    ////////// swap layers

    static_cast<TestLayerAndroid*>(copy1->getChild(0)->getChild(0))->m_isDonePainting = true;
    static_cast<TestLayerAndroid*>(copy1->getChild(0)->getChild(1))->m_isDonePainting = true;

    XLOGC("painting should be %p, queued %p", copy1, copy2);
    swapped = false;
    ASSERT_TRUE(drawGL(manager, &swapped));
    ASSERT_TRUE(swapped); // paint complete, new layer tree to paint
    XLOGC("drawing should be %p, painting %p", copy1, copy2);


    ASSERT_EQ(TreeManager::getTotalPaintedSurfaceCount(), 3);


    ////////// swap layers again

    static_cast<TestLayerAndroid*>(copy2->getChild(0)->getChild(0))->m_isDonePainting = true;
    static_cast<TestLayerAndroid*>(copy2->getChild(0)->getChild(1))->m_isDonePainting = true;

    swapped = false;
    ASSERT_FALSE(drawGL(manager, &swapped));
    ASSERT_TRUE(swapped); // paint complete, no new layer tree to paint

    ASSERT_EQ(TreeManager::getTotalPaintedSurfaceCount(), 2);

    ////////// remove all painting layers

    paintChildA->detachFromParent();
    SkSafeUnref(paintChildA);
    paintChildC->detachFromParent();
    SkSafeUnref(paintChildC);


    copy = new LayerAndroid(root);
    copy->showLayer(0);
    manager.updateWithTree(copy, false);

    swapped = false;
    ASSERT_FALSE(drawGL(manager, &swapped));
    ASSERT_TRUE(swapped); // paint complete, no new layer tree to paint

    ASSERT_EQ(TreeManager::getTotalPaintedSurfaceCount(), 0);
}

} // namespace WebCore
