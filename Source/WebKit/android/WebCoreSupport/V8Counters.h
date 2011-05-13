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

#ifndef V8Counters_h
#define V8Counters_h

#if USE(V8)

#ifdef ANDROID_INSTRUMENT

#include <PlatformString.h>
#include <v8.h>
#include <wtf/HashMap.h>

namespace WebCore {

class V8Counters {
public:
    // Counter callbacks, see v8.h
    static int* counterForName(const char* name);

    static void* createHistogram(const char* name,
                                 int min,
                                 int max,
                                 size_t buckets);

    static void addHistogramSample(void* histogram, int sample);

    static void initCounters();
    static void dumpCounters();
private:
    class Counter {
    public:
        Counter(bool isHistogram);

        int count() { return m_count; }
        int sampleTotal() { return m_sampleTotal; }
        bool isHistogram() { return m_isHistogram; }
        void addSample(int32_t sample);

        operator int*() { return &m_count; }
    private:
        int m_count;
        int m_sampleTotal;
        bool m_isHistogram;
    };

    static HashMap<String, Counter*> m_counters;
};

}

#endif // ANDROID_INSTRUMENT
#endif // USE(V8)
#endif // V8Counters_h
