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

#ifndef _JNI_INSTANCE_H_
#define _JNI_INSTANCE_H_

#include <JavaVM/jni.h>
#include <wtf/RefPtr.h>

namespace android {
class WeakJavaInstance;
}

using namespace WTF;

namespace JSC {

namespace Bindings {

class JavaClass;

class JObjectWrapper
{
public:
    JObjectWrapper(jobject instance);    
    ~JObjectWrapper();
    
    void ref() { _refCount++; }
    void deref() 
    { 
        if (--_refCount == 0) 
            delete this; 
    }

    jobject getLocalRef() const;

private:
    JNIEnv *_env;
    unsigned int _refCount;
    jobject _instance;  // it is a global weak reference.

    jmethodID mWeakRefGet;  // cache WeakReference::Get method id
};

class JavaInstance
{
public:
    JavaInstance(jobject instance);
    ~JavaInstance();

    void ref() { _refCount++; }
    void deref() 
    { 
        if (--_refCount == 0) 
            delete this; 
    }

    JavaClass* getClass() const;

    bool invokeMethod(const char* name, const NPVariant* args, uint32_t argsCount, NPVariant* result);

    // Returns a local reference, and the caller must delete
    // the returned reference after use.
    jobject getLocalRef() const {
        return _instance->getLocalRef();
    }

private:
    RefPtr<JObjectWrapper> _instance;
    unsigned int _refCount;
    mutable JavaClass* _class;
};

} // namespace Bindings

} // namespace JSC

#endif // _JNI_INSTANCE_H_
