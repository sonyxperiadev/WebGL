/*
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

#ifndef CachedDebug_H
#define CachedDebug_H

#ifndef DUMP_NAV_CACHE
#ifdef NDEBUG
#define DUMP_NAV_CACHE 0
#else
#define DUMP_NAV_CACHE 1
#endif
#endif

#ifndef DEBUG_NAV_UI
#ifdef NDEBUG
#define DEBUG_NAV_UI 0
#else
#define DEBUG_NAV_UI 1
#endif
#endif

#if DEBUG_NAV_UI
#define DBG_NAV_LOGD_NO_PARAM   0   /* needed so we can call (format, ...) */
#define DBG_NAV_LOG_THROTTLE_INTERVAL   1000
#define DBG_NAV_LOG(message) LOGD("%s %s", __FUNCTION__, message)
#define DBG_NAV_LOGD(format, ...) LOGD("%s " format, __FUNCTION__, __VA_ARGS__)
#define DBG_NAV_LOGD_THROTTLE(format, ...) \
do { \
    static uint32_t gPrevLogTime = 0; \
    uint32_t curTime = SkTime::GetMSecs(); \
    if (curTime - gPrevLogTime > DBG_NAV_LOG_THROTTLE_INTERVAL) { \
        LOGD("%s " format, __FUNCTION__, __VA_ARGS__); \
        gPrevLogTime = curTime; \
    } \
} while (false)
#define DEBUG_NAV_UI_LOGD(...) LOGD(__VA_ARGS__)
#else
#define DBG_NAV_LOG(message) ((void)0)
#define DBG_NAV_LOGD(format, ...) ((void)0)
#define DBG_NAV_LOGD_THROTTLE(format, ...) ((void)0)
#define DEBUG_NAV_UI_LOGD(...) ((void)0)
#endif

#if DUMP_NAV_CACHE != 0 && !defined DUMP_NAV_CACHE_USING_PRINTF && defined NDEBUG
#define DUMP_NAV_CACHE_USING_PRINTF
#endif

#if DUMP_NAV_CACHE
#ifdef DUMP_NAV_CACHE_USING_PRINTF
#include <stdio.h>
extern FILE* gNavCacheLogFile;
#define NAV_CACHE_LOG_FILE "/data/data/com.android.browser/navlog"
#define DUMP_NAV_LOGD(...) do { if (gNavCacheLogFile) \
    fprintf(gNavCacheLogFile, __VA_ARGS__); else LOGD(__VA_ARGS__); } while (false)
#else
#define DUMP_NAV_LOGD(...) LOGD(__VA_ARGS__)
#endif
#else
#define DUMP_NAV_LOGD(...) ((void)0)
#endif

#endif
