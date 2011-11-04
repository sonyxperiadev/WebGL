/*
 * Copyright 2011, The Android Open Source Project
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

#include "PerformanceMonitor.h"

#include <wtf/text/CString.h>

#include <wtf/CurrentTime.h>
#include <cutils/log.h>
#include <wtf/text/CString.h>
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "PerformanceMonitor", __VA_ARGS__)

namespace WebCore {

PerformanceMonitor::PerformanceMonitor()
{
}

PerformanceMonitor::~PerformanceMonitor()
{
}

void PerformanceMonitor::start(const String &tag)
{
    if (tag.isEmpty())
        return;
    PerfItem *item;
    if (m_tags.contains(tag))
        item = m_tags.get(tag);
    else {
        item = new PerfItem();
        m_tags.set(tag, item);
    }
    gettimeofday(&(item->start_time), NULL);
}

void PerformanceMonitor::stop(const String &tag)
{
    if (!m_tags.contains(tag))
        return;
    PerfItem *item = m_tags.get(tag);
    struct timeval end;
    gettimeofday(&end, NULL);
    long seconds, useconds;
    seconds  = end.tv_sec  - item->start_time.tv_sec;
    useconds = end.tv_usec - item->start_time.tv_usec;

    float mtime = (seconds * 1000.0) + (useconds/1000.0);

    float avg = 0;
    if (item->average_ms) {
        item->average_ms = (item->average_ms + mtime) / 2;
    } else
        item->average_ms = mtime;
}

float PerformanceMonitor::getAverageDuration(const String &tag)
{
    if (tag.isEmpty() || !m_tags.contains(tag))
        return 0;
    return m_tags.get(tag)->average_ms;
}

void PerformanceMonitor::display(int limit)
{
    bool shown = false;
    HashMap<String, PerfItem*, StringHash>::iterator end = m_tags.end();
    for (HashMap<String, PerfItem*, StringHash>::iterator it = m_tags.begin(); it != end; ++it) {
        PerfItem* item = it->second;
        if (item->average_ms > limit) {
           if (!shown) {
                XLOGC("=== DISPLAY MONITOR ====");
                shown = true;
           }
           XLOGC("item %s took longer than %d ms: %.2f", it->first.latin1().data(), limit, item->average_ms);
        }
    }
    if (shown)
        XLOGC("=== END DISPLAY MONITOR ====");
}

} // namespace WebCore
