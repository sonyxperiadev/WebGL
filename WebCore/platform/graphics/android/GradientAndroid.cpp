/*
 * Copyright 2006, The Android Open Source Project
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Gradient.h"

#include "android_graphics.h"
#include "CSSParser.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "SkCanvas.h"
#include "SkColorShader.h"
#include "SkGradientShader.h"
#include "SkPaint.h"

class PlatformGradientRec {
public:
    PlatformGradientRec() : m_shader(NULL) {}
    ~PlatformGradientRec() { m_shader->safeUnref(); }

    SkShader*           m_shader;
    SkShader::TileMode  m_tileMode;
};

namespace WebCore {

void Gradient::platformDestroy()
{
    delete m_gradient;
    m_gradient = 0;
}

static U8CPU F2B(float x)
{
    return (int)(x * 255);
}

SkShader* Gradient::getShader(SkShader::TileMode mode) {
    if (NULL == m_gradient) {
        m_gradient = new PlatformGradientRec;
    } else if (m_gradient->m_tileMode == mode) {
        return m_gradient->m_shader;
    }

    SkPoint pts[2];

    android_setpt(&pts[0], m_p0);
    android_setpt(&pts[1], m_p1);
    size_t count = m_stops.size();
    SkAutoMalloc    storage(count * (sizeof(SkColor) + sizeof(SkScalar)));
    SkColor*        colors = (SkColor*)storage.get();
    SkScalar*       pos = (SkScalar*)(colors + count);

    Vector<ColorStop>::iterator iter = m_stops.begin();
    int i = -1;
    while (i++, iter != m_stops.end()) {
        pos[i] = SkFloatToScalar(iter->stop);
        colors[i] = SkColorSetARGB(F2B(iter->alpha), F2B(iter->red),
            F2B(iter->green), F2B(iter->blue));
        ++iter;
    }

    SkShader* s;
    if (0 == count) {
        // it seems the spec says a zero-size gradient draws transparent
        s = new SkColorShader(0);
    } else if (m_radial) {
        s = SkGradientShader::CreateRadial(pts[0], SkFloatToScalar(m_r0),
                                           colors, pos, count, mode);
    } else {
        s = SkGradientShader::CreateLinear(pts, colors, pos, count, mode);
    }

    m_gradient->m_shader->safeUnref();
    m_gradient->m_shader = s;
    m_gradient->m_tileMode = mode;
    return s;
}

void Gradient::fill(GraphicsContext* context, const FloatRect& rect)
{
    SkRect r;
    SkPaint paint;
    // we don't care about the mode, so try to use the existing one
    SkShader::TileMode mode = m_gradient ? m_gradient->m_tileMode :
                                            SkShader::kClamp_TileMode;

    paint.setAntiAlias(true);
    paint.setShader(this->getShader(mode));
    android_gc2canvas(context)->drawRect(*android_setrect(&r, rect), paint);
}


} //namespace
