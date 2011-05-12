/*
 * Copyright 2010, The Android Open Source Project
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

#include "config.h"
#include "DeviceMotionClientImpl.h"

#include "WebViewCore.h"
#include <DeviceMotionController.h>
#include <Frame.h>
#include <JNIHelp.h>

namespace android {

using JSC::Bindings::getJNIEnv;

enum javaServiceClassMethods {
    ServiceMethodStart = 0,
    ServiceMethodStop,
    ServiceMethodSuspend,
    ServiceMethodResume,
    ServiceMethodCount
};
static jmethodID javaServiceClassMethodIDs[ServiceMethodCount];

DeviceMotionClientImpl::DeviceMotionClientImpl(WebViewCore* webViewCore)
    : m_webViewCore(webViewCore)
    , m_javaServiceObject(0)
{
    ASSERT(m_webViewCore);
}

DeviceMotionClientImpl::~DeviceMotionClientImpl()
{
    releaseJavaInstance();
}

jobject DeviceMotionClientImpl::getJavaInstance()
{
    // Lazily get the Java object. We can't do this until the WebViewCore is all
    // set up.
    if (m_javaServiceObject)
        return m_javaServiceObject;

    JNIEnv* env = getJNIEnv();

    ASSERT(m_webViewCore);
    jobject object = m_webViewCore->getDeviceMotionService();
    if (!object)
        return 0;

    // Get the Java DeviceMotionService class.
    jclass javaServiceClass = env->GetObjectClass(object);
    ASSERT(javaServiceClass);

    // Set up the methods we wish to call on the Java DeviceMotionService
    // class.
    javaServiceClassMethodIDs[ServiceMethodStart] =
        env->GetMethodID(javaServiceClass, "start", "()V");
    javaServiceClassMethodIDs[ServiceMethodStop] =
        env->GetMethodID(javaServiceClass, "stop", "()V");
    javaServiceClassMethodIDs[ServiceMethodSuspend] =
        env->GetMethodID(javaServiceClass, "suspend", "()V");
    javaServiceClassMethodIDs[ServiceMethodResume] =
        env->GetMethodID(javaServiceClass, "resume", "()V");
    env->DeleteLocalRef(javaServiceClass);

    m_javaServiceObject = getJNIEnv()->NewGlobalRef(object);
    getJNIEnv()->DeleteLocalRef(object);

    ASSERT(m_javaServiceObject);
    return m_javaServiceObject;
}

void DeviceMotionClientImpl::releaseJavaInstance()
{
    if (m_javaServiceObject)
        getJNIEnv()->DeleteGlobalRef(m_javaServiceObject);
}

void DeviceMotionClientImpl::startUpdating()
{
    jobject javaInstance = getJavaInstance();
    if (!javaInstance)
        return;
    getJNIEnv()->CallVoidMethod(javaInstance, javaServiceClassMethodIDs[ServiceMethodStart]);
}

void DeviceMotionClientImpl::stopUpdating()
{
    jobject javaInstance = getJavaInstance();
    if (!javaInstance)
        return;
    getJNIEnv()->CallVoidMethod(javaInstance, javaServiceClassMethodIDs[ServiceMethodStop]);
}

void DeviceMotionClientImpl::onMotionChange(PassRefPtr<DeviceMotionData> motion)
{
    m_lastMotion = motion;
    m_controller->didChangeDeviceMotion(m_lastMotion.get());
}

void DeviceMotionClientImpl::suspend()
{
    jobject javaInstance = getJavaInstance();
    if (!javaInstance)
        return;
    getJNIEnv()->CallVoidMethod(javaInstance, javaServiceClassMethodIDs[ServiceMethodSuspend]);
}

void DeviceMotionClientImpl::resume()
{
    jobject javaInstance = getJavaInstance();
    if (!javaInstance)
        return;
    getJNIEnv()->CallVoidMethod(javaInstance, javaServiceClassMethodIDs[ServiceMethodResume]);
}

} // namespace android
