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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// The functions in this file are used to configure the mock GeolocationService
// for the LayoutTests.

#include "config.h"

#include "Coordinates.h"
#include "GeolocationServiceMock.h"
#include "Geoposition.h"
#include "JavaSharedClient.h"
#include "PositionError.h"
#include "WebCoreJni.h"
#include <JNIHelp.h>
#include <JNIUtility.h>
#include <wtf/CurrentTime.h>

using namespace WebCore;

namespace android {

static const char* javaMockGeolocationClass = "android/webkit/MockGeolocation";

static void setPosition(JNIEnv* env, jobject, double latitude, double longitude, double accuracy)
{
    RefPtr<Coordinates> coordinates = Coordinates::create(latitude,
                                                          longitude,
                                                          false, 0.0,  // altitude,
                                                          accuracy,
                                                          false, 0.0,  // altitudeAccuracy,
                                                          false, 0.0,  // heading
                                                          false, 0.0);  // speed
    RefPtr<Geoposition> position = Geoposition::create(coordinates.release(), WTF::currentTime());
    GeolocationServiceMock::setPosition(position.release());
}

static void setError(JNIEnv* env, jobject, int code, jstring message)
{
    PositionError::ErrorCode codeEnum = static_cast<PositionError::ErrorCode>(code);
    String messageString = to_string(env, message);
    RefPtr<PositionError> error = PositionError::create(codeEnum, messageString);
    GeolocationServiceMock::setError(error.release());
}

static JNINativeMethod gMockGeolocationMethods[] = {
    { "nativeSetPosition", "(DDD)V", (void*) setPosition },
    { "nativeSetError", "(ILjava/lang/String;)V", (void*) setError }
};

int register_mock_geolocation(JNIEnv* env)
{
    jclass mockGeolocation = env->FindClass(javaMockGeolocationClass);
    LOG_ASSERT(mockGeolocation, "Unable to find class");
    return jniRegisterNativeMethods(env, javaMockGeolocationClass, gMockGeolocationMethods, NELEM(gMockGeolocationMethods));
}

}
