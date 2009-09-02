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
#include "Frame.h"
#include "Page.h"

namespace WebCore {

static const char* permissionDeniedErrorMessage = "User denied Geolocation";

Geolocation::GeoNotifier::GeoNotifier(Geolocation* geolocation, PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PassRefPtr<PositionOptions> options)
    : m_geolocation(geolocation)
    , m_successCallback(successCallback)
    , m_errorCallback(errorCallback)
    , m_options(options)
    , m_timer(this, &Geolocation::GeoNotifier::timerFired)
    , m_fatalError(0)
{
    ASSERT(m_geolocation);
    ASSERT(m_successCallback);
    // If no options were supplied from JS, we should have created a default set
    // of options in JSGeolocationCustom.cpp.
    ASSERT(m_options);
}

void Geolocation::GeoNotifier::setFatalError(PassRefPtr<PositionError> error)
{
    m_fatalError = error;
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

    if (m_fatalError) {
        if (m_errorCallback) {
            m_errorCallback->handleEvent(m_fatalError.get());
        }
        // This will cause this notifier to be deleted.
        m_geolocation->fatalErrorOccurred(this);
        return;
    }

    if (m_errorCallback) {
        RefPtr<PositionError> error = PositionError::create(PositionError::TIMEOUT, "Timed out");
        m_errorCallback->handleEvent(error.get());
    }
    m_geolocation->requestTimedOut(this);
}

Geolocation::Geolocation(Frame* frame)
    : m_frame(frame)
    , m_service(GeolocationService::create(this))
    , m_allowGeolocation(Unknown)
    , m_shouldClearCache(false)
{
    if (!m_frame)
        return;
    ASSERT(m_frame->document());
    m_frame->document()->setUsingGeolocation(true);
}

void Geolocation::disconnectFrame()
{
    m_service->stopUpdating();
    if (m_frame && m_frame->document())
        m_frame->document()->setUsingGeolocation(false);
    m_frame = 0;
}

void Geolocation::getCurrentPosition(PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PassRefPtr<PositionOptions> options)
{
    RefPtr<GeoNotifier> notifier = makeRequest(successCallback, errorCallback, options);
    ASSERT(notifier);

    m_oneShots.add(notifier);
}

int Geolocation::watchPosition(PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PassRefPtr<PositionOptions> options)
{
    RefPtr<GeoNotifier> notifier = makeRequest(successCallback, errorCallback, options);
    ASSERT(notifier);

    static int sIdentifier = 0;
    m_watchers.set(++sIdentifier, notifier);

    return sIdentifier;
}

PassRefPtr<Geolocation::GeoNotifier> Geolocation::makeRequest(PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PassRefPtr<PositionOptions> options)
{
    RefPtr<GeoNotifier> notifier = GeoNotifier::create(this, successCallback, errorCallback, options);

    // Check whether permissions have already been denied. Note that if this is the case,
    // the permission state can not change again in the lifetime of this page.
    if (isDenied()) {
        notifier->setFatalError(PositionError::create(PositionError::PERMISSION_DENIED, permissionDeniedErrorMessage));
    } else {
        if (m_service->startUpdating(notifier->m_options.get()))
            notifier->startTimerIfNeeded();
        else {
            notifier->setFatalError(PositionError::create(PositionError::UNKNOWN_ERROR, "Failed to start Geolocation service"));
        }
    }

    return notifier.release();
}

void Geolocation::fatalErrorOccurred(Geolocation::GeoNotifier* notifier)
{
    // This request has failed fatally. Remove it from our lists.
    m_oneShots.remove(notifier);
    for (GeoNotifierMap::iterator iter = m_watchers.begin(); iter != m_watchers.end(); ++iter) {
        if (iter->second == notifier) {
            m_watchers.remove(iter);
            break;
        }
    }

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
    m_allowGeolocation = allowed ? Yes : No;
    
    if (isAllowed()) {
        makeSuccessCallbacks();
    } else {
        WTF::RefPtr<WebCore::PositionError> error = PositionError::create(PositionError::PERMISSION_DENIED, permissionDeniedErrorMessage);
        error->setIsFatal(true);
        handleError(error.get());
    }
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
    copyValuesToVector(m_watchers, copy);
    
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
    copyValuesToVector(m_watchers, watchersCopy);

    // Clear the lists before we make the callbacks, to avoid clearing notifiers
    // added by calls to Geolocation methods from the callbacks.
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

void Geolocation::geolocationServicePositionChanged(GeolocationService*)
{
    ASSERT(m_service->lastPosition());

    // Stop all currently running timers.
    stopTimers();

    if (!isAllowed()) {
        // requestPermission() will ask the chrome for permission. This may be
        // implemented synchronously or asynchronously. In both cases,
        // makeSucessCallbacks() will be called if permission is granted, so
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
    copyValuesToVector(m_watchers, watchersCopy);

    // Clear the lists before we make the callbacks, to avoid clearing notifiers
    // added by calls to Geolocation methods from the callbacks.
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

} // namespace WebCore
