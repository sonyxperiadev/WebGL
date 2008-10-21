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

LOCAL_PATH:= $(call my-dir)

# libkjs.so
# ==========================================================
include $(CLEAR_VARS)

#LOCAL_CFLAGS += -E -v

# This comment block is read by tools/webkitsync/diff.cpp
# Don't remove it or move it. 
# If you edit it, keep it in alphabetical order
#
# The following files are intentionally not included
# LOCAL_SRC_FILES_EXCLUDED := \
#	bindings/jni/jni_jsobject.cpp \
#	bindings/test*.cpp \
#	kjs/AllInOneFile.cpp \
#	kjs/CollectorHeapIntrospector.cpp \
#	kjs/testkjs.cpp \
#	pcre/dftables.c \
#	pcre/pcre_maketables.c \
#	pcre/ucptable.c \
#	wtf/OwnPtrWin.cpp \

# This comment block is read by tools/webkitsync/diff.cpp
# Don't remove it or move it. 
# If you edit it, keep it in alphabetical order
#
# The following directory wildcard matches are intentionally not included
# If an entry starts with '/', any subdirectory may match
# If an entry starts with '^', the first directory must match
# LOCAL_DIR_WILDCARD_EXCLUDED := \
#	^API/* \
#	^JavaScriptCore.apolloproj/* \
#	/qt/* \

LOCAL_SRC_FILES := \
	bindings/NP_jsobject.cpp \
	bindings/c/c_class.cpp \
	bindings/c/c_instance.cpp \
	bindings/c/c_runtime.cpp \
	bindings/c/c_utility.cpp \
	bindings/jni/jni_android.cpp \
	bindings/jni/jni_class.cpp \
	bindings/jni/jni_instance.cpp \
	bindings/jni/jni_runtime.cpp \
	bindings/jni/jni_utility.cpp \
	bindings/npruntime.cpp \
	bindings/runtime.cpp \
	bindings/runtime_array.cpp \
	bindings/runtime_method.cpp \
	bindings/runtime_object.cpp \
	bindings/runtime_root.cpp \
	 \
	kjs/CommonIdentifiers.cpp \
	kjs/DateMath.cpp \
	kjs/ExecState.cpp \
	kjs/JSGlobalObject.cpp \
	kjs/JSImmediate.cpp \
	kjs/JSLock.cpp \
	kjs/JSVariableObject.cpp \
	kjs/JSWrapperObject.cpp \
	kjs/Parser.cpp \
	kjs/PropertyNameArray.cpp \
	kjs/array_instance.cpp \
	kjs/array_object.cpp \
	kjs/bool_object.cpp \
	kjs/collector.cpp \
	kjs/date_object.cpp \
	kjs/debugger.cpp \
	kjs/dtoa.cpp \
	kjs/error_object.cpp \
	kjs/function.cpp \
	kjs/function_object.cpp \
	kjs/grammar.y \
	kjs/identifier.cpp \
	kjs/internal.cpp \
	kjs/interpreter.cpp \
	kjs/lexer.cpp \
	kjs/list.cpp \
	kjs/lookup.cpp \
	kjs/math_object.cpp \
	kjs/nodes.cpp \
	kjs/nodes2string.cpp \
	kjs/number_object.cpp \
	kjs/object.cpp \
	kjs/object_object.cpp \
	kjs/operations.cpp \
	kjs/property_map.cpp \
	kjs/property_slot.cpp \
	kjs/regexp.cpp \
	kjs/regexp_object.cpp \
	kjs/scope_chain.cpp \
	kjs/string_object.cpp \
	kjs/ustring.cpp \
	kjs/value.cpp \
	 \
	pcre/pcre_compile.cpp \
	pcre/pcre_exec.cpp \
	pcre/pcre_tables.cpp \
	pcre/pcre_ucp_searchfuncs.cpp \
	 \
	pcre/pcre_xclass.cpp \
	wtf/Assertions.cpp \
	wtf/FastMalloc.cpp \
	wtf/HashTable.cpp \
	wtf/TCSystemAlloc.cpp \
    wtf/unicode/UTF8.cpp

LOCAL_YACCFLAGS := -p kjsyy

LOCAL_CFLAGS += \
	-DINCLUDE_FASTMALLOC_IN_HEADERS \
	-DHAVE_CONFIG_H \
	-DHAVE_PCREPOSIX \
	-Wno-format \
	-Wno-endif-labels \
	-include "signal.h" \
	-include "string.h" \
	-include "stdio.h" \
	-include "time.h" \
	-include "unistd.h" \
	-include "algorithm" \
	-include "strings.h" \
	-include "assert.h" \
	-include "../JavaVM/jni.h"

ifeq ($(TARGET_BUILD_TYPE),release)
	LOCAL_CFLAGS += -fno-strict-aliasing
endif

ifeq ($(TARGET_ARCH),arm)
	LOCAL_CFLAGS += -D_ANDROID_ARM_ -Darm -fvisibility=hidden
endif

LOCAL_MODULE:= libkjs

# generated headers

# LOCAL_MODULE_CLASS must be set before calling local-intermediates-dir
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
intermediates := $(call local-intermediates-dir)

KJS_OBJECTS := $(addprefix $(intermediates)/kjs/, \
				array_object.lut.h \
				date_object.lut.h \
				math_object.lut.h \
				number_object.lut.h \
				regexp_object.lut.h \
				string_object.lut.h \
			)
$(KJS_OBJECTS): PRIVATE_PATH := $(LOCAL_PATH)
$(KJS_OBJECTS): PRIVATE_CUSTOM_TOOL = perl $(PRIVATE_PATH)/kjs/create_hash_table $< -i > $@
$(KJS_OBJECTS): $(intermediates)/%.lut.h : $(LOCAL_PATH)/%.cpp
	$(transform-generated-source)


LEXER_HEADER := $(intermediates)/lexer.lut.h
$(LEXER_HEADER): PRIVATE_PATH := $(LOCAL_PATH)
$(LEXER_HEADER): PRIVATE_CUSTOM_TOOL = perl $(PRIVATE_PATH)/kjs/create_hash_table $< -i > $@
$(LEXER_HEADER): $(intermediates)/%.lut.h : $(LOCAL_PATH)/kjs/keywords.table
	$(transform-generated-source)

CHARTABLES := $(intermediates)/chartables.c
$(CHARTABLES): PRIVATE_PATH := $(LOCAL_PATH)
$(CHARTABLES): PRIVATE_CUSTOM_TOOL = perl $(PRIVATE_PATH)/pcre/dftables $@
$(CHARTABLES): $(LOCAL_PATH)/pcre/pcre_internal.h
	$(transform-generated-source)

$(intermediates)/pcre/pcre_tables.o : $(CHARTABLES)

ALL_GENERATED_SOURCES += $(CHARTABLES)
LOCAL_GENERATED_SOURCES := $(KJS_OBJECTS) $(LEXER_HEADER) $(CHARTABLES)

LOCAL_COPY_HEADERS_TO := JavaScriptCore
LOCAL_COPY_HEADERS := \
	bindings/NP_jsobject.h \
	bindings/npruntime.h \
	bindings/npruntime_impl.h \
	bindings/npruntime_internal.h \
	bindings/runtime.h \
	bindings/runtime_object.h \
	bindings/runtime_root.h \
	pcre/pcre.h \
	$(call find-subdir-subdir-files, kjs, \*.h, ) \
	$(call find-subdir-subdir-files, wtf, \*.h, ) \
	$(call find-subdir-subdir-files, wtf/unicode, \*.h, ) \
	$(call find-subdir-subdir-files, wtf/unicode/icu, \*.h, ) \

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/pcre \
	$(LOCAL_PATH)/kjs \
	$(LOCAL_PATH)/wtf \
	$(LOCAL_PATH)/bindings \
	$(LOCAL_PATH)/bindings/c \
	$(LOCAL_PATH)/bindings/jni \
	$(LOCAL_PATH)/../WebCore/platform/android \
	$(LOCAL_PATH)/../WebCore/platform/android/stl \
	$(intermediates)/kjs \
	$(call include-path-for, corecg) \
	external/icu4c/common \

include $(BUILD_STATIC_LIBRARY)
