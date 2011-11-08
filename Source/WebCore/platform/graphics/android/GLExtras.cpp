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

#include "DrawExtra.h"
#include "FindCanvas.h"
#include "GLExtras.h"
#include "IntRect.h"
#include "TilesManager.h"
#include "android_graphics.h"

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "GLExtras", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "GLExtras", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

// Touch ring border width. This is doubled if the ring is not pressed
#define RING_BORDER_WIDTH 1
// Color of the ring is 0x6633b5e5 (copied from framework's holo_light)
#define COLOR_HOLO_LIGHT &m_lightRingTexture, 0x33, 0xb5, 0xe5, 0.4f
// Color of the ring is 0x660099cc (copied from framework's holo_dark)
#define COLOR_HOLO_DARK &m_darkRingTexture, 0x00, 0x99, 0xcc, 0.6f
// Put a cap on the number of matches to draw.  If the current page has more
// matches than this, only draw the focused match. This both prevents clutter
// on the page and keeps the performance happy
#define MAX_NUMBER_OF_MATCHES_TO_DRAW 101

GLExtras::GLExtras()
    : m_findOnPage(0)
    , m_ring(0)
    , m_drawExtra(0)
    , m_lightRingTexture(-1)
    , m_darkRingTexture(-1)
{
}

GLExtras::~GLExtras()
{
}

void GLExtras::drawRing(SkRect& srcRect, int* texture, int r, int g, int b, float a)
{
    if (*texture == -1)
        *texture = GLUtils::createSampleColorTexture(r, g, b);

    if (srcRect.fRight <= srcRect.fLeft || srcRect.fBottom <= srcRect.fTop) {
        // Invalid rect, reject it
        return;
    }
    XLOG("drawQuad [%fx%f, %f, %f]", srcRect.fLeft, srcRect.fTop,
         srcRect.width(), srcRect.height());
    TilesManager::instance()->shader()->drawQuad(srcRect, *texture, a);
}

void GLExtras::drawRegion(const SkRegion& region, bool fill,
                          bool drawBorder, bool useDark)
{
    if (region.isEmpty())
        return;
    if (fill) {
        SkRegion::Iterator rgnIter(region);
        while (!rgnIter.done()) {
            const SkIRect& ir = rgnIter.rect();
            SkRect r;
            r.set(ir.fLeft, ir.fTop, ir.fRight, ir.fBottom);
            if (useDark)
                drawRing(r, COLOR_HOLO_DARK);
            else
                drawRing(r, COLOR_HOLO_LIGHT);
            rgnIter.next();
        }
    }
    if (fill && !drawBorder)
        return;
    SkPath path;
    if (!region.getBoundaryPath(&path))
        return;
    SkPath::Iter iter(path, true);
    SkPath::Verb verb;
    SkPoint pts[4];
    SkRegion clip;
    SkIRect startRect;
    while ((verb = iter.next(pts)) != SkPath::kDone_Verb) {
        if (verb == SkPath::kLine_Verb) {
            SkRect r;
            r.set(pts, 2);
            SkIRect line;
            int borderWidth = RING_BORDER_WIDTH;
            if (!fill)
                borderWidth *= 2;
            line.fLeft = r.fLeft - borderWidth;
            line.fRight = r.fRight + borderWidth;
            line.fTop = r.fTop - borderWidth;
            line.fBottom = r.fBottom + borderWidth;
            if (clip.intersects(line)) {
                clip.op(line, SkRegion::kReverseDifference_Op);
                if (clip.isEmpty())
                    continue; // Nothing to draw, continue
                line = clip.getBounds();
                if (SkIRect::Intersects(startRect, line)) {
                    clip.op(startRect, SkRegion::kDifference_Op);
                    if (clip.isEmpty())
                        continue; // Nothing to draw, continue
                    line = clip.getBounds();
                }
            } else {
                clip.setRect(line);
            }
            r.set(line.fLeft, line.fTop, line.fRight, line.fBottom);
            if (useDark)
                drawRing(r, COLOR_HOLO_DARK);
            else
                drawRing(r, COLOR_HOLO_LIGHT);
            if (startRect.isEmpty()) {
                startRect.set(line.fLeft, line.fTop, line.fRight, line.fBottom);
            }
        }
        if (verb == SkPath::kMove_Verb) {
            startRect.setEmpty();
        }
    }
}

void GLExtras::drawCursorRings()
{
    SkRegion region;
    for (size_t i = 0; i < m_ring->rings().size(); i++) {
        IntRect rect = m_ring->rings().at(i);
        if (i == 0)
            region.setRect(rect);
        else
            region.op(rect, SkRegion::kUnion_Op);
    }
    drawRegion(region, m_ring->m_isPressed, !m_ring->m_isButton, false);
}

void GLExtras::drawFindOnPage(SkRect& viewport)
{
    WTF::Vector<MatchInfo>* matches = m_findOnPage->matches();
    XLOG("drawFindOnPage, matches: %p", matches);
    if (!matches || !m_findOnPage->isCurrentLocationValid())
        return;
    int count = matches->size();
    int current = m_findOnPage->currentMatchIndex();
    XLOG("match count: %d", count);
    if (count < MAX_NUMBER_OF_MATCHES_TO_DRAW)
        for (int i = 0; i < count; i++) {
            MatchInfo& info = matches->at(i);
            const SkRegion& region = info.getLocation();
            SkIRect rect = region.getBounds();
            if (rect.intersect(viewport.fLeft, viewport.fTop,
                               viewport.fRight, viewport.fBottom))
                drawRegion(region, i == current, false, true);
#ifdef DEBUG
            else
                XLOG("Quick rejecting [%dx%d, %d, %d", rect.fLeft, rect.fTop,
                     rect.width(), rect.height());
#endif // DEBUG
        }
    else {
        MatchInfo& info = matches->at(current);
        drawRegion(info.getLocation(), true, false, true);
    }
}

void GLExtras::drawGL(IntRect& webViewRect, SkRect& viewport, int titleBarHeight)
{
    if (m_drawExtra) {
        if (m_drawExtra == m_ring)
            drawCursorRings();
        else if (m_drawExtra == m_findOnPage)
            drawFindOnPage(viewport);
        else
            XLOGC("m_drawExtra %p is unknown! (cursor: %p, find: %p",
                  m_drawExtra, m_ring, m_findOnPage);
    }
}
