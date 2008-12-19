/* 
 *
 * Copyright 2006, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *
 *     http://www.apache.org/licenses/LICENSE-2.0 
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License.
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
