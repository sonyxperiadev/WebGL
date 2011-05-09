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
    ~PlatformGradientRec() { SkSafeUnref(m_shader); }

    SkShader*           m_shader;
    SkShader::TileMode  m_tileMode;
    int                 m_colorCountWhenShaderWasBuilt;
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

SkShader* Gradient::getShader(SkShader::TileMode mode)
{
    if (NULL == m_gradient)
        m_gradient = new PlatformGradientRec;
    else if (mode == m_gradient->m_tileMode)
        return m_gradient->m_shader;

    // need to ensure that the m_stops array is sorted. We call getColor()
    // which, as a side effect, does the sort.
    // TODO: refactor Gradient.h to formally expose a sort method
    {
        float r, g, b, a;
        this->getColor(0, &r, &g, &b, &a);
    }

    SkPoint pts[2] = { m_p0, m_p1 };    // convert to SkPoint

    const size_t count = m_stops.size();
    SkAutoMalloc    storage(count * (sizeof(SkColor) + sizeof(SkScalar)));
    SkColor*        colors = (SkColor*)storage.get();
    SkScalar*       pos = (SkScalar*)(colors + count);

    Vector<ColorStop>::iterator iter = m_stops.begin();
    for (int i = 0; iter != m_stops.end(); i++) {
        pos[i] = SkFloatToScalar(iter->stop);
        colors[i] = SkColorSetARGB(F2B(iter->alpha), F2B(iter->red),
                                   F2B(iter->green), F2B(iter->blue));
        ++iter;
    }

    SkShader* s;
    if (m_radial)
        s = SkGradientShader::CreateTwoPointRadial(pts[0],
                                                   SkFloatToScalar(m_r0),
                                                   pts[1],
                                                   SkFloatToScalar(m_r1),
                                                   colors, pos, count, mode);
    else
        s = SkGradientShader::CreateLinear(pts, colors, pos, count, mode);

    if (NULL == s)
        s = new SkColorShader(0);

    // zap our previous shader, if present
    SkSafeUnref(m_gradient->m_shader);
    m_gradient->m_shader = s;
    m_gradient->m_tileMode = mode;
    SkMatrix matrix = m_gradientSpaceTransformation;
    s->setLocalMatrix(matrix);

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
    android_gc2canvas(context)->drawRect(rect, paint);
}


} //namespace
