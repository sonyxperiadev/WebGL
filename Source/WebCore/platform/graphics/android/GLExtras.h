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

#ifndef GLExtras_h
#define GLExtras_h

#include "SkRect.h"
#include "SkRegion.h"

namespace android {
    class FindOnPage;
    class CursorRing;
    class DrawExtra;
}

namespace WebCore {

class GLExtras {
public:
    GLExtras();
    virtual ~GLExtras();

    void drawGL(IntRect& webViewRect, SkRect& viewport, int titleBarHeight);
    void setFindOnPageExtra(android::FindOnPage* findOnPage) {
        m_findOnPage = findOnPage;
    }
    void setCursorRingExtra(android::CursorRing* ring) { m_ring = ring; }
    void setDrawExtra(android::DrawExtra* extra) { m_drawExtra = extra; }

private:
    void drawRing(SkRect& srcRect, int* texture, int r, int g, int b, float a);
    void drawRegion(const SkRegion& region, bool fill, bool drawBorder,
                    bool useDark = false);
    void drawCursorRings();
    void drawFindOnPage(SkRect& viewport);

    android::FindOnPage* m_findOnPage;
    android::CursorRing* m_ring;
    android::DrawExtra* m_drawExtra;
    int m_lightRingTexture;
    int m_darkRingTexture;
};

} // namespace WebCore

#endif // GLExtras_h
