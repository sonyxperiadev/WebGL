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

#include "config.h"
#include "FontPlatformData.h"

#include "SkPaint.h"
#include "SkTypeface.h"

//#define TRACE_FONTPLATFORMDATA_LIFE
//#define COUNT_FONTPLATFORMDATA_LIFE

#ifdef COUNT_FONTPLATFORMDATA_LIFE
static int gCount;
static int gMaxCount;

static void inc_count()
{
    if (++gCount > gMaxCount)
    {
        gMaxCount = gCount;
        SkDebugf("---------- FontPlatformData %d\n", gMaxCount);
    }
}

static void dec_count() { --gCount; }
#else
    #define inc_count()
    #define dec_count()
#endif

#ifdef TRACE_FONTPLATFORMDATA_LIFE
    #define trace(num)  SkDebugf("FontPlatformData%d %p %g %d %d\n", num, mTypeface, mTextSize, mFakeBold, mFakeItalic)
#else
    #define trace(num)
#endif

namespace WebCore {

FontPlatformData::FontPlatformData()
    : mTypeface(NULL), mTextSize(0), mFakeBold(false), mFakeItalic(false)
{
    inc_count();
    trace(1);
}

FontPlatformData::FontPlatformData(const FontPlatformData& src)
{
    if (hashTableDeletedFontValue() != src.mTypeface) {
        src.mTypeface->safeRef();
    }

    mTypeface   = src.mTypeface;

    mTextSize   = src.mTextSize;
    mFakeBold   = src.mFakeBold;
    mFakeItalic = src.mFakeItalic;

    inc_count();
    trace(2);
}

FontPlatformData::FontPlatformData(SkTypeface* tf, float textSize, bool fakeBold, bool fakeItalic)
    : mTypeface(tf), mTextSize(textSize), mFakeBold(fakeBold), mFakeItalic(fakeItalic)
{
    if (hashTableDeletedFontValue() != mTypeface) {
        mTypeface->safeRef();
    }
    
    inc_count();
    trace(3);
}

FontPlatformData::FontPlatformData(const FontPlatformData& src, float textSize)
    : mTypeface(src.mTypeface), mTextSize(textSize), mFakeBold(src.mFakeBold), mFakeItalic(src.mFakeItalic)
{
    if (hashTableDeletedFontValue() != mTypeface) {
        mTypeface->safeRef();
    }

    inc_count();
    trace(4);
}
    
FontPlatformData::FontPlatformData(float size, bool bold, bool oblique)
    : mTypeface(NULL), mTextSize(size), mFakeBold(bold), mFakeItalic(oblique)
{
    inc_count();
    trace(5);
}

FontPlatformData::~FontPlatformData()
{
    dec_count();
#ifdef TRACE_FONTPLATFORMDATA_LIFE
    SkDebugf("----------- ~FontPlatformData\n");
#endif

    if (hashTableDeletedFontValue() != mTypeface) {
        mTypeface->safeUnref();
    }
}

FontPlatformData& FontPlatformData::operator=(const FontPlatformData& src)
{
    if (hashTableDeletedFontValue() != src.mTypeface) {
        src.mTypeface->safeRef();
    }
    if (hashTableDeletedFontValue() != mTypeface) {
        mTypeface->safeUnref();
    }

    mTypeface   = src.mTypeface;
    mTextSize   = src.mTextSize;
    mFakeBold   = src.mFakeBold;
    mFakeItalic = src.mFakeItalic;
    
    return *this;
}

void FontPlatformData::setupPaint(SkPaint* paint) const
{
    float ts = mTextSize;
    if (!(ts > 0))
        ts = 12;

    paint->setAntiAlias(true);
    paint->setSubpixelText(true);
    paint->setHinting(SkPaint::kSlight_Hinting);
    paint->setTextSize(SkFloatToScalar(ts));
    paint->setTypeface(mTypeface);
    paint->setFakeBoldText(mFakeBold);
    paint->setTextSkewX(mFakeItalic ? -SK_Scalar1/4 : 0);
    paint->setTextEncoding(SkPaint::kUTF16_TextEncoding);
}

bool FontPlatformData::operator==(const FontPlatformData& a) const
{
    return  mTypeface == a.mTypeface &&
            mTextSize == a.mTextSize &&
            mFakeBold == a.mFakeBold &&
            mFakeItalic == a.mFakeItalic;
}

unsigned FontPlatformData::hash() const
{
    unsigned h;
    
    if (hashTableDeletedFontValue() == mTypeface) {
        h = reinterpret_cast<unsigned>(mTypeface);
    } else {
        h = SkTypeface::UniqueID(mTypeface);
    }
    
    uint32_t sizeAsInt = *reinterpret_cast<const uint32_t*>(&mTextSize);

    h ^= 0x01010101 * (((int)mFakeBold << 1) | (int)mFakeItalic);
    h ^= sizeAsInt;
    return h;
}

}

