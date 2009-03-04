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

#LOCAL_CFLAGS += -E -v

# This comment block is read by tools/webkitsync/diff.cpp
# Don't remove it or move it. 
# If you edit it, keep it in alphabetical order
#
# The following files are intentionally not included
# LOCAL_SRC_FILES_EXCLUDED := \
#	kjs/AllInOneFile.cpp \
#	kjs/CollectorHeapIntrospector.cpp \
#	kjs/grammar.y \
#	kjs/testkjs.cpp \
#	pcre/dftables.c \
#	pcre/pcre_maketables.c \
#	pcre/ucptable.cpp \
#	wtf/OwnPtrWin.cpp \
#	wtf/GOwnPtr.cpp \
#	wtf/*Gtk.cpp \
#	wtf/*Qt.cpp \
#	wtf/*Win.cpp \

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
#	/gtk/* \
#	/qt/* \

LOCAL_SRC_FILES := \
	\
	VM/CTI.cpp \
	VM/CodeBlock.cpp \
	VM/CodeGenerator.cpp \
	VM/ExceptionHelpers.cpp \
	VM/Machine.cpp \
	VM/Opcode.cpp \
	VM/RegisterFile.cpp \
	VM/SamplingTool.cpp \
	\
	debugger/Debugger.cpp \
	debugger/DebuggerCallFrame.cpp \
	\
	kjs/Parser.cpp \
	kjs/Shell.cpp \
	kjs/collector.cpp \
	kjs/dtoa.cpp \
	kjs/identifier.cpp \
	kjs/interpreter.cpp \
	kjs/lexer.cpp \
	kjs/lookup.cpp \
	kjs/nodes.cpp \
	kjs/nodes2string.cpp \
	kjs/operations.cpp \
	kjs/regexp.cpp \
	kjs/ustring.cpp \
	\
	pcre/pcre_compile.cpp \
	pcre/pcre_exec.cpp \
	pcre/pcre_tables.cpp \
	pcre/pcre_ucp_searchfuncs.cpp \
	pcre/pcre_xclass.cpp \
	\
	profiler/HeavyProfile.cpp \
	profiler/Profile.cpp \
	profiler/ProfileGenerator.cpp \
	profiler/ProfileNode.cpp \
	profiler/Profiler.cpp \
	profiler/TreeProfile.cpp \
	\
	runtime/ArgList.cpp \
	runtime/Arguments.cpp \
	runtime/ArrayConstructor.cpp \
	runtime/ArrayPrototype.cpp \
	runtime/BooleanConstructor.cpp \
	runtime/BooleanObject.cpp \
	runtime/BooleanPrototype.cpp \
	runtime/CallData.cpp \
	runtime/CommonIdentifiers.cpp \
	runtime/ConstructData.cpp \
	runtime/DateConstructor.cpp \
	runtime/DateInstance.cpp \
	runtime/DateMath.cpp \
	runtime/DatePrototype.cpp \
	runtime/Error.cpp \
	runtime/ErrorConstructor.cpp \
	runtime/ErrorInstance.cpp \
	runtime/ErrorPrototype.cpp \
	runtime/ExecState.cpp \
	runtime/FunctionConstructor.cpp \
	runtime/FunctionPrototype.cpp \
	runtime/GetterSetter.cpp \
	runtime/GlobalEvalFunction.cpp \
	runtime/InitializeThreading.cpp \
	runtime/InternalFunction.cpp \
	runtime/JSActivation.cpp \
	runtime/JSArray.cpp \
	runtime/JSCell.cpp \
	runtime/JSFunction.cpp \
	runtime/JSGlobalData.cpp \
	runtime/JSGlobalObject.cpp \
	runtime/JSGlobalObjectFunctions.cpp \
	runtime/JSImmediate.cpp \
	runtime/JSLock.cpp \
	runtime/JSNotAnObject.cpp \
	runtime/JSNumberCell.cpp \
	runtime/JSObject.cpp \
	runtime/JSPropertyNameIterator.cpp \
	runtime/JSStaticScopeObject.cpp \
	runtime/JSString.cpp \
	runtime/JSValue.cpp \
	runtime/JSVariableObject.cpp \
	runtime/JSWrapperObject.cpp \
	runtime/MathObject.cpp \
	runtime/NativeErrorConstructor.cpp \
	runtime/NativeErrorPrototype.cpp \
	runtime/NumberConstructor.cpp \
	runtime/NumberObject.cpp \
	runtime/NumberPrototype.cpp \
	runtime/ObjectConstructor.cpp \
	runtime/ObjectPrototype.cpp \
	runtime/PropertyNameArray.cpp \
	runtime/PropertySlot.cpp \
	runtime/PrototypeFunction.cpp \
	runtime/RegExpConstructor.cpp \
	runtime/RegExpObject.cpp \
	runtime/RegExpPrototype.cpp \
	runtime/ScopeChain.cpp \
	runtime/SmallStrings.cpp \
	runtime/StringConstructor.cpp \
	runtime/StringObject.cpp \
	runtime/StringPrototype.cpp \
	runtime/StructureID.cpp \
	runtime/StructureIDChain.cpp \
	\
	wrec/CharacterClassConstructor.cpp \
	wrec/WREC.cpp \
	\
	wtf/android/MainThreadAndroid.cpp \
	wtf/Assertions.cpp \
	wtf/FastMalloc.cpp \
	wtf/HashTable.cpp \
	wtf/MainThread.cpp \
	wtf/RefCountedLeakCounter.cpp \
	wtf/TCSystemAlloc.cpp \
	wtf/ThreadingPthreads.cpp \
	wtf/unicode/CollatorDefault.cpp \
	wtf/unicode/UTF8.cpp \
	wtf/unicode/icu/CollatorICU.cpp

# Rule to build grammar.y with our custom bison.
GEN := $(intermediates)/kjs/grammar.cpp
$(GEN) : PRIVATE_YACCFLAGS := -p kjsyy
$(GEN) : $(LOCAL_PATH)/kjs/grammar.y
	$(call local-transform-y-to-cpp,.cpp)
$(GEN) : $(LOCAL_BISON)
LOCAL_GENERATED_SOURCES += $(GEN)

# generated headers
KJS_OBJECTS := $(addprefix $(intermediates)/runtime/, \
				ArrayPrototype.lut.h \
				DatePrototype.lut.h \
				MathObject.lut.h \
				NumberConstructor.lut.h \
				RegExpConstructor.lut.h \
				RegExpObject.lut.h \
				StringPrototype.lut.h \
			)
$(KJS_OBJECTS): PRIVATE_PATH := $(LOCAL_PATH)
$(KJS_OBJECTS): PRIVATE_CUSTOM_TOOL = perl $(PRIVATE_PATH)/kjs/create_hash_table $< -i > $@
$(KJS_OBJECTS): $(LOCAL_PATH)/kjs/create_hash_table
$(KJS_OBJECTS): $(intermediates)/%.lut.h : $(LOCAL_PATH)/%.cpp
	$(transform-generated-source)


LEXER_HEADER := $(intermediates)/lexer.lut.h
$(LEXER_HEADER): PRIVATE_PATH := $(LOCAL_PATH)
$(LEXER_HEADER): PRIVATE_CUSTOM_TOOL = perl $(PRIVATE_PATH)/kjs/create_hash_table $< -i > $@
$(LEXER_HEADER): $(LOCAL_PATH)/kjs/create_hash_table
$(LEXER_HEADER): $(intermediates)/%.lut.h : $(LOCAL_PATH)/kjs/keywords.table
	$(transform-generated-source)

CHARTABLES := $(intermediates)/chartables.c
$(CHARTABLES): PRIVATE_PATH := $(LOCAL_PATH)
$(CHARTABLES): PRIVATE_CUSTOM_TOOL = perl $(PRIVATE_PATH)/pcre/dftables $@
$(CHARTABLES): $(LOCAL_PATH)/pcre/dftables
$(CHARTABLES): $(LOCAL_PATH)/pcre/pcre_internal.h
	$(transform-generated-source)

$(intermediates)/pcre/pcre_tables.o : $(CHARTABLES)

LOCAL_GENERATED_SOURCES += $(KJS_OBJECTS) $(LEXER_HEADER) $(CHARTABLES)
