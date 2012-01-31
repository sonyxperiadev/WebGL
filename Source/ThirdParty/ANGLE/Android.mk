#
# Copyright (C) 2011, 2012, Sony Ericsson Mobile Communications AB
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the Sony Ericsson Mobile Communications AB nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL SONY ERICSSON MOBILE COMMUNICATIONS AB BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# Compiler src
LOCAL_SRC_FILES := \
    Compiler.cpp \
    InfoSink.cpp \
    Initialize.cpp \
    InitializeDll.cpp \
    IntermTraverse.cpp \
    Intermediate.cpp \
    ParseHelper.cpp \
    PoolAlloc.cpp \
    QualifierAlive.cpp \
    RemoveTree.cpp \
    ShaderLang.cpp \
    SymbolTable.cpp \
    ValidateLimitations.cpp \
    VariableInfo.cpp \
    debug.cpp \
    intermOut.cpp \
    ossource_posix.cpp \
    parseConst.cpp \
    util.cpp

# Code generator
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
    CodeGenGLSL.cpp \
    OutputGLSL.cpp \
    TranslatorGLSL.cpp \
    VersionGLSL.cpp

# Generated files
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
    glslang_lex.cpp \
    glslang_tab.cpp

# Preprocessor
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
    preprocessor/atom.c \
    preprocessor/cpp.c \
    preprocessor/cppstruct.c \
    preprocessor/memory.c \
    preprocessor/scanner.c \
    preprocessor/symbols.c \
    preprocessor/tokens.c
