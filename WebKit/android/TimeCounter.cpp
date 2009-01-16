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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "WebCore"

#include "config.h"
#include "TimeCounter.h"

#include "CString.h"
#include "Cache.h"
#include "KURL.h"
#include "SystemTime.h"
#include <runtime/JSGlobalObject.h>
#include <utils/Log.h>

using namespace WebCore;

namespace android {

#ifdef ANDROID_INSTRUMENT

static double sStartTotalTime;
static uint32_t sStartThreadTime;

uint32_t TimeCounter::sTotalTimeUsed[TimeCounter::TotalTimeCounterCount];
uint32_t TimeCounter::sCounter[TimeCounter::TotalTimeCounterCount];
uint32_t TimeCounter::sStartTime[TimeCounter::TotalTimeCounterCount];

static const char* timeCounterNames[] = {
    "calculate style", 
    "css parsing", 
    "Java callback (frame bridge)",
    "layout", 
    "native 1 (frame bridge)",
 //   "paint", 
    "parsing (may include calcStyle or Java callback)", 
    "native 3 (resource load)", 
    "native 2 (shared timer)", 
    "native 4 (webview core)"
};

void TimeCounter::record(enum Type type, const char* functionName)
{
    recordNoCounter(type, functionName);
    sCounter[type]++;
}

void TimeCounter::recordNoCounter(enum Type type, const char* functionName)
{
    uint32_t elapsed = get_thread_msec() - sStartTime[type];
    sTotalTimeUsed[type] += elapsed;
    if (elapsed > 1000)
        LOGW("***** %s() used %d ms\n", functionName, elapsed);
}

void TimeCounter::report(const KURL& url, int live, int dead)
{
    String urlString = url;
    int totalTime = static_cast<int>((currentTime() - sStartTotalTime) * 1000);
    int threadTime = get_thread_msec() - sStartThreadTime;
    LOGD("*-* Total load time: %d ms, thread time: %d ms for %s\n",
            totalTime, threadTime, urlString.utf8().data());
// FIXME: JSGlobalObject no longer records time
//    JSC::JSGlobalObject::reportTimeCounter();
    for (Type type = (Type) 0; type < TotalTimeCounterCount; type = (Type) (type + 1)) {
        char scratch[256];
        int index = sprintf(scratch, "*-* Total %s time: %d ms", 
            timeCounterNames[type], sTotalTimeUsed[type]);
        if (sCounter[type] > 0)
            sprintf(&scratch[index], " called %d times", sCounter[type]);
        LOGD("%s", scratch);
    }
    LOGD("Current cache has %d bytes live and %d bytes dead", live, dead);
}

void TimeCounter::reset() {
// FIXME: JSGlobalObject no longer records time
//    JSC::JSGlobalObject::resetTimeCounter();
    bzero(sTotalTimeUsed, sizeof(sTotalTimeUsed));
    bzero(sCounter, sizeof(sCounter));
    LOGD("*-* Start browser instrument\n");
    sStartTotalTime = currentTime();
    sStartThreadTime = get_thread_msec();
}

void TimeCounter::start(enum Type type)
{
    sStartTime[type] = get_thread_msec();
}


#endif

}
