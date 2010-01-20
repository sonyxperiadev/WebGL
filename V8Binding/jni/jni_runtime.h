/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef _JNI_RUNTIME_H_
#define _JNI_RUNTIME_H_

#include "JNIUtility.h"
#include "JavaInstanceV8.h"

#if USE(V8)
#include "JavaStringV8.h"
#endif

namespace JSC
{

namespace Bindings
{

class JavaString
{
public:
    JavaString()
    {
        m_impl.init();
    }

    JavaString(JNIEnv* e, jstring s)
    {
        m_impl.init(e, s);
    }

    JavaString(jstring s)
    {
        m_impl.init(getJNIEnv(), s);
    }

    const char* UTF8String() const { return m_impl.UTF8String(); }
    const jchar* uchars() const { return m_impl.uchars(); }
    int length() const { return m_impl.length(); }

private:
    JavaStringImpl m_impl;
};

class JavaParameter
{
public:
    JavaParameter () : _JNIType(invalid_type) {};
    JavaParameter (JNIEnv *env, jstring type);
    virtual ~JavaParameter() { }

    const char* type() const { return _type.UTF8String(); }
    JNIType getJNIType() const { return _JNIType; }
    
private:
    JavaString _type;
    JNIType _JNIType;
};


class JavaField
{
public:
    JavaField (JNIEnv *env, jobject aField);

    const JavaString& name() const { return _name; }
    const char* type() const { return _type.UTF8String(); }

    JNIType getJNIType() const { return _JNIType; }
    
private:
    JavaString _name;
    JavaString _type;
    JNIType _JNIType;
    RefPtr<JObjectWrapper> _field;
};


class JavaMethod
{
public:
    JavaMethod(JNIEnv* env, jobject aMethod);
    ~JavaMethod();

    const JavaString& name() const { return _name; }
    const char* returnType() const { return _returnType.UTF8String(); };
    JavaParameter* parameterAt(int i) const { return &_parameters[i]; };
    int numParameters() const { return _numParameters; };
    
    const char *signature() const;
    JNIType JNIReturnType() const;

    jmethodID methodID (jobject obj) const;
    
    bool isStatic() const { return _isStatic; }

private:
    JavaParameter* _parameters;
    int _numParameters;
    JavaString _name;
    mutable char* _signature;
    JavaString _returnType;
    JNIType _JNIReturnType;
    mutable jmethodID _methodID;
    bool _isStatic;
};

} // namespace Bindings

} // namespace JSC

#endif // _JNI_RUNTIME_H_
