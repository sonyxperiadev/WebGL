/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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

#ifndef npruntime_impl_h
#define npruntime_impl_h

#if PLATFORM(CHROMIUM)
#include "bindings/npruntime.h"
#else
#include "npruntime.h"  // use V8Binding/npapi version
#endif

// This file exists to support WebCore, which expects to be able to call upon
// portions of the NPRuntime implementation.

// A simple mapping for now.  FIXME We should probably just adopt the
// underscore prefix as our naming convention too.

#define _NPN_ReleaseVariantValue NPN_ReleaseVariantValue
#define _NPN_GetStringIdentifier NPN_GetStringIdentifier
#define _NPN_GetStringIdentifiers NPN_GetStringIdentifiers
#define _NPN_GetIntIdentifier NPN_GetIntIdentifier
#define _NPN_IdentifierIsString NPN_IdentifierIsString
#define _NPN_UTF8FromIdentifier NPN_UTF8FromIdentifier
#define _NPN_IntFromIdentifier NPN_IntFromIdentifier
#define _NPN_CreateObject NPN_CreateObject
#define _NPN_RetainObject NPN_RetainObject
#define _NPN_ReleaseObject NPN_ReleaseObject
#define _NPN_DeallocateObject NPN_DeallocateObject
#define _NPN_Invoke NPN_Invoke
#define _NPN_InvokeDefault NPN_InvokeDefault
#define _NPN_Evaluate NPN_Evaluate
#define _NPN_GetProperty NPN_GetProperty
#define _NPN_SetProperty NPN_SetProperty
#define _NPN_RemoveProperty NPN_RemoveProperty
#define _NPN_HasProperty NPN_HasProperty
#define _NPN_HasMethod NPN_HasMethod
#define _NPN_SetException NPN_SetException
#define _NPN_Enumerate NPN_Enumerate
#define _NPN_Construct NPN_Construct

#endif
