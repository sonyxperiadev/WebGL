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

#include "jni_utility.h"
#include "jni_instance.h"

#include "CString.h"

namespace JSC
{

namespace Bindings
{

class JavaString
{
public:
    JavaString() { }

    void _commonInit (JNIEnv *e, jstring s)
    {
        int size = e->GetStringLength(s);
        const char* cs = getCharactersFromJStringInEnv(e, s);
        {
            _utf8String = WebCore::CString(cs, size);
        }
        releaseCharactersForJStringInEnv (e, s, cs);
    }
    
    JavaString (JNIEnv *e, jstring s) {
        _commonInit (e, s);
    }
    
    JavaString (jstring s) {
        _commonInit (getJNIEnv(), s);
    }
    
    ~JavaString() { }
    
    int length() const { return _utf8String.length(); }
    
    const char* UTF8String() const {
        return _utf8String.data();
    }

private:
    WebCore::CString _utf8String;
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

    const char* name() const { return _name.UTF8String(); }
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

    const char* name() const { return _name.UTF8String(); }
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
