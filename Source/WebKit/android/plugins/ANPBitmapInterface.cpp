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

// must include config.h first for webkit to fiddle with new/delete
#include "config.h"
#include "SkANP.h"
#include "SkColorPriv.h"

static bool anp_getPixelPacking(ANPBitmapFormat fmt, ANPPixelPacking* packing) {
    switch (fmt) {
        case kRGBA_8888_ANPBitmapFormat:
            if (packing) {
                packing->AShift = SK_A32_SHIFT;
                packing->ABits  = SK_A32_BITS;
                packing->RShift = SK_R32_SHIFT;
                packing->RBits  = SK_R32_BITS;
                packing->GShift = SK_G32_SHIFT;
                packing->GBits  = SK_G32_BITS;
                packing->BShift = SK_B32_SHIFT;
                packing->BBits  = SK_B32_BITS;
            }
            return true;
        case kRGB_565_ANPBitmapFormat:
            if (packing) {
                packing->AShift = 0;
                packing->ABits  = 0;
                packing->RShift = SK_R16_SHIFT;
                packing->RBits  = SK_R16_BITS;
                packing->GShift = SK_G16_SHIFT;
                packing->GBits  = SK_G16_BITS;
                packing->BShift = SK_B16_SHIFT;
                packing->BBits  = SK_B16_BITS;
            }
            return true;
        default:
            break;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void ANPBitmapInterfaceV0_Init(ANPInterface* value) {
    ANPBitmapInterfaceV0* i = reinterpret_cast<ANPBitmapInterfaceV0*>(value);

    ASSIGN(i, getPixelPacking);
}
