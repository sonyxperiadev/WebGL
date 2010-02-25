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
#include "IntRect.h"
#include "PlatformString.h"

class SkPicture;
struct SkIRect;
class SkRegion;

namespace android {

class CachedRoot;

class CopyPaste {
public:
    static void buildSelection(const SkPicture& , const SkIRect& area,
        const SkIRect& selStart, const SkIRect& selEnd, SkRegion* region);
    static SkIRect findClosest(const SkPicture& , const SkIRect& area,
        int x, int y);
    static String text(const SkPicture& ,  const SkIRect& area,
        const SkRegion& );
};

class SelectText : public DrawExtra {
public:
    SelectText() {
        m_selStart.setEmpty();
        m_selEnd.setEmpty();
    }
    virtual void draw(SkCanvas* , LayerAndroid* );
    const String getSelection();
    void moveSelection(const SkPicture* , int x, int y, bool extendSelection);
    void setDrawPointer(bool drawPointer) { m_drawPointer = drawPointer; }
    void setDrawRegion(bool drawRegion) { m_drawRegion = drawRegion; }
    void setVisibleRect(const IntRect& rect) { m_visibleRect = rect; }
private:
    friend class WebView;
    void drawSelectionPointer(SkCanvas* );
    void drawSelectionRegion(SkCanvas* );
    static void getSelectionArrow(SkPath* );
    void getSelectionCaret(SkPath* );
    SkIRect m_selStart;
    SkIRect m_selEnd;
    SkIRect m_visibleRect;
    SkRegion m_selRegion;
    const SkPicture* m_picture;
    float m_inverseScale;
    int m_selectX;
    int m_selectY;
    bool m_drawRegion;
    bool m_drawPointer;
    bool m_extendSelection;
};

}

#endif
