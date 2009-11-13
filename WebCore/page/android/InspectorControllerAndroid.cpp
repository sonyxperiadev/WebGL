/*
 * Copyright 2007, The Android Open Source Project
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
#include "InspectorController.h"

#include "InspectorBackend.h"
#include "InspectorClient.h"
#include "InspectorDOMAgent.h"
#include "InspectorFrontend.h"

#include "Frame.h"
#include "Node.h"
#if USE(JSC)
#include "Profile.h"
#endif
// This stub file was created to avoid building and linking in all the
// Inspector codebase. If you would like to enable the Inspector, do the
// following steps:
// 1. Replace this file in WebCore/Android.mk with the common
//    implementation, ie page/InsepctorController.cpp
// 2. Add the JS API files to JavaScriptCore/Android.mk:
// ?  API/JSBase.cpp \
//      API/JSCallbackConstructor.cpp \
//      API/JSCallbackFunction.cpp \
//      API/JSCallbackObject.cpp \
//      API/JSClassRef.cpp \
//      API/JSContextRef.cpp \
//      API/JSObjectRef.cpp \
//      API/JSStringRef.cpp \
//      API/JSValueRef.cpp
// 3. Add the following LOCAL_C_INCLUDES to JavaScriptCore/Android.mk:
// ?$(LOCAL_PATH)/API \
//      $(LOCAL_PATH)/ForwardingHeaders \
//      $(LOCAL_PATH)/../../WebKit \
// 4. Rebuild WebKit
//
// Note, for a functional Inspector, you must implement InspectorClientAndroid.

namespace WebCore {

struct InspectorResource : public RefCounted<InspectorResource> {
};

#if ENABLE(DATABASE)
struct InspectorDatabaseResource : public RefCounted<InspectorDatabaseResource> {
};
#endif

#if ENABLE(DOM_STORAGE)
struct InspectorDOMStorageResource : public RefCounted<InspectorDatabaseResource> {
};
#endif

InspectorController::InspectorController(Page*, InspectorClient* client)
{
    m_client = client;
}

InspectorController::~InspectorController() { m_client->inspectorDestroyed(); }

void InspectorController::windowScriptObjectAvailable() {}
void InspectorController::didCommitLoad(DocumentLoader*) {}
void InspectorController::identifierForInitialRequest(unsigned long, DocumentLoader*, ResourceRequest const&) {}
void InspectorController::willSendRequest(DocumentLoader*, unsigned long, ResourceRequest&, ResourceResponse const&) {}
void InspectorController::didReceiveResponse(DocumentLoader*, unsigned long, ResourceResponse const&) {}
void InspectorController::didReceiveContentLength(DocumentLoader*, unsigned long, int) {}
void InspectorController::didFinishLoading(DocumentLoader*, unsigned long) {}
void InspectorController::didLoadResourceFromMemoryCache(DocumentLoader*, const CachedResource*) {}
void InspectorController::frameDetachedFromParent(Frame*) {}
void InspectorController::addMessageToConsole(WebCore::MessageSource, WebCore::MessageType, WebCore::MessageLevel, WebCore::String const&, unsigned int, WebCore::String const&) {}
void InspectorController::addMessageToConsole(WebCore::MessageSource, WebCore::MessageType, WebCore::MessageLevel, ScriptCallStack*) {}
#if ENABLE(DATABASE)
void InspectorController::didOpenDatabase(Database*, String const&, String const&, String const&) {}
#endif
#if ENABLE(DOM_STORAGE)
    void InspectorController::didUseDOMStorage(StorageArea* storageArea, bool isLocalStorage, Frame* frame)  {}
#endif
bool InspectorController::enabled() const { return false; }
void InspectorController::inspect(Node*) {}
bool InspectorController::windowVisible() { return false; }
void InspectorController::resourceRetrievedByXMLHttpRequest(unsigned long identifier, const ScriptString& sourceString) {}
void InspectorController::scriptImported(unsigned long identifier, const String& sourceString) {}
void InspectorController::inspectedPageDestroyed() {}

void InspectorController::inspectedWindowScriptObjectCleared(Frame* frame) {}
void InspectorController::startGroup(MessageSource source, ScriptCallStack* callFrame) {}
void InspectorController::endGroup(MessageSource source, unsigned lineNumber, const String& sourceURL) {}
void InspectorController::startTiming(const String& title) {}
bool InspectorController::stopTiming(const String& title, double& elapsed) { return false; }
void InspectorController::count(const String& title, unsigned lineNumber, const String& sourceID) {}

void InspectorController::mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags) {}
void InspectorController::handleMousePressOnNode(Node*) {}

#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorController::didPause() {}
#endif

}  // namespace WebCore
