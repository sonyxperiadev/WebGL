/*
 * Copyright 2009, The Android Open Source Project
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

// BEGIN android-added
// Add config.h to avoid compiler error in uobject.h
// ucnv.h includes uobject.h indirectly and uobjetcs.h defines new/delete.
// new/delete are also defined in WebCorePrefix.h which auto included in Android make.
//
// config.h has to be on top of the include list.
#include "config.h"
// END android-added

#include "EmojiFont.h"
#include <icu4c/common/unicode/ucnv.h>

namespace android {

U_STABLE UConverter* U_EXPORT2
ucnv_open_emoji(const char *converterName, UErrorCode *err) {
    if (EmojiFont::IsAvailable()) {
        if (strcmp(converterName, "Shift_JIS") == 0) {
            converterName = EmojiFont::GetShiftJisConverterName();
        }
    }
    return ucnv_open(converterName, err);
}

} // end namespace android
