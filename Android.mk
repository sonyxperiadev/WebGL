##
## Copyright 2009, The Android Open Source Project
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
##  * Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
##  * Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
## EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
## PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
## CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
## EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
## PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
## PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
## OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##

LOCAL_PATH := $(call my-dir)

# Two ways to control which JS engine is used:
# 1. use JS_ENGINE environment variable, value can be either 'jsc' or 'v8'
#    This is the preferred way.
# 2. if JS_ENGINE is not set, or is not 'jsc' or 'v8', this makefile picks
#    up a default engine to build.
#    To help setup buildbot, a new environment variable, USE_ALT_JS_ENGINE,
#    can be set to true, so that two builds can be different but without
#    specifying which JS engine to use.

# Check JS_ENGINE environment variable
ifeq ($(JS_ENGINE),jsc)
  include $(LOCAL_PATH)/Android.jsc.mk
else
  ifeq ($(JS_ENGINE),v8)
    include $(LOCAL_PATH)/Android.v8.mk
  else
    # No JS engine is specified, pickup the one we want as default.
    ifeq ($(USE_ALT_JS_ENGINE),true)
      include $(LOCAL_PATH)/Android.v8.mk
    else
      include $(LOCAL_PATH)/Android.jsc.mk
    endif  # USE_ALT_JS_ENGINE == true
  endif  # JS_ENGINE == v8
endif # JS_ENGINE == jsc
