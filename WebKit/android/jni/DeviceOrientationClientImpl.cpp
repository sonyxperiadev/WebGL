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
#include "DeviceOrientationClientImpl.h"

#include "WebViewCore.h"
#include <DeviceOrientationController.h>
#include <Frame.h>
#include <JNIHelp.h>

namespace android {

using JSC::Bindings::getJNIEnv;

enum javaDeviceOrientationServiceClassMethods {
    DeviceOrientationServiceMethodStart = 0,
    DeviceOrientationServiceMethodStop,
    DeviceOrientationServiceMethodSuspend,
    DeviceOrientationServiceMethodResume,
    DeviceOrientationServiceMethodCount
};
static jmethodID javaDeviceOrientationServiceClassMethodIDs[DeviceOrientationServiceMethodCount];

DeviceOrientationClientImpl::DeviceOrientationClientImpl(WebViewCore* webViewCore)
    : m_webViewCore(webViewCore)
    , m_javaDeviceOrientationServiceObject(0)
{
    ASSERT(m_webViewCore);
}

DeviceOrientationClientImpl::~DeviceOrientationClientImpl()
{
    releaseJavaInstance();
}

jobject DeviceOrientationClientImpl::getJavaInstance()
{
    // Lazily get the Java object. We can't do this until the WebViewCore is all
    // set up.
    if (m_javaDeviceOrientationServiceObject)
        return m_javaDeviceOrientationServiceObject;

    JNIEnv* env = getJNIEnv();

    ASSERT(m_webViewCore);
    jobject object = m_webViewCore->getDeviceOrientationService();

    // Get the Java DeviceOrientationService class.
    jclass javaDeviceOrientationServiceClass = env->GetObjectClass(object);
    ASSERT(javaDeviceOrientationServiceClass);

    // Set up the methods we wish to call on the Java DeviceOrientationService
    // class.
    javaDeviceOrientationServiceClassMethodIDs[DeviceOrientationServiceMethodStart] =
        env->GetMethodID(javaDeviceOrientationServiceClass, "start", "()V");
    javaDeviceOrientationServiceClassMethodIDs[DeviceOrientationServiceMethodStop] =
        env->GetMethodID(javaDeviceOrientationServiceClass, "stop", "()V");
    javaDeviceOrientationServiceClassMethodIDs[DeviceOrientationServiceMethodSuspend] =
        env->GetMethodID(javaDeviceOrientationServiceClass, "suspend", "()V");
    javaDeviceOrientationServiceClassMethodIDs[DeviceOrientationServiceMethodResume] =
        env->GetMethodID(javaDeviceOrientationServiceClass, "resume", "()V");
    env->DeleteLocalRef(javaDeviceOrientationServiceClass);

    m_javaDeviceOrientationServiceObject = getJNIEnv()->NewGlobalRef(object);
    getJNIEnv()->DeleteLocalRef(object);

    ASSERT(m_javaDeviceOrientationServiceObject);
    return m_javaDeviceOrientationServiceObject;
}

void DeviceOrientationClientImpl::releaseJavaInstance()
{
    ASSERT(m_javaDeviceOrientationServiceObject);
    getJNIEnv()->DeleteGlobalRef(m_javaDeviceOrientationServiceObject);
}

void DeviceOrientationClientImpl::startUpdating()
{
    getJNIEnv()->CallVoidMethod(getJavaInstance(),
        javaDeviceOrientationServiceClassMethodIDs[DeviceOrientationServiceMethodStart]);
}

void DeviceOrientationClientImpl::stopUpdating()
{
    getJNIEnv()->CallVoidMethod(getJavaInstance(),
        javaDeviceOrientationServiceClassMethodIDs[DeviceOrientationServiceMethodStop]);
}

void DeviceOrientationClientImpl::onOrientationChange(PassRefPtr<DeviceOrientation> orientation)
{
    m_lastOrientation = orientation;
    m_controller->didChangeDeviceOrientation(m_lastOrientation.get());
}

void DeviceOrientationClientImpl::suspend()
{
    getJNIEnv()->CallVoidMethod(getJavaInstance(),
        javaDeviceOrientationServiceClassMethodIDs[DeviceOrientationServiceMethodSuspend]);
}

void DeviceOrientationClientImpl::resume()
{
    getJNIEnv()->CallVoidMethod(getJavaInstance(),
        javaDeviceOrientationServiceClassMethodIDs[DeviceOrientationServiceMethodResume]);
}

} // namespace android
