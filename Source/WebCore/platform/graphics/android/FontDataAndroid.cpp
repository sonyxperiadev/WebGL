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
#include "SkFontHost.h"
#include "SkPaint.h"
#include "SkTypeface.h"
#include "SkTime.h"

using namespace android;

namespace WebCore {

void SimpleFontData::platformInit()
{
    SkPaint  paint;
    SkPaint::FontMetrics skiaFontMetrics;
    
    m_platformData.setupPaint(&paint);
    paint.getFontMetrics(&skiaFontMetrics);

    float d = SkScalarToFloat(skiaFontMetrics.fDescent);
    float s = SkScalarToFloat(skiaFontMetrics.fDescent - skiaFontMetrics.fAscent);
    float a = s - d;

    m_fontMetrics.setAscent(a);
    m_fontMetrics.setDescent(d);
    m_fontMetrics.setXHeight(SkScalarToFloat(-skiaFontMetrics.fAscent) * 0.56f);   // hack I stole from the window's port
    m_fontMetrics.setLineSpacing(a + d);
    m_fontMetrics.setLineGap(SkScalarToFloat(skiaFontMetrics.fLeading));

    if (platformData().orientation() == Vertical && !isTextOrientationFallback()) {
        static const uint32_t vheaTag = SkSetFourByteTag('v', 'h', 'e', 'a');
        static const uint32_t vorgTag = SkSetFourByteTag('V', 'O', 'R', 'G');
        const SkFontID fontID = m_platformData.uniqueID();
        size_t vheaSize = SkFontHost::GetTableSize(fontID, vheaTag);
        size_t vorgSize = SkFontHost::GetTableSize(fontID, vorgTag);
        if ((vheaSize > 0) || (vorgSize > 0))
            m_hasVerticalGlyphs = true;
    }
}

void SimpleFontData::platformCharWidthInit()
{
    m_avgCharWidth = 0.f;
    m_maxCharWidth = 0.f;
    initCharWidths();
}

void SimpleFontData::platformDestroy()
{
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_derivedFontData)
        m_derivedFontData = DerivedFontData::create(isCustomFont());
    if (!m_derivedFontData->smallCaps)
        m_derivedFontData->smallCaps = new SimpleFontData(FontPlatformData(m_platformData, fontDescription.computedSize() * 0.7f));

    return m_derivedFontData->smallCaps.get();
}

SimpleFontData* SimpleFontData::emphasisMarkFontData(const FontDescription& fontDescription) const
{
    if (!m_derivedFontData)
        m_derivedFontData = DerivedFontData::create(isCustomFont());
    if (!m_derivedFontData->emphasisMark)
        m_derivedFontData->emphasisMark = new SimpleFontData(FontPlatformData(m_platformData, fontDescription.computedSize() * 0.5f));

    return m_derivedFontData->emphasisMark.get();
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
    m_treatAsFixedPitch = m_platformData.isFixedPitch();
}

FloatRect SimpleFontData::platformBoundsForGlyph(Glyph) const
{
    return FloatRect();
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    SkASSERT(sizeof(glyph) == 2);   // compile-time assert

    SkPaint  paint;

    m_platformData.setupPaint(&paint);

    float advanceWidth;
    if (EmojiFont::IsEmojiGlyph(glyph))
        advanceWidth = EmojiFont::GetAdvanceWidth(glyph, paint);
    else {
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
        advanceWidth = SkScalarToFloat(paint.measureText(&glyph, 2));
    }
    return advanceWidth;
}


}
