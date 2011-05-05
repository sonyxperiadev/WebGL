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

#ifndef GLWebViewState_h
#define GLWebViewState_h

#if USE(ACCELERATED_COMPOSITING)

#include "DrawExtra.h"
#include "IntRect.h"
#include "SkCanvas.h"
#include "SkRect.h"
#include "SkRegion.h"
#include "TiledPage.h"
#include <utils/threads.h>

// Performance measurements probe
// To use it, enable the visual indicators in debug mode.
// turning off the visual indicators will flush the measures.
// #define MEASURES_PERF
#define MAX_MEASURES_PERF 2000

namespace WebCore {

class BaseLayerAndroid;
class LayerAndroid;

/////////////////////////////////////////////////////////////////////////////////
// GL Architecture
/////////////////////////////////////////////////////////////////////////////////
//
// To draw things, WebView use a tree of layers. The root of that tree is a
// BaseLayerAndroid, which may have numerous LayerAndroid over it. The content
// of those layers are SkPicture, the content of the BaseLayer is an PictureSet.
//
// When drawing, we therefore have one large "surface" that is the BaseLayer,
// and (possibly) additional surfaces (usually smaller), which are the
// LayerAndroids. The BaseLayer usually corresponds to the normal web page
// content, the Layers are used for some parts such as specific divs (e.g. fixed
// position divs, or elements using CSS3D transforms, or containing video,
// plugins, etc.).
//
// *** NOTE: The GL drawing architecture only paints the BaseLayer for now.
//
// The rendering model is to use tiles to display the BaseLayer (as obviously a
// BaseLayer's area can be arbitrarly large). The idea is to compute a set of
// tiles covering the viewport's area, paint those tiles using the webview's
// content (i.e. the BaseLayer's PictureSet), then display those tiles.
// We check which tile we should use at every frame.
//
// Overview
// ---------
//
// The tiles are grouped into a TiledPage -- basically a map of tiles covering
// the BaseLayer's surface. When drawing, we ask the TiledPage to prepare()
// itself then draw itself on screen. The prepare() function is the one
// that schedules tiles to be painted -- i.e. the subset of tiles that intersect
// with the current viewport. When they are ready, we can display
// the TiledPage.
//
// Note that BaseLayerAndroid::drawGL() will return true to the java side if
// there is a need to be called again (i.e. if we do not have up to date
// textures or a transition is going on).
//
// Tiles are implemented as a BaseTile. It knows how to paint itself with the
// PictureSet, and to display itself. A GL texture is usually associated to it.
//
// We also works with two TiledPages -- one to display the page at the
// current scale factor, and another we use to paint the page at a different
// scale factor. I.e. when we zoom, we use TiledPage A, with its tiles scaled
// accordingly (and therefore possible loss of quality): this is fast as it's
// purely a hardware operation. When the user is done zooming, we ask for
// TiledPage B to be painted at the new scale factor, covering the
// viewport's area. When B is ready, we swap it with A.
//
// Texture allocation
// ------------------
//
// Obviously we cannot have every BaseTile having a GL texture -- we need to
// get the GL textures from an existing pool, and reuse them.
//
// The way we do it is that when we call TiledPage::prepare(), we group the
// tiles we need (i.e. in the viewport and dirty) into a TilesSet and call
// BaseTile::reserveTexture() for each tile (which ensures there is a specific
// GL textures backing the BaseTiles).
//
// reserveTexture() will ask the TilesManager for a texture. The allocation
// mechanism goal is to (in order):
// - prefers to allocate the same texture as the previous time
// - prefers to allocate textures that are as far from the viewport as possible
// - prefers to allocate textures that are used by different TiledPages
//
// Note that to compute the distance of each tile from the viewport, each time
// we prepare() a TiledPage. Also during each prepare() we compute which tiles
// are dirty based on the info we have received from webkit.
//
// BaseTile Invalidation
// ------------------
//
// We do not want to redraw a tile if the tile is up-to-date. A tile is
// considered to be dirty an in need of redrawing in the following cases
//  - the tile has acquires a new texture
//  - webkit invalidates all or part of the tiles contents
//
// To handle the case of webkit invalidation we store two ids (counters) of the
// pictureSets in the tile.  The first id (A) represents the pictureSet used to
// paint the tile and the second id (B) represents the pictureSet in which the
// tile was invalidated by webkit. Thus, if A < B then tile is dirty.
//
// Painting scheduling
// -------------------
//
// The next operation is to schedule this TilesSet to be painted
// (TilesManager::schedulePaintForTilesSet()). TexturesGenerator
// will get the TilesSet and ask the BaseTiles in it to be painted.
//
// BaseTile::paintBitmap() will paint the texture using the BaseLayer's
// PictureSet (calling TiledPage::paintBaseLayerContent() which in turns
// calls GLWebViewState::paintBaseLayerContent()).
//
// Note that TexturesGenerator is running in a separate thread, the textures
// are shared using EGLImages (this is necessary to not slow down the rendering
// speed -- updating GL textures in the main GL thread would slow things down).
//
/////////////////////////////////////////////////////////////////////////////////

class GLWebViewState {
public:
    enum GLScaleStates {
        kNoScaleRequest = 0,
        kWillScheduleRequest = 1,
        kRequestNewScale = 2,
        kReceivedNewScale = 3
    };
    typedef int32_t GLScaleState;

    GLWebViewState(android::Mutex* globalButtonMutex);
    ~GLWebViewState();
    GLScaleState scaleRequestState() const { return m_scaleRequestState; }
    void setScaleRequestState(GLScaleState state) { m_scaleRequestState = state; }
    float currentScale() const { return m_currentScale; }
    void setCurrentScale(float scale) { m_currentScale = scale; }
    float futureScale() const { return m_futureScale; }
    void setFutureScale(float scale) { m_futureScale = scale; }
    const SkIRect& futureViewport() const { return m_futureViewportTileBounds; }
    void setFutureViewport(const SkIRect& viewport) { m_futureViewportTileBounds = viewport; }
    double updateTime() const { return m_updateTime; }
    void setUpdateTime(double value) { m_updateTime = value; }
    double zoomInTransitionTime(double currentTime);
    double zoomOutTransitionTime(double currentTime);
    float zoomInTransparency(double currentTime);
    float zoomOutTransparency(double currentTime);
    void resetTransitionTime() { m_transitionTime = -1; }

    unsigned int paintBaseLayerContent(SkCanvas* canvas);
    void setBaseLayer(BaseLayerAndroid* layer, const SkRegion& inval, bool showVisualIndicator,
                      bool isPictureAfterFirstLayout);
    void setExtra(BaseLayerAndroid*, SkPicture&, const IntRect&, bool allowSame);
    void scheduleUpdate(const double& currentTime, const SkIRect& viewport, float scale);
    void paintExtras();

    void setRings(Vector<IntRect>& rings, bool isPressed);
    void resetRings();
    void drawFocusRing(IntRect& rect);

    TiledPage* sibling(TiledPage* page);
    TiledPage* frontPage();
    TiledPage* backPage();
    void swapPages();

    // dimensions of the current base layer
    int baseContentWidth();
    int baseContentHeight();

    void setViewport(SkRect& viewport, float scale);

    // a rect containing the coordinates of all tiles in the current viewport
    const SkIRect& viewportTileBounds() const { return m_viewportTileBounds; }
    // a rect containing the viewportTileBounds before there was a scale change
    const SkIRect& preZoomBounds() const { return m_preZoomBounds; }
    void setPreZoomBounds(const SkIRect& bounds) { m_preZoomBounds = bounds; }

    unsigned int currentPictureCounter() const { return m_currentPictureCounter; }

    void lockBaseLayerUpdate() { m_baseLayerUpdate = false; }
    void unlockBaseLayerUpdate();

    bool moving() {
        // This will only works if we are not zooming -- we check
        // for this in BaseLayerAndroid::drawBasePictureInGL()
        if ((m_viewport.fLeft != m_previousViewport.fLeft ||
            m_viewport.fTop != m_previousViewport.fTop) &&
            m_viewport.width() == m_previousViewport.width() &&
            m_viewport.height() == m_previousViewport.height())
            return true;
        return false;
    }

    bool drawGL(IntRect& rect, SkRect& viewport, IntRect* invalRect,
                IntRect& webViewRect, int titleBarHeight,
                IntRect& clip, float scale, SkColor color = SK_ColorWHITE);

    void setBackgroundColor(SkColor color) { m_backgroundColor = color; }
    SkColor getBackgroundColor() { return m_backgroundColor; }

#ifdef MEASURES_PERF
    void dumpMeasures();
#endif

    void resetFrameworkInval();
    void addDirtyArea(const IntRect& rect);
    void resetLayersDirtyArea();
    LayerAndroid* previouslyUsedRoot() { return m_previouslyUsedRoot; }

private:
    void inval(const IntRect& rect); // caller must hold m_baseLayerLock
    void invalRegion(const SkRegion& region);

    // Delay between scheduling a new page when the scale
    // factor changes (i.e. zooming in or out)
    static const double s_updateInitialDelay = 0.3; // 300 ms
    // If the scale factor continued to change and we completed
    // the original delay, we push back the update by this value
    static const double s_updateDelay = 0.1; // 100 ms

    // Delay for the transition between the two pages
    static const double s_zoomInTransitionDelay = 0.1; // 100 ms
    static const double s_invZoomInTransitionDelay = 10;
    static const double s_zoomOutTransitionDelay = 0.2; // 200 ms
    static const double s_invZoomOutTransitionDelay = 5;

    GLScaleState m_scaleRequestState;
    float m_currentScale;
    float m_futureScale;
    double m_updateTime;
    double m_transitionTime;
    android::Mutex m_tiledPageLock;
    SkRect m_viewport;
    SkRect m_previousViewport;
    SkIRect m_viewportTileBounds;
    SkIRect m_futureViewportTileBounds;
    SkIRect m_preZoomBounds;
    android::Mutex m_baseLayerLock;
    BaseLayerAndroid* m_baseLayer;
    BaseLayerAndroid* m_currentBaseLayer;
    LayerAndroid* m_previouslyUsedRoot;

    unsigned int m_currentPictureCounter;
    bool m_usePageA;
    TiledPage* m_tiledPageA;
    TiledPage* m_tiledPageB;
    IntRect m_lastInval;
    IntRect m_frameworkInval;
    IntRect m_frameworkLayersInval;
    android::Mutex* m_globalButtonMutex;

    bool m_baseLayerUpdate;
    SkRegion m_invalidateRegion;

    SkColor m_backgroundColor;
    double m_prevDrawTime;

#ifdef MEASURES_PERF
    unsigned int m_totalTimeCounter;
    int m_timeCounter;
    double m_delayTimes[MAX_MEASURES_PERF];
    bool m_measurePerfs;
#endif
    bool m_displayRings;
    Vector<IntRect> m_rings;
    bool m_ringsIsPressed;
    int m_focusRingTexture;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // GLWebViewState_h
