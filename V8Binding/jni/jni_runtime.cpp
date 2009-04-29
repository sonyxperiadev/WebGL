/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#ifdef NDEBUG
#define JS_LOG(formatAndArgs...) ((void)0)
#else
#define JS_LOG(formatAndArgs...) { \
    fprintf (stderr, "%s:%d -- %s:  ", __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, formatAndArgs); \
}
#endif

using namespace JSC::Bindings;

JavaParameter::JavaParameter (JNIEnv *env, jstring type)
{
    _type = JavaString (env, type);
    _JNIType = JNITypeFromClassName (_type.UTF8String());
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
#ifdef ANDROID_FIX
    env->DeleteLocalRef(modifierClass);
#endif
}

JavaMethod::~JavaMethod() 
{
    if (_signature)
        free(_signature);
    delete [] _parameters;
};

// JNI method signatures use '/' between components of a class name, but
// we get '.' between components from the reflection API.
static void appendClassName(UString& aString, const char* className)
{
    ASSERT(JSLock::lockCount() > 0);
    
    char *result, *cp = strdup(className);
    
    result = cp;
    while (*cp) {
        if (*cp == '.')
            *cp = '/';
        cp++;
    }
        
    aString.append(result);

    free (result);
}

const char *JavaMethod::signature() const 
{
    if (!_signature) {
        JSLock lock(false);

        UString signatureBuilder("(");
        for (int i = 0; i < _numParameters; i++) {
            JavaParameter* aParameter = parameterAt(i);
            JNIType _JNIType = aParameter->getJNIType();
            if (_JNIType == array_type)
                appendClassName(signatureBuilder, aParameter->type());
            else {
                signatureBuilder.append(signatureFromPrimitiveType(_JNIType));
                if (_JNIType == object_type) {
                    appendClassName(signatureBuilder, aParameter->type());
                    signatureBuilder.append(";");
                }
            }
        }
        signatureBuilder.append(")");
        
        const char *returnType = _returnType.UTF8String();
        if (_JNIReturnType == array_type) {
            appendClassName(signatureBuilder, returnType);
        } else {
            signatureBuilder.append(signatureFromPrimitiveType(_JNIReturnType));
            if (_JNIReturnType == object_type) {
                appendClassName(signatureBuilder, returnType);
                signatureBuilder.append(";");
            }
        }
        
        _signature = strdup(signatureBuilder.ascii());
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


