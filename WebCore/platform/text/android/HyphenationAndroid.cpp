/*
 * Copyright 2010, The Android Open Source Project
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "hyphenation_android"

#include "config.h"
#include "Hyphenation.h"
#include "NotImplemented.h"
#include <utils/AssetManager.h>
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

// For external hyphenation library.
#include "hyphen.h"

extern android::AssetManager* globalAssetManager();

using namespace WTF;

namespace WebCore {

static HyphenDict* getHyphenDict() {
    static HyphenDict *dict = 0;
    static bool isDictInitialized = false;
    if (!isDictInitialized && !dict) {
        isDictInitialized = true;
        android::AssetManager* am = globalAssetManager();
        // Only support English for now.
        android::Asset* a = am->open("webkit/hyph_en_US.dic",
            android::Asset::ACCESS_BUFFER);
        if (!a) {
            LOGW("asset webkit/hyph_en_US.dic not found!");
            return 0;
        }
        LOGD("dictionary length is %d", a->getLength());
        const CString dictContents = String(static_cast<const char*>(a->getBuffer(false)),
            a->getLength()).utf8();
        dict = hnj_hyphen_load_from_buffer(dictContents.data(),
            dictContents.length());
        delete a;
    }
    return dict;
}

size_t lastHyphenLocation(const UChar* characters, size_t length, size_t beforeIndex)
{
    static const size_t MIN_WORD_LEN = 5;
    static const size_t MAX_WORD_LEN = 100;
    if (beforeIndex <= 0 || length < MIN_WORD_LEN || length > MAX_WORD_LEN)
        return 0;

    HyphenDict *dict = getHyphenDict();
    if (!dict)
        return 0;

    char word[MAX_WORD_LEN];
    for (size_t i = 0; i < length; ++i) {
        const UChar ch = characters[i];
        // Only English for now.
        // To really make it language aware, we need something like language
        // detection or rely on the langAttr in the html element.  Though
        // seems right now the langAttr is not used or quite implemented in
        // webkit.
        if (!isASCIIAlpha(ch))
            return 0;
        word[i] = ch;
    }

    static const int EXTRA_BUFFER = 5;
    char hyphens[MAX_WORD_LEN + EXTRA_BUFFER];
    if (hnj_hyphen_hyphenate(dict, word, length, hyphens) == 0)
        for (size_t i = beforeIndex - 1; i > 0; --i) {
            if (hyphens[i] & 1)
                return i + 1;
        }

    return 0;
}

} // namespace WebCore
