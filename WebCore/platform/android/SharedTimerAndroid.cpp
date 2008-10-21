/* 
**
** Copyright 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "config.h"
#include "SharedTimer.h"
#include "SystemTime.h"
#include "JavaSharedClient.h"
#include "TimerClient.h"
#define LOG_TAG "Timers"
#undef LOG
#include "utils/Log.h"

namespace WebCore {

    // Single timer, shared to implement all the timers managed by the Timer class.
    // Not intended to be used directly; use the Timer class instead.

    void setSharedTimerFiredFunction(void (*f)())
    {
        if (JavaSharedClient::GetTimerClient())
            JavaSharedClient::GetTimerClient()->setSharedTimerCallback(f);
    }

    // The fire time is relative to the classic POSIX epoch of January 1, 1970,
    // as the result of currentTime() is.

    void setSharedTimerFireTime(double fireTime)
    {
        long long timeInMS = (long long)((fireTime - currentTime()) * 1000);
    
        LOGV("setSharedTimerFireTime: in %ld millisec", timeInMS);
        if (JavaSharedClient::GetTimerClient())
            JavaSharedClient::GetTimerClient()->setSharedTimer(timeInMS);
    }
    
    void stopSharedTimer()
    {
        if (JavaSharedClient::GetTimerClient())
            JavaSharedClient::GetTimerClient()->stopSharedTimer();
    }

}

