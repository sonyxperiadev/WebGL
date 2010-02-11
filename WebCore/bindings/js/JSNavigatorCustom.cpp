/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All Rights Reserved.
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSNavigator.h"

#include "ExceptionCode.h"
#include "Navigator.h"
#include <runtime/InternalFunction.h>

#if PLATFORM(ANDROID)
#include "JSCustomApplicationInstalledCallback.h"
#endif

namespace WebCore {

using namespace JSC;

void JSNavigator::markChildren(MarkStack& markStack)
{
    Base::markChildren(markStack);

    JSGlobalData& globalData = *Heap::heap(this)->globalData();

    markDOMObjectWrapper(markStack, globalData, impl()->optionalGeolocation());
}

#if PLATFORM(ANDROID) && ENABLE(APPLICATION_INSTALLED)

JSC::JSValue  WebCore::JSNavigator::isApplicationInstalled(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() < 2) {
        setDOMException(exec, SYNTAX_ERR);
        return jsUndefined();
    }

    if (!args.at(1).inherits(&InternalFunction::info)) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    String appName = args.at(0).toString(exec);

    JSObject* object;
    if (!(object = args.at(1).getObject())) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    RefPtr<ApplicationInstalledCallback> callback(JSCustomApplicationInstalledCallback::create(
            object, static_cast<JSDOMGlobalObject*>(exec->dynamicGlobalObject())));

    if (!m_impl->isApplicationInstalled(appName, callback.release()))
        setDOMException(exec, INVALID_STATE_ERR);
    return jsUndefined();
}

#endif

}
