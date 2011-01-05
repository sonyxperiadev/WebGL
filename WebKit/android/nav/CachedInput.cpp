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

#include "CachedPrefix.h"
#include "CachedInput.h"

namespace android {

void CachedInput::init() {
    bzero(this, sizeof(CachedInput));
    mName = WTF::String();
}

void CachedInput::setTypeFromElement(WebCore::HTMLInputElement* element)
{
    ASSERT(element);

    if (element->isPasswordField())
        mType = PASSWORD;
    else if (element->isSearchField())
        mType = SEARCH;
    else if (element->isEmailField())
        mType = EMAIL;
    else if (element->isNumberField())
        mType = NUMBER;
    else if (element->isTelephoneField())
        mType = TELEPHONE;
    else if (element->isURLField())
        mType = URL;
    else
        mType = NORMAL_TEXT_FIELD;
}

#if DUMP_NAV_CACHE

#define DEBUG_PRINT_BOOL(field) \
    DUMP_NAV_LOGD("// bool " #field "=%s;\n", b->field ? "true" : "false")

CachedInput* CachedInput::Debug::base() const {
    CachedInput* nav = (CachedInput*) ((char*) this - OFFSETOF(CachedInput, mDebug));
    return nav;
}

static void printWebCoreString(const char* label,
        const WTF::String& string) {
    char scratch[256];
    size_t index = snprintf(scratch, sizeof(scratch), label);
    const UChar* ch = string.characters();
    while (ch && *ch && index < sizeof(scratch)) {
        UChar c = *ch++;
        if (c < ' ' || c >= 0x7f) c = ' ';
        scratch[index++] = c;
    }
    DUMP_NAV_LOGD("%.*s\"\n", index, scratch);
}

void CachedInput::Debug::print() const
{
    CachedInput* b = base();
    DEBUG_PRINT_BOOL(mAutoComplete);
    DUMP_NAV_LOGD("// void* mForm=%p;\n", b->mForm);
    printWebCoreString("// char* mName=\"", b->mName);
    DUMP_NAV_LOGD("// int mMaxLength=%d;\n", b->mMaxLength);
    DUMP_NAV_LOGD("// int mPaddingLeft=%d;\n", b->mPaddingLeft);
    DUMP_NAV_LOGD("// int mPaddingTop=%d;\n", b->mPaddingTop);
    DUMP_NAV_LOGD("// int mPaddingRight=%d;\n", b->mPaddingRight);
    DUMP_NAV_LOGD("// int mPaddingBottom=%d;\n", b->mPaddingBottom);
    DUMP_NAV_LOGD("// float mTextSize=%f;\n", b->mTextSize);
    DUMP_NAV_LOGD("// int mLineHeight=%d;\n", b->mLineHeight);
    DUMP_NAV_LOGD("// Type mType=%d;\n", b->mType);
    DEBUG_PRINT_BOOL(mIsRtlText);
    DEBUG_PRINT_BOOL(mIsTextField);
    DEBUG_PRINT_BOOL(mIsTextArea);
}

#endif

}
