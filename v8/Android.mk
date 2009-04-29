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
	src/zone.cc

ifeq ($(TARGET_ARCH),arm)
	LOCAL_SRC_FILES += \
		src/assembler-arm.cc \
		src/builtins-arm.cc \
		src/codegen-arm.cc \
		src/cpu-arm.cc \
		src/disasm-arm.cc \
		src/frames-arm.cc \
		src/ic-arm.cc \
		src/jump-target-arm.cc \
		src/macro-assembler-arm.cc \
		src/regexp-macro-assembler-arm.cc \
		src/register-allocator-arm.cc \
		src/stub-cache-arm.cc \
		src/virtual-frame-arm.cc
endif

ifeq ($(TARGET_ARCH),x86)
  LOCAL_SRC_FILES += \
	  src/assembler-ia32.cc \
		src/builtins-ia32.cc \
		src/codegen-ia32.cc \
		src/cpu-ia32.cc \
		src/disasm-ia32.cc \
		src/frames-ia32.cc \
		src/ic-ia32.cc \
		src/jump-target-ia32.cc \
		src/macro-assembler-ia32.cc \
		src/regexp-macro-assembler-ia32.cc \
		src/register-allocator-ia32.cc \
		src/stub-cache-ia32.cc \
		src/virtual-frame-ia32.cc
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

LOCAL_JS_LIBRARY_FILES := $(addprefix $(LOCAL_PATH)/, \
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
	src/macros.py \
)

# Generate libraries.cc
GEN1 := $(intermediates)/libraries.cc $(intermediates)/libraries-empty.cc
$(GEN1): SCRIPT := $(LOCAL_PATH)/tools/js2c.py
$(GEN1): $(LOCAL_JS_LIBRARY_FILES)
	@echo "Generating libraries.cc"
	@mkdir -p $(dir $@)
	python $(SCRIPT) $(GEN1) CORE $(LOCAL_JS_LIBRARY_FILES)
LOCAL_GENERATED_SOURCES += $(intermediates)/libraries.cc

LOCAL_CFLAGS += \
	-Wno-endif-labels \
	-Wno-import \
	-Wno-format \
	-fno-exceptions

ifeq ($(TARGET_ARCH),arm)
	LOCAL_CFLAGS += -DARM
endif

LOCAL_C_INCLUDES += $(LOCAL_PATH)/src

include $(BUILD_STATIC_LIBRARY)
