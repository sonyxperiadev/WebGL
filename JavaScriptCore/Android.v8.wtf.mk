# wtf source files

LOCAL_SRC_FILES := \
	pcre/pcre_compile.cpp \
	pcre/pcre_exec.cpp \
	pcre/pcre_tables.cpp \
	pcre/pcre_ucp_searchfuncs.cpp \
	pcre/pcre_xclass.cpp \
	\
	wtf/Assertions.cpp \
	wtf/ByteArray.cpp \
	wtf/CurrentTime.cpp \
	wtf/DateMath.cpp \
	wtf/FastMalloc.cpp \
	wtf/HashTable.cpp \
	wtf/MainThread.cpp \
	wtf/RandomNumber.cpp \
	wtf/RefCountedLeakCounter.cpp \
	wtf/TCSystemAlloc.cpp \
	wtf/Threading.cpp \
	wtf/ThreadingPthreads.cpp \
	\
	wtf/android/MainThreadAndroid.cpp \
	\
	wtf/TypeTraits.cpp \
	wtf/dtoa.cpp \
	\
	wtf/unicode/CollatorDefault.cpp \
	wtf/unicode/UTF8.cpp \
	\
	wtf/unicode/icu/CollatorICU.cpp

CHARTABLES := $(intermediates)/chartables.c
$(CHARTABLES): PRIVATE_PATH := $(LOCAL_PATH)
$(CHARTABLES): PRIVATE_CUSTOM_TOOL = perl $(PRIVATE_PATH)/pcre/dftables $@
$(CHARTABLES): $(LOCAL_PATH)/pcre/dftables
$(CHARTABLES): $(LOCAL_PATH)/pcre/pcre_internal.h
	$(transform-generated-source)

$(intermediates)/pcre/pcre_tables.o : $(CHARTABLES)

# We do not add $(CHARTABLES) to LOCAL_GENERATED_SOURCES because the chartables.c file
# is explicitly #included in pcre_tables.cpp.
