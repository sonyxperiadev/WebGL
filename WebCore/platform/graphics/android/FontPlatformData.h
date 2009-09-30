/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef FontPlatformData_H
#define FontPlatformData_H

#ifndef NDEBUG
#include "PlatformString.h"
#endif

#include "StringImpl.h"

class SkPaint;
class SkTypeface;

namespace WebCore {

class FontPlatformData {
public:
    static FontPlatformData Deleted() {
        return FontPlatformData(NULL, -1, false, false);
    }

    FontPlatformData();
    FontPlatformData(const FontPlatformData&);
    FontPlatformData(SkTypeface*, float textSize, bool fakeBold, bool fakeItalic);
    FontPlatformData(const FontPlatformData& src, float textSize);
    FontPlatformData(float size, bool syntheticBold, bool syntheticOblique);

    ~FontPlatformData();

    FontPlatformData(WTF::HashTableDeletedValueType)
        : mTypeface(hashTableDeletedFontValue()) { }
    bool isHashTableDeletedValue() const {
        return mTypeface == hashTableDeletedFontValue();
    }

    FontPlatformData& operator=(const FontPlatformData&);
    bool operator==(const FontPlatformData& a) const;

    void     setupPaint(SkPaint*) const;
    float size() const { return mTextSize; }
    unsigned hash() const;

#ifndef NDEBUG
    String description() const { return ""; }
#endif

private:
    SkTypeface* mTypeface;
    float       mTextSize;
    bool        mFakeBold;
    bool        mFakeItalic;

    static SkTypeface* hashTableDeletedFontValue() {
        return reinterpret_cast<SkTypeface*>(-1);
    }
};
    
} /* namespace */

#endif
