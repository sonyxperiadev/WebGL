/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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
#include "MockGeolocationService.h"

#include "Geoposition.h"
#include "PositionError.h"
#include "PositionOptions.h"

namespace WebCore {

MockGeolocationService::MockGeolocationServiceSet* MockGeolocationService::s_instances = 0;
RefPtr<Geoposition>* MockGeolocationService::s_lastPosition;
RefPtr<PositionError>* MockGeolocationService::s_lastError;

GeolocationService* MockGeolocationService::create(GeolocationServiceClient* client)
{
    initStatics();
    return new MockGeolocationService(client);
}

MockGeolocationService::MockGeolocationService(GeolocationServiceClient* client)
    : GeolocationService(client)
    , m_timer(this, &MockGeolocationService::timerFired)
{
    s_instances->add(this);
}

MockGeolocationService::~MockGeolocationService()
{
    MockGeolocationServiceSet::iterator iter = s_instances->find(this);
    ASSERT(iter != s_instances->end());
    s_instances->remove(iter);
    cleanUpStatics();
}

void MockGeolocationService::setPosition(PassRefPtr<Geoposition> position)
{
    initStatics();
    *s_lastPosition = position;
    *s_lastError = 0;
    makeGeolocationCallbackFromAllInstances();
}

void MockGeolocationService::setError(PassRefPtr<PositionError> error)
{
    initStatics();
    *s_lastError = error;
    *s_lastPosition = 0;
    makeGeolocationCallbackFromAllInstances();
}

bool MockGeolocationService::startUpdating(PositionOptions* options)
{
    m_timer.startOneShot(0);
    return true;
}

void MockGeolocationService::timerFired(Timer<MockGeolocationService>* timer)
{
    ASSERT(timer == m_timer);
    makeGeolocationCallback();
}

void MockGeolocationService::makeGeolocationCallbackFromAllInstances()
{
    MockGeolocationServiceSet::const_iterator end = s_instances->end();
    for (MockGeolocationServiceSet::const_iterator iter = s_instances->begin();
         iter != end;
         ++iter) {
        (*iter)->makeGeolocationCallback();
    }
}

void MockGeolocationService::makeGeolocationCallback()
{
    if (*s_lastPosition) {
        positionChanged();
    } else if (*s_lastError) {
        errorOccurred();
    }
}

void MockGeolocationService::initStatics()
{
    if (s_instances == 0) {
        s_instances = new MockGeolocationServiceSet;
        s_lastPosition = new RefPtr<Geoposition>;
        s_lastError = new RefPtr<PositionError>;
    }
}

void MockGeolocationService::cleanUpStatics()
{
    if (s_instances->size() == 0) {
        delete s_instances;
        s_instances = 0;
        delete s_lastPosition;
        delete s_lastError;
    }
}

} // namespace WebCore
