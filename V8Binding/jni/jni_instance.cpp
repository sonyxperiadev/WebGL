/*
 * Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
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

#define LOG_TAG "v8binding"

#include "config.h"

#include "jni_class.h"
#include "jni_instance.h"
#include "jni_runtime.h"
#include "jni_utility.h"

#include <assert.h>
#include <utils/Log.h>

using namespace JSC::Bindings;

JavaInstance::JavaInstance (jobject instance)
{
    _instance = new JObjectWrapper(instance);
    _class = 0;
    _refCount = 0;
}

JavaInstance::~JavaInstance () 
{
    _instance = 0;
    delete _class;
}

JavaClass* JavaInstance::getClass() const 
{
    if (_class == 0) {
        jobject local_ref = getLocalRef();
        _class = new JavaClass(local_ref);
        getJNIEnv()->DeleteLocalRef(local_ref);
    }
    return _class;
}

bool JavaInstance::invokeMethod(const char* methodName, const NPVariant* args, uint32_t count, NPVariant* resultValue)
{
    int i;
    jvalue *jArgs;
    JavaMethod *method = 0;

    VOID_TO_NPVARIANT(*resultValue);

    MethodList methodList = getClass()->methodsNamed(methodName);

    size_t numMethods = methodList.size();
    
    // Try to find a good match for the overloaded method.  The 
    // fundamental problem is that JavaScript doesn have the
    // notion of method overloading and Java does.  We could 
    // get a bit more sophisticated and attempt to does some
    // type checking as we as checking the number of parameters.
    JavaMethod *aMethod;
    for (size_t methodIndex = 0; methodIndex < numMethods; methodIndex++) {
        aMethod = methodList[methodIndex];
        if (aMethod->numParameters() == count) {
            method = aMethod;
            break;
        }
    }
    if (method == 0) {
        LOGW("unable to find an appropiate method\n");
        return false;
    }
    
    const JavaMethod *jMethod = static_cast<const JavaMethod*>(method);
    
    if (count > 0) {
        jArgs = (jvalue *)malloc (count * sizeof(jvalue));
    }
    else
        jArgs = 0;
        
    for (i = 0; i < count; i++) {
        JavaParameter* aParameter = jMethod->parameterAt(i);
        jArgs[i] = convertNPVariantToJValue(args[i], aParameter->getJNIType(), aParameter->type());
    }
        
    jvalue result;

    // The following code can be conditionally removed once we have a Tiger update that
    // contains the new Java plugin.  It is needed for builds prior to Tiger.
    {    
        jobject obj = getLocalRef();
        switch (jMethod->JNIReturnType()){
            case void_type:
                callJNIMethodIDA<void>(obj, jMethod->methodID(obj), jArgs);
                break;            
            case object_type:
                result.l = callJNIMethodIDA<jobject>(obj, jMethod->methodID(obj), jArgs);
                break;
            case boolean_type:
                result.z = callJNIMethodIDA<jboolean>(obj, jMethod->methodID(obj), jArgs);
                break;
            case byte_type:
                result.b = callJNIMethodIDA<jbyte>(obj, jMethod->methodID(obj), jArgs);
                break;
            case char_type:
                result.c = callJNIMethodIDA<jchar>(obj, jMethod->methodID(obj), jArgs);
                break;            
            case short_type:
                result.s = callJNIMethodIDA<jshort>(obj, jMethod->methodID(obj), jArgs);
                break;
            case int_type:
                result.i = callJNIMethodIDA<jint>(obj, jMethod->methodID(obj), jArgs);
                break;
            
            case long_type:
                result.j = callJNIMethodIDA<jlong>(obj, jMethod->methodID(obj), jArgs);
                break;
            case float_type:
                result.f = callJNIMethodIDA<jfloat>(obj, jMethod->methodID(obj), jArgs);
                break;
            case double_type:
                result.d = callJNIMethodIDA<jdouble>(obj, jMethod->methodID(obj), jArgs);
                break;
            case invalid_type:
            default:
                break;
        }
        getJNIEnv()->DeleteLocalRef(obj);
    }
    
    convertJValueToNPVariant(result, jMethod->JNIReturnType(), jMethod->returnType(), resultValue);
    free (jArgs);

    return true;
}

JObjectWrapper::JObjectWrapper(jobject instance)
: _refCount(0)
{
    assert (instance != 0);

    // Cache the JNIEnv used to get the global ref for this java instanace.
    // It'll be used to delete the reference.
    _env = getJNIEnv();

    jclass localClsRef = _env->FindClass("java/lang/ref/WeakReference");
    jmethodID weakRefInit = _env->GetMethodID(localClsRef, "<init>",
                                    "(Ljava/lang/Object;)V");
    mWeakRefGet = _env->GetMethodID(localClsRef, "get",
                                   "()Ljava/lang/Object;");

    jobject weakRef = _env->NewObject(localClsRef, weakRefInit, instance);

    _instance = _env->NewGlobalRef(weakRef);
    
    LOGV("new global ref %p for %p\n", _instance, instance);

    if  (_instance == NULL) {
        fprintf (stderr, "%s:  could not get GlobalRef for %p\n", __PRETTY_FUNCTION__, instance);
    }

    _env->DeleteLocalRef(weakRef);
    _env->DeleteLocalRef(localClsRef);
}

JObjectWrapper::~JObjectWrapper() {
    LOGV("deleting global ref %p\n", _instance);
    _env->DeleteGlobalRef(_instance);
}

jobject JObjectWrapper::getLocalRef() const {
    jobject real = _env->CallObjectMethod(_instance, mWeakRefGet);
    if (!real)
        LOGE("The real object has been deleted");
    return _env->NewLocalRef(real);
}
