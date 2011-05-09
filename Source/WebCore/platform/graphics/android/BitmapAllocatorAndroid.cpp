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

#include "config.h"
#include "BitmapAllocatorAndroid.h"
#include "SharedBufferStream.h"
#include "SkImageRef_GlobalPool.h"
#include "SkImageRef_ashmem.h"

// made this up, so we don't waste a file-descriptor on small images, plus
// we don't want to lose too much on the round-up to a page size (4K)
#define MIN_ASHMEM_ALLOC_SIZE   (32*1024)


static bool should_use_ashmem(const SkBitmap& bm) {
    return bm.getSize() >= MIN_ASHMEM_ALLOC_SIZE;
}

///////////////////////////////////////////////////////////////////////////////

namespace WebCore {

BitmapAllocatorAndroid::BitmapAllocatorAndroid(SharedBuffer* data,
                                               int sampleSize)
{
    fStream = new SharedBufferStream(data);
    fSampleSize = sampleSize;
}

BitmapAllocatorAndroid::~BitmapAllocatorAndroid()
{
    fStream->unref();
}

bool BitmapAllocatorAndroid::allocPixelRef(SkBitmap* bitmap, SkColorTable*)
{
    SkPixelRef* ref;
    if (should_use_ashmem(*bitmap)) {
//        SkDebugf("ashmem [%d %d]\n", bitmap->width(), bitmap->height());
        ref = new SkImageRef_ashmem(fStream, bitmap->config(), fSampleSize);
    } else {
//        SkDebugf("globalpool [%d %d]\n", bitmap->width(), bitmap->height());
        ref = new SkImageRef_GlobalPool(fStream, bitmap->config(), fSampleSize);
    }
    bitmap->setPixelRef(ref)->unref();
    return true;
}

}
