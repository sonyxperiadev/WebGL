/*
 * Copyright (C) 2008 The Android Open Source Project
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

#define LOG_TAG "plugin"

#include "utils/Log.h"
#include "android_npapi.h"
#include <stdarg.h>

static void anp_log(NPP inst, ANPLogType logType, const char format[], ...) {
    va_list args;
    va_start(args, format);

    android_LogPriority priority;
    switch (logType) {
        case kError_ANPLogType:
            priority = ANDROID_LOG_ERROR;
            break;
        case kWarning_ANPLogType:
            priority = ANDROID_LOG_WARN;
            break;
        case kDebug_ANPLogType:
            priority = ANDROID_LOG_DEBUG;
            break;
        default:
            priority = ANDROID_LOG_UNKNOWN;
            break;
    }
    LOG_PRI_VA(priority, "plugin", format, args);

    va_end(args);
}

void ANPLogInterfaceV0_Init(ANPInterface* value) {
    ANPLogInterfaceV0* i = reinterpret_cast<ANPLogInterfaceV0*>(value);

    i->log = anp_log;
}

