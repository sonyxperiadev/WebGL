/*
 * Copyright 2010, The Android Open Source Project
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

#ifndef SearchPopupMenuAndroid_h
#define SearchPopupMenuAndroid_h

#include "SearchPopupMenu.h"

namespace WebCore {

class IntRect;
class PopupMenu;
class FrameView;

class DummyPopup : public PopupMenu {
 public:
     virtual ~DummyPopup() {}
     virtual void show(const IntRect&, FrameView*, int index) { }
     virtual void hide() { }
     virtual void updateFromElement() { }
     virtual void disconnectClient() { }
};

class SearchPopupMenuAndroid : public SearchPopupMenu {
public:
    SearchPopupMenuAndroid() : m_popup(adoptRef(new DummyPopup)) { }
    virtual PopupMenu* popupMenu() { return m_popup.get(); }
    virtual void saveRecentSearches(const AtomicString&, const Vector<String>&) { }
    virtual void loadRecentSearches(const AtomicString&, Vector<String>&) { }
    virtual bool enabled() { return false; }

private:
    RefPtr<PopupMenu> m_popup;
};

}

#endif // SearchPopupMenuAndroid_h
