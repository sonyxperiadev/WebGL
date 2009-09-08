##
##
## Copyright 2007, The Android Open Source Project
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##

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

d := V8Binding
LOCAL_PATH := $(BASE_PATH)/$d
WEBCORE_PATH := $(BASE_PATH)/WebCore
intermediates := $(base_intermediates)/WebCore
JAVASCRIPTCORE_PATH := $(BASE_PATH)/JavaScriptCore
include $(LOCAL_PATH)/V8Binding.derived.mk
WEBKIT_SRC_FILES += $(addprefix $d/, $(LOCAL_SRC_FILES))
WEBKIT_SRC_FILES += $(addprefix WebCore/, $(WEBCORE_SRC_FILES))

# Include WTF source file.
d := JavaScriptCore
LOCAL_PATH := $(BASE_PATH)/$d
intermediates := $(base_intermediates)/$d
include $(LOCAL_PATH)/Android.v8.wtf.mk
WEBKIT_SRC_FILES += $(addprefix $d/,$(LOCAL_SRC_FILES))

# Include source files for WebCore
d := WebCore
LOCAL_PATH := $(BASE_PATH)/$d
intermediates := $(base_intermediates)/$d
include $(LOCAL_PATH)/Android.v8.mk
WEBKIT_SRC_FILES += $(addprefix $d/,$(LOCAL_SRC_FILES))

# Include the derived source files for WebCore. Uses the same path as
# WebCore
include $(LOCAL_PATH)/Android.v8.derived.mk
WEBKIT_SRC_FILES += $(addprefix $d/,$(LOCAL_SRC_FILES))

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
LOCAL_CFLAGS += -include "WebCorePrefixAndroid.h"
LOCAL_CFLAGS += -fvisibility=hidden

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -Darm
endif

ifeq ($(ENABLE_SVG),true)
LOCAL_CFLAGS += -DENABLE_SVG=1
endif

# Temporary disable SVG_ANIMATION.
ifeq ($(ENABLE_SVG_ANIMATION),true)
LOCAL_CFLAGS += -DENABLE_SVG_ANIMATION=1
endif

ifeq ($(WEBCORE_INSTRUMENTATION),true)
LOCAL_CFLAGS += -DANDROID_INSTRUMENT
endif

# LOCAL_LDLIBS is used in simulator builds only and simulator builds are only
# valid on Linux
LOCAL_LDLIBS += -lpthread -ldl

# Build our list of include paths. We include WebKit/android/icu first so that
# any files that include <unicode/ucnv.h> will include our ucnv.h first. We
# also add external/ as an include directory so that we can specify the real
# icu header directory as a more exact reference to avoid including our ucnv.h.
LOCAL_C_INCLUDES := \
	$(JNI_H_INCLUDE) \
	$(LOCAL_PATH)/WebKit/android/icu \
	external/ \
	external/icu4c/common \
	external/icu4c/i18n \
	external/libxml2/include \
	external/skia/emoji \
	external/skia/include/core \
	external/skia/include/effects \
	external/skia/include/images \
	external/skia/include/ports \
	external/skia/include/utils \
	external/skia/src/ports \
	external/sqlite/dist \
	frameworks/base/core/jni/android/graphics \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/WebCore \
	$(LOCAL_PATH)/WebCore/accessibility \
	$(LOCAL_PATH)/WebCore/css \
	$(LOCAL_PATH)/WebCore/dom \
	$(LOCAL_PATH)/WebCore/editing \
	$(LOCAL_PATH)/WebCore/history \
	$(LOCAL_PATH)/WebCore/html \
	$(LOCAL_PATH)/WebCore/html/canvas \
	$(LOCAL_PATH)/WebCore/inspector \
	$(LOCAL_PATH)/WebCore/loader \
	$(LOCAL_PATH)/WebCore/loader/appcache \
	$(LOCAL_PATH)/WebCore/loader/icon \
	$(LOCAL_PATH)/WebCore/page \
	$(LOCAL_PATH)/WebCore/page/android \
	$(LOCAL_PATH)/WebCore/page/animation \
	$(LOCAL_PATH)/WebCore/platform \
	$(LOCAL_PATH)/WebCore/platform/android \
	$(LOCAL_PATH)/WebCore/platform/animation \
	$(LOCAL_PATH)/WebCore/platform/graphics \
	$(LOCAL_PATH)/WebCore/platform/graphics/android \
	$(LOCAL_PATH)/WebCore/platform/graphics/network \
	$(LOCAL_PATH)/WebCore/platform/graphics/skia \
	$(LOCAL_PATH)/WebCore/platform/graphics/transforms \
	$(LOCAL_PATH)/WebCore/platform/image-decoders \
	$(LOCAL_PATH)/WebCore/platform/mock \
	$(LOCAL_PATH)/WebCore/platform/network \
	$(LOCAL_PATH)/WebCore/platform/network/android \
	$(LOCAL_PATH)/WebCore/platform/sql \
	$(LOCAL_PATH)/WebCore/platform/text \
	$(LOCAL_PATH)/WebCore/plugins \
	$(LOCAL_PATH)/WebCore/plugins/android \
	$(LOCAL_PATH)/WebCore/rendering \
	$(LOCAL_PATH)/WebCore/rendering/style \
	$(LOCAL_PATH)/WebCore/storage \
	$(LOCAL_PATH)/WebCore/workers \
	$(LOCAL_PATH)/WebCore/xml \
	$(LOCAL_PATH)/WebKit/android \
	$(LOCAL_PATH)/WebKit/android/WebCoreSupport \
	$(LOCAL_PATH)/WebKit/android/jni \
	$(LOCAL_PATH)/WebKit/android/nav \
	$(LOCAL_PATH)/WebKit/android/plugins \
	$(LOCAL_PATH)/WebKit/android/stl \
	$(LOCAL_PATH)/JavaScriptCore/wtf \
	$(LOCAL_PATH)/JavaScriptCore/wtf/unicode \
	$(LOCAL_PATH)/JavaScriptCore/wtf/unicode/icu \
	$(BINDING_C_INCLUDES) \
	$(base_intermediates)/WebCore/ \
	$(base_intermediates)/WebCore/bindings \
	$(base_intermediates)/WebCore/css \
	$(base_intermediates)/WebCore/html \
	$(base_intermediates)/WebCore/platform \
	$(base_intermediates)/JavaScriptCore

ifeq ($(ENABLE_SVG), true)
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) \
	$(LOCAL_PATH)/WebCore/platform/graphics/filters \
	$(LOCAL_PATH)/WebCore/svg \
	$(LOCAL_PATH)/WebCore/svg/animation \
	$(LOCAL_PATH)/WebCore/svg/graphics \
	$(LOCAL_PATH)/WebCore/svg/graphics/filters \
	$(base_intermediates)/WebCore/svg
endif

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
	libicudata \
	libicui18n \
	libmedia

ifeq ($(WEBCORE_INSTRUMENTATION),true)
LOCAL_SHARED_LIBRARIES += libhardware_legacy
endif

# We have to use the android version of libdl when we are not on the simulator
ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

# Build the list of static libraries
LOCAL_STATIC_LIBRARIES += libxml2 libv8

# Redefine LOCAL_SRC_FILES to be all the WebKit source files
LOCAL_SRC_FILES := $(WEBKIT_SRC_FILES)

# Define this for use in other makefiles.
WEBKIT_C_INCLUDES := $(LOCAL_C_INCLUDES)
WEBKIT_CFLAGS := $(LOCAL_CFLAGS)
WEBKIT_GENERATED_SOURCES := $(LOCAL_GENERATED_SOURCES)
WEBKIT_LDLIBS := $(LOCAL_LDLIBS)
WEBKIT_SHARED_LIBRARIES := $(LOCAL_SHARED_LIBRARIES)
WEBKIT_STATIC_LIBRARIES := $(LOCAL_STATIC_LIBRARIES)

# Build the library all at once
include $(BUILD_STATIC_LIBRARY)

# Now build the shared library using only the exported jni entry point. This
# will strip out any unused code from the entry point.
include $(CLEAR_VARS)
# if you need to make webcore huge (for debugging), enable this line
#LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libwebcore
LOCAL_LDLIBS := $(WEBKIT_LDLIBS)
LOCAL_SHARED_LIBRARIES := $(WEBKIT_SHARED_LIBRARIES)
LOCAL_STATIC_LIBRARIES := libwebcore $(WEBKIT_STATIC_LIBRARIES)
LOCAL_LDFLAGS := -fvisibility=hidden
LOCAL_CFLAGS := $(WEBKIT_CFLAGS)
LOCAL_C_INCLUDES := $(WEBKIT_C_INCLUDES)
LOCAL_PATH := $(BASE_PATH)
LOCAL_SRC_FILES := \
	WebKit/android/jni/WebCoreJniOnLoad.cpp
include $(BUILD_SHARED_LIBRARY)

# Build the wds client
include $(BASE_PATH)/WebKit/android/wds/client/Android.mk

# Build the performance command line tool.
#include $(BASE_PATH)/perf/Android.mk

# Build the webkit merge tool.
include $(BASE_PATH)/WebKitTools/android/webkitmerge/Android.mk

# Build libv8 and v8shell
ifeq ($(ENABLE_V8_SNAPSHOT),true)
  include $(BASE_PATH)/V8Binding/Android.mksnapshot.mk
endif
include $(BASE_PATH)/V8Binding/Android.libv8.mk
include $(BASE_PATH)/V8Binding/Android.v8shell.mk
