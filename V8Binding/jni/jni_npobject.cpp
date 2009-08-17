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


#include "config.h"

#include "jni_npobject.h"
#include "jni_class.h"
#include "jni_instance.h"
#include "jni_runtime.h"
#include "jni_utility.h"

namespace JSC { namespace Bindings {
static NPObject* AllocJavaNPObject(NPP, NPClass*)
{
    JavaNPObject* obj =
        static_cast<JavaNPObject*>(malloc(sizeof(JavaNPObject)));
    if (obj == 0)
        return 0;
    bzero(obj, sizeof(JavaNPObject));
    return reinterpret_cast<NPObject*>(obj);
}

static void FreeJavaNPObject(NPObject* npobj)
{
    JavaNPObject* obj = reinterpret_cast<JavaNPObject*>(npobj);
    obj->_instance = 0;  // free does not call the destructor
    free(obj);
}

static NPClass JavaNPObjectClass = {
    NP_CLASS_STRUCT_VERSION,
    AllocJavaNPObject,  // allocate,
    FreeJavaNPObject,  // free,
    0,  // invalidate
    JavaNPObject_HasMethod,
    JavaNPObject_Invoke,
    0,  // invokeDefault,
    JavaNPObject_HasProperty,
    JavaNPObject_GetProperty,
    0,  // setProperty
    0,  // removeProperty
    0,  // enumerate
    0   // construct
};


NPObject* JavaInstanceToNPObject(JavaInstance* instance) {
    JavaNPObject* object = reinterpret_cast<JavaNPObject*>(_NPN_CreateObject(0, &JavaNPObjectClass));
    object->_instance = instance;
    return reinterpret_cast<NPObject*>(object);
}


// Returns null if obj is not a wrapper of JavaInstance
JavaInstance* ExtractJavaInstance(NPObject* obj) {
    if (obj->_class == &JavaNPObjectClass) {
        return reinterpret_cast<JavaNPObject*>(obj)->_instance.get();
    }
    return 0;
}

bool JavaNPObject_HasMethod(NPObject* obj, NPIdentifier identifier) {
    JavaInstance* instance = ExtractJavaInstance(obj);
    if (instance == 0)
        return false;
    NPUTF8* name = _NPN_UTF8FromIdentifier(identifier);
    if (name == 0)
        return false;

    bool result = (instance->getClass()->methodsNamed(name).size() > 0);

    // TODO: use NPN_MemFree
    free(name);

    return result;
}

bool JavaNPObject_Invoke(NPObject* obj, NPIdentifier identifier,
        const NPVariant* args, uint32_t argCount, NPVariant* result) {
    JavaInstance* instance = ExtractJavaInstance(obj);
    if (instance == 0)
        return false;
    NPUTF8* name = _NPN_UTF8FromIdentifier(identifier);
    if (name == 0)
        return false;

    bool r = instance->invokeMethod(name, args, argCount, result);

    // TODO: use NPN_MemFree
    free(name);
    return r;

}

bool JavaNPObject_HasProperty(NPObject* obj, NPIdentifier identifier) {
    JavaInstance* instance = ExtractJavaInstance(obj);
    if (instance == 0)
        return false;
    NPUTF8* name = _NPN_UTF8FromIdentifier(identifier);
    if (name == 0)
        return false;
    bool result = instance->getClass()->fieldNamed(name) != 0;
    free(name);
    return result;
}

bool JavaNPObject_GetProperty(NPObject* obj, NPIdentifier identifier, NPVariant* result) {
    VOID_TO_NPVARIANT(*result);
    JavaInstance* instance = ExtractJavaInstance(obj);
    if (instance == 0)
        return false;
    NPUTF8* name = _NPN_UTF8FromIdentifier(identifier);
    if (name == 0)
        return false;

    JavaField* field = instance->getClass()->fieldNamed(name);
    free(name);  // TODO: use NPN_MemFree

    if (field == 0) {
        return false;
    }

    jobject local_ref = instance->getLocalRef();
    jvalue value = getJNIField(local_ref,
                               field->getJNIType(),
                               field->name(),
                               field->type());
    getJNIEnv()->DeleteLocalRef(local_ref);

    convertJValueToNPVariant(value, field->getJNIType(), field->type(), result);

    return true;
}

}}  // namespace
