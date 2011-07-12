/*
 * Copyright (c) 2010 The Android Open Source Project. All rights reserved.
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


#ifndef AutoFillStringUtils_h_
#define AutoFillStringUtils_h_

#include "ChromiumIncludes.h"
#include "HTMLFormControlElement.h"
#include <wtf/text/WTFString.h>

using WebCore::HTMLFormControlElement;

const string16 kText = ASCIIToUTF16("text");
const string16 kHidden = ASCIIToUTF16("hidden");
const string16 kSelectOne = ASCIIToUTF16("select-one");

inline string16 WTFStringToString16(const WTF::String& wtfString)
{
    WTF::String str = wtfString;

    if (str.charactersWithNullTermination())
        return string16(str.charactersWithNullTermination());
    else
        return string16();
}

inline string16 nameForAutofill(const HTMLFormControlElement& element)
{
    // Taken from WebKit/chromium/src/WebFormControlElement.cpp, ported
    // to use WebCore types for accessing element properties.
    String name = element.name();
    String trimmedName = name.stripWhiteSpace();
    if (!trimmedName.isEmpty())
        return WTFStringToString16(trimmedName);
    name = element.getIdAttribute();
    trimmedName = name.stripWhiteSpace();
    if (!trimmedName.isEmpty())
        return WTFStringToString16(trimmedName);
    return string16();
}

inline string16 formControlType(const HTMLFormControlElement& element)
{
    // Taken from WebKit/chromium/src/WebFormControlElement.cpp, ported
    // to use WebCore types for accessing element properties.
    return WTFStringToString16(element.type());
}

#endif

