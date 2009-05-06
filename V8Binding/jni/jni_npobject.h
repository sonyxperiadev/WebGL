/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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

#ifndef JNI_NPOBJECT_H_
#define JNI_NPOBJECT_H_

#include "npruntime.h"
#include "jni_runtime.h"

#include <wtf/RefPtr.h>
#include <JavaVM/jni.h>

namespace JSC { namespace Bindings {

struct JavaNPObject {
    NPObject _object;
    RefPtr<JavaInstance> _instance;
};

NPObject* JavaInstanceToNPObject(JavaInstance* instance);
JavaInstance* ExtractJavaInstance(NPObject* obj);

bool JavaNPObject_HasMethod(NPObject* obj, NPIdentifier name);
bool JavaNPObject_Invoke(NPObject* obj, NPIdentifier methodName, const NPVariant* args, uint32_t argCount, NPVariant* result);
bool JavaNPObject_HasProperty(NPObject* obj, NPIdentifier name);
bool JavaNPObject_GetProperty(NPObject* obj, NPIdentifier name, NPVariant* ressult);

} }

#endif JNI_NPOBJECT_H_
