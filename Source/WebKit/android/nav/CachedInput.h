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

#ifndef CachedInput_h
#define CachedInput_h

#include "CachedDebug.h"
#include "HTMLInputElement.h"
#include "PlatformString.h"

namespace android {

class CachedInput {
public:
    CachedInput() {
        // Initiaized to 0 in its array, so nothing to do in the
        // constructor
    }

    enum Type {
        NONE = -1,
        NORMAL_TEXT_FIELD = 0,
        TEXT_AREA = 1,
        PASSWORD = 2,
        SEARCH = 3,
        EMAIL = 4,
        NUMBER = 5,
        TELEPHONE = 6,
        URL = 7
    };

    bool autoComplete() const { return mAutoComplete; }
    void* formPointer() const { return mForm; }
    void init();
    void setTypeFromElement(WebCore::HTMLInputElement*);
    Type getType() const { return mType; }
    bool isRtlText() const { return mIsRtlText; }
    bool isTextField() const { return mIsTextField; }
    bool isTextArea() const { return mIsTextArea; }
    int lineHeight() const { return mLineHeight; }
    int maxLength() const { return mMaxLength; };
    const WTF::String& name() const { return mName; }
    int paddingBottom() const { return mPaddingBottom; }
    int paddingLeft() const { return mPaddingLeft; }
    int paddingRight() const { return mPaddingRight; }
    int paddingTop() const { return mPaddingTop; }
    void setAutoComplete(bool autoComplete) { mAutoComplete = autoComplete; }
    void setFormPointer(void* form) { mForm = form; }
    void setIsRtlText(bool isRtlText) { mIsRtlText = isRtlText; }
    void setIsTextField(bool isTextField) { mIsTextField = isTextField; }
    void setIsTextArea(bool isTextArea) { mIsTextArea = isTextArea; }
    void setLineHeight(int height) { mLineHeight = height; }
    void setMaxLength(int maxLength) { mMaxLength = maxLength; }
    void setName(const WTF::String& name) { mName = name; }
    void setPaddingBottom(int bottom) { mPaddingBottom = bottom; }
    void setPaddingLeft(int left) { mPaddingLeft = left; }
    void setPaddingRight(int right) { mPaddingRight = right; }
    void setPaddingTop(int top) { mPaddingTop = top; }
    void setSpellcheck(bool spellcheck) { mSpellcheck = spellcheck; }
    void setTextSize(float textSize) { mTextSize = textSize; }
    bool spellcheck() const { return mSpellcheck; }
    float textSize() const { return mTextSize; }

private:

    void* mForm;
    int mLineHeight;
    int mMaxLength;
    WTF::String mName;
    int mPaddingBottom;
    int mPaddingLeft;
    int mPaddingRight;
    int mPaddingTop;
    float mTextSize;
    Type mType;
    bool mAutoComplete : 1;
    bool mSpellcheck : 1;
    bool mIsRtlText : 1;
    bool mIsTextField : 1;
    bool mIsTextArea : 1;
#if DUMP_NAV_CACHE
public:
    class Debug {
public:
        CachedInput* base() const;
        void print() const;
    } mDebug;
#endif
};

}

#endif
