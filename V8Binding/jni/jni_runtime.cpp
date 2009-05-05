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

#include "config.h"

#include "jni_runtime.h"
#include "jni_utility.h"

using namespace JSC::Bindings;

JavaParameter::JavaParameter (JNIEnv *env, jstring type)
{
    _type = JavaString (env, type);
    _JNIType = JNITypeFromClassName (_type.UTF8String());
}


JavaField::JavaField (JNIEnv *env, jobject aField)
{
    // Get field type
    jobject fieldType = callJNIMethod<jobject>(aField, "getType", "()Ljava/lang/Class;");
    jstring fieldTypeName = (jstring)callJNIMethod<jobject>(fieldType, "getName", "()Ljava/lang/String;");
    _type = JavaString(env, fieldTypeName);
    _JNIType = JNITypeFromClassName (_type.UTF8String());

    // Get field name
    jstring fieldName = (jstring)callJNIMethod<jobject>(aField, "getName", "()Ljava/lang/String;");
    _name = JavaString(env, fieldName);

    _field = new JObjectWrapper(aField);
}


JavaMethod::JavaMethod (JNIEnv *env, jobject aMethod)
{
    // Get return type
    jobject returnType = callJNIMethod<jobject>(aMethod, "getReturnType", "()Ljava/lang/Class;");
    jstring returnTypeName = (jstring)callJNIMethod<jobject>(returnType, "getName", "()Ljava/lang/String;");
    _returnType =JavaString (env, returnTypeName);
    _JNIReturnType = JNITypeFromClassName (_returnType.UTF8String());
    env->DeleteLocalRef (returnType);
    env->DeleteLocalRef (returnTypeName);

    // Get method name
    jstring methodName = (jstring)callJNIMethod<jobject>(aMethod, "getName", "()Ljava/lang/String;");
    _name = JavaString (env, methodName);
    env->DeleteLocalRef (methodName);

    // Get parameters
    jarray jparameters = (jarray)callJNIMethod<jobject>(aMethod, "getParameterTypes", "()[Ljava/lang/Class;");
    _numParameters = env->GetArrayLength (jparameters);
    _parameters = new JavaParameter[_numParameters];
    
    int i;
    for (i = 0; i < _numParameters; i++) {
        jobject aParameter = env->GetObjectArrayElement ((jobjectArray)jparameters, i);
        jstring parameterName = (jstring)callJNIMethod<jobject>(aParameter, "getName", "()Ljava/lang/String;");
        _parameters[i] = JavaParameter(env, parameterName);
        env->DeleteLocalRef (aParameter);
        env->DeleteLocalRef (parameterName);
    }
    env->DeleteLocalRef (jparameters);

    // Created lazily.
    _signature = 0;
    _methodID = 0;
    
    jclass modifierClass = env->FindClass("java/lang/reflect/Modifier");
    int modifiers = callJNIMethod<jint>(aMethod, "getModifiers", "()I");
    _isStatic = (bool)callJNIStaticMethod<jboolean>(modifierClass, "isStatic", "(I)Z", modifiers);
    env->DeleteLocalRef(modifierClass);
}

JavaMethod::~JavaMethod() 
{
    if (_signature)
        free(_signature);
    delete [] _parameters;
};


class SignatureBuilder {
public:
    explicit SignatureBuilder(int init_size) {
        if (init_size <= 0)
            init_size = 16;
        size_ = init_size;
        length_ = 0;
        storage_ = (char*)malloc(size_ * sizeof(char));
    }

    ~SignatureBuilder() {
        free(storage_);
    }

    void append(const char* s) {
        int l = strlen(s);
        expandIfNeeded(l);
        memcpy(storage_ + length_, s, l);
        length_ += l;
    }

    // JNI method signatures use '/' between components of a class name, but
    // we get '.' between components from the reflection API.
    void appendClassName(const char* className) {
       int l = strlen(className);
       expandIfNeeded(l);

       char* sp = storage_ + length_;
       const char* cp = className;
    
       while (*cp) {
           if (*cp == '.')
               *sp = '/';
           else
               *sp = *cp;

           cp++;
           sp++;
       }

       length_ += l;
    }

    // make a duplicated copy of the content.
    char* ascii() {
        if (length_ == 0)
            return NULL;
        storage_[length_] = '\0';
        return strndup(storage_, length_);
    }

private:
    void expandIfNeeded(int l) {
        // expand storage if needed
        if (l + length_ >= size_) {
            int new_size = 2 * size_;
            if (l + length_ >= new_size)
                new_size = l + length_ + 1;

            char* new_storage = (char*)malloc(new_size * sizeof(char));
            memcpy(new_storage, storage_, length_);
            size_ = new_size;
            free(storage_);
            storage_ = new_storage;
        }
    }

    int size_;
    int length_;
    char* storage_;
};

const char *JavaMethod::signature() const 
{
    if (!_signature) {
        SignatureBuilder signatureBuilder(64);
        signatureBuilder.append("(");
        for (int i = 0; i < _numParameters; i++) {
            JavaParameter* aParameter = parameterAt(i);
            JNIType _JNIType = aParameter->getJNIType();
            if (_JNIType == array_type)
                signatureBuilder.appendClassName(aParameter->type());
            else {
                signatureBuilder.append(signatureFromPrimitiveType(_JNIType));
                if (_JNIType == object_type) {
                    signatureBuilder.appendClassName(aParameter->type());
                    signatureBuilder.append(";");
                }
            }
        }
        signatureBuilder.append(")");
        
        const char *returnType = _returnType.UTF8String();
        if (_JNIReturnType == array_type) {
            signatureBuilder.appendClassName(returnType);
        } else {
            signatureBuilder.append(signatureFromPrimitiveType(_JNIReturnType));
            if (_JNIReturnType == object_type) {
                signatureBuilder.appendClassName(returnType);
                signatureBuilder.append(";");
            }
        }
        
        _signature = signatureBuilder.ascii();
    }
    
    return _signature;
}

JNIType JavaMethod::JNIReturnType() const
{
    return _JNIReturnType;
}

jmethodID JavaMethod::methodID (jobject obj) const
{
    if (_methodID == 0) {
        _methodID = getMethodID (obj, _name.UTF8String(), signature());
    }
    return _methodID;
}


