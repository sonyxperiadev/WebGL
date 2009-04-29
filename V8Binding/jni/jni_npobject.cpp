
#include "jni_npobject.h"

namespace V8 { namespace Binding {

static NPClass JavaNPClass = {
    NP_CLASS_STRUCT_VERSION,
    0,  // allocate,
    0,  // free,
    0,  // invalidate
    JavaNPObject_HasMethod,
    JavaNPObject_Invoke,
    0,  // invokeDefault,
    JavaNPObject_HasProperty,
    JavaNPobject_GetProperty,
    JavaNPObject_SetProperty,
    0,  // removeProperty
    0,  // enumerate
    0   // construct
};

NPObject* JavaInstanceToNPObject(PassRefPtr<JavaInstance> instance) {
    JavaNPObject* object = new JavaNPObject(instance);
    return static_cast<NPObject*>(object);
}

bool JavaNPObject_HasMethod(NPObject* obj, NPIdentifier name) {
    // FIXME: for now, always pretend the object has the named method.
    return true;
}

bool JavaNPObject_Invoke(NPobject* obj, NPIdentifier methodName, const NPVariant* args, uint32_t argCount, NPVariant* result) {

}

bool JavaNPObject_HasProperty(NPObject* obj, NPIdentifier name) {
}

bool JavaNPObject_GetProperty(NPObject* obj, NPIdentifier name, NPVariant* ressult) {
}

bool JavaNPObject_SetProperty(NPObject* obj, NPIdentifier name, const NPVariant* value) {

}

} }  // namespace V8::Binding
