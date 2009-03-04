##
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
## PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
## CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
## EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
## PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
## PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
## OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	Intercept.cpp \
	MyJavaVM.cpp \
	main.cpp

WEBCORE := external/webkit/WebCore
WEBKIT := external/webkit/WebKit
JSC := external/webkit/JavaScriptCore

LOCAL_CFLAGS += -include "WebCorePrefixAndroid.h"

LOCAL_C_INCLUDES := \
	$(JNI_H_INCLUDE) \
	external/webkit \
	$(WEBCORE) \
	$(WEBCORE)/page \
	$(WEBCORE)/platform \
	$(WEBCORE)/platform/graphics \
	$(WEBCORE)/platform/network \
	$(WEBCORE)/platform/text \
	$(WEBCORE)/rendering \
	$(WEBKIT) \
	$(WEBKIT)/android/stl \
	$(JSC) \
	external/icu4c/common \
	external/libxml2/include \
	external/skia/include/effects \
	external/skia/include/images \
	external/skia/include/ports \
	external/skia/include/utils \
	external/skia/src/ports \
	external/sqlite/dist \
	$(WEBCORE)/bindings/js \
	$(WEBCORE)/bridge \
	$(WEBCORE)/bridge/c \
	$(WEBCORE)/bridge/jni \
	$(WEBCORE)/css \
	$(WEBCORE)/dom \
	$(WEBCORE)/editing \
	$(WEBCORE)/history \
	$(WEBCORE)/html \
	$(WEBCORE)/inspector \
	$(WEBCORE)/loader \
	$(WEBCORE)/loader/appcache \
	$(WEBCORE)/loader/icon \
	$(WEBCORE)/page/android \
	$(WEBCORE)/page/animation \
	$(WEBCORE)/platform/android \
	$(WEBCORE)/platform/graphics/android \
	$(WEBCORE)/platform/graphics/network \
	$(WEBCORE)/platform/image-decoders \
	$(WEBCORE)/platform/network/android \
	$(WEBCORE)/platform/sql \
	$(WEBCORE)/plugins \
	$(WEBCORE)/rendering/style \
	$(WEBCORE)/storage \
	$(WEBCORE)/xml \
	$(WEBKIT)/android \
	$(WEBKIT)/android/jni \
	$(WEBKIT)/android/nav \
	$(WEBKIT)/android/plugins \
	$(WEBKIT)/android/WebCoreSupport \
	$(JSC)/API \
	$(JSC)/VM \
	$(JSC)/debugger \
	$(JSC)/kjs \
	$(JSC)/icu \
	$(JSC)/pcre \
	$(JSC)/profiler \
	$(JSC)/runtime \
	$(JSC)/wtf \
	$(JSC)/wtf/unicode \
	$(JSC)/wtf/unicode/icu \
	$(JSC)/ForwardingHeaders \
	$(call include-path-for, corecg graphics)

LOCAL_SHARED_LIBRARIES := libwebcore

LOCAL_MODULE:= webcore_test

include $(BUILD_EXECUTABLE)

