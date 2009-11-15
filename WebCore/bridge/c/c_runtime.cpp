/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "c_runtime.h"

#include "c_instance.h"
#include "c_utility.h"
#include "npruntime_impl.h"
#ifdef ANDROID_NPN_SETEXCEPTION
#include "runtime/Error.h"
#endif  // ANDROID_NPN_SETEXCEPTION
#include <runtime/JSLock.h>

namespace JSC {
namespace Bindings {

#ifdef ANDROID_NPN_SETEXCEPTION
/*
 * When throwing an exception, we need to use the current ExecState.
 * The following two methods implement a similar solution to the
 * Objective-C implementation, where _NPN_SetException set a global
 * exception (using SetGlobalException).
 * We then test (using MoveGlobalExceptionToExecState) if the exception
 * is set, after each javascript call that might result in an exception.
 * If the exception is set we throw it with the passed ExecState.
 */

static UString* globalLastException = 0;

void SetGlobalException(const NPUTF8* exception)
{
    if (globalLastException != 0) {
        delete globalLastException;
        globalLastException = 0;
    }
    if (exception != 0)
        globalLastException = new UString(exception);
}

void MoveGlobalExceptionToExecState(ExecState* exec)
{
    if (!globalLastException)
        return;
    JSLock lock(exec);
    throwError(exec, GeneralError, *globalLastException);
    SetGlobalException(0);
}
#endif // ANDROID_NPN_SETEXCEPTION

JSValue CField::valueFromInstance(ExecState* exec, const Instance* inst) const
{
    const CInstance* instance = static_cast<const CInstance*>(inst);
    NPObject* obj = instance->getObject();
    if (obj->_class->getProperty) {
        NPVariant property;
        VOID_TO_NPVARIANT(property);

#ifdef ANDROID_NPN_SETEXCEPTION
        SetGlobalException(0);
#endif  // ANDROID_NPN_SETEXCEPTION
        bool result;
        {
            JSLock::DropAllLocks dropAllLocks(SilenceAssertionsOnly);
            result = obj->_class->getProperty(obj, _fieldIdentifier, &property);
        }
#ifdef ANDROID_NPN_SETEXCEPTION
        MoveGlobalExceptionToExecState(exec);
#endif  // ANDROID_NPN_SETEXCEPTION
        if (result) {
            JSValue result = convertNPVariantToValue(exec, &property, instance->rootObject());
            _NPN_ReleaseVariantValue(&property);
            return result;
        }
    }
    return jsUndefined();
}

void CField::setValueToInstance(ExecState *exec, const Instance *inst, JSValue aValue) const
{
    const CInstance* instance = static_cast<const CInstance*>(inst);
    NPObject* obj = instance->getObject();
    if (obj->_class->setProperty) {
        NPVariant variant;
        convertValueToNPVariant(exec, aValue, &variant);

#ifdef ANDROID_NPN_SETEXCEPTION
        SetGlobalException(0);
#endif  // ANDROID_NPN_SETEXCEPTION
        {
            JSLock::DropAllLocks dropAllLocks(SilenceAssertionsOnly);
            obj->_class->setProperty(obj, _fieldIdentifier, &variant);
        }

        _NPN_ReleaseVariantValue(&variant);
#ifdef ANDROID_NPN_SETEXCEPTION
        MoveGlobalExceptionToExecState(exec);
#endif  // ANDROID_NPN_SETEXCEPTION
    }
}

} }

#endif // ENABLE(NETSCAPE_PLUGIN_API)
