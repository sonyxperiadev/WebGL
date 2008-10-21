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
#include "DragClientAndroid.h"
#include "wtf/Assertions.h"

#define LOG_TAG "WebCore"
#undef LOG
#include <utils/Log.h>

//#define notImplemented() ASSERT(0)
#define notImplemented() LOGV("%s\n", __PRETTY_FUNCTION__);

namespace WebCore {

void DragClientAndroid::dragControllerDestroyed() { notImplemented(); delete this; }

void DragClientAndroid::willPerformDragDestinationAction(DragDestinationAction, DragData*) { notImplemented(); }

DragDestinationAction DragClientAndroid::actionMaskForDrag(DragData*) { notImplemented(); return DragDestinationActionNone; }

DragSourceAction DragClientAndroid::dragSourceActionMaskForPoint(const IntPoint&) { notImplemented(); return DragSourceActionNone; }

void* DragClientAndroid::createDragImageForLink(KURL&, String const&, Frame*) { return NULL; }
void DragClientAndroid::willPerformDragSourceAction(DragSourceAction, IntPoint const&, Clipboard*) {}
void DragClientAndroid::startDrag(void*, IntPoint const&, IntPoint const&, Clipboard*, Frame*, bool) {}

}
