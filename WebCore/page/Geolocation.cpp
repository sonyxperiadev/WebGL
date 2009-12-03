/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Geolocation.h"

#include "Chrome.h"
#include "Document.h"
#include "DOMWindow.h"
#include "EventNames.h"
#include "Frame.h"
#include "Page.h"
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"
#include "SQLValue.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

static const char permissionDeniedErrorMessage[] = "User denied Geolocation";

Geolocation::GeoNotifier::GeoNotifier(Geolocation* geolocation, PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PassRefPtr<PositionOptions> options)
    : m_geolocation(geolocation)
    , m_successCallback(successCallback)
    , m_errorCallback(errorCallback)
    , m_options(options)
    , m_timer(this, &Geolocation::GeoNotifier::timerFired)
{
    ASSERT(m_geolocation);
    ASSERT(m_successCallback);
    // If no options were supplied from JS, we should have created a default set
    // of options in JSGeolocationCustom.cpp.
    ASSERT(m_options);
}

void Geolocation::GeoNotifier::setFatalError(PassRefPtr<PositionError> error)
{
    // This method is called at most once on a given GeoNotifier object.
    ASSERT(!m_fatalError);
    m_fatalError = error;
    m_timer.startOneShot(0);
}

bool Geolocation::GeoNotifier::hasZeroTimeout() const
{
    return m_options->hasTimeout() && m_options->timeout() == 0;
}

void Geolocation::GeoNotifier::setCachedPosition(Geoposition* cachedPosition)
{
    // We do not take owenership from the caller, but add our own ref count.
    m_cachedPosition = cachedPosition;
    m_timer.startOneShot(0);
}

void Geolocation::GeoNotifier::startTimerIfNeeded()
{
    if (m_options->hasTimeout())
        m_timer.startOneShot(m_options->timeout() / 1000.0);
}

void Geolocation::GeoNotifier::timerFired(Timer<GeoNotifier>*)
{
    m_timer.stop();

    // Cache our pointer to the Geolocation object, as this object could be
    // deleted by a call to clearWatch in a callback.
    Geolocation* geolocation = m_geolocation;

    if (m_fatalError) {
        if (m_errorCallback)
            m_errorCallback->handleEvent(m_fatalError.get());
        // This will cause this notifier to be deleted.
        geolocation->fatalErrorOccurred(this);
        return;
    }

    if (m_cachedPosition) {
        m_successCallback->handleEvent(m_cachedPosition.get());
        geolocation->requestReturnedCachedPosition(this);
        return;
    }

    if (m_errorCallback) {
        RefPtr<PositionError> error = PositionError::create(PositionError::TIMEOUT, "Timeout expired");
        m_errorCallback->handleEvent(error.get());
    }
    geolocation->requestTimedOut(this);
}

void Geolocation::Watchers::set(int id, PassRefPtr<GeoNotifier> notifier)
{
    m_idToNotifierMap.set(id, notifier);
    m_notifierToIdMap.set(notifier, id);
}

void Geolocation::Watchers::remove(int id)
{
    IdToNotifierMap::iterator iter = m_idToNotifierMap.find(id);
    if (iter == m_idToNotifierMap.end())
        return;
    m_notifierToIdMap.remove(iter->second);
    m_idToNotifierMap.remove(iter);
}

void Geolocation::Watchers::remove(GeoNotifier* notifier)
{
    NotifierToIdMap::iterator iter = m_notifierToIdMap.find(notifier);
    if (iter == m_notifierToIdMap.end())
        return;
    m_idToNotifierMap.remove(iter->second);
    m_notifierToIdMap.remove(iter);
}

bool Geolocation::Watchers::contains(GeoNotifier* notifier) const
{
    return m_notifierToIdMap.contains(notifier);
}

void Geolocation::Watchers::clear()
{
    m_idToNotifierMap.clear();
    m_notifierToIdMap.clear();
}

bool Geolocation::Watchers::isEmpty() const
{
    return m_idToNotifierMap.isEmpty();
}

void Geolocation::Watchers::getNotifiersVector(Vector<RefPtr<GeoNotifier> >& copy) const
{
    copyValuesToVector(m_idToNotifierMap, copy);
}

static const char* databaseName = "/CachedPosition.db";

class CachedPositionManager {
  public:
    CachedPositionManager()
    {
        if (s_instances++ == 0) {
            s_cachedPosition = new RefPtr<Geoposition>;
            *s_cachedPosition = readFromDB();
        }
    }
    ~CachedPositionManager()
    {
        if (--s_instances == 0) {
            if (*s_cachedPosition)
                writeToDB(s_cachedPosition->get());
            delete s_cachedPosition;
        }
    }
    void setCachedPosition(Geoposition* cachedPosition)
    {
        // We do not take owenership from the caller, but add our own ref count.
        *s_cachedPosition = cachedPosition;
    }
    Geoposition* cachedPosition()
    {
        return s_cachedPosition->get();
    }
    static void setDatabasePath(String databasePath)
    {
        if (!s_databaseFile)
            s_databaseFile = new String;
        *s_databaseFile = databasePath + databaseName;
        // If we don't have have a cached position, attempt to read one from the
        // DB at the new path.
        if (s_instances && *s_cachedPosition == 0)
            *s_cachedPosition = readFromDB();
    }

  private:
    static PassRefPtr<Geoposition> readFromDB()
    {
        SQLiteDatabase database;
        if (!s_databaseFile || !database.open(*s_databaseFile))
            return 0;

        // Create the table here, such that even if we've just created the
        // DB, the commands below should succeed.
        if (!database.executeCommand("CREATE TABLE IF NOT EXISTS CachedPosition ("
                "latitude REAL NOT NULL, "
                "longitude REAL NOT NULL, "
                "altitude REAL, "
                "accuracy REAL NOT NULL, "
                "altitudeAccuracy REAL, "
                "heading REAL, "
                "speed REAL, "
                "timestamp INTEGER NOT NULL)"))
            return 0;

        SQLiteStatement statement(database, "SELECT * FROM CachedPosition");
        if (statement.prepare() != SQLResultOk)
            return 0;

        if (statement.step() != SQLResultRow)
            return 0;

        bool providesAltitude = statement.getColumnValue(2).type() != SQLValue::NullValue;
        bool providesAltitudeAccuracy = statement.getColumnValue(4).type() != SQLValue::NullValue;
        bool providesHeading = statement.getColumnValue(5).type() != SQLValue::NullValue;
        bool providesSpeed = statement.getColumnValue(6).type() != SQLValue::NullValue;
        RefPtr<Coordinates> coordinates = Coordinates::create(statement.getColumnDouble(0),  // latitude
                                                              statement.getColumnDouble(1),  // longitude
                                                              providesAltitude, statement.getColumnDouble(2), // altitude
                                                              statement.getColumnDouble(3),  // accuracy
                                                              providesAltitudeAccuracy, statement.getColumnDouble(4), // altitudeAccuracy
                                                              providesHeading, statement.getColumnDouble(5), // heading
                                                              providesSpeed, statement.getColumnDouble(6)); // speed
        return Geoposition::create(coordinates.release(), statement.getColumnInt64(7));  // timestamp
    }
    static void writeToDB(Geoposition* position)
    {
        ASSERT(position);

        SQLiteDatabase database;
        if (!s_databaseFile || !database.open(*s_databaseFile))
            return;

        SQLiteTransaction transaction(database);

        if (!database.executeCommand("DELETE FROM CachedPosition"))
            return;

        SQLiteStatement statement(database, "INSERT INTO CachedPosition ("
            "latitude, "
            "longitude, "
            "altitude, "
            "accuracy, "
            "altitudeAccuracy, "
            "heading, "
            "speed, "
            "timestamp) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
        if (statement.prepare() != SQLResultOk)
            return;

        statement.bindDouble(1, position->coords()->latitude());
        statement.bindDouble(2, position->coords()->longitude());
        if (position->coords()->canProvideAltitude())
            statement.bindDouble(3, position->coords()->altitude());
        else
            statement.bindNull(3);
        statement.bindDouble(4, position->coords()->accuracy());
        if (position->coords()->canProvideAltitudeAccuracy())
            statement.bindDouble(5, position->coords()->altitudeAccuracy());
        else
            statement.bindNull(5);
        if (position->coords()->canProvideHeading())
            statement.bindDouble(6, position->coords()->heading());
        else
            statement.bindNull(6);
        if (position->coords()->canProvideSpeed())
            statement.bindDouble(7, position->coords()->speed());
        else
            statement.bindNull(7);
        statement.bindInt64(8, position->timestamp());
        if (!statement.executeCommand())
            return;

        transaction.commit();
    }
    static int s_instances;
    static RefPtr<Geoposition>* s_cachedPosition;
    static String* s_databaseFile;
};

int CachedPositionManager::s_instances = 0;
RefPtr<Geoposition>* CachedPositionManager::s_cachedPosition;
String* CachedPositionManager::s_databaseFile = 0;


Geolocation::Geolocation(Frame* frame)
    : EventListener(GeolocationEventListenerType)
    , m_frame(frame)
    , m_service(GeolocationService::create(this))
    , m_allowGeolocation(Unknown)
    , m_shouldClearCache(false)
    , m_cachedPositionManager(new CachedPositionManager)
{
    if (!m_frame)
        return;
    ASSERT(m_frame->document());
    m_frame->document()->setUsingGeolocation(true);

    if (m_frame->domWindow())
        m_frame->domWindow()->addEventListener(eventNames().unloadEvent, this, false);
}

Geolocation::~Geolocation()
{
    if (m_frame && m_frame->domWindow())
        m_frame->domWindow()->removeEventListener(eventNames().unloadEvent, this, false);
}

void Geolocation::disconnectFrame()
{
    m_service->stopUpdating();
    if (m_frame && m_frame->document())
        m_frame->document()->setUsingGeolocation(false);
    m_frame = 0;

    delete m_cachedPositionManager;
}

void Geolocation::getCurrentPosition(PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PassRefPtr<PositionOptions> options)
{
    RefPtr<GeoNotifier> notifier = startRequest(successCallback, errorCallback, options);
    ASSERT(notifier);

    m_oneShots.add(notifier);
}

int Geolocation::watchPosition(PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PassRefPtr<PositionOptions> options)
{
    RefPtr<GeoNotifier> notifier = startRequest(successCallback, errorCallback, options);
    ASSERT(notifier);

    static int nextAvailableWatchId = 1;
    // In case of overflow, make sure the ID remains positive, but reuse the ID values.
    if (nextAvailableWatchId < 1)
        nextAvailableWatchId = 1;
    m_watchers.set(nextAvailableWatchId, notifier.release());
    return nextAvailableWatchId++;
}

PassRefPtr<Geolocation::GeoNotifier> Geolocation::startRequest(PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PassRefPtr<PositionOptions> options)
{
    RefPtr<GeoNotifier> notifier = GeoNotifier::create(this, successCallback, errorCallback, options);

    // Check whether permissions have already been denied. Note that if this is the case,
    // the permission state can not change again in the lifetime of this page.
    if (isDenied())
        notifier->setFatalError(PositionError::create(PositionError::PERMISSION_DENIED, permissionDeniedErrorMessage));
    else {
        if (haveSuitableCachedPosition(notifier->m_options.get())) {
            ASSERT(m_cachedPositionManager->cachedPosition());
            if (isAllowed())
                notifier->setCachedPosition(m_cachedPositionManager->cachedPosition());
            else {
                m_requestsAwaitingCachedPosition.add(notifier);
                requestPermission();
            }
        } else {
            if (notifier->hasZeroTimeout() || m_service->startUpdating(notifier->m_options.get()))
                notifier->startTimerIfNeeded();
            else
                notifier->setFatalError(PositionError::create(PositionError::UNKNOWN_ERROR, "Failed to start Geolocation service"));
        }
    }

    return notifier.release();
}

void Geolocation::fatalErrorOccurred(Geolocation::GeoNotifier* notifier)
{
    // This request has failed fatally. Remove it from our lists.
    m_oneShots.remove(notifier);
    m_watchers.remove(notifier);

    if (!hasListeners())
        m_service->stopUpdating();
}

void Geolocation::requestTimedOut(GeoNotifier* notifier)
{
    // If this is a one-shot request, stop it.
    m_oneShots.remove(notifier);

    if (!hasListeners())
        m_service->stopUpdating();
}

void Geolocation::requestReturnedCachedPosition(GeoNotifier* notifier)
{
    // If this is a one-shot request, stop it.
    if (m_oneShots.contains(notifier)) {
        m_oneShots.remove(notifier);
        if (!hasListeners())
            m_service->stopUpdating();
        return;
    }

    // Otherwise, if the watch still exists, start the service to get updates.
    if (m_watchers.contains(notifier)) {
        if (m_service->startUpdating(notifier->m_options.get()))
            notifier->startTimerIfNeeded();
        else
            notifier->setFatalError(PositionError::create(PositionError::UNKNOWN_ERROR, "Failed to start Geolocation service"));
    }
}

bool Geolocation::haveSuitableCachedPosition(PositionOptions* options)
{
    if (m_cachedPositionManager->cachedPosition() == 0)
        return false;
    if (!options->hasMaximumAge())
        return true;
    if (options->maximumAge() == 0)
        return false;
    DOMTimeStamp currentTimeMillis = currentTime() * 1000.0;
    return m_cachedPositionManager->cachedPosition()->timestamp() > currentTimeMillis - options->maximumAge();
}

void Geolocation::clearWatch(int watchId)
{
    m_watchers.remove(watchId);
    
    if (!hasListeners())
        m_service->stopUpdating();
}

void Geolocation::suspend()
{
    if (hasListeners())
        m_service->suspend();
}

void Geolocation::resume()
{
    if (hasListeners())
        m_service->resume();
}

void Geolocation::setIsAllowed(bool allowed)
{
    // This may be due to either a new position from the service, or a cached
    // position.
    m_allowGeolocation = allowed ? Yes : No;

    if (!isAllowed()) {
        RefPtr<PositionError> error = PositionError::create(PositionError::PERMISSION_DENIED, permissionDeniedErrorMessage);
        error->setIsFatal(true);
        handleError(error.get());
        return;
    }

    // If the service has a last position, use it to call back for all requests.
    // If any of the requests are waiting for permission for a cached position,
    // the position from the service will be at least as fresh.
    if (m_service->lastPosition())
        makeSuccessCallbacks();
    else {
        GeoNotifierSet::const_iterator end = m_requestsAwaitingCachedPosition.end();
        for (GeoNotifierSet::const_iterator iter = m_requestsAwaitingCachedPosition.begin(); iter != end; ++iter)
            (*iter)->setCachedPosition(m_cachedPositionManager->cachedPosition());
    }
    m_requestsAwaitingCachedPosition.clear();
}

void Geolocation::sendError(Vector<RefPtr<GeoNotifier> >& notifiers, PositionError* error)
{
     Vector<RefPtr<GeoNotifier> >::const_iterator end = notifiers.end();
     for (Vector<RefPtr<GeoNotifier> >::const_iterator it = notifiers.begin(); it != end; ++it) {
         RefPtr<GeoNotifier> notifier = *it;
         
         if (notifier->m_errorCallback)
             notifier->m_errorCallback->handleEvent(error);
     }
}

void Geolocation::sendPosition(Vector<RefPtr<GeoNotifier> >& notifiers, Geoposition* position)
{
    Vector<RefPtr<GeoNotifier> >::const_iterator end = notifiers.end();
    for (Vector<RefPtr<GeoNotifier> >::const_iterator it = notifiers.begin(); it != end; ++it) {
        RefPtr<GeoNotifier> notifier = *it;
        ASSERT(notifier->m_successCallback);
        
        notifier->m_timer.stop();
        notifier->m_successCallback->handleEvent(position);
    }
}

void Geolocation::stopTimer(Vector<RefPtr<GeoNotifier> >& notifiers)
{
    Vector<RefPtr<GeoNotifier> >::const_iterator end = notifiers.end();
    for (Vector<RefPtr<GeoNotifier> >::const_iterator it = notifiers.begin(); it != end; ++it) {
        RefPtr<GeoNotifier> notifier = *it;
        notifier->m_timer.stop();
    }
}

void Geolocation::stopTimersForOneShots()
{
    Vector<RefPtr<GeoNotifier> > copy;
    copyToVector(m_oneShots, copy);
    
    stopTimer(copy);
}

void Geolocation::stopTimersForWatchers()
{
    Vector<RefPtr<GeoNotifier> > copy;
    m_watchers.getNotifiersVector(copy);
    
    stopTimer(copy);
}

void Geolocation::stopTimers()
{
    stopTimersForOneShots();
    stopTimersForWatchers();
}

void Geolocation::handleError(PositionError* error)
{
    ASSERT(error);
    
    Vector<RefPtr<GeoNotifier> > oneShotsCopy;
    copyToVector(m_oneShots, oneShotsCopy);

    Vector<RefPtr<GeoNotifier> > watchersCopy;
    m_watchers.getNotifiersVector(watchersCopy);

    // Clear the lists before we make the callbacks, to avoid clearing notifiers
    // added by calls to Geolocation methods from the callbacks, and to prevent
    // further callbacks to these notifiers.
    m_oneShots.clear();
    if (error->isFatal())
        m_watchers.clear();

    sendError(oneShotsCopy, error);
    sendError(watchersCopy, error);

    if (!hasListeners())
        m_service->stopUpdating();
}

void Geolocation::requestPermission()
{
    if (m_allowGeolocation > Unknown)
        return;

    if (!m_frame)
        return;
    
    Page* page = m_frame->page();
    if (!page)
        return;
    
    m_allowGeolocation = InProgress;

    // Ask the chrome: it maintains the geolocation challenge policy itself.
    page->chrome()->requestGeolocationPermissionForFrame(m_frame, this);
}

void Geolocation::geolocationServicePositionChanged(GeolocationService* service)
{
    ASSERT_UNUSED(service, service == m_service);
    ASSERT(m_service->lastPosition());

    m_cachedPositionManager->setCachedPosition(m_service->lastPosition());

    // Stop all currently running timers.
    stopTimers();
    
    if (!isAllowed()) {
        // requestPermission() will ask the chrome for permission. This may be
        // implemented synchronously or asynchronously. In both cases,
        // makeSuccessCallbacks() will be called if permission is granted, so
        // there's nothing more to do here.
        requestPermission();
        return;
    }

    makeSuccessCallbacks();
}

void Geolocation::makeSuccessCallbacks()
{
    ASSERT(m_service->lastPosition());
    ASSERT(isAllowed());
    
    Vector<RefPtr<GeoNotifier> > oneShotsCopy;
    copyToVector(m_oneShots, oneShotsCopy);
    
    Vector<RefPtr<GeoNotifier> > watchersCopy;
    m_watchers.getNotifiersVector(watchersCopy);
    
    // Clear the lists before we make the callbacks, to avoid clearing notifiers
    // added by calls to Geolocation methods from the callbacks, and to prevent
    // further callbacks to these notifiers.
    m_oneShots.clear();

    sendPosition(oneShotsCopy, m_service->lastPosition());
    sendPosition(watchersCopy, m_service->lastPosition());

    if (!hasListeners())
        m_service->stopUpdating();
}

void Geolocation::geolocationServiceErrorOccurred(GeolocationService* service)
{
    ASSERT(service->lastError());
    
    handleError(service->lastError());
}

bool Geolocation::operator==(const EventListener& listener)
{
    if (listener.type() != GeolocationEventListenerType)
        return false;
    const Geolocation* geolocation = static_cast<const Geolocation*>(&listener);
    return m_frame == geolocation->m_frame;
}

void Geolocation::handleEvent(ScriptExecutionContext*, Event* event)
{
    ASSERT_UNUSED(event, event->type() == eventNames().unloadEvent);
    // Cancel any ongoing requests on page unload. This is required to release
    // references to JS callbacks in the page, to allow the frame to be cleaned up
    // by WebKit.
    m_oneShots.clear();
    m_watchers.clear();
}

void Geolocation::setDatabasePath(String databasePath)
{
    CachedPositionManager::setDatabasePath(databasePath);
}

} // namespace WebCore
