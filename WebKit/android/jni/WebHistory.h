/*
 * Copyright 2006, The Android Open Source Project
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

#ifndef ANDROID_WEBKIT_WEBHISTORY_H
#define ANDROID_WEBKIT_WEBHISTORY_H

#include <jni.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {
    class HistoryItem;
}

namespace android {

class AutoJObject;

class WebHistory {
public:
    static jbyteArray Flatten(JNIEnv*, WTF::Vector<char>&, WebCore::HistoryItem*);
    static void AddItem(const AutoJObject&, WebCore::HistoryItem*);
    static void RemoveItem(const AutoJObject&, int);
    static void UpdateHistoryIndex(const AutoJObject&, int);
};

// there are two scale factors saved with each history item. mScale reflects the
// viewport scale factor, default to 100 means 100%. mScreenWidthScale records
// the scale factor for the screen width used to wrap the text paragraph.
class WebHistoryItem : public WTF::RefCounted<WebHistoryItem> {
public:
    WebHistoryItem(WebHistoryItem* parent)
        : mParent(parent)
        , mObject(NULL)
        , mScale(100)
        , mScreenWidthScale(100)
        , mActive(false)
        , mHistoryItem(NULL) {}
    WebHistoryItem(JNIEnv*, jobject, WebCore::HistoryItem*);
    ~WebHistoryItem();
    void updateHistoryItem(WebCore::HistoryItem* item);
    void setScale(int s)                   { mScale = s; }
    void setScreenWidthScale(int s)        { mScreenWidthScale = s; }
    void setActive()                       { mActive = true; }
    void setParent(WebHistoryItem* parent) { mParent = parent; }
    WebHistoryItem* parent() { return mParent.get(); }
    int scale()              { return mScale; }
    int screenWidthScale()   { return mScreenWidthScale; }
    WebCore::HistoryItem* historyItem() { return mHistoryItem; }
private:
    RefPtr<WebHistoryItem> mParent;
    jobject         mObject;
    int             mScale;
    int             mScreenWidthScale;
    bool            mActive;
    WebCore::HistoryItem* mHistoryItem;
};

};

#endif
