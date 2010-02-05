/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "EmojiFont.h"
#include "Font.h"
#include "FontCache.h"
#include "SimpleFontData.h"
#include "FloatRect.h"
#include "FontDescription.h"
#include "SkPaint.h"
#include "SkTypeface.h"
#include "SkTime.h"

using namespace android;

namespace WebCore {

void SimpleFontData::platformInit()
{
    SkPaint  paint;
    SkPaint::FontMetrics metrics;
    
    m_platformData.setupPaint(&paint);
    (void)paint.getFontMetrics(&metrics);

    // use ceil instead of round to favor descent, given a lot of accidental
    // clipping of descenders (e.g. 14pt 'g') in textedit fields
    int d = SkScalarCeil(metrics.fDescent);
    int s = SkScalarRound(metrics.fDescent - metrics.fAscent);
    int a = s - d;

    m_ascent = a;
    m_descent = d;
    m_xHeight = SkScalarToFloat(-metrics.fAscent) * 0.56f;   // hack I stole from the window's port
    m_lineSpacing = a + d;
    m_lineGap = SkScalarRound(metrics.fLeading);
}

void SimpleFontData::platformCharWidthInit()
{
    m_avgCharWidth = 0.f;
    m_maxCharWidth = 0.f;
    initCharWidths();
}

void SimpleFontData::platformDestroy()
{
    delete m_smallCapsFontData;
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        m_smallCapsFontData = new SimpleFontData(FontPlatformData(m_platformData, fontDescription.computedSize() * 0.7f));
    }
    return m_smallCapsFontData;
}

bool SimpleFontData::containsCharacters(const UChar* characters, int length) const
{
    SkPaint     paint;

    m_platformData.setupPaint(&paint);
    paint.setTextEncoding(SkPaint::kUTF16_TextEncoding);
    return paint.containsText(characters, length << 1);
}

void SimpleFontData::determinePitch()
{
    m_treatAsFixedPitch = false;
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    SkASSERT(sizeof(glyph) == 2);   // compile-time assert

    SkPaint  paint;

    m_platformData.setupPaint(&paint);

    if (EmojiFont::IsEmojiGlyph(glyph))
        return EmojiFont::GetAdvanceWidth(glyph, paint);
    else {
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
        return SkScalarToFloat(paint.measureText(&glyph, 2));
    }
}

}
