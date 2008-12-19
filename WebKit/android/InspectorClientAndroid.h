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

#ifndef InspectorClientAndroid_h
#define InspectorClientAndroid_h

#include "InspectorClient.h"

namespace WebCore {

class InspectorClientAndroid : public InspectorClient {
public:
    virtual ~InspectorClientAndroid() {  }

    virtual void inspectorDestroyed() {}

    virtual Page* createPage() { return NULL; }

    virtual String localizedStringsURL() { return String(); }

    virtual void showWindow() {}
    virtual void closeWindow() {}

    virtual void attachWindow() {}
    virtual void detachWindow() {}

    virtual void setAttachedWindowHeight(unsigned height) {}

    virtual void highlight(Node*) {}
    virtual void hideHighlight() {}

    virtual void inspectedURLChanged(const String& newURL) {}
    
    // new as of 38068
    virtual void populateSetting(const String&, InspectorController::Setting&) {}
 	virtual void storeSetting(const String&, const InspectorController::Setting&) {}
 	virtual void removeSetting(const String&) {}
};

}

#endif
