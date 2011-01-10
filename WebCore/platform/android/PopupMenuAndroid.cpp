/*
 * This file is part of the popup menu implementation for <select> elements in WebCore.
 *
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "PopupMenuAndroid.h"

#include "PopupMenuClient.h"
#include "SkTDArray.h"
#include "WebViewCore.h"

class PopupReply : public android::WebCoreReply {
public:
    PopupReply(const IntRect& rect, android::WebViewCore* view, PopupMenuClient* client)
        : m_rect(rect)
        , m_viewImpl(view)
        , m_popupClient(client)
    {
    }

    virtual ~PopupReply() {}

    virtual void replyInt(int value)
    {
        if (m_popupClient) {
            m_popupClient->popupDidHide();
            // -2 is a special value signaling that the popup was canceled.
            if (-2 == value)
                return;
            m_popupClient->valueChanged(value, true);
        }
        if (m_viewImpl)
            m_viewImpl->contentInvalidate(m_rect);
    }

    virtual void replyIntArray(const int*, int) {
        // Should never be called.
        SkASSERT(false);
    }

    void disconnectClient()
    {
        m_popupClient = 0;
        m_viewImpl = 0;
    }
private:
    IntRect m_rect;
    // FIXME: Do not need this if we handle ChromeClientAndroid::formStateDidChange
    android::WebViewCore* m_viewImpl;
    PopupMenuClient* m_popupClient;
};

namespace WebCore {

PopupMenuAndroid::PopupMenuAndroid(PopupMenuClient* menuList)
    : m_popupClient(menuList)
    , m_reply(0)
{
}
PopupMenuAndroid::~PopupMenuAndroid()
{
    disconnectClient();
}

void PopupMenuAndroid::disconnectClient()
{
    m_popupClient = 0;
    if (m_reply) {
        m_reply->disconnectClient();
        Release(m_reply);
        m_reply = 0;
    }
}
// Copied from WebViewCore.cpp.  Once we move ListBox handling to this class,
// we can remove the one in WebViewCore.cpp.
// Convert a WTF::String into an array of characters where the first
// character represents the length, for easy conversion to java.
static uint16_t* stringConverter(const WTF::String& text)
{
    size_t length = text.length();
    uint16_t* itemName = new uint16_t[length+1];
    itemName[0] = (uint16_t)length;
    uint16_t* firstChar = &(itemName[1]);
    memcpy((void*)firstChar, text.characters(), sizeof(UChar)*length);
    return itemName;
}

void PopupMenuAndroid::show(const IntRect& rect, FrameView* frameView, int)
{
    android::WebViewCore* viewImpl = android::WebViewCore::getWebViewCore(frameView);
    m_reply = new PopupReply(rect, viewImpl, m_popupClient);
    Retain(m_reply);

    SkTDArray<const uint16_t*> names;
    // Possible values for enabledArray.  Keep in Sync with values in
    // InvokeListBox.Container in WebView.java
    enum OptionStatus {
        OPTGROUP = -1,
        OPTION_DISABLED = 0,
        OPTION_ENABLED = 1,
    };
    SkTDArray<int> enabledArray;
    SkTDArray<int> selectedArray;
    int size = m_popupClient->listSize();
    // If we use this for ListBoxes in addition to MenuLists, we will need to
    // account for 'multiple'
    bool multiple = false;
    for (int i = 0; i < size; i++) {
        *names.append() = stringConverter(m_popupClient->itemText(i));
        if (m_popupClient->itemIsSeparator(i)) {
            *enabledArray.append() = OPTION_DISABLED;
        } else if (m_popupClient->itemIsLabel(i)) {
            *enabledArray.append() = OPTGROUP;
        } else {
            // Must be an Option
            *enabledArray.append() = m_popupClient->itemIsEnabled(i)
                    ? OPTION_ENABLED : OPTION_DISABLED;
            if (multiple && m_popupClient->itemIsSelected(i))
                *selectedArray.append() = i;
        }
    }

    viewImpl->listBoxRequest(m_reply,
                             names.begin(),
                             size,
                             enabledArray.begin(),
                             enabledArray.count(),
                             multiple,
                             selectedArray.begin(),
                             multiple ? selectedArray.count() : m_popupClient->selectedIndex());
}

} // namespace WebCore
