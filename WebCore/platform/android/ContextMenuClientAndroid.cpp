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
#include "ContextMenuClientAndroid.h"
#include "NotImplemented.h"
#include "wtf/Assertions.h"

namespace WebCore {
    
void ContextMenuClientAndroid::contextMenuDestroyed() { delete this; }

PlatformMenuDescription ContextMenuClientAndroid::getCustomMenuFromDefaultItems(ContextMenu*) { notImplemented(); return 0; }
void ContextMenuClientAndroid::contextMenuItemSelected(ContextMenuItem*, const ContextMenu*) { notImplemented(); }

void ContextMenuClientAndroid::downloadURL(const KURL& url) { notImplemented(); }
void ContextMenuClientAndroid::copyImageToClipboard(const HitTestResult&) { notImplemented(); }
void ContextMenuClientAndroid::searchWithGoogle(const Frame*) { notImplemented(); }
void ContextMenuClientAndroid::lookUpInDictionary(Frame*) { notImplemented(); }
void ContextMenuClientAndroid::speak(const String&) { notImplemented(); }
void ContextMenuClientAndroid::stopSpeaking() { notImplemented(); }

}
