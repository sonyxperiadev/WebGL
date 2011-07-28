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

#ifndef ChromiumIncludes_h
#define ChromiumIncludes_h

#include "config.h"

// Include all external/chromium files in this file so the problems with the LOG
// and LOG_ASSERT defines can be handled in one place.

// Undefine LOG and LOG_ASSERT before including chrome code, and if they were
// defined attempt to set the macros to the Android logging macros (which are
// the only ones that actually log).

#ifdef LOG
#define LOG_WAS_DEFINED LOG
#undef LOG
#endif

#ifdef LOG_ASSERT
#define LOG_ASSERT_WAS_DEFINED LOG_ASSERT
#undef LOG_ASSERT
#endif

#include <android/net/android_network_library_impl.h>
#include <android/jni/jni_utils.h>
#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop_proxy.h>
#include <base/openssl_util.h>
#include <base/string_util.h>
#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>
#include <base/sys_string_conversions.h>
#include <base/threading/thread.h>
#include <base/time.h>
#include <base/tuple.h>
#include <base/utf_string_conversions.h>
#include <chrome/browser/net/sqlite_persistent_cookie_store.h>
#include <net/base/auth.h>
#include <net/base/cert_verifier.h>
#include <net/base/cookie_monster.h>
#include <net/base/cookie_policy.h>
#include <net/base/data_url.h>
#include <net/base/host_resolver.h>
#include <net/base/io_buffer.h>
#include <net/base/load_flags.h>
#include <net/base/net_errors.h>
#include <net/base/mime_util.h>
#include <net/base/net_util.h>
#include <net/base/openssl_private_key_store.h>
#include <net/base/ssl_cert_request_info.h>
#include <net/base/ssl_config_service.h>
#include <net/disk_cache/disk_cache.h>
#include <net/http/http_auth_handler_factory.h>
#include <net/http/http_cache.h>
#include <net/http/http_network_layer.h>
#include <net/http/http_response_headers.h>
#include <net/proxy/proxy_config_service_android.h>
#include <net/proxy/proxy_service.h>
#include <net/url_request/url_request.h>
#include <net/url_request/url_request_context.h>

#if ENABLE(WEB_AUTOFILL)
#include <autofill/autofill_manager.h>
#include <autofill/autofill_profile.h>
#include <autofill/personal_data_manager.h>
#include <base/logging.h>
#include <base/memory/scoped_vector.h>
#include <base/string16.h>
#include <base/utf_string_conversions.h>
#include <chrome/browser/autofill/autofill_host.h>
#include <chrome/browser/profiles/profile.h>
#include <content/browser/tab_contents/tab_contents.h>
#include <webkit/glue/form_data.h>
#include <webkit/glue/form_field.h>
#endif

#undef LOG
#if defined(LOG_WAS_DEFINED) && defined(LOG_PRI)
#define LOG(priority, tag, ...) LOG_PRI(ANDROID_##priority, tag, __VA_ARGS__)
#endif

#undef LOG_ASSERT
#if defined(LOG_ASSERT_WAS_DEFINED) && defined(LOG_FATAL_IF)
#define LOG_ASSERT(cond, ...) LOG_FATAL_IF(!(cond), ## __VA_ARGS__)
#endif

#endif
