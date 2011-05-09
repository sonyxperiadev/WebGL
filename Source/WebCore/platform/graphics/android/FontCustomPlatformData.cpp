/*
 * Copyright 2008, The Android Open Source Project
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
#include "FontCustomPlatformData.h"

#include "SkTypeface.h"
#include "SkStream.h"
#include "SharedBuffer.h"
#include "FontPlatformData.h"

namespace WebCore {

FontCustomPlatformData::FontCustomPlatformData(SkTypeface* face)
{
    SkSafeRef(face);
    m_typeface = face;
}

FontCustomPlatformData::~FontCustomPlatformData()
{
    SkSafeUnref(m_typeface);
    // the unref is enough to release the font data...
}

FontPlatformData FontCustomPlatformData::fontPlatformData(int size, bool bold, bool italic,
    FontOrientation, FontRenderingMode)
{
    // turn bold/italic into fakeBold/fakeItalic
    if (m_typeface != NULL) {
        if (m_typeface->isBold() == bold)
            bold = false;
        if (m_typeface->isItalic() == italic)
            italic = false;
    }
    return FontPlatformData(m_typeface, size, bold, italic);
}

FontCustomPlatformData* createFontCustomPlatformData(SharedBuffer* buffer)
{
    // pass true until we know how we can share the data, and not have to
    // make a copy of it.
    SkStream* stream = new SkMemoryStream(buffer->data(), buffer->size(), true);
    SkTypeface* face = SkTypeface::CreateFromStream(stream);
    // Release the stream.
    stream->unref();
    if (0 == face) {
        SkDebugf("--------- SkTypeface::CreateFromBuffer failed %d\n",
                 buffer->size());
        return NULL;
    }

    SkAutoUnref aur(face);

    return new FontCustomPlatformData(face);
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalIgnoringCase(format, "truetype") || equalIgnoringCase(format, "opentype")
#if ENABLE(OPENTYPE_SANITIZER)
        || equalIgnoringCase(format, "woff")
#endif
    ;
}

}
