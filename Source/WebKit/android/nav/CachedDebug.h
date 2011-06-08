/*
 * Copyright 2007, The Android Open Source Project
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

#ifndef CachedDebug_h
#define CachedDebug_h

#define DUMP_NAV_CACHE 0
#define DEBUG_NAV_UI 0
#define DEBUG_NAV_UI_VERBOSE 0

#if DEBUG_NAV_UI
#define DBG_NAV_LOG(message) LOGD("%s %s", __FUNCTION__, message)
#define DBG_NAV_LOGD(format, ...) LOGD("%s " format, __FUNCTION__, __VA_ARGS__)
#define DEBUG_NAV_UI_LOGD(...) LOGD(__VA_ARGS__)
#else
#define DBG_NAV_LOG(message) ((void)0)
#define DBG_NAV_LOGD(format, ...) ((void)0)
#define DEBUG_NAV_UI_LOGD(...) ((void)0)
#endif

#if DEBUG_NAV_UI_VERBOSE
#define DBG_NAV_LOGV(format, ...) LOGD("%s " format, __FUNCTION__, __VA_ARGS__)
#else
#define DBG_NAV_LOGV(format, ...) ((void)0)
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
#define DUMP_NAV_LOGX(format, ...) do { if (gNavCacheLogFile) \
    fprintf(gNavCacheLogFile, format, __VA_ARGS__); \
    else LOGD("%s " format, __FUNCTION__, __VA_ARGS__); } while (false)
#else
#define DUMP_NAV_LOGD(...) LOGD(__VA_ARGS__)
#define DUMP_NAV_LOGX(format, ...) LOGD("%s " format, __FUNCTION__, __VA_ARGS__)
#endif
#else
#define DUMP_NAV_LOGD(...) ((void)0)
#define DUMP_NAV_LOGX(...) ((void)0)
#endif

#endif
