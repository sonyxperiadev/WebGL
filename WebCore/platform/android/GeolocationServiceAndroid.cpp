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
#include "GeolocationServiceAndroid.h"

#include <JNIHelp.h>  // For jniRegisterNativeMethods
#include <jni_utility.h>  // For getJNIEnv

#include "Frame.h"
#include "Geoposition.h"
#include "PlatformBridge.h"
#include "PositionError.h"
#include "PositionOptions.h"
#include "WebViewCore.h"
#include <wtf/CurrentTime.h>

using JSC::Bindings::getJNIEnv;
using std::max;

namespace WebCore {

// GeolocationServiceBridge is the bridge to the Java implementation. It manages
// the lifetime of the Java object. It is an implementation detail of
// GeolocationServiceAndroid.
class GeolocationServiceBridge {
public:
    typedef GeolocationServiceAndroid ListenerInterface;
    GeolocationServiceBridge(ListenerInterface* listener);
    ~GeolocationServiceBridge();

    void start();
    void stop();
    void setEnableGps(bool enable);

    // Static wrapper functions to hide JNI nastiness.
    static void newLocationAvailable(JNIEnv *env, jclass, jlong nativeObject, jobject location);
    static void newErrorAvailable(JNIEnv *env, jclass, jlong nativeObject, jstring message);
    static PassRefPtr<Geoposition> convertLocationToGeoposition(JNIEnv *env, const jobject &location);

private:
    void startJavaImplementation();
    void stopJavaImplementation();

    ListenerInterface* m_listener;
    jobject m_javaGeolocationServiceObject;
};

static const char* kJavaGeolocationServiceClass = "android/webkit/GeolocationService";
enum kJavaGeolocationServiceClassMethods {
    GEOLOCATION_SERVICE_METHOD_INIT = 0,
    GEOLOCATION_SERVICE_METHOD_START,
    GEOLOCATION_SERVICE_METHOD_STOP,
    GEOLOCATION_SERVICE_METHOD_SET_ENABLE_GPS,
    GEOLOCATION_SERVICE_METHOD_COUNT,
};
static jmethodID javaGeolocationServiceClassMethodIDs[GEOLOCATION_SERVICE_METHOD_COUNT];

static const JNINativeMethod kJavaGeolocationServiceClassNativeMethods[] = {
    { "nativeNewLocationAvailable", "(JLandroid/location/Location;)V",
        (void*) GeolocationServiceBridge::newLocationAvailable },
    { "nativeNewErrorAvailable", "(JLjava/lang/String;)V",
        (void*) GeolocationServiceBridge::newErrorAvailable }
};

static const char *kJavaLocationClass = "android/location/Location";
enum kJavaLocationClassMethods {
    LOCATION_METHOD_GET_LATITUDE = 0,
    LOCATION_METHOD_GET_LONGITUDE,
    LOCATION_METHOD_HAS_ALTITUDE,
    LOCATION_METHOD_GET_ALTITUDE,
    LOCATION_METHOD_HAS_ACCURACY,
    LOCATION_METHOD_GET_ACCURACY,
    LOCATION_METHOD_HAS_BEARING,
    LOCATION_METHOD_GET_BEARING,
    LOCATION_METHOD_HAS_SPEED,
    LOCATION_METHOD_GET_SPEED,
    LOCATION_METHOD_GET_TIME,
    LOCATION_METHOD_COUNT,
};
static jmethodID javaLocationClassMethodIDs[LOCATION_METHOD_COUNT];

GeolocationServiceBridge::GeolocationServiceBridge(ListenerInterface* listener)
    : m_listener(listener)
    , m_javaGeolocationServiceObject(0)
{
    ASSERT(m_listener);
    startJavaImplementation();
}

GeolocationServiceBridge::~GeolocationServiceBridge()
{
    stop();
    stopJavaImplementation();
}

void GeolocationServiceBridge::start()
{
    ASSERT(m_javaGeolocationServiceObject);
    getJNIEnv()->CallVoidMethod(m_javaGeolocationServiceObject,
                                javaGeolocationServiceClassMethodIDs[GEOLOCATION_SERVICE_METHOD_START]);
}

void GeolocationServiceBridge::stop()
{
    ASSERT(m_javaGeolocationServiceObject);
    getJNIEnv()->CallVoidMethod(m_javaGeolocationServiceObject,
                                javaGeolocationServiceClassMethodIDs[GEOLOCATION_SERVICE_METHOD_STOP]);
}

void GeolocationServiceBridge::setEnableGps(bool enable)
{
    ASSERT(m_javaGeolocationServiceObject);
    getJNIEnv()->CallVoidMethod(m_javaGeolocationServiceObject,
                                javaGeolocationServiceClassMethodIDs[GEOLOCATION_SERVICE_METHOD_SET_ENABLE_GPS],
                                enable);
}

void GeolocationServiceBridge::newLocationAvailable(JNIEnv* env, jclass, jlong nativeObject, jobject location)
{
    ASSERT(nativeObject);
    ASSERT(location);
    GeolocationServiceBridge* object = reinterpret_cast<GeolocationServiceBridge*>(nativeObject);
    object->m_listener->newPositionAvailable(convertLocationToGeoposition(env, location));
}

void GeolocationServiceBridge::newErrorAvailable(JNIEnv* env, jclass, jlong nativeObject, jstring message)
{
    GeolocationServiceBridge* object = reinterpret_cast<GeolocationServiceBridge*>(nativeObject);
    RefPtr<PositionError> error =
        PositionError::create(PositionError::POSITION_UNAVAILABLE, android::to_string(env, message));
    object->m_listener->newErrorAvailable(error.release());
}

PassRefPtr<Geoposition> GeolocationServiceBridge::convertLocationToGeoposition(JNIEnv *env,
                                                                               const jobject &location)
{
    // altitude is optional and may not be supplied.
    bool hasAltitude =
        env->CallBooleanMethod(location, javaLocationClassMethodIDs[LOCATION_METHOD_HAS_ALTITUDE]);
    double altitude =
        hasAltitude ?
        env->CallDoubleMethod(location, javaLocationClassMethodIDs[LOCATION_METHOD_GET_ALTITUDE]) :
        0.0;
    // accuracy is required, but is not supplied by the emulator.
    double accuracy =
        env->CallBooleanMethod(location, javaLocationClassMethodIDs[LOCATION_METHOD_HAS_ACCURACY]) ?
        env->CallFloatMethod(location, javaLocationClassMethodIDs[LOCATION_METHOD_GET_ACCURACY]) :
        0.0;
    // heading is optional and may not be supplied.
    bool hasHeading =
        env->CallBooleanMethod(location, javaLocationClassMethodIDs[LOCATION_METHOD_HAS_BEARING]);
    double heading =
        hasHeading ?
        env->CallFloatMethod(location, javaLocationClassMethodIDs[LOCATION_METHOD_GET_BEARING]) :
        0.0;
    // speed is optional and may not be supplied.
    bool hasSpeed =
        env->CallBooleanMethod(location, javaLocationClassMethodIDs[LOCATION_METHOD_HAS_SPEED]);
    double speed =
        hasSpeed ?
        env->CallFloatMethod(location, javaLocationClassMethodIDs[LOCATION_METHOD_GET_SPEED]) :
        0.0;

    RefPtr<Coordinates> newCoordinates = WebCore::Coordinates::create(
        env->CallDoubleMethod(location, javaLocationClassMethodIDs[LOCATION_METHOD_GET_LATITUDE]),
        env->CallDoubleMethod(location, javaLocationClassMethodIDs[LOCATION_METHOD_GET_LONGITUDE]),
        hasAltitude, altitude,
        accuracy,
        false, 0.0,  // altitudeAccuracy not provided.
        hasHeading, heading,
        hasSpeed, speed);

    return WebCore::Geoposition::create(
         newCoordinates.release(),
         env->CallLongMethod(location, javaLocationClassMethodIDs[LOCATION_METHOD_GET_TIME]));
}

void GeolocationServiceBridge::startJavaImplementation()
{
    JNIEnv* env = getJNIEnv();

    // Get the Java GeolocationService class.
    jclass javaGeolocationServiceClass = env->FindClass(kJavaGeolocationServiceClass);
    ASSERT(javaGeolocationServiceClass);

    // Set up the methods we wish to call on the Java GeolocationService class.
    javaGeolocationServiceClassMethodIDs[GEOLOCATION_SERVICE_METHOD_INIT] =
            env->GetMethodID(javaGeolocationServiceClass, "<init>", "(J)V");
    javaGeolocationServiceClassMethodIDs[GEOLOCATION_SERVICE_METHOD_START] =
            env->GetMethodID(javaGeolocationServiceClass, "start", "()V");
    javaGeolocationServiceClassMethodIDs[GEOLOCATION_SERVICE_METHOD_STOP] =
            env->GetMethodID(javaGeolocationServiceClass, "stop", "()V");
    javaGeolocationServiceClassMethodIDs[GEOLOCATION_SERVICE_METHOD_SET_ENABLE_GPS] =
            env->GetMethodID(javaGeolocationServiceClass, "setEnableGps", "(Z)V");

    // Create the Java GeolocationService object.
    jlong nativeObject = reinterpret_cast<jlong>(this);
    jobject object = env->NewObject(javaGeolocationServiceClass,
                                    javaGeolocationServiceClassMethodIDs[GEOLOCATION_SERVICE_METHOD_INIT],
                                    nativeObject);

    m_javaGeolocationServiceObject = getJNIEnv()->NewGlobalRef(object);
    ASSERT(m_javaGeolocationServiceObject);

    // Register to handle calls to native methods of the Java GeolocationService
    // object. We register once only.
    static int registered = jniRegisterNativeMethods(env,
                                                     kJavaGeolocationServiceClass,
                                                     kJavaGeolocationServiceClassNativeMethods,
                                                     NELEM(kJavaGeolocationServiceClassNativeMethods));
    ASSERT(registered == NELEM(kJavaGeolocationServiceClassNativeMethods));

    // Set up the methods we wish to call on the Java Location class.
    jclass javaLocationClass = env->FindClass(kJavaLocationClass);
    ASSERT(javaLocationClass);
    javaLocationClassMethodIDs[LOCATION_METHOD_GET_LATITUDE] =
        env->GetMethodID(javaLocationClass, "getLatitude", "()D");
    javaLocationClassMethodIDs[LOCATION_METHOD_GET_LONGITUDE] =
        env->GetMethodID(javaLocationClass, "getLongitude", "()D");
    javaLocationClassMethodIDs[LOCATION_METHOD_HAS_ALTITUDE] =
        env->GetMethodID(javaLocationClass, "hasAltitude", "()Z");
    javaLocationClassMethodIDs[LOCATION_METHOD_GET_ALTITUDE] =
        env->GetMethodID(javaLocationClass, "getAltitude", "()D");
    javaLocationClassMethodIDs[LOCATION_METHOD_HAS_ACCURACY] =
        env->GetMethodID(javaLocationClass, "hasAccuracy", "()Z");
    javaLocationClassMethodIDs[LOCATION_METHOD_GET_ACCURACY] =
        env->GetMethodID(javaLocationClass, "getAccuracy", "()F");
    javaLocationClassMethodIDs[LOCATION_METHOD_HAS_BEARING] =
        env->GetMethodID(javaLocationClass, "hasBearing", "()Z");
    javaLocationClassMethodIDs[LOCATION_METHOD_GET_BEARING] =
        env->GetMethodID(javaLocationClass, "getBearing", "()F");
    javaLocationClassMethodIDs[LOCATION_METHOD_HAS_SPEED] =
        env->GetMethodID(javaLocationClass, "hasSpeed", "()Z");
    javaLocationClassMethodIDs[LOCATION_METHOD_GET_SPEED] =
        env->GetMethodID(javaLocationClass, "getSpeed", "()F");
    javaLocationClassMethodIDs[LOCATION_METHOD_GET_TIME] =
        env->GetMethodID(javaLocationClass, "getTime", "()J");
}

void GeolocationServiceBridge::stopJavaImplementation()
{
    // Called by GeolocationServiceAndroid on WebKit thread.
    ASSERT(m_javaGeolocationServiceObject);
    getJNIEnv()->DeleteGlobalRef(m_javaGeolocationServiceObject);
}

// GeolocationServiceAndroid is the Android implmentation of Geolocation
// service. Each object of this class owns an object of type
// GeolocationServiceBridge, which in turn owns a Java GeolocationService
// object. Therefore, there is a 1:1 mapping between Geolocation,
// GeolocationServiceAndroid, GeolocationServiceBridge and Java
// GeolocationService objects. In the case where multiple Geolocation objects
// exist simultaneously, the corresponsing Java GeolocationService objects all
// register with the platform location service. It is the platform service that
// handles making sure that updates are passed to all Geolocation objects.
GeolocationService* GeolocationServiceAndroid::create(GeolocationServiceClient* client)
{
    return new GeolocationServiceAndroid(client);
}

GeolocationService::FactoryFunction* GeolocationService::s_factoryFunction = &GeolocationServiceAndroid::create;

GeolocationServiceAndroid::GeolocationServiceAndroid(GeolocationServiceClient* client)
    : GeolocationService(client)
    , m_timer(this, &GeolocationServiceAndroid::timerFired)
    , m_javaBridge(0)
{
}

bool GeolocationServiceAndroid::startUpdating(PositionOptions* options)
{
    // This method is called every time a new watch or one-shot position request
    // is started. If we already have a position or an error, call back
    // immediately.
    if (m_lastPosition || m_lastError) {
        ASSERT(m_javaBridge);
        m_timer.startOneShot(0);
    }

    // Lazilly create the Java object.
    bool haveJavaBridge = m_javaBridge;
    if (!haveJavaBridge)
        m_javaBridge.set(new GeolocationServiceBridge(this));
    ASSERT(m_javaBridge);

    // On Android, high power == GPS. Set whether to use GPS before we start the
    // implementation.
    // FIXME: Checking for the presence of options will probably not be required
    // once WebKit bug 27254 is fixed.
    if (options && options->enableHighAccuracy())
        m_javaBridge->setEnableGps(true);

    // We need only start the service when it's first created.
    if (!haveJavaBridge) {
        // If the browser is paused, don't start the service. It will be started
        // when we get the call to resume.
        if (!PlatformBridge::isWebViewPaused())
            m_javaBridge->start();
    }

    return true;
}

void GeolocationServiceAndroid::stopUpdating()
{
    // Called when the Geolocation object has no watches or one shots in
    // progress.
    m_javaBridge.clear();
    // Reset last position and error to make sure that we always try to get a
    // new position from the system service when a request is first made.
    m_lastPosition = 0;
    m_lastError = 0;
}

void GeolocationServiceAndroid::suspend()
{
    // If the Geolocation object has called stopUpdating, and hence the bridge
    // object has been destroyed, we should not receive calls to this method
    // until startUpdating is called again and the bridge is recreated.
    ASSERT(m_javaBridge);
    m_javaBridge->stop();
}

void GeolocationServiceAndroid::resume()
{
    // If the Geolocation object has called stopUpdating, and hence the bridge
    // object has been destroyed, we should not receive calls to this method
    // until startUpdating is called again and the bridge is recreated.
    ASSERT(m_javaBridge);
    m_javaBridge->start();
}

// Note that there is no guarantee that subsequent calls to this method offer a
// more accurate or updated position.
void GeolocationServiceAndroid::newPositionAvailable(PassRefPtr<Geoposition> position)
{
    ASSERT(position);
    if (!m_lastPosition
        || isPositionMovement(m_lastPosition.get(), position.get())
        || isPositionMoreAccurate(m_lastPosition.get(), position.get())
        || isPositionMoreTimely(m_lastPosition.get(), position.get())) {
        m_lastPosition = position;
        // Remove the last error.
        m_lastError = 0;
        positionChanged();
    }
}

void GeolocationServiceAndroid::newErrorAvailable(PassRefPtr<PositionError> error)
{
    ASSERT(error);
    // We leave the last position
    m_lastError = error;
    errorOccurred();
}

void GeolocationServiceAndroid::timerFired(Timer<GeolocationServiceAndroid>* timer) {
    ASSERT(&m_timer == timer);
    ASSERT(m_lastPosition || m_lastError);
    if (m_lastPosition)
        positionChanged();
    else
        errorOccurred();
}

bool GeolocationServiceAndroid::isPositionMovement(Geoposition* position1, Geoposition* position2)
{
    ASSERT(position1 && position2);
    // For the small distances in which we are likely concerned, it's reasonable
    // to approximate the distance between the two positions as the sum of the
    // differences in latitude and longitude.
    double delta = fabs(position1->coords()->latitude() - position2->coords()->latitude())
        + fabs(position1->coords()->longitude() - position2->coords()->longitude());
    // Approximate conversion from degrees of arc to metres.
    delta *= 60 * 1852;
    // The threshold is when the distance between the two positions exceeds the
    // worse (larger) of the two accuracies.
    int maxAccuracy = max(position1->coords()->accuracy(), position2->coords()->accuracy());
    return delta > maxAccuracy;
}

bool GeolocationServiceAndroid::isPositionMoreAccurate(Geoposition* position1, Geoposition* position2)
{
    ASSERT(position1 && position2);
    return position2->coords()->accuracy() < position1->coords()->accuracy();
}

bool GeolocationServiceAndroid::isPositionMoreTimely(Geoposition* position1, Geoposition* position2)
{
    ASSERT(position1 && position2);
    DOMTimeStamp currentTimeMillis = WTF::currentTime() * 1000.0;
    DOMTimeStamp maximumAgeMillis = 10 * 60 * 1000;  // 10 minutes
    return currentTimeMillis - position1->timestamp() > maximumAgeMillis;
}

} // namespace WebCore
