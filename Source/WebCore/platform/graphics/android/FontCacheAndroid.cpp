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
#include "FontCache.h"

#include "Font.h"
#include "FontPlatformData.h"
#include "NotImplemented.h"
#include "SimpleFontData.h"
#include "SkPaint.h"
#include "SkTypeface.h"
#include "SkUtils.h"
#include <wtf/text/CString.h>

namespace WebCore {

static const char* getFallbackFontName(const FontDescription& fontDescription)
{
    switch (fontDescription.genericFamily()) {
    case FontDescription::StandardFamily:
    case FontDescription::SerifFamily:
        return "serif";
    case FontDescription::SansSerifFamily:
        return "sans-serif";
    case FontDescription::MonospaceFamily:
        return "monospace";
    case FontDescription::CursiveFamily:
        return "cursive";
    case FontDescription::FantasyFamily:
        return "fantasy";
    case FontDescription::NoFamily:
    default:
        return "";
    }
}

static bool isFallbackFamily(String family)
{
    return family.startsWith("-webkit-")
        || equalIgnoringCase(family, "serif")
        || equalIgnoringCase(family, "sans-serif")
        || equalIgnoringCase(family, "sans")
        || equalIgnoringCase(family, "monospace")
        || equalIgnoringCase(family, "times")   // skia aliases for serif
        || equalIgnoringCase(family, "times new roman")
        || equalIgnoringCase(family, "palatino")
        || equalIgnoringCase(family, "georgia")
        || equalIgnoringCase(family, "baskerville")
        || equalIgnoringCase(family, "goudy")
        || equalIgnoringCase(family, "cursive")
        || equalIgnoringCase(family, "fantasy")
        || equalIgnoringCase(family, "ITC Stone Serif")
        || equalIgnoringCase(family, "arial")   // skia aliases for sans-serif
        || equalIgnoringCase(family, "helvetica")
        || equalIgnoringCase(family, "tahoma")
        || equalIgnoringCase(family, "verdana")
        || equalIgnoringCase(family, "courier") // skia aliases for monospace
        || equalIgnoringCase(family, "courier new")
        || equalIgnoringCase(family, "monaco");
}

static char* AtomicStringToUTF8String(const AtomicString& utf16)
{
    SkASSERT(sizeof(uint16_t) == sizeof(utf16.characters()[0]));
    const uint16_t* uni = (uint16_t*)utf16.characters();

    size_t bytes = SkUTF16_ToUTF8(uni, utf16.length(), 0);
    char* utf8 = (char*)sk_malloc_throw(bytes + 1);

    (void)SkUTF16_ToUTF8(uni, utf16.length(), utf8);
    utf8[bytes] = 0;
    return utf8;
}


void FontCache::platformInit()
{
}

const SimpleFontData* FontCache::getFontDataForCharacters(const Font& font, const UChar* characters, int length)
{
    // since all of our fonts logically map to the fallback, we can always claim
    // that each font supports all characters.
    return font.primaryFont();
}

SimpleFontData* FontCache::getSimilarFontPlatformData(const Font& font)
{
    return 0;
}

SimpleFontData* FontCache::getLastResortFallbackFont(const FontDescription& description)
{
    static const AtomicString sansStr("sans-serif");
    static const AtomicString serifStr("serif");
    static const AtomicString monospaceStr("monospace");

    FontPlatformData* fontPlatformData = 0;
    switch (description.genericFamily()) {
    case FontDescription::SerifFamily:
        fontPlatformData = getCachedFontPlatformData(description, serifStr);
        break;
    case FontDescription::MonospaceFamily:
        fontPlatformData = getCachedFontPlatformData(description, monospaceStr);
        break;
    case FontDescription::SansSerifFamily:
    default:
        fontPlatformData = getCachedFontPlatformData(description, sansStr);
        break;
    }

    ASSERT(fontPlatformData);
    return getCachedFontData(fontPlatformData);
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    char* storage = 0;
    const char* name = 0;
    FontPlatformData* result = 0;

    if (family.length()) {
        storage = AtomicStringToUTF8String(family);
        name = storage;
    } else
        name = getFallbackFontName(fontDescription);

    int style = SkTypeface::kNormal;
    if (fontDescription.weight() >= FontWeightBold)
        style |= SkTypeface::kBold;
    if (fontDescription.italic())
        style |= SkTypeface::kItalic;

    // CreateFromName always returns a typeface, falling back to a default font
    // if the one requested is not found. Calling Equal() with a null pointer
    // serves to compare the returned font against the default, with the caveat
    // that the default is always of normal style. If we detect the default, we
    // ignore it and allow WebCore to give us the next font on the CSS fallback
    // list. The only exception is if the family name is a commonly used generic
    // family, as when called by getSimilarFontPlatformData() and
    // getLastResortFallbackFont(). In this case, the default font is an
    // acceptable result.

    SkTypeface* tf = SkTypeface::CreateFromName(name, SkTypeface::kNormal);

    if (!SkTypeface::Equal(tf, 0) || isFallbackFamily(family.string())) {
        // We had to use normal styling to see if this was a default font. If
        // we need bold or italic, replace with the corrected typeface.
        if (style != SkTypeface::kNormal) {
            tf->unref();
            tf = SkTypeface::CreateFromName(name, (SkTypeface::Style)style);
        }

        result = new FontPlatformData(tf, fontDescription.computedSize(),
                            (style & SkTypeface::kBold) && !tf->isBold(),
                            (style & SkTypeface::kItalic) && !tf->isItalic());
    }

    tf->unref();
    sk_free(storage);
    return result;
}

    // new as of SVN change 36269, Sept 8, 2008
void FontCache::getTraitsInFamily(const AtomicString& familyName, Vector<unsigned>& traitsMasks)
{
    // Don't understand this yet, but it seems safe to leave unimplemented
}

}
