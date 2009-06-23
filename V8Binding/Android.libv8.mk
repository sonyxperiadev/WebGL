##
LOCAL_PATH := $(call my-dir)
# libv8.so
# ===================================================
include $(CLEAR_VARS)

# Set up the target identity
LOCAL_MODULE := libv8
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
intermediates := $(call local-intermediates-dir)

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := \
	src/accessors.cc \
	src/allocation.cc \
	src/api.cc \
	src/assembler.cc \
	src/ast.cc \
	src/bootstrapper.cc \
	src/builtins.cc \
	src/checks.cc \
	src/code-stubs.cc \
	src/codegen.cc \
	src/compilation-cache.cc \
	src/compiler.cc \
	src/contexts.cc \
	src/conversions.cc \
	src/counters.cc \
	src/dateparser.cc \
	src/disassembler.cc \
	src/execution.cc \
	src/factory.cc \
	src/flags.cc \
	src/frames.cc \
	src/func-name-inferrer.cc \
	src/global-handles.cc \
	src/handles.cc \
	src/hashmap.cc \
	src/heap.cc \
	src/ic.cc \
	src/interpreter-irregexp.cc \
	src/jsregexp.cc \
	src/jump-target.cc \
	src/log.cc \
	src/mark-compact.cc \
	src/messages.cc \
	src/objects.cc \
	src/oprofile-agent.cc \
	src/parser.cc \
	src/property.cc \
	src/regexp-macro-assembler.cc \
	src/regexp-macro-assembler-irregexp.cc \
	src/regexp-stack.cc \
	src/register-allocator.cc \
	src/rewriter.cc \
	src/runtime.cc \
	src/scanner.cc \
	src/scopeinfo.cc \
	src/scopes.cc \
	src/serialize.cc \
	src/snapshot-common.cc \
	src/spaces.cc \
	src/string-stream.cc \
	src/stub-cache.cc \
	src/token.cc \
	src/top.cc \
	src/unicode.cc \
	src/usage-analyzer.cc \
	src/utils.cc \
	src/v8-counters.cc \
	src/v8.cc \
	src/v8threads.cc \
	src/variables.cc \
	src/virtual-frame.cc \
	src/version.cc \
	src/zone.cc

ifeq ($(TARGET_ARCH),arm)
  LOCAL_SRC_FILES += \
		src/arm/assembler-arm.cc \
		src/arm/builtins-arm.cc \
		src/arm/codegen-arm.cc \
		src/arm/cpu-arm.cc \
		src/arm/disasm-arm.cc \
		src/arm/frames-arm.cc \
		src/arm/ic-arm.cc \
		src/arm/jump-target-arm.cc \
		src/arm/macro-assembler-arm.cc \
		src/arm/regexp-macro-assembler-arm.cc \
		src/arm/register-allocator-arm.cc \
		src/arm/stub-cache-arm.cc \
		src/arm/virtual-frame-arm.cc
endif

ifeq ($(TARGET_ARCH),x86)
  LOCAL_SRC_FILES += \
	  src/ia32/assembler-ia32.cc \
		src/ia32/builtins-ia32.cc \
		src/ia32/codegen-ia32.cc \
		src/ia32/cpu-ia32.cc \
		src/ia32/disasm-ia32.cc \
		src/ia32/frames-ia32.cc \
		src/ia32/ic-ia32.cc \
		src/ia32/jump-target-ia32.cc \
		src/ia32/macro-assembler-ia32.cc \
		src/ia32/regexp-macro-assembler-ia32.cc \
		src/ia32/register-allocator-ia32.cc \
		src/ia32/stub-cache-ia32.cc \
		src/ia32/virtual-frame-ia32.cc
endif

ifeq ($(TARGET_OS),linux)
  LOCAL_SRC_FILES += \
	  src/platform-linux.cc \
		src/platform-posix.cc
endif

ifeq ($(TARGET_BUILD_TYPE),debug)
	LOCAL_SRC_FILES += \
		src/objects-debug.cc \
		src/prettyprinter.cc \
		src/regexp-macro-assembler-tracer.cc
endif

# Don't use snapshot for now.
LOCAL_SRC_FILES += \
  src/snapshot-empty.cc \
	src/dtoa-config.c

LOCAL_SRC_FILES := $(addprefix v8/, $(LOCAL_SRC_FILES))

LOCAL_JS_LIBRARY_FILES := $(addprefix $(LOCAL_PATH)/v8/, \
	src/runtime.js \
	src/v8natives.js \
	src/array.js \
	src/string.js \
	src/uri.js \
	src/math.js \
	src/messages.js \
	src/apinatives.js \
	src/date-delay.js \
	src/regexp-delay.js \
	src/json-delay.js \
	src/macros.py \
)

# Generate libraries.cc
GEN1 := $(intermediates)/libraries.cc $(intermediates)/libraries-empty.cc
$(GEN1): SCRIPT := $(LOCAL_PATH)/v8/tools/js2c.py
$(GEN1): $(LOCAL_JS_LIBRARY_FILES)
	@echo "Generating libraries.cc"
	@mkdir -p $(dir $@)
	python $(SCRIPT) $(GEN1) CORE $(LOCAL_JS_LIBRARY_FILES)
LOCAL_GENERATED_SOURCES += $(intermediates)/libraries.cc

# GCC 4.3.1 has a bug that generates wrong code, -fno-tree-sink fixes the
# issue.
LOCAL_CFLAGS += \
	-Wno-endif-labels \
	-Wno-import \
	-Wno-format \
	-fno-exceptions

ifeq ($(TARGET_ARCH),arm)
	LOCAL_CFLAGS += -DARM -DV8_TARGET_ARCH_ARM
endif

ifeq ($(TARGET_ARCH),x86)
	LOCAL_CLFAGS += -DV8_TARGET_ARCH_IA32
endif

LOCAL_C_INCLUDES += $(LOCAL_PATH)/v8/src

include $(BUILD_STATIC_LIBRARY)
