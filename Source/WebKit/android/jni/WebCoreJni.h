/*
 * Copyright 2008, The Android Open Source Project
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebCoreJni_h
#define WebCoreJni_h

#include "ChromiumIncludes.h"
#include "PlatformString.h"
#include <jni.h>

namespace android {

// A helper class that automatically deletes the local reference to the jobject
// returned from getRealObject.
class AutoJObject {
public:
    AutoJObject(const AutoJObject& other)
        : m_env(other.m_env)
        , m_obj(other.m_obj ? other.m_env->NewLocalRef(other.m_obj) : NULL) {}
    ~AutoJObject() {
        if (m_obj)
            m_env->DeleteLocalRef(m_obj);
    }
    jobject get() const {
        return m_obj;
    }
    // Releases the local reference to the caller. The caller *must* delete the
    // local reference when it is done with it.
    jobject release() {
        jobject obj = m_obj;
        m_obj = 0;
        return obj;
    }
    JNIEnv* env() const {
        return m_env;
    }
private:
    AutoJObject(); // Not permitted.
    AutoJObject(JNIEnv* env, jobject obj)
        : m_env(env)
        , m_obj(obj) {}
    JNIEnv* m_env;
    jobject m_obj;
    friend AutoJObject getRealObject(JNIEnv*, jobject);
};

// Get the real object stored in the weak reference returned as an
// AutoJObject.
AutoJObject getRealObject(JNIEnv*, jobject);

// Helper method for check java exceptions. Returns true if an exception
// occurred and logs the exception.
bool checkException(JNIEnv* env);

// Create a WTF::String object from a jstring object.
WTF::String jstringToWtfString(JNIEnv*, jstring);
// Returns a local reference to a new jstring. If validOnZeroLength is true then
// passing in an empty WTF String will result in an empty jstring. Otherwise
// an empty WTF String returns 0.
jstring wtfStringToJstring(JNIEnv*, const WTF::String&, bool validOnZeroLength = false);

#if USE(CHROME_NETWORK_STACK)
string16 jstringToString16(JNIEnv*, jstring);

std::string jstringToStdString(JNIEnv*, jstring);
// Returns a local reference to a new jstring. If validOnZeroLength is true then
// passing in an empty std::string will result in an empty jstring. Otherwise
// an empty std::string returns 0.
jstring stdStringToJstring(JNIEnv*, const std::string&, bool validOnZeroLength = false);
#endif

}

#endif
