/*
 * Copyright 2006, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

class WebHistoryItem : public WTF::RefCounted<WebHistoryItem> {
public:
    WebHistoryItem(WebHistoryItem* parent)
        : mParent(parent)
        , mObject(NULL)
        , mJVM(NULL)
        , mScale(100)
        , mActive(false)
        , mHistoryItem(NULL) {}
    WebHistoryItem(JNIEnv*, jobject, WebCore::HistoryItem*);
    ~WebHistoryItem();
    void updateHistoryItem(WebCore::HistoryItem* item);
    void setScale(int s)                   { mScale = s; }
    void setActive()                       { mActive = true; }
    void setParent(WebHistoryItem* parent) { mParent = parent; }
    WebHistoryItem* parent() { return mParent.get(); }
    int scale()              { return mScale; }
    WebCore::HistoryItem* historyItem() { return mHistoryItem; }
private:
    RefPtr<WebHistoryItem> mParent;
    jobject         mObject;
    JavaVM*         mJVM;
    int             mScale;
    bool            mActive;
    WebCore::HistoryItem* mHistoryItem;
};

};

#endif
