##
LOCAL_PATH := $(call my-dir)
# libv8.so
# ===================================================
include $(CLEAR_VARS)

# Set up the target identity
LOCAL_MODULE := libv8
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
intermediates := $(call local-intermediates-dir)

# Android.v8common.mk defines common V8_LOCAL_SRC_FILES
# and V8_LOCAL_JS_LIBRARY_FILES
V8_LOCAL_SRC_FILES :=
V8_LOCAL_JS_LIBRARY_FILES :=
include $(LOCAL_PATH)/Android.v8common.mk

# Target can only be linux
V8_LOCAL_SRC_FILES += \
  src/platform-linux.cc \
  src/platform-posix.cc

LOCAL_SRC_FILES := $(addprefix v8/, $(V8_LOCAL_SRC_FILES))

LOCAL_JS_LIBRARY_FILES := $(addprefix $(LOCAL_PATH)/v8/, $(V8_LOCAL_JS_LIBRARY_FILES))

# Copy js2c.py to intermediates directory and invoke there to avoid generating
# jsmin.pyc in the source directory
JS2C_PY := $(intermediates)/js2c.py $(intermediates)/jsmin.py
$(JS2C_PY): $(intermediates)/%.py : $(LOCAL_PATH)/v8/tools/%.py | $(ACP)
	@echo "Copying $@"
	$(copy-file-to-target)

LOCAL_GENERATED_SOURCES :=
# Generate libraries.cc
GEN1 := $(intermediates)/libraries.cc $(intermediates)/libraries-empty.cc
$(GEN1): SCRIPT := $(intermediates)/js2c.py
$(GEN1): $(LOCAL_JS_LIBRARY_FILES) $(JS2C_PY)
	@echo "Generating libraries.cc"
	@mkdir -p $(dir $@)
	python $(SCRIPT) $(GEN1) CORE $(LOCAL_JS_LIBRARY_FILES)
V8_GENERATED_LIBRARIES := $(intermediates)/libraries.cc


LOCAL_GENERATED_SOURCES += $(V8_GENERATED_LIBRARIES)

# Generate snapshot.cc
ifeq ($(ENABLE_V8_SNAPSHOT),true)
GEN := $(intermediates)/snapshot.cc
$(GEN): MKSNAPSHOT := $(HOST_OUT_EXECUTABLES)/mksnapshot
# mksnapshot is the module name. This dependency rule makes sure that mksnapshot
# is built before calling the following rule.
$(GEN): mksnapshot
	@echo "Generating snapshot.cc"
	@mkdir -p $(dir $@)
	$(MKSNAPSHOT) $(GEN)
LOCAL_GENERATED_SOURCES += $(intermediates)/snapshot.cc
else
LOCAL_SRC_FILES += \
  v8/src/snapshot-empty.cc
endif

LOCAL_CFLAGS += \
	-Wno-endif-labels \
	-Wno-import \
	-Wno-format \
	-fno-exceptions

ifeq ($(TARGET_ARCH),arm)
	LOCAL_CFLAGS += -DARM -DV8_TARGET_ARCH_ARM
endif

ifeq ($(TARGET_ARCH),x86)
	LOCAL_CFLAGS += -DV8_TARGET_ARCH_IA32
endif

ifeq ($(ENABLE_V8_LOGGING_AND_PROFILING),true)
	LOCAL_CFLAGS += -DENABLE_LOGGING_AND_PROFILING
endif

ifeq ($(DEBUG_V8),true)
	LOCAL_CFLAGS += -DDEBUG -UNDEBUG
endif

LOCAL_C_INCLUDES += $(LOCAL_PATH)/v8/src

include $(BUILD_STATIC_LIBRARY)
