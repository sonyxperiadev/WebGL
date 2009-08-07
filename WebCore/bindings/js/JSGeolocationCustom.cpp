/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "JSGeolocation.h"

#include "DOMWindow.h"
#include "ExceptionCode.h"
#include "Geolocation.h"
#include "GeolocationService.h"
#include "JSCustomPositionCallback.h"
#include "JSCustomPositionErrorCallback.h"
#include "JSDOMWindow.h"
#include "PositionOptions.h"

using namespace JSC;

namespace WebCore {

const long PositionOptions::infinity = -1;

static PassRefPtr<PositionCallback> createPositionCallback(ExecState* exec, JSValue value)
{
    JSObject* object = value.getObject();
    if (!object) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return 0;
    }

    Frame* frame = toJSDOMWindow(exec->lexicalGlobalObject())->impl()->frame();
    ASSERT(frame);
    return JSCustomPositionCallback::create(object, frame);
}

static PassRefPtr<PositionErrorCallback> createPositionErrorCallback(ExecState* exec, JSValue value)
{
    // No value is OK.
    if (value.isUndefinedOrNull()) {
        return 0;
    }

    JSObject* object = value.getObject();
    if (!object) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return 0;
    }

    Frame* frame = toJSDOMWindow(exec->lexicalGlobalObject())->impl()->frame();
    ASSERT(frame);
    return JSCustomPositionErrorCallback::create(object, frame);
}

// If value represents a non-negative number, the value is truncated to a long
// and assigned to result, and the function returns true. Otherwise, result is
// not set, and the function returns false.
static bool getNonNegativeLong(ExecState* exec, const JSValue& value, long* result)
{
    if (!value.isNumber() || (value.toNumber(exec) < 0)) {
        return false;
    }
    *result = value.toNumber(exec);
    return true;
}

static PassRefPtr<PositionOptions> createPositionOptions(ExecState* exec, JSValue value)
{
    // Create default options.
    RefPtr<PositionOptions> options = PositionOptions::create();

    if (value.isUndefinedOrNull()) {
        // Use default options.
        return options;
    }

    JSObject* object = value.getObject();
    if (!object) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return 0;
    }

    JSValue enableHighAccuracyValue = object->get(exec, Identifier(exec, "enableHighAccuracy"));
    if (!enableHighAccuracyValue.isUndefinedOrNull()) {
        if (!enableHighAccuracyValue.isBoolean()) {
            setDOMException(exec, TYPE_MISMATCH_ERR);
            return 0;
        }
        options->setEnableHighAccuracy(enableHighAccuracyValue.toBoolean(exec));
    }

    JSValue timeoutValue = object->get(exec, Identifier(exec, "timeout"));
    if (!timeoutValue.isUndefinedOrNull()) {
        long timeout;
        if (getNonNegativeLong(exec, timeoutValue, &timeout))
            options->setTimeout(timeout);
        else {
            setDOMException(exec, TYPE_MISMATCH_ERR);
            return 0;
        }
    }

    JSValue maximumAgeValue = object->get(exec, Identifier(exec, "maximumAge"));
    if (!maximumAgeValue.isUndefinedOrNull()) {
        long maximumAge;
        if (getNonNegativeLong(exec, maximumAgeValue, &maximumAge))
            options->setTimeout(maximumAge);
        else {
            setDOMException(exec, TYPE_MISMATCH_ERR);
            return 0;
        }
    }

    return options;
}

JSValue JSGeolocation::getCurrentPosition(ExecState* exec, const ArgList& args)
{
    // Arguments: PositionCallback, (optional)PositionErrorCallback, (optional)PositionOptions

    RefPtr<PositionCallback> positionCallback = createPositionCallback(exec, args.at(0));
    if (exec->hadException())
        return jsUndefined();
    ASSERT(positionCallback);

    RefPtr<PositionErrorCallback> positionErrorCallback = createPositionErrorCallback(exec, args.at(1));
    if (exec->hadException())
        return jsUndefined();

    RefPtr<PositionOptions> positionOptions = createPositionOptions(exec, args.at(2));
    if (exec->hadException())
        return jsUndefined();
    ASSERT(positionOptions);

    m_impl->getCurrentPosition(positionCallback.release(), positionErrorCallback.release(), positionOptions.release());
    return jsUndefined();
}

JSValue JSGeolocation::watchPosition(ExecState* exec, const ArgList& args)
{
    // Arguments: PositionCallback, (optional)PositionErrorCallback, (optional)PositionOptions

    RefPtr<PositionCallback> positionCallback = createPositionCallback(exec, args.at(0));
    if (exec->hadException())
        return jsUndefined();
    ASSERT(positionCallback);

    RefPtr<PositionErrorCallback> positionErrorCallback = createPositionErrorCallback(exec, args.at(1));
    if (exec->hadException())
        return jsUndefined();

    RefPtr<PositionOptions> positionOptions = createPositionOptions(exec, args.at(2));
    if (exec->hadException())
        return jsUndefined();
    ASSERT(positionOptions);

    int watchID = m_impl->watchPosition(positionCallback.release(), positionErrorCallback.release(), positionOptions.release());
    return jsNumber(exec, watchID);
}

} // namespace WebCore
