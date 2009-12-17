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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorClientAndroid_h
#define InspectorClientAndroid_h

#include "InspectorClient.h"

namespace android {

class InspectorClientAndroid : public InspectorClient {
public:
    virtual ~InspectorClientAndroid() {  }

    virtual void inspectorDestroyed() { delete this; }

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

    virtual void populateSetting(const String& key, String* value) {}
    virtual void storeSetting(const String& key, const String& value) {}
    virtual String hiddenPanels() { return String(); }
    virtual void inspectorWindowObjectCleared() {}
};

}

#endif
