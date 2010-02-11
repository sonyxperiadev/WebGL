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
#include "Pattern.h"

#include "android_graphics.h"
#include "GraphicsContext.h"
#include "SkBitmapRef.h"
#include "SkCanvas.h"
#include "SkColorShader.h"
#include "SkShader.h"
#include "SkPaint.h"

namespace WebCore {

static SkShader::TileMode toTileMode(bool doRepeat) {
    return doRepeat ? SkShader::kRepeat_TileMode : SkShader::kClamp_TileMode;
}

void Pattern::platformDestroy()
{
    m_pattern->safeUnref();
    m_pattern = 0;
}

SkShader* Pattern::platformPattern(const AffineTransform&)
{
    if (m_pattern)
        return m_pattern;

    SkBitmapRef* ref = tileImage()->nativeImageForCurrentFrame();
    if (!ref)
        return 0;
    m_pattern = SkShader::CreateBitmapShader(ref->bitmap(),
                                             toTileMode(m_repeatX),
                                             toTileMode(m_repeatY));
    m_pattern->setLocalMatrix(m_patternSpaceTransformation);
    return m_pattern;
}

void Pattern::setPlatformPatternSpaceTransform()
{
    if (m_pattern)
        m_pattern->setLocalMatrix(m_patternSpaceTransformation);
}

} //namespace
