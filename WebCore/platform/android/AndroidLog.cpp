/* 
 **
 ** Copyright 2008, The Android Open Source Project
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
#include "AndroidLog.h"
#include <stdio.h>

#define LOG_TAG "WebCore"
#undef LOG
#include <utils/Log.h>

namespace WebCore {

#define BUF_SIZE    1024

void ANDROID_LOGD(const char *fmt, ...) {
    va_list ap;
    char buf[BUF_SIZE];

    va_start(ap, fmt);
    vsnprintf(buf, BUF_SIZE, fmt, ap);
    va_end(ap);

    LOGD(buf);
}

void ANDROID_LOGW(const char *fmt, ...) {
    va_list ap;
    char buf[BUF_SIZE];

    va_start(ap, fmt);
    vsnprintf(buf, BUF_SIZE, fmt, ap);
    va_end(ap);

    LOGW(buf);
}

void ANDROID_LOGV(const char *fmt, ...) {
    va_list ap;
    char buf[BUF_SIZE];

    va_start(ap, fmt);
    vsnprintf(buf, BUF_SIZE, fmt, ap);
    va_end(ap);

    LOGV(buf);
}
}
