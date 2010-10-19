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

#ifndef BaseLayerAndroid_h
#define BaseLayerAndroid_h

#include "GLWebViewState.h"
#include "IntRect.h"
#include "PictureSet.h"
#include "SkLayer.h"

namespace WebCore {

class BaseLayerAndroid : public SkLayer {

public:
#ifdef DEBUG_COUNT
    static int count();
#endif
    BaseLayerAndroid();
    virtual ~BaseLayerAndroid();

#if USE(ACCELERATED_COMPOSITING)
    void setGLWebViewState(GLWebViewState* infos) { m_glWebViewState = infos; }
#endif
    void setContent(const android::PictureSet& src);
    android::PictureSet* content() { return &m_content; }
    // This method will paint using the current PictureSet onto
    // the passed canvas. We used it to paint the GL tiles as well as
    // WebView::copyBaseContentToPicture(), so a lock is necessary as
    // we are running in different threads.
    void draw(SkCanvas* canvas);

    bool drawGL(IntRect& rect, SkRect& viewport,
                float scale, SkColor color = SK_ColorWHITE);

private:
#if USE(ACCELERATED_COMPOSITING)
    bool drawBasePictureInGL(SkRect& viewport, float scale);

    GLWebViewState* m_glWebViewState;
    android::Mutex m_drawLock;
#endif
    android::PictureSet m_content;
    SkRect m_previousVisible;
};

} // namespace WebCore

#endif // BaseLayerAndroid_h
