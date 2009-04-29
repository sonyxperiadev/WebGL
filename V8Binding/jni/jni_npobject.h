// A wrapper of a JNI value as an NPObject


#ifndef JNI_NPOBJECT_H_
#define JNI_NPOBJECT_H_

#include "npapi.h"
#include <JavaVM/jni.h>

namespace V8 { namespace V8Binding {

struct JavaNPObject {
    NPObject _object;
    RefPtr<JavaInstance> _instance;
};

NPObject* JavaInstanceToNPObject(PassRefPtr<JavaInstance> instance);

bool JavaNPObject_HasMethod(NPObject* obj, NPIdentifier name);
bool JavaNPObject_Invoke(NPobject* obj, NPIdentifier methodName, const NPVariant* args, uint32_t argCount, NPVariant* result);
bool JavaNPObject_HasProperty(NPObject* obj, NPIdentifier name);
bool JavaNPObject_GetProperty(NPObject* obj, NPIdentifier name, NPVariant* ressult);
bool JavaNPObject_SetProperty(NPObject* obj, NPIdentifier name, const NPVariant* value);

} }

#endif JNI_NPOBJECT_H_
