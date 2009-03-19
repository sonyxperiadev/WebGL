/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

