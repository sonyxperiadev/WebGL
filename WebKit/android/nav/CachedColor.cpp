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

#include "CachedPrefix.h"
#include "CachedColor.h"

namespace android {

#if DUMP_NAV_CACHE

#define DEBUG_PRINT_COLOR(field) \
    DUMP_NAV_LOGD("// SkColor " #field "=0x%08x;\n", b->field)

CachedColor* CachedColor::Debug::base() const {
    CachedColor* nav = (CachedColor*) ((char*) this - OFFSETOF(CachedColor, mDebug));
    return nav;
}

void CachedColor::Debug::print() const
{
    CachedColor* b = base();
    DEBUG_PRINT_COLOR(mFillColor);
    DUMP_NAV_LOGD("// int mInnerWidth=%d;\n", b->mInnerWidth);
    DUMP_NAV_LOGD("// int mOuterWidth=%d;\n", b->mOuterWidth);
    DUMP_NAV_LOGD("// int mOutset=%d;\n", b->mOutset);
    DEBUG_PRINT_COLOR(mPressedInnerColor);
    DEBUG_PRINT_COLOR(mPressedOuterColor);
    DUMP_NAV_LOGD("// int mRadius=%d;\n", b->mRadius);
    DEBUG_PRINT_COLOR(mSelectedInnerColor);
    DEBUG_PRINT_COLOR(mSelectedOuterColor);
}

#endif

}

