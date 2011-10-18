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
#include "GlyphPageTreeNode.h"
#include "HarfbuzzSkia.h"
#include "SimpleFontData.h"
#include "SkFontHost.h"
#include "SkPaint.h"
#include "SkTemplates.h"
#include "SkUtils.h"
#include "VerticalTextMap.h"


using namespace android;

namespace WebCore {

#define NO_BREAK_SPACE_UNICHAR 0xA0

static int substituteWithVerticalGlyphs(const FontPlatformData& platformData, uint16_t* glyphs, unsigned bufferLength)
{
    HB_FaceRec_* hbFace = platformData.harfbuzzFace();
    if (!hbFace->gsub) {
        // if there is no GSUB table, treat it as not covered
        return 0Xffff;
    }

    HB_Buffer buffer;
    hb_buffer_new(&buffer);
    for (unsigned i = 0; i < bufferLength; ++i)
        hb_buffer_add_glyph(buffer, glyphs[i], 0, i);

    HB_UShort scriptIndex;
    HB_UShort featureIndex;

    HB_GSUB_Select_Script(hbFace->gsub, HB_MAKE_TAG('D', 'F', 'L', 'T'), &scriptIndex);
    HB_GSUB_Select_Feature(hbFace->gsub, HB_MAKE_TAG('v', 'e', 'r', 't'), scriptIndex, 0xffff, &featureIndex);
    HB_GSUB_Add_Feature(hbFace->gsub, featureIndex, 1);
    HB_GSUB_Select_Feature(hbFace->gsub, HB_MAKE_TAG('v', 'r', 't', '2'), scriptIndex, 0xffff, &featureIndex);
    HB_GSUB_Add_Feature(hbFace->gsub, featureIndex, 1);

    int error = HB_GSUB_Apply_String(hbFace->gsub, buffer);
    if (!error) {
        for (unsigned i = 0; i < bufferLength; ++i)
            glyphs[i] = static_cast<Glyph>(buffer->out_string[i].gindex);
    }
    return error;
}

bool GlyphPage::fill(unsigned offset, unsigned length, UChar* buffer, unsigned bufferLength, const SimpleFontData* fontData)
{
    if (SkUTF16_IsHighSurrogate(buffer[bufferLength-1])) {
        SkDebugf("%s last char is high-surrogate", __FUNCTION__);
        return false;
    }
    
    SkPaint paint;
    fontData->platformData().setupPaint(&paint);
    paint.setTextEncoding(SkPaint::kUTF16_TextEncoding);

    SkAutoSTMalloc <GlyphPage::size, uint16_t> glyphStorage(length);
    uint16_t* glyphs = glyphStorage.get();
    UChar *textBuffer = buffer;
    UChar vTextBuffer[bufferLength];

    if (fontData->platformData().orientation() == Vertical && !fontData->hasVerticalGlyphs()) {
        // Convert to vertical form if there is no vertical glyphs.
        for (unsigned i = 0; i < bufferLength; ++i) {
            vTextBuffer[i] = VerticalTextMap::getVerticalForm(buffer[i]);
            if (!vTextBuffer[i])
                vTextBuffer[i] = buffer[i];
        }
        textBuffer = vTextBuffer;
    }

    unsigned count = paint.textToGlyphs(textBuffer, bufferLength << 1, glyphs);
    if (count != length) {
        SkDebugf("%s count != length\n", __FUNCTION__);
        return false;
    }

    if (fontData->hasVerticalGlyphs()) {
        bool lookVariants = false;
        for (unsigned i = 0; i < bufferLength; ++i) {
            if (!Font::isCJKIdeograph(textBuffer[i])) {
                lookVariants = true;
                continue;
            }
        }
        if (lookVariants)
            substituteWithVerticalGlyphs(fontData->platformData(), glyphs, bufferLength);
    }

    unsigned allGlyphs = 0; // track if any of the glyphIDs are non-zero

    // search for emoji. If we knew for sure that buffer was a contiguous range
    // of chars, we could quick-reject the range to avoid this loop (usually)
    if (EmojiFont::IsAvailable()) {
        const UChar* curr = textBuffer;
        for (unsigned i = 0; i < length; i++) {
            SkUnichar uni = SkUTF16_NextUnichar(&curr);
            uint16_t glyphID = glyphs[i];
            // only sniff if the normal font failed to recognize it
            if (!glyphID)
                glyphID = EmojiFont::UnicharToGlyph(uni);
            setGlyphDataForIndex(offset + i, glyphID, fontData);
            allGlyphs |= glyphID;
        }
    } else {
        for (unsigned i = 0; i < length; i++) {
            uint16_t glyphID = glyphs[i];
            setGlyphDataForIndex(offset + i, glyphID, fontData);
            allGlyphs |= glyphID;
        }
    }
    return allGlyphs != 0;
}

}
