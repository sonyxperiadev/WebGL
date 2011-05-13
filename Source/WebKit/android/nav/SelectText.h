/*
 * Copyright 2008, The Android Open Source Project
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

#ifndef SELECT_TEXT_H
#define SELECT_TEXT_H

#include "DrawExtra.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "SkPath.h"
#include "SkPicture.h"
#include "SkRect.h"
#include "SkRegion.h"

namespace android {

class CachedRoot;

class SelectText : public DrawExtra {
public:
    SelectText();
    virtual ~SelectText();
    virtual void draw(SkCanvas* , LayerAndroid* , IntRect* );
    void extendSelection(const IntRect& vis, int x, int y);
    const String getSelection();
    bool hitSelection(int x, int y) const;
    void moveSelection(const IntRect& vis, int x, int y);
    void reset();
    IntPoint selectableText(const CachedRoot* );
    void selectAll();
    int selectionX() const;
    int selectionY() const;
    void setDrawPointer(bool drawPointer) { m_drawPointer = drawPointer; }
    void setExtendSelection(bool extend) { m_extendSelection = extend; }
    bool startSelection(const CachedRoot* , const IntRect& vis, int x, int y);
    bool wordSelection(const CachedRoot* , const IntRect& vis, int x, int y);
public:
    float m_inverseScale; // inverse scale, x, y used for drawing select path
    int m_selectX;
    int m_selectY;
private:
    class FirstCheck;
    class EdgeCheck;
    void drawSelectionPointer(SkCanvas* , IntRect* );
    void drawSelectionRegion(SkCanvas* , IntRect* );
    SkIRect findClosest(FirstCheck& , const SkPicture& , int* base);
    SkIRect findEdge(const SkPicture& , const SkIRect& area,
        int x, int y, bool left, int* base);
    SkIRect findLeft(const SkPicture& picture, const SkIRect& area,
        int x, int y, int* base);
    SkIRect findRight(const SkPicture& picture, const SkIRect& area,
        int x, int y, int* base);
    static void getSelectionArrow(SkPath* );
    void getSelectionCaret(SkPath* );
    bool hitCorner(int cx, int cy, int x, int y) const;
    void setVisibleRect(const IntRect& );
    void swapAsNeeded();
    SkIPoint m_original; // computed start of extend selection
    SkIPoint m_startOffset; // difference from global to layer
    SkIRect m_selStart;
    SkIRect m_selEnd;
    SkIRect m_lastStart;
    SkIRect m_lastEnd;
    SkIRect m_lastDrawnStart;
    SkIRect m_lastDrawnEnd;
    SkIRect m_wordBounds;
    int m_startBase;
    int m_endBase;
    int m_layerId;
    SkIRect m_visibleRect; // constrains picture computations to visible area
    SkRegion m_lastSelRegion;
    SkRegion m_selRegion; // computed from sel start, end
    SkPicture m_startControl;
    SkPicture m_endControl;
    const SkPicture* m_picture;
    bool m_drawPointer;
    bool m_extendSelection; // false when trackball is moving pointer
    bool m_flipped;
    bool m_hitTopLeft;
    bool m_startSelection;
    bool m_wordSelection;
    bool m_outsideWord;
};

}

namespace WebCore {

void ReverseBidi(UChar* chars, int len);

}

#endif
