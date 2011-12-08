/*
 * Copyright 2010, The Android Open Source Project
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
#include "GLWebViewState.h"

#if USE(ACCELERATED_COMPOSITING)

#include "BaseLayerAndroid.h"
#include "ClassTracker.h"
#include "GLUtils.h"
#include "ImagesManager.h"
#include "LayerAndroid.h"
#include "ScrollableLayerAndroid.h"
#include "SkPath.h"
#include "TilesManager.h"
#include "TilesTracker.h"
#include "TreeManager.h"
#include <wtf/CurrentTime.h>

#include <pthread.h>

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "GLWebViewState", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "GLWebViewState", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

#define FIRST_TILED_PAGE_ID 1
#define SECOND_TILED_PAGE_ID 2

#define FRAMERATE_CAP 0.01666 // We cap at 60 fps

// log warnings if scale goes outside this range
#define MIN_SCALE_WARNING 0.1
#define MAX_SCALE_WARNING 10

namespace WebCore {

using namespace android;

GLWebViewState::GLWebViewState()
    : m_zoomManager(this)
    , m_currentPictureCounter(0)
    , m_usePageA(true)
    , m_frameworkInval(0, 0, 0, 0)
    , m_frameworkLayersInval(0, 0, 0, 0)
    , m_isScrolling(false)
    , m_goingDown(true)
    , m_goingLeft(false)
    , m_expandedTileBoundsX(0)
    , m_expandedTileBoundsY(0)
    , m_highEndGfx(false)
    , m_scale(1)
    , m_layersRenderingMode(kAllTextures)
{
    m_viewport.setEmpty();
    m_futureViewportTileBounds.setEmpty();
    m_viewportTileBounds.setEmpty();
    m_preZoomBounds.setEmpty();

    m_tiledPageA = new TiledPage(FIRST_TILED_PAGE_ID, this);
    m_tiledPageB = new TiledPage(SECOND_TILED_PAGE_ID, this);

#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("GLWebViewState");
#endif
#ifdef MEASURES_PERF
    m_timeCounter = 0;
    m_totalTimeCounter = 0;
    m_measurePerfs = false;
#endif
}

GLWebViewState::~GLWebViewState()
{
    // Take care of the transfer queue such that Tex Gen thread will not stuck
    TilesManager::instance()->unregisterGLWebViewState(this);

    // We have to destroy the two tiled pages first as their destructor
    // may depend on the existence of this GLWebViewState and some of its
    // instance variables in order to complete.
    // Explicitely, currently we need to have the m_paintingBaseLayer around
    // in order to complete any pending paint operations (the tiled pages
    // will remove any pending operations, and wait if one is underway).
    delete m_tiledPageA;
    delete m_tiledPageB;
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("GLWebViewState");
#endif

}

void GLWebViewState::setBaseLayer(BaseLayerAndroid* layer, const SkRegion& inval,
                                  bool showVisualIndicator, bool isPictureAfterFirstLayout)
{
    if (!layer || isPictureAfterFirstLayout) {
        // TODO: move this into TreeManager
        m_tiledPageA->discardTextures();
        m_tiledPageB->discardTextures();
    }
    if (layer) {
        XLOG("new base layer %p, (inval region empty %d) with child %p", layer, inval.isEmpty(), layer->getChild(0));
        layer->setState(this);
        layer->markAsDirty(inval); // TODO: set in webview.cpp
    }
    m_treeManager.updateWithTree(layer, isPictureAfterFirstLayout);
    m_glExtras.setDrawExtra(0);

#ifdef MEASURES_PERF
    if (m_measurePerfs && !showVisualIndicator)
        dumpMeasures();
    m_measurePerfs = showVisualIndicator;
#endif

    TilesManager::instance()->setShowVisualIndicator(showVisualIndicator);
}

void GLWebViewState::scrollLayer(int layerId, int x, int y)
{
    m_treeManager.updateScrollableLayer(layerId, x, y);

    // TODO: only inval the area of the scrolled layer instead of
    // doing a fullInval()
    if (m_layersRenderingMode == kSingleSurfaceRendering)
        fullInval();
}

void GLWebViewState::invalRegion(const SkRegion& region)
{
    if (m_layersRenderingMode == kSingleSurfaceRendering) {
        // TODO: do the union of both layers tree to compute
        //the minimum inval instead of doing a fullInval()
        fullInval();
        return;
    }
    SkRegion::Iterator iterator(region);
    while (!iterator.done()) {
        SkIRect r = iterator.rect();
        IntRect ir(r.fLeft, r.fTop, r.width(), r.height());
        inval(ir);
        iterator.next();
    }
}

void GLWebViewState::inval(const IntRect& rect)
{
    m_currentPictureCounter++;
    if (!rect.isEmpty()) {
        // find which tiles fall within the invalRect and mark them as dirty
        m_tiledPageA->invalidateRect(rect, m_currentPictureCounter);
        m_tiledPageB->invalidateRect(rect, m_currentPictureCounter);
        if (m_frameworkInval.isEmpty())
            m_frameworkInval = rect;
        else
            m_frameworkInval.unite(rect);
        XLOG("intermediate invalRect(%d, %d, %d, %d) after unite with rect %d %d %d %d", m_frameworkInval.x(),
             m_frameworkInval.y(), m_frameworkInval.width(), m_frameworkInval.height(),
             rect.x(), rect.y(), rect.width(), rect.height());
    }
    TilesManager::instance()->getProfiler()->nextInval(rect, zoomManager()->currentScale());
}

unsigned int GLWebViewState::paintBaseLayerContent(SkCanvas* canvas)
{
    m_treeManager.drawCanvas(canvas, m_layersRenderingMode == kSingleSurfaceRendering);
    return m_currentPictureCounter;
}

TiledPage* GLWebViewState::sibling(TiledPage* page)
{
    return (page == m_tiledPageA) ? m_tiledPageB : m_tiledPageA;
}

TiledPage* GLWebViewState::frontPage()
{
    android::Mutex::Autolock lock(m_tiledPageLock);
    return m_usePageA ? m_tiledPageA : m_tiledPageB;
}

TiledPage* GLWebViewState::backPage()
{
    android::Mutex::Autolock lock(m_tiledPageLock);
    return m_usePageA ? m_tiledPageB : m_tiledPageA;
}

void GLWebViewState::swapPages()
{
    android::Mutex::Autolock lock(m_tiledPageLock);
    m_usePageA ^= true;
    TiledPage* oldPage = m_usePageA ? m_tiledPageB : m_tiledPageA;
    zoomManager()->swapPages();
    oldPage->discardTextures();
}

int GLWebViewState::baseContentWidth()
{
    return m_treeManager.baseContentWidth();
}
int GLWebViewState::baseContentHeight()
{
    return m_treeManager.baseContentHeight();
}

void GLWebViewState::setViewport(SkRect& viewport, float scale)
{
    if ((m_viewport == viewport) &&
        (zoomManager()->futureScale() == scale))
        return;

    m_goingDown = m_viewport.fTop - viewport.fTop <= 0;
    m_goingLeft = m_viewport.fLeft - viewport.fLeft >= 0;
    m_viewport = viewport;

    XLOG("New VIEWPORT %.2f - %.2f %.2f - %.2f (w: %2.f h: %.2f scale: %.2f currentScale: %.2f futureScale: %.2f)",
         m_viewport.fLeft, m_viewport.fTop, m_viewport.fRight, m_viewport.fBottom,
         m_viewport.width(), m_viewport.height(), scale,
         zoomManager()->currentScale(), zoomManager()->futureScale());

    const float invTileContentWidth = scale / TilesManager::tileWidth();
    const float invTileContentHeight = scale / TilesManager::tileHeight();

    m_viewportTileBounds.set(
            static_cast<int>(floorf(viewport.fLeft * invTileContentWidth)),
            static_cast<int>(floorf(viewport.fTop * invTileContentHeight)),
            static_cast<int>(ceilf(viewport.fRight * invTileContentWidth)),
            static_cast<int>(ceilf(viewport.fBottom * invTileContentHeight)));

    // allocate max possible number of tiles visible with this viewport
    int viewMaxTileX = static_cast<int>(ceilf((viewport.width()-1) * invTileContentWidth)) + 1;
    int viewMaxTileY = static_cast<int>(ceilf((viewport.height()-1) * invTileContentHeight)) + 1;

    int maxTextureCount = (viewMaxTileX + m_expandedTileBoundsX * 2) *
        (viewMaxTileY + m_expandedTileBoundsY * 2) * (m_highEndGfx ? 4 : 2);

    TilesManager::instance()->setMaxTextureCount(maxTextureCount);
    m_tiledPageA->updateBaseTileSize();
    m_tiledPageB->updateBaseTileSize();
}

#ifdef MEASURES_PERF
void GLWebViewState::dumpMeasures()
{
    for (int i = 0; i < m_timeCounter; i++) {
        XLOGC("%d delay: %d ms", m_totalTimeCounter + i,
             static_cast<int>(m_delayTimes[i]*1000));
        m_delayTimes[i] = 0;
    }
    m_totalTimeCounter += m_timeCounter;
    m_timeCounter = 0;
}
#endif // MEASURES_PERF

void GLWebViewState::resetFrameworkInval()
{
    m_frameworkInval.setX(0);
    m_frameworkInval.setY(0);
    m_frameworkInval.setWidth(0);
    m_frameworkInval.setHeight(0);
}

void GLWebViewState::addDirtyArea(const IntRect& rect)
{
    if (rect.isEmpty())
        return;

    IntRect inflatedRect = rect;
    inflatedRect.inflate(8);
    if (m_frameworkLayersInval.isEmpty())
        m_frameworkLayersInval = inflatedRect;
    else
        m_frameworkLayersInval.unite(inflatedRect);
}

void GLWebViewState::resetLayersDirtyArea()
{
    m_frameworkLayersInval.setX(0);
    m_frameworkLayersInval.setY(0);
    m_frameworkLayersInval.setWidth(0);
    m_frameworkLayersInval.setHeight(0);
}

void GLWebViewState::drawBackground(Color& backgroundColor)
{
    if (TilesManager::instance()->invertedScreen()) {
        float color = 1.0 - ((((float) backgroundColor.red() / 255.0) +
                      ((float) backgroundColor.green() / 255.0) +
                      ((float) backgroundColor.blue() / 255.0)) / 3.0);
        glClearColor(color, color, color, 1);
    } else {
        glClearColor((float)backgroundColor.red() / 255.0,
                     (float)backgroundColor.green() / 255.0,
                     (float)backgroundColor.blue() / 255.0, 1);
    }
    glClear(GL_COLOR_BUFFER_BIT);
}

double GLWebViewState::setupDrawing(IntRect& viewRect, SkRect& visibleRect,
                                    IntRect& webViewRect, int titleBarHeight,
                                    IntRect& screenClip, float scale)
{
    int left = viewRect.x();
    int top = viewRect.y();
    int width = viewRect.width();
    int height = viewRect.height();

    ShaderProgram* shader = TilesManager::instance()->shader();
    if (shader->program() == -1) {
        XLOG("Reinit shader");
        shader->init();
    }
    shader->setViewport(visibleRect, scale);
    shader->setViewRect(viewRect);
    shader->setWebViewRect(webViewRect);
    shader->setTitleBarHeight(titleBarHeight);
    shader->setScreenClip(screenClip);
    shader->resetBlending();

    shader->calculateAnimationDelta();

    glViewport(left + shader->getAnimationDeltaX(),
               top - shader->getAnimationDeltaY(),
               width, height);

    double currentTime = WTF::currentTime();

    setViewport(visibleRect, scale);
    m_zoomManager.processNewScale(currentTime, scale);

    return currentTime;
}

bool GLWebViewState::setLayersRenderingMode(TexturesResult& nbTexturesNeeded)
{
    bool invalBase = false;

    if (!nbTexturesNeeded.full)
        TilesManager::instance()->setMaxLayerTextureCount(0);
    else
        TilesManager::instance()->setMaxLayerTextureCount((2*nbTexturesNeeded.full)+1);

    int maxTextures = TilesManager::instance()->maxLayerTextureCount();
    LayersRenderingMode layersRenderingMode = m_layersRenderingMode;

    m_layersRenderingMode = kSingleSurfaceRendering;
    if (nbTexturesNeeded.fixed < maxTextures)
        m_layersRenderingMode = kFixedLayers;
    if (nbTexturesNeeded.scrollable < maxTextures)
        m_layersRenderingMode = kScrollableAndFixedLayers;
    if (nbTexturesNeeded.clipped < maxTextures)
        m_layersRenderingMode = kClippedTextures;
    if (nbTexturesNeeded.full < maxTextures)
        m_layersRenderingMode = kAllTextures;

    if (!maxTextures && !nbTexturesNeeded.full)
        m_layersRenderingMode = kAllTextures;

    if (m_layersRenderingMode < layersRenderingMode
        && m_layersRenderingMode != kAllTextures)
        invalBase = true;

    if (m_layersRenderingMode > layersRenderingMode
        && m_layersRenderingMode != kClippedTextures)
        invalBase = true;

#ifdef DEBUG
    if (m_layersRenderingMode != layersRenderingMode) {
        char* mode[] = { "kAllTextures", "kClippedTextures",
            "kScrollableAndFixedLayers", "kFixedLayers", "kSingleSurfaceRendering" };
        XLOGC("Change from mode %s to %s -- We need textures: fixed: %d,"
              " scrollable: %d, clipped: %d, full: %d, max textures: %d",
              static_cast<char*>(mode[layersRenderingMode]),
              static_cast<char*>(mode[m_layersRenderingMode]),
              nbTexturesNeeded.fixed,
              nbTexturesNeeded.scrollable,
              nbTexturesNeeded.clipped,
              nbTexturesNeeded.full, maxTextures);
    }
#endif

    // For now, anything below kClippedTextures is equivalent
    // to kSingleSurfaceRendering
    // TODO: implement the other rendering modes
    if (m_layersRenderingMode > kClippedTextures)
        m_layersRenderingMode = kSingleSurfaceRendering;

    // update the base surface if needed
    if (m_layersRenderingMode != layersRenderingMode
        && invalBase) {
        m_tiledPageA->discardTextures();
        m_tiledPageB->discardTextures();
        fullInval();
        return true;
    }
    return false;
}

void GLWebViewState::fullInval()
{
    // TODO -- use base layer's size.
    IntRect ir(0, 0, 1E6, 1E6);
    inval(ir);
}

bool GLWebViewState::drawGL(IntRect& rect, SkRect& viewport, IntRect* invalRect,
                            IntRect& webViewRect, int titleBarHeight,
                            IntRect& clip, float scale,
                            bool* treesSwappedPtr, bool* newTreeHasAnimPtr)
{
    m_scale = scale;
    TilesManager::instance()->getProfiler()->nextFrame(viewport.fLeft,
                                                       viewport.fTop,
                                                       viewport.fRight,
                                                       viewport.fBottom,
                                                       scale);
    TilesManager::instance()->incDrawGLCount();

#ifdef DEBUG
    TilesManager::instance()->getTilesTracker()->clear();
#endif

    float viewWidth = (viewport.fRight - viewport.fLeft) * TILE_PREFETCH_RATIO;
    float viewHeight = (viewport.fBottom - viewport.fTop) * TILE_PREFETCH_RATIO;
    bool useMinimalMemory = TilesManager::instance()->useMinimalMemory();
    bool useHorzPrefetch = useMinimalMemory ? 0 : viewWidth < baseContentWidth();
    bool useVertPrefetch = useMinimalMemory ? 0 : viewHeight < baseContentHeight();
    m_expandedTileBoundsX = (useHorzPrefetch) ? TILE_PREFETCH_DISTANCE : 0;
    m_expandedTileBoundsY = (useVertPrefetch) ? TILE_PREFETCH_DISTANCE : 0;

    XLOG("drawGL, rect(%d, %d, %d, %d), viewport(%.2f, %.2f, %.2f, %.2f)",
         rect.x(), rect.y(), rect.width(), rect.height(),
         viewport.fLeft, viewport.fTop, viewport.fRight, viewport.fBottom);

    resetLayersDirtyArea();

    // when adding or removing layers, use the the paintingBaseLayer's tree so
    // that content that moves to the base layer from a layer is synchronized

    if (scale < MIN_SCALE_WARNING || scale > MAX_SCALE_WARNING)
        XLOGC("WARNING, scale seems corrupted before update: %e", scale);

    // Here before we draw, update the BaseTile which has updated content.
    // Inside this function, just do GPU blits from the transfer queue into
    // the BaseTiles' texture.
    TilesManager::instance()->transferQueue()->updateDirtyBaseTiles();

    // Upload any pending ImageTexture
    // Return true if we still have some images to upload.
    // TODO: upload as many textures as possible within a certain time limit
    bool ret = ImagesManager::instance()->prepareTextures(this);

    if (scale < MIN_SCALE_WARNING || scale > MAX_SCALE_WARNING)
        XLOGC("WARNING, scale seems corrupted after update: %e", scale);

    // gather the textures we can use
    TilesManager::instance()->gatherLayerTextures();

    double currentTime = setupDrawing(rect, viewport, webViewRect, titleBarHeight, clip, scale);


    TexturesResult nbTexturesNeeded;
    bool fastSwap = isScrolling() || m_layersRenderingMode == kSingleSurfaceRendering;
    ret |= m_treeManager.drawGL(currentTime, rect, viewport,
                                scale, fastSwap,
                                treesSwappedPtr, newTreeHasAnimPtr,
                                &nbTexturesNeeded);
    if (!ret)
        resetFrameworkInval();

    int nbTexturesForImages = ImagesManager::instance()->nbTextures();
    XLOG("*** We have %d textures for images, %d full, %d clipped, total %d / %d",
          nbTexturesForImages, nbTexturesNeeded.full, nbTexturesNeeded.clipped,
          nbTexturesNeeded.full + nbTexturesForImages,
          nbTexturesNeeded.clipped + nbTexturesForImages);
    nbTexturesNeeded.full += nbTexturesForImages;
    nbTexturesNeeded.clipped += nbTexturesForImages;
    ret |= setLayersRenderingMode(nbTexturesNeeded);

    FloatRect extrasclip(0, 0, rect.width(), rect.height());
    TilesManager::instance()->shader()->clip(extrasclip);

    m_glExtras.drawGL(webViewRect, viewport, titleBarHeight);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Clean up GL textures for video layer.
    TilesManager::instance()->videoLayerManager()->deleteUnusedTextures();
    ret |= TilesManager::instance()->invertedScreenSwitch();

    if (ret) {
        // ret==true && empty inval region means we've inval'd everything,
        // but don't have new content. Keep redrawing full view (0,0,0,0)
        // until tile generation catches up and we swap pages.
        bool fullScreenInval = m_frameworkInval.isEmpty();

        if (TilesManager::instance()->invertedScreenSwitch()) {
            fullScreenInval = true;
            TilesManager::instance()->setInvertedScreenSwitch(false);
        }

        if (!fullScreenInval) {
            FloatRect frameworkInval = TilesManager::instance()->shader()->rectInInvScreenCoord(
                    m_frameworkInval);
            // Inflate the invalidate rect to avoid precision lost.
            frameworkInval.inflate(1);
            IntRect inval(frameworkInval.x(), frameworkInval.y(),
                    frameworkInval.width(), frameworkInval.height());

            inval.unite(m_frameworkLayersInval);

            invalRect->setX(inval.x());
            invalRect->setY(inval.y());
            invalRect->setWidth(inval.width());
            invalRect->setHeight(inval.height());

            XLOG("invalRect(%d, %d, %d, %d)", inval.x(),
                    inval.y(), inval.width(), inval.height());

            if (!invalRect->intersects(rect)) {
                // invalidate is occurring offscreen, do full inval to guarantee redraw
                fullScreenInval = true;
            }
        }

        if (fullScreenInval) {
            invalRect->setX(0);
            invalRect->setY(0);
            invalRect->setWidth(0);
            invalRect->setHeight(0);
        }
    } else {
        resetFrameworkInval();
    }

#ifdef MEASURES_PERF
    if (m_measurePerfs) {
        m_delayTimes[m_timeCounter++] = delta;
        if (m_timeCounter >= MAX_MEASURES_PERF)
            dumpMeasures();
    }
#endif

#ifdef DEBUG
    TilesManager::instance()->getTilesTracker()->showTrackTextures();
    ImagesManager::instance()->showImages();
#endif

    return ret;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
