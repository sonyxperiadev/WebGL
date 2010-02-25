/*
 * Copyright 2007, The Android Open Source Project
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

#include "CachedPrefix.h"
#include "android_graphics.h"
#include "CachedRoot.h"
#include "IntRect.h"
#include "LayerAndroid.h"
#include "SkCanvas.h"
#include "SkCornerPathEffect.h"
#include "SkPath.h"
#include "SkRegion.h"
#include "WebViewCore.h"

namespace android {

///////////////////////////////////////////////////////////////////////////////

const static SkColor cursorOuterColors[] = {
    SkColorSetARGB(0xff, 0xB3, 0x3F, 0x08), // normal ring select
    SkColorSetARGB(0xff, 0x46, 0xb0, 0x00), // fake ring select, for phone, email, text
    SkColorSetARGB(0xff, 0xAD, 0x5C, 0x0A), // normal ring pressed
    SkColorSetARGB(0xff, 0x36, 0xc0, 0x00)  // fake ring pressed
};

const static SkColor cursorInnerColors[] = {
    SkColorSetARGB(0xff, 0xFE, 0x92, 0x30), // normal ring select
    SkColorSetARGB(0xff, 0x8c, 0xd9, 0x00), // fake ring select, for phone, email, text
    SkColorSetARGB(0xff, 0xFE, 0xBD, 0x3A), // normal ring pressed
    SkColorSetARGB(0xff, 0x7c, 0xe9, 0x00)  // fake ring pressed
};

const static SkColor cursorPressedColors[] = {
    SkColorSetARGB(0x80, 0xFF, 0xC6, 0x4B), // normal ring pressed
    SkColorSetARGB(0x80, 0x7c, 0xe9, 0x00)  // fake ring pressed
};

#define CURSOR_RING_ROUNDEDNESS SkIntToScalar(5) // used to draw corners
#define CURSOR_RING_INNER_DIAMETER SkFixedToScalar(SkIntToFixed(3)>>1) // 3/2 == 1.5
#define CURSOR_RING_OUTER_OUTSET 2 // used to inflate rects added to region

void CursorRing::draw(SkCanvas* canvas, LayerAndroid* layer)
{
#if USE(ACCELERATED_COMPOSITING)
    int layerId = m_node->isInLayer() ? m_frame->layer(m_node)->uniqueId() : -1;
    if (layer->uniqueId() != layerId)
        return;
#endif
    if (canvas->quickReject(m_bounds, SkCanvas::kAA_EdgeType)) {
        DBG_NAV_LOGD("canvas->quickReject cursorNode=%d (nodePointer=%p)"
            " bounds=(%d,%d,w=%d,h=%d)", m_node->index(), m_node->nodePointer(),
            m_bounds.x(), m_bounds.y(), m_bounds.width(), m_bounds.height());
        m_followedLink = false;
        return;
    }
    unsigned rectCount = m_rings.size();
    SkRegion rgn;
    SkPath path;
    for (unsigned i = 0; i < rectCount; i++)
    {
        SkRect  r(m_rings[i]);
        SkIRect ir;

        r.round(&ir);
        ir.inset(-CURSOR_RING_OUTER_OUTSET, -CURSOR_RING_OUTER_OUTSET);
        rgn.op(ir, SkRegion::kUnion_Op);
    }
    rgn.getBoundaryPath(&path);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setPathEffect(new SkCornerPathEffect(CURSOR_RING_ROUNDEDNESS))->unref();
    if (m_flavor >= NORMAL_ANIMATING) { // pressed
        paint.setColor(cursorPressedColors[m_flavor - NORMAL_ANIMATING]);
        canvas->drawPath(path, paint);
    }
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(CURSOR_RING_OUTER_DIAMETER);
    paint.setColor(cursorOuterColors[m_flavor]);
    canvas->drawPath(path, paint);
    paint.setStrokeWidth(CURSOR_RING_INNER_DIAMETER);
    paint.setColor(cursorInnerColors[m_flavor]);
    canvas->drawPath(path, paint);
}

bool CursorRing::setup()
{
    m_node->localCursorRings(m_frame, &m_rings);
    if (!m_rings.size()) {
        DBG_NAV_LOG("!rings.size()");
        m_viewImpl->m_hasCursorBounds = false;
        return false;
    }
    m_isButton = false;
    m_viewImpl->gButtonMutex.lock();
    // If this is a button drawn by us (rather than webkit) do not draw the
    // cursor ring, since its cursor will be shown by a change in what we draw.
    // Should be in sync with recordButtons, since that will be called
    // before this.
    if (m_viewImpl->m_buttons.size() > 0) {
        WebCore::Node* cursorPointer = (WebCore::Node*) m_node->nodePointer();
        Container* end = m_viewImpl->m_buttons.end();
        for (Container* ptr = m_viewImpl->m_buttons.begin(); ptr != end; ptr++) {
            if (ptr->matches(cursorPointer)) {
                m_isButton = true;
                break;
            }
        }
    }
    m_viewImpl->gButtonMutex.unlock();
    m_bounds = m_node->localBounds(m_frame);
    m_viewImpl->updateCursorBounds(m_root, m_frame, m_node);

    bool useHitBounds = m_node->useHitBounds();
    if (useHitBounds)
        m_bounds = m_node->localHitBounds(m_frame);
    if (useHitBounds || m_node->useBounds()) {
        m_rings.clear();
        m_rings.append(m_bounds);
    }
    m_bounds.inflate(SkScalarCeil(CURSOR_RING_OUTER_DIAMETER));
    if (!m_node->hasCursorRing() || (m_node->isPlugin() && m_node->isFocus()))
        return false;
    m_flavor = NORMAL_FLAVOR;
    if (!m_isButton) {
        m_flavor = m_node->isSyntheticLink() ? FAKE_FLAVOR : NORMAL_FLAVOR;
        if (m_followedLink) {
            m_flavor = static_cast<Flavor>(m_flavor + NORMAL_ANIMATING);
        }
#if DEBUG_NAV_UI
        const WebCore::IntRect& ring = m_rings[0];
        DBG_NAV_LOGD("cursorNode=%d (nodePointer=%p) flavor=%s rings=%d"
            " (%d, %d, %d, %d) isPlugin=%s",
            m_node->index(), m_node->nodePointer(),
            m_flavor == FAKE_FLAVOR ? "FAKE_FLAVOR" :
            m_flavor == NORMAL_ANIMATING ? "NORMAL_ANIMATING" :
            m_flavor == FAKE_ANIMATING ? "FAKE_ANIMATING" : "NORMAL_FLAVOR",
            m_rings.size(), ring.x(), ring.y(), ring.width(), ring.height(),
            m_node->isPlugin() ? "true" : "false");
#endif
    }
    return true;
}

}
