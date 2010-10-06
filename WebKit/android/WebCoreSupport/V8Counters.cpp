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


#ifdef ANDROID_INSTRUMENT

#define LOG_TAG "WebCore"

#include "config.h"
#include "V8Counters.h"

#include "NotImplemented.h"
#include <utils/Log.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

#if USE(V8)

namespace WebCore {

V8Counters::Counter::Counter(bool isHistogram)
    : m_count(0), m_sampleTotal(0), m_isHistogram(isHistogram) { }

void V8Counters::Counter::addSample(int sample)
{
  m_count++;
  m_sampleTotal += sample;
}

HashMap<String, V8Counters::Counter*> V8Counters::m_counters;

// static
int* V8Counters::counterForName(const char* name)
{
    Counter* counter = m_counters.get(name);
    if (!counter) {
        counter = new Counter(false);
        m_counters.add(name, counter);
    }
    return *counter;
}

// static
void* V8Counters::createHistogram(const char* name, int min, int max,
        size_t buckets)
{
    Counter* counter = new Counter(true);
    m_counters.add(name, counter);
    return counter;
}

// static
void V8Counters::addHistogramSample(void* histogram, int sample)
{
    Counter* counter = reinterpret_cast<Counter*>(histogram);
    counter->addSample(sample);
}

// static
void V8Counters::initCounters()
{
    static bool isInitialized = false;
    if (!isInitialized) {
        v8::V8::SetCounterFunction(counterForName);
        v8::V8::SetCreateHistogramFunction(createHistogram);
        v8::V8::SetAddHistogramSampleFunction(addHistogramSample);
        isInitialized = true;
    }
}

// static
void V8Counters::dumpCounters()
{
    LOGD("+----------------------------------------+-------------+\n");
    LOGD("| Name                                   | Value       |\n");
    LOGD("+----------------------------------------+-------------+\n");
    typedef HashMap<String, V8Counters::Counter*>::iterator CounterIterator;
    for (CounterIterator iter = m_counters.begin(); iter != m_counters.end(); ++iter) {
        Counter* counter = iter->second;
        if (counter->isHistogram()) {
            LOGD("| c:%-36s | %11i |\n", iter->first.latin1().data(), counter->count());
            LOGD("| t:%-36s | %11i |\n", iter->first.latin1().data(), counter->sampleTotal());
          } else {
            LOGD("| %-38s | %11i |\n", iter->first.latin1().data(), counter->count());
          }
    }
    LOGD("+----------------------------------------+-------------+\n");
}

}

#endif // ANDROID_INSTRUMENT

#endif // USE(V8)
