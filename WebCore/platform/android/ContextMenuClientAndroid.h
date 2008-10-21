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

#ifndef ContextMenuClientAndroid_h
#define ContextMenuClientAndroid_h

#include "ContextMenuClient.h"

namespace WebCore {

class ContextMenuClientAndroid : public ContextMenuClient {
public:
    virtual void contextMenuDestroyed();
    
    virtual PlatformMenuDescription getCustomMenuFromDefaultItems(ContextMenu*);
    virtual void contextMenuItemSelected(ContextMenuItem*, const ContextMenu*);
    
    virtual void downloadURL(const KURL& url);
    virtual void copyImageToClipboard(const HitTestResult&);
    virtual void searchWithGoogle(const Frame*);
    virtual void lookUpInDictionary(Frame*);
    virtual void speak(const String&);
    virtual void stopSpeaking();
};

}

#endif
