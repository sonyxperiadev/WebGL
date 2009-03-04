/*
 * Copyright 2008, The Android Open Source Project
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

#ifndef PLUGIN_DEBUG_ANDROID_H__
#define PLUGIN_DEBUG_ANDROID_H__

#ifdef ANDROID_PLUGINS

// Define PLUGIN_DEBUG_LOCAL in an individual C++ file to enable for
// that file only.

// Define PLUGIN_DEBUG_GLOBAL to 1 to turn plug-in debug for all
// Android plug-in code in this direectory.
#define PLUGIN_DEBUG_GLOBAL     0

#if PLUGIN_DEBUG_GLOBAL || defined(PLUGIN_DEBUG_LOCAL)
# define PLUGIN_LOG(A, B...)    do { LOGI( A , ## B ); } while(0)
#else
# define PLUGIN_LOG(A, B...)    do { } while(0)
#endif

#endif

#endif // defined(PLUGIN_DEBUG_ANDROID_H__)
