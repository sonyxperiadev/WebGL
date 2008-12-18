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

#ifndef DragClientAndroid_h
#define DragClientAndroid_h

#include "DragClient.h"

namespace WebCore {

    class DragClientAndroid : public WebCore::DragClient {
    public:
        virtual void willPerformDragDestinationAction(DragDestinationAction, DragData*);
        virtual void willPerformDragSourceAction(DragSourceAction, const IntPoint&, Clipboard*);
        virtual DragDestinationAction actionMaskForDrag(DragData*);
        //We work in window rather than view coordinates here
        virtual DragSourceAction dragSourceActionMaskForPoint(const IntPoint&);
        
        virtual void startDrag(DragImageRef dragImage, const IntPoint& dragImageOrigin, const IntPoint& eventPos, Clipboard*, Frame*, bool linkDrag = false);
        virtual DragImageRef createDragImageForLink(KURL&, const String& label, Frame*);

        virtual void dragControllerDestroyed();
    };
        
}

#endif
