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

# Control SVG compiling in webkit.
# Default is true unless explictly disabled.
ifneq ($(ENABLE_SVG),false)
    ENABLE_SVG = true
endif

# Control complex scripts support compiling in webkit.
# Default is true unless explictly disabled.
ifneq ($(SUPPORT_COMPLEX_SCRIPTS),false)
    SUPPORT_COMPLEX_SCRIPTS = true
endif

# Two ways to control which JS engine is used:
# 1. use JS_ENGINE environment variable, value can be either 'jsc' or 'v8'
#    This is the preferred way.
# 2. if JS_ENGINE is not set, or is not 'jsc' or 'v8', this makefile picks
#    up a default engine to build.
#    To help setup buildbot, a new environment variable, USE_ALT_JS_ENGINE,
#    can be set to true, so that two builds can be different but without
#    specifying which JS engine to use.
# To enable JIT in Android's JSC, please set ENABLE_JSC_JIT environment
# variable to true.

# To enable JIT in Android's JSC, please set ENABLE_JSC_JIT environment
# variable to true.

# Read JS_ENGINE environment variable
JAVASCRIPT_ENGINE = $(JS_ENGINE)

# We default to the V8 JS engine.
DEFAULT_ENGINE = v8
ALT_ENGINE = jsc

ifneq ($(JAVASCRIPT_ENGINE),jsc)
  ifneq ($(JAVASCRIPT_ENGINE),v8)
    # No JS engine is specified, pickup the one we want as default.
    ifeq ($(USE_ALT_JS_ENGINE),true)
      JAVASCRIPT_ENGINE = $(ALT_ENGINE)
    else
      JAVASCRIPT_ENGINE = $(DEFAULT_ENGINE)
    endif
  endif
endif
# We can't use V8 on the simulator
ifeq ($(TARGET_SIMULATOR),true)
  JAVASCRIPT_ENGINE = jsc
endif

# V8 also requires an ARMv7 CPU, and since we must use jsc, we cannot
# use the Chrome http stack either.
ifneq ($(strip $(ARCH_ARM_HAVE_ARMV7A)),true)
  JAVASCRIPT_ENGINE := jsc
  USE_ALT_HTTP := true
endif

# See if the user has specified a stack they want to use
HTTP_STACK = $(HTTP)
# We default to the Chrome HTTP stack.
DEFAULT_HTTP = chrome
ALT_HTTP = android

ifneq ($(HTTP_STACK),chrome)
  ifneq ($(HTTP_STACK),android)
    # No HTTP stack is specified, pickup the one we want as default.
    ifeq ($(USE_ALT_HTTP),true)
      HTTP_STACK = $(ALT_HTTP)
    else
      HTTP_STACK = $(DEFAULT_HTTP)
    endif
  endif
endif

# The Chrome stack can not be used with JSC and hence can not be used be used
# with the simulator.
ifeq ($(JAVASCRIPT_ENGINE),jsc)
  ifeq ($(HTTP_STACK),chrome)
    $(error Can not build with JSC and the Chrome HTTP stack)
  endif
endif

# Read the environment variable to determine if Autofill is compiled.
# The default is on. Chrome HTTP stack must be used when Autofill
# is turned on.
ifneq ($(ENABLE_AUTOFILL),false)
  ENABLE_AUTOFILL = true
endif
ifneq ($(HTTP_STACK),chrome)
  ENABLE_AUTOFILL = false
endif

BASE_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Define our module and find the intermediates directory
LOCAL_MODULE := libwebcore
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
base_intermediates := $(call local-intermediates-dir)

# Using := here prevents recursive expansion
WEBKIT_SRC_FILES :=

# We have to use bison 2.3
include $(BASE_PATH)/bison_check.mk

# Build our list of include paths. We include WebKit/android/icu first so that
# any files that include <unicode/ucnv.h> will include our ucnv.h first. We
# also add external/ as an include directory so that we can specify the real
# icu header directory as a more exact reference to avoid including our ucnv.h.
#
# Note that JavasCriptCore/ must be included after WebCore/, so that we pick up
# the right config.h.
LOCAL_C_INCLUDES := \
	$(JNI_H_INCLUDE) \
	$(LOCAL_PATH)/WebKit/android/icu \
	external/ \
	external/icu4c/common \
	external/icu4c/i18n \
	external/libxml2/include \
	external/libxslt \
	external/hyphenation \
	external/skia/emoji \
	external/skia/include/core \
	external/skia/include/effects \
	external/skia/include/images \
	external/skia/include/ports \
	external/skia/include/utils \
	external/skia/src/ports \
	external/sqlite/dist \
	frameworks/base/core/jni/android/graphics \
	frameworks/base/include

LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) \
	$(LOCAL_PATH)/WebCore \
	$(LOCAL_PATH)/WebCore/accessibility \
	$(LOCAL_PATH)/WebCore/bindings/ \
	$(LOCAL_PATH)/WebCore/bindings/generic \
	$(LOCAL_PATH)/WebCore/css \
	$(LOCAL_PATH)/WebCore/dom \
	$(LOCAL_PATH)/WebCore/editing \
	$(LOCAL_PATH)/WebCore/fileapi \
	$(LOCAL_PATH)/WebCore/history \
	$(LOCAL_PATH)/WebCore/history/android \
	$(LOCAL_PATH)/WebCore/html \
	$(LOCAL_PATH)/WebCore/html/canvas \
	$(LOCAL_PATH)/WebCore/html/parser \
	$(LOCAL_PATH)/WebCore/inspector \
	$(LOCAL_PATH)/WebCore/loader \
	$(LOCAL_PATH)/WebCore/loader/appcache \
	$(LOCAL_PATH)/WebCore/loader/archive \
	$(LOCAL_PATH)/WebCore/loader/archive/android \
	$(LOCAL_PATH)/WebCore/loader/cache \
	$(LOCAL_PATH)/WebCore/loader/icon \
	$(LOCAL_PATH)/WebCore/notifications \
	$(LOCAL_PATH)/WebCore/page \
	$(LOCAL_PATH)/WebCore/page/android \
	$(LOCAL_PATH)/WebCore/page/animation \
	$(LOCAL_PATH)/WebCore/platform \
	$(LOCAL_PATH)/WebCore/platform/android \
	$(LOCAL_PATH)/WebCore/platform/animation \
	$(LOCAL_PATH)/WebCore/platform/graphics \
	$(LOCAL_PATH)/WebCore/platform/graphics/android \
	$(LOCAL_PATH)/WebCore/platform/graphics/filters \
	$(LOCAL_PATH)/WebCore/platform/graphics/gpu \
	$(LOCAL_PATH)/WebCore/platform/graphics/network \
	$(LOCAL_PATH)/WebCore/platform/graphics/skia \
	$(LOCAL_PATH)/WebCore/platform/graphics/transforms \
	$(LOCAL_PATH)/WebCore/platform/image-decoders \
	$(LOCAL_PATH)/WebCore/platform/image-decoders/bmp \
	$(LOCAL_PATH)/WebCore/platform/image-decoders/gif \
	$(LOCAL_PATH)/WebCore/platform/image-decoders/ico \
	$(LOCAL_PATH)/WebCore/platform/image-decoders/jpeg \
	$(LOCAL_PATH)/WebCore/platform/image-decoders/png \
	$(LOCAL_PATH)/WebCore/platform/image-decoders/webp \
	$(LOCAL_PATH)/WebCore/platform/mock \
	$(LOCAL_PATH)/WebCore/platform/network \
	$(LOCAL_PATH)/WebCore/platform/network/android \
	$(LOCAL_PATH)/WebCore/platform/sql \
	$(LOCAL_PATH)/WebCore/platform/text \
	$(LOCAL_PATH)/WebCore/platform/text/transcoder \
	$(LOCAL_PATH)/WebCore/plugins \
	$(LOCAL_PATH)/WebCore/plugins/android \
	$(LOCAL_PATH)/WebCore/rendering \
	$(LOCAL_PATH)/WebCore/rendering/style \
	$(LOCAL_PATH)/WebCore/rendering/svg \
	$(LOCAL_PATH)/WebCore/storage \
	$(LOCAL_PATH)/WebCore/svg \
	$(LOCAL_PATH)/WebCore/svg/animation \
	$(LOCAL_PATH)/WebCore/svg/graphics \
	$(LOCAL_PATH)/WebCore/svg/graphics/filters \
	$(LOCAL_PATH)/WebCore/svg/properties \
	$(LOCAL_PATH)/WebCore/websockets \
	$(LOCAL_PATH)/WebCore/workers \
	$(LOCAL_PATH)/WebCore/xml

LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) \
	$(LOCAL_PATH)/WebKit/android \
	$(LOCAL_PATH)/WebKit/android/WebCoreSupport \
	$(LOCAL_PATH)/WebKit/android/jni \
	$(LOCAL_PATH)/WebKit/android/nav \
	$(LOCAL_PATH)/WebKit/android/plugins

LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) \
	$(LOCAL_PATH)/JavaScriptCore \
	$(LOCAL_PATH)/JavaScriptCore/wtf \
	$(LOCAL_PATH)/JavaScriptCore/wtf/unicode \
	$(LOCAL_PATH)/JavaScriptCore/wtf/unicode/icu

LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) \
	$(base_intermediates)/WebCore/ \
	$(base_intermediates)/WebCore/css \
	$(base_intermediates)/WebCore/html \
	$(base_intermediates)/WebCore/platform

# The following includes are needed by the AutoFill feature, or the chrome http
# stack
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) \
	$(LOCAL_PATH)/WebKit/chromium \
	$(LOCAL_PATH)/WebKit/chromium/public \
	external/chromium/chrome/browser \
	external/chromium/chrome/renderer \
	external/chromium \
	external/chromium/chrome \
	external/skia

ifeq ($(JAVASCRIPT_ENGINE),v8)
# Include WTF source file.
d := JavaScriptCore
LOCAL_PATH := $(BASE_PATH)/$d
intermediates := $(base_intermediates)/$d
include $(LOCAL_PATH)/Android.v8.wtf.mk
WEBKIT_SRC_FILES += $(addprefix $d/,$(LOCAL_SRC_FILES))
endif  # JAVASCRIPT_ENGINE == v8

# Include source files for WebCore
d := WebCore
LOCAL_PATH := $(BASE_PATH)/$d
JAVASCRIPTCORE_PATH := $(BASE_PATH)/JavaScriptCore
intermediates := $(base_intermediates)/$d
include $(LOCAL_PATH)/Android.mk
ifeq ($(JAVASCRIPT_ENGINE),jsc)
include $(LOCAL_PATH)/Android.jscbindings.mk
endif
ifeq ($(JAVASCRIPT_ENGINE),v8)
include $(LOCAL_PATH)/Android.v8bindings.mk
endif
WEBKIT_SRC_FILES += $(addprefix $d/,$(LOCAL_SRC_FILES))
LOCAL_C_INCLUDES += $(BINDING_C_INCLUDES)

# Include the derived source files for WebCore. Uses the same path as
# WebCore
include $(LOCAL_PATH)/Android.derived.mk
ifeq ($(JAVASCRIPT_ENGINE),jsc)
include $(LOCAL_PATH)/Android.derived.jscbindings.mk
endif
ifeq ($(JAVASCRIPT_ENGINE),v8)
include $(LOCAL_PATH)/Android.derived.v8bindings.mk
endif

# Include source files for android WebKit port
d := WebKit
LOCAL_PATH := $(BASE_PATH)/$d
intermediates := $(base_intermediates)/$d
include $(LOCAL_PATH)/Android.mk
WEBKIT_SRC_FILES += $(addprefix $d/,$(LOCAL_SRC_FILES))

# Redefine LOCAL_PATH here so the build system is not confused
LOCAL_PATH := $(BASE_PATH)

# Define our compiler flags
LOCAL_CFLAGS += -Wno-endif-labels -Wno-import -Wno-format
LOCAL_CFLAGS += -fno-strict-aliasing
LOCAL_CFLAGS += -include "WebCorePrefix.h"
LOCAL_CFLAGS += -fvisibility=hidden
LOCAL_CFLAGS += -DALWAYS_INLINE=inline
# Make sure assert.h is included before assert is defined
LOCAL_CFLAGS += -include "assert.h"
ifeq ($(HTTP_STACK),chrome)
LOCAL_CFLAGS += -DGOOGLEURL
LOCAL_CFLAGS += -DWTF_USE_CHROME_NETWORK_STACK
endif # HTTP_STACK == chrome
LOCAL_CPPFLAGS := -Wno-sign-promo

# Adds GL and EGL extensions for the GL backend
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES

# Enable JSC JIT if JSC is used and ENABLE_JSC_JIT environment
# variable is set to true
ifeq ($(JAVASCRIPT_ENGINE),jsc)
ifeq ($(ENABLE_JSC_JIT),true)
LOCAL_CFLAGS += -DENABLE_ANDROID_JSC_JIT=1
endif
endif

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -Darm
# remove this warning: "note: the mangling of 'va_list' has changed in GCC 4.4"
LOCAL_CFLAGS += -Wno-psabi
endif

# need a flag to tell the C side when we're on devices with large memory
# budgets (i.e. larger than the low-end devices that initially shipped)
ifeq ($(ARCH_ARM_HAVE_VFP),true)
LOCAL_CFLAGS += -DANDROID_LARGE_MEMORY_DEVICE
endif

ifeq ($(ENABLE_SVG),true)
LOCAL_CFLAGS += -DENABLE_SVG=1 -DENABLE_SVG_ANIMATION=1
endif

ifeq ($(ENABLE_WTF_USE_ACCELERATED_COMPOSITING),false)
LOCAL_CFLAGS += -DWTF_USE_ACCELERATED_COMPOSITING=0
endif

ifeq ($(ENABLE_WTF_USE_ACCELERATED_COMPOSITING),true)
LOCAL_CFLAGS += -DWTF_USE_ACCELERATED_COMPOSITING=1
endif

ifeq ($(WEBCORE_INSTRUMENTATION),true)
LOCAL_CFLAGS += -DANDROID_INSTRUMENT
endif

# LOCAL_LDLIBS is used in simulator builds only and simulator builds are only
# valid on Linux
LOCAL_LDLIBS += -lpthread -ldl

# Build the list of shared libraries
LOCAL_SHARED_LIBRARIES := \
	libandroid_runtime \
	libnativehelper \
	libsqlite \
	libskia \
	libutils \
	libui \
	libcutils \
	libicuuc \
	libicui18n \
	libmedia \
	libsurfaceflinger_client \
	libEGL \
	libGLESv2 \
	libgui

ifeq ($(WEBCORE_INSTRUMENTATION),true)
LOCAL_SHARED_LIBRARIES += libhardware_legacy
endif

# We have to use the android version of libdl when we are not on the simulator
ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl libstlport
# We have to fake out some headers when using stlport.
LOCAL_C_INCLUDES += \
	external/chromium/android
include external/stlport/libstlport.mk
endif

# We need Harfbuzz library to support complex scripts(Arabic, Thai, Hindi...).
ifeq ($(SUPPORT_COMPLEX_SCRIPTS),true)
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) \
	external/harfbuzz/src \
	external/harfbuzz/contrib
LOCAL_SHARED_LIBRARIES += libharfbuzz
LOCAL_CFLAGS += -DSUPPORT_COMPLEX_SCRIPTS=1
endif

# Build the list of static libraries
LOCAL_STATIC_LIBRARIES := libxml2 libxslt libhyphenation
ifeq ($(JAVASCRIPT_ENGINE),v8)
LOCAL_STATIC_LIBRARIES += libv8
endif

ifeq ($(HTTP_STACK),chrome)
LOCAL_STATIC_LIBRARIES += libchromium_net
LOCAL_SHARED_LIBRARIES += libcrypto libssl libz
endif # HTTP_STACK == chrome

ifeq ($(ENABLE_AUTOFILL),true)
LOCAL_SHARED_LIBRARIES += libexpat
endif

# Redefine LOCAL_SRC_FILES to be all the WebKit source files
LOCAL_SRC_FILES := $(WEBKIT_SRC_FILES)

# Define this for use in other makefiles.
WEBKIT_C_INCLUDES := $(LOCAL_C_INCLUDES)
WEBKIT_CFLAGS := $(LOCAL_CFLAGS)
WEBKIT_CPPFLAGS := $(LOCAL_CPPFLAGS)
WEBKIT_GENERATED_SOURCES := $(LOCAL_GENERATED_SOURCES)
WEBKIT_LDLIBS := $(LOCAL_LDLIBS)
WEBKIT_SHARED_LIBRARIES := $(LOCAL_SHARED_LIBRARIES)
WEBKIT_STATIC_LIBRARIES := $(LOCAL_STATIC_LIBRARIES)

# Build the library all at once
include $(BUILD_STATIC_LIBRARY)

ifeq ($(JAVASCRIPT_ENGINE),jsc)
# Now build libjs as a static library.
include $(CLEAR_VARS)
LOCAL_MODULE := libjs
LOCAL_LDLIBS := $(WEBKIT_LDLIBS)
LOCAL_SHARED_LIBRARIES := $(WEBKIT_SHARED_LIBRARIES)
LOCAL_STATIC_LIBRARIES := $(WEBKIT_STATIC_LIBRARIES)
LOCAL_CFLAGS := $(WEBKIT_CFLAGS)
# Include source files for JavaScriptCore
d := JavaScriptCore
LOCAL_PATH := $(BASE_PATH)/$d
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
# Cannot use base_intermediates as this is a new module
intermediates := $(call local-intermediates-dir)
include $(LOCAL_PATH)/Android.mk
# Redefine LOCAL_SRC_FILES with the correct prefix
LOCAL_SRC_FILES := $(addprefix $d/,$(LOCAL_SRC_FILES))
# Use the base path to resolve file names
LOCAL_PATH := $(BASE_PATH)
# Append jsc intermediate include paths to the WebKit include list.
LOCAL_C_INCLUDES := $(WEBKIT_C_INCLUDES) \
	$(intermediates) \
	$(intermediates)/parser \
	$(intermediates)/runtime \
# Build libjs
include $(BUILD_STATIC_LIBRARY)
endif  # JAVASCRIPT_ENGINE == jsc

# Now build the shared library using only the exported jni entry point. This
# will strip out any unused code from the entry point.
include $(CLEAR_VARS)
# if you need to make webcore huge (for debugging), enable this line
#LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libwebcore
LOCAL_LDLIBS := $(WEBKIT_LDLIBS)
LOCAL_SHARED_LIBRARIES := $(WEBKIT_SHARED_LIBRARIES)
LOCAL_STATIC_LIBRARIES := libwebcore $(WEBKIT_STATIC_LIBRARIES)
ifeq ($(JAVASCRIPT_ENGINE),jsc)
LOCAL_STATIC_LIBRARIES += libjs
endif
LOCAL_LDFLAGS := -fvisibility=hidden
LOCAL_CFLAGS := $(WEBKIT_CFLAGS)
LOCAL_CPPFLAGS := $(WEBKIT_CPPFLAGS)
LOCAL_C_INCLUDES := $(WEBKIT_C_INCLUDES)
LOCAL_PATH := $(BASE_PATH)
LOCAL_SRC_FILES := \
	WebKit/android/jni/WebCoreJniOnLoad.cpp

ifeq ($(ENABLE_AUTOFILL),true)
# AutoFill requires some cpp files from Chromium to link with
# libchromium_net. We cannot compile them into libchromium_net
# because they have cpp file extensions, not .cc.
LOCAL_CFLAGS += -DWEBKIT_IMPLEMENTATION=1
LOCAL_SRC_FILES += \
	WebKit/android/WebCoreSupport/autofill/MainThreadProxy.cpp \
	WebKit/chromium/src/WebCString.cpp \
	WebKit/chromium/src/WebRegularExpression.cpp \
	WebKit/chromium/src/WebString.cpp
endif

# Do this dependency by hand. The reason we have to do this is because the
# headers that this file pulls in are generated during the build of webcore.
# We make all of our object files depend on those files so that they are built
# before we try to compile the file.
LOCAL_ADDITIONAL_DEPENDENCIES := $(filter %.h, $(WEBKIT_GENERATED_SOURCES))
include $(BUILD_SHARED_LIBRARY)

# Build the wds client
include $(BASE_PATH)/WebKit/android/wds/client/Android.mk

# Build the performance command line tool.
include $(BASE_PATH)/WebKit/android/benchmark/Android.mk

# Build the webkit merge tool.
include $(BASE_PATH)/WebKitTools/android/webkitmerge/Android.mk
