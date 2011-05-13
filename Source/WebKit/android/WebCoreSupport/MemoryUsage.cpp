/*
 * Copyright 2010 The Android Open Source Project
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
#include "MemoryUsage.h"

#include <malloc.h>
#include <wtf/CurrentTime.h>

#if USE(V8)
#include <v8.h>
#endif // USE(V8)

using namespace WTF;

class MemoryUsageCache {
public:
    MemoryUsageCache()
        : m_cachedMemoryUsage(0)
        , m_cacheTime(0)
    {
    }

    int getCachedMemoryUsage(bool forceFresh);

private:
    unsigned m_cachedMemoryUsage;
    double m_cacheTime;
    static const int CACHE_VALIDITY_MS = 2000;
};

int MemoryUsageCache::getCachedMemoryUsage(bool forceFresh)
{
    if (!forceFresh && currentTimeMS() <= m_cacheTime + CACHE_VALIDITY_MS)
        return m_cachedMemoryUsage;

    struct mallinfo minfo = mallinfo();
    m_cachedMemoryUsage = (minfo.hblkhd + minfo.arena) >> 20;

#if USE(V8)
    v8::HeapStatistics stat;
    v8::V8::GetHeapStatistics(&stat);
    unsigned v8Usage = stat.total_heap_size() >> 20;
    m_cachedMemoryUsage += v8Usage;
#endif // USE(V8)

    m_cacheTime = currentTimeMS();
    return m_cachedMemoryUsage;
}

int MemoryUsage::memoryUsageMb(bool forceFresh)
{
    static MemoryUsageCache cache;
    return cache.getCachedMemoryUsage(forceFresh);
}

int MemoryUsage::m_lowMemoryUsageMb = 0;
int MemoryUsage::m_highMemoryUsageMb = 0;
int MemoryUsage::m_highUsageDeltaMb = 0;
