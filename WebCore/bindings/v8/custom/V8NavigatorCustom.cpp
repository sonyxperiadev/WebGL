/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8Navigator.h"

#include "RuntimeEnabledFeatures.h"
#include "V8DOMWindow.h"
#include "V8DOMWrapper.h"

#if PLATFORM(ANDROID)
#include "ExceptionCode.h"
#include "V8CustomApplicationInstalledCallback.h"
#include "V8Proxy.h"
#endif

namespace WebCore {

v8::Handle<v8::Value> toV8(Navigator* impl)
{
    if (!impl)
        return v8::Null();
    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(impl);
    if (wrapper.IsEmpty()) {
        wrapper = V8Navigator::wrap(impl);
        if (!wrapper.IsEmpty())
            V8DOMWrapper::setHiddenWindowReference(impl->frame(), V8DOMWindow::navigatorIndex, wrapper);
    }
    return wrapper;
}

#if PLATFORM(ANDROID) && ENABLE(APPLICATION_INSTALLED)

static PassRefPtr<ApplicationInstalledCallback> createApplicationInstalledCallback(
        v8::Local<v8::Value> value, bool& succeeded)
{
    succeeded = true;

    if (!value->IsFunction()) {
        succeeded = false;
        throwError("The second argument should be a function");
        return 0;
    }

    Frame* frame = V8Proxy::retrieveFrameForCurrentContext();
    return V8CustomApplicationInstalledCallback::create(value, frame);
}

v8::Handle<v8::Value> V8Navigator::isApplicationInstalledCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.isApplicationInstalled()");
    bool succeeded = false;

    if (args.Length() < 2)
        return throwError("Two arguments required: an application name and a callback.", V8Proxy::SyntaxError);

    if (!args[0]->IsString())
        return throwError("The first argument should be a string.");

    RefPtr<ApplicationInstalledCallback> callback =
        createApplicationInstalledCallback(args[1], succeeded);
    if (!succeeded)
        return v8::Undefined();

    ASSERT(callback);

    Navigator* navigator = V8Navigator::toNative(args.Holder());
    if (!navigator->isApplicationInstalled(toWebCoreString(args[0]), callback.release()))
        return throwError(INVALID_STATE_ERR);

    return v8::Undefined();
}

#endif // PLATFORM(ANDROID) && ENABLE(APPLICATION_INSTALLED)

} // namespace WebCore
