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
#	JSC/AllInOneFile.cpp \
#	JSC/CollectorHeapIntrospector.cpp \
#	JSC/grammar.y \
#	JSC/testJSC.cpp \
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
	\
	bytecode/CodeBlock.cpp \
	bytecode/JumpTable.cpp \
	bytecode/Opcode.cpp \
	bytecode/SamplingTool.cpp \
	bytecode/StructureStubInfo.cpp \
	bytecompiler/BytecodeGenerator.cpp \
	\
	debugger/Debugger.cpp \
	debugger/DebuggerActivation.cpp \
	debugger/DebuggerCallFrame.cpp \
	\
	interpreter/CallFrame.cpp \
	interpreter/Interpreter.cpp \
	interpreter/RegisterFile.cpp \
	\
	parser/Lexer.cpp \
	parser/Nodes.cpp \
	parser/Parser.cpp \
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
	runtime/Collector.cpp \
	runtime/CommonIdentifiers.cpp \
	runtime/Completion.cpp \
	runtime/ConstructData.cpp \
	runtime/DateConstructor.cpp \
	runtime/DateInstance.cpp \
	runtime/DateMath.cpp \
	runtime/DatePrototype.cpp \
	runtime/Error.cpp \
	runtime/ErrorConstructor.cpp \
	runtime/ErrorInstance.cpp \
	runtime/ErrorPrototype.cpp \
	runtime/ExceptionHelpers.cpp \
	runtime/FunctionConstructor.cpp \
	runtime/FunctionPrototype.cpp \
	runtime/GetterSetter.cpp \
	runtime/GlobalEvalFunction.cpp \
	runtime/Identifier.cpp \
	runtime/InitializeThreading.cpp \
	runtime/InternalFunction.cpp \
	runtime/JSActivation.cpp \
	runtime/JSArray.cpp \
	runtime/JSByteArray.cpp \
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
	runtime/Lookup.cpp \
	runtime/MathObject.cpp \
	runtime/NativeErrorConstructor.cpp \
	runtime/NativeErrorPrototype.cpp \
	runtime/NumberConstructor.cpp \
	runtime/NumberObject.cpp \
	runtime/NumberPrototype.cpp \
	runtime/ObjectConstructor.cpp \
	runtime/ObjectPrototype.cpp \
	runtime/Operations.cpp \
	runtime/PropertyNameArray.cpp \
	runtime/PropertySlot.cpp \
	runtime/PrototypeFunction.cpp \
	runtime/RegExp.cpp \
	runtime/RegExpConstructor.cpp \
	runtime/RegExpObject.cpp \
	runtime/RegExpPrototype.cpp \
	runtime/ScopeChain.cpp \
	runtime/SmallStrings.cpp \
	runtime/StringConstructor.cpp \
	runtime/StringObject.cpp \
	runtime/StringPrototype.cpp \
	runtime/Structure.cpp \
	runtime/StructureChain.cpp \
	runtime/UString.cpp \
	\
	wrec/CharacterClass.cpp \
	wrec/CharacterClassConstructor.cpp \
	wrec/WREC.cpp \
	wrec/WRECFunctors.cpp \
	wrec/WRECGenerator.cpp \
	wrec/WRECParser.cpp \
	\
	wtf/android/MainThreadAndroid.cpp \
	wtf/Assertions.cpp \
	wtf/ByteArray.cpp \
	wtf/CurrentTime.cpp \
	wtf/dtoa.cpp \
	wtf/FastMalloc.cpp \
	wtf/HashTable.cpp \
	wtf/MainThread.cpp \
	wtf/RandomNumber.cpp \
	wtf/RefCountedLeakCounter.cpp \
	wtf/TCSystemAlloc.cpp \
	wtf/Threading.cpp \
	wtf/ThreadingPthreads.cpp \
	wtf/unicode/CollatorDefault.cpp \
	wtf/unicode/UTF8.cpp \
	wtf/unicode/icu/CollatorICU.cpp

# Rule to build grammar.y with our custom bison.
GEN := $(intermediates)/parser/Grammar.cpp
$(GEN) : PRIVATE_YACCFLAGS := -p jscyy
$(GEN) : $(LOCAL_PATH)/parser/Grammar.y
	$(call local-transform-y-to-cpp,.cpp)
$(GEN) : $(LOCAL_BISON)
LOCAL_GENERATED_SOURCES += $(GEN)

# generated headers
JSC_OBJECTS := $(addprefix $(intermediates)/runtime/, \
				ArrayPrototype.lut.h \
				DatePrototype.lut.h \
				MathObject.lut.h \
				NumberConstructor.lut.h \
				RegExpConstructor.lut.h \
				RegExpObject.lut.h \
				StringPrototype.lut.h \
			)
$(JSC_OBJECTS): PRIVATE_PATH := $(LOCAL_PATH)
$(JSC_OBJECTS): PRIVATE_CUSTOM_TOOL = perl $(PRIVATE_PATH)/create_hash_table $< -i > $@
$(JSC_OBJECTS): $(LOCAL_PATH)/create_hash_table
$(JSC_OBJECTS): $(intermediates)/%.lut.h : $(LOCAL_PATH)/%.cpp
	$(transform-generated-source)


LEXER_HEADER := $(intermediates)/Lexer.lut.h
$(LEXER_HEADER): PRIVATE_PATH := $(LOCAL_PATH)
$(LEXER_HEADER): PRIVATE_CUSTOM_TOOL = perl $(PRIVATE_PATH)/create_hash_table $< -i > $@
$(LEXER_HEADER): $(LOCAL_PATH)/create_hash_table
$(LEXER_HEADER): $(intermediates)/%.lut.h : $(LOCAL_PATH)/parser/Keywords.table
	$(transform-generated-source)

CHARTABLES := $(intermediates)/chartables.c
$(CHARTABLES): PRIVATE_PATH := $(LOCAL_PATH)
$(CHARTABLES): PRIVATE_CUSTOM_TOOL = perl $(PRIVATE_PATH)/pcre/dftables $@
$(CHARTABLES): $(LOCAL_PATH)/pcre/dftables
$(CHARTABLES): $(LOCAL_PATH)/pcre/pcre_internal.h
	$(transform-generated-source)

$(intermediates)/pcre/pcre_tables.o : $(CHARTABLES)

LOCAL_GENERATED_SOURCES += $(JSC_OBJECTS) $(LEXER_HEADER) $(CHARTABLES)
