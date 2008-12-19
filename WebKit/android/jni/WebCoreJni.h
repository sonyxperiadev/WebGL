/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_WEBKIT_WEBCOREJNI_H
#define ANDROID_WEBKIT_WEBCOREJNI_H

#include "PlatformString.h"
#include <jni.h>

namespace android {

// A helper class that automatically deletes the local reference to the jobject
// returned from getRealObject.
class AutoJObject {
public:
    ~AutoJObject() {
        if (m_obj)
            m_env->DeleteLocalRef(m_obj);
    }
    jobject get() const {
        return m_obj;
    }
    JNIEnv* env() const {
        return m_env;
    }
private:
    AutoJObject(JNIEnv* env, jobject obj)
        : m_env(env)
        , m_obj(obj) {}
    JNIEnv* m_env;
    jobject m_obj;
    friend AutoJObject getRealObject(JNIEnv*, jobject);
};

// Get the real object stored in the WeakReference returned as an
// AutoJObject.
AutoJObject getRealObject(JNIEnv*, jobject);

// Convert the given jobject to a WeakReference and create a new global
// reference to that WeakReference.
jobject adoptGlobalRef(JNIEnv*, jobject);

// Helper method for check java exceptions. Returns true if an exception
// occurred and logs the exception.
bool checkException(JNIEnv* env);

// Get the JavaVM pointer for the given JNIEnv pointer
JavaVM* jnienv_to_javavm(JNIEnv* env);

// Get the JNIEnv pointer for the given JavaVM pointer
JNIEnv* javavm_to_jnienv(JavaVM* vm);

// Create a WebCore::String object from a jstring object.
WebCore::String to_string(JNIEnv* env, jstring str);

}

#endif
