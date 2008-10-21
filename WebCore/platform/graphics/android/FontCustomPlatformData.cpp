/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
    face->safeRef();
    m_typeface = face;
}

FontCustomPlatformData::~FontCustomPlatformData()
{
    m_typeface->safeUnref();
    // the unref is enough to release the font data...
}

FontPlatformData FontCustomPlatformData::fontPlatformData(int size, bool bold, bool italic)
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
    if (NULL == face) {
        SkDebugf("--------- SkTypeface::CreateFromBuffer failed %d\n",
                 buffer->size());
        return NULL;
    }

    SkAutoUnref aur(face);

    return new FontCustomPlatformData(face);
}

}
