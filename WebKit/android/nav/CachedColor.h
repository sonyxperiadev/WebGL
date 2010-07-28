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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CachedColor_H
#define CachedColor_H

#include "CachedDebug.h"
#include "Color.h"
#include "Length.h"
#include "SkColor.h"

using namespace WebCore;

namespace android {

class CachedColor {
public:
    CachedColor() {
        // Initiaized to 0 in its array, so nothing to do in the
        // constructor
    }
    bool operator==(const CachedColor& o) const {
        return memcmp(&o, this, sizeof(this)) == 0; }
    SkColor fillColor() const { return mFillColor; }
    void init();
    int innerWidth() const { return mInnerWidth; }
    int outerWidth() const { return mOuterWidth; }
    int outset() const { return mOutset; }
    SkColor pressedInnerColor() const { return mPressedInnerColor; }
    SkColor pressedOuterColor() const { return mPressedOuterColor; }
    int radius() const { return mRadius; }
    SkColor selectedInnerColor() const { return mSelectedInnerColor; }
    SkColor selectedOuterColor() const { return mSelectedOuterColor; }
    void setFillColor(const Color& c) { mFillColor = c.rgb(); }
    void setInnerWidth(Length l) { mInnerWidth = l.value(); }
    void setOuterWidth(Length l) { mOuterWidth = l.value(); }
    void setOutset(Length l) { mOutset = l.value(); }
    void setPressedInnerColor(const Color& c) { mPressedInnerColor = c.rgb(); }
    void setPressedOuterColor(const Color& c) { mPressedOuterColor = c.rgb(); }
    void setRadius(Length l) { mRadius = l.value(); }
    void setSelectedInnerColor(const Color& c) { mSelectedInnerColor = c.rgb(); }
    void setSelectedOuterColor(const Color& c) { mSelectedOuterColor = c.rgb(); }
private:
    SkColor mFillColor;
    int mInnerWidth;
    int mOuterWidth;
    int mOutset;
    SkColor mPressedInnerColor;
    SkColor mPressedOuterColor;
    int mRadius;
    SkColor mSelectedInnerColor;
    SkColor mSelectedOuterColor;
#if DUMP_NAV_CACHE
public:
    class Debug {
public:
        CachedColor* base() const;
        void print() const;
    } mDebug;
#endif
};

}

#endif
