/* 
**
** Copyright 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "config.h"
#include "InspectorController.h"
#include "Frame.h"
#include "Node.h"

/*
// This stub file was created to avoid building and linking in all the
// Inspector codebase. If you would like to enable the Inspector, do the
// following steps:
// 1. Replace this file in WebCore/Makefile.android with the common
//    implementation, ie page/InsepctorController.cpp
// 2. Add the JS API files to JavaScriptCore/Makefile.android:
// ?  API/JSBase.cpp \
//      API/JSCallbackConstructor.cpp \
//      API/JSCallbackFunction.cpp \
//      API/JSCallbackObject.cpp \
//      API/JSClassRef.cpp \
//      API/JSContextRef.cpp \
//      API/JSObjectRef.cpp \
//      API/JSStringRef.cpp \
//      API/JSValueRef.cpp
// 3. Add the following LOCAL_C_INCLUDES to JavaScriptCore/Makefile.android:
// ?$(LOCAL_PATH)/API \
//      $(LOCAL_PATH)/ForwardingHeaders \
//      $(LOCAL_PATH)/../../WebKit \
// 4. Rebuild WebKit
//
// Note, for a functional Inspector, you must implement InspectorClientAndroid
*/

namespace WebCore {

struct InspectorResource : public RefCounted<InspectorResource> {
};

struct InspectorDatabaseResource : public RefCounted<InspectorDatabaseResource> {
};

InspectorController::InspectorController(Page*, InspectorClient*) {}
InspectorController::~InspectorController() {}

void InspectorController::windowScriptObjectAvailable() {}
void InspectorController::didCommitLoad(DocumentLoader*) {}
void InspectorController::identifierForInitialRequest(unsigned long, DocumentLoader*, ResourceRequest const&) {}
void InspectorController::willSendRequest(DocumentLoader*, unsigned long, ResourceRequest&, ResourceResponse const&) {}
void InspectorController::didReceiveResponse(DocumentLoader*, unsigned long, ResourceResponse const&) {}
void InspectorController::didReceiveContentLength(DocumentLoader*, unsigned long, int) {}
void InspectorController::didFinishLoading(DocumentLoader*, unsigned long) {}
void InspectorController::didLoadResourceFromMemoryCache(DocumentLoader*, ResourceRequest const&, ResourceResponse const&, int) {}
void InspectorController::frameDetachedFromParent(Frame*) {}

void InspectorController::addMessageToConsole(MessageSource, MessageLevel, String const&, unsigned int, String const&) {}
#if ENABLE(DATABASE)
void InspectorController::didOpenDatabase(Database*, String const&, String const&, String const&) {}
#endif
bool InspectorController::enabled() const { return false; }
void InspectorController::inspect(Node*) {}
bool InspectorController::windowVisible() { return false; }

}
