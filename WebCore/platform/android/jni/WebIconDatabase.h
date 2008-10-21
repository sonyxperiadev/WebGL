/* //device/libs/WebKitLib/WebKit/WebCore/platform/android/jni/android_webkit_webicondatabase.h
**
** Copyright 2006, The Android Open Source Project
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

#ifndef ANDROID_WEBKIT_WEBICONDATABASE_H
#define ANDROID_WEBKIT_WEBICONDATABASE_H

#include "IconDatabaseClient.h"
#include "utils/threads.h"
#include "wtf/Vector.h"

#include <jni.h>

namespace WebCore {
    class Image;
    class String;
}

namespace android {

    class WebIconDatabaseClient {
    public:
        virtual ~WebIconDatabaseClient() {}
        virtual void didAddIconForPageUrl(const WebCore::String& pageUrl) = 0;
    };

    class WebIconDatabase : public WebCore::IconDatabaseClient {
    public:
        WebIconDatabase() : mDeliveryRequested(false) {}
        // IconDatabaseClient method
        virtual void dispatchDidAddIconForPageURL(const WebCore::String& pageURL);

        static void RegisterForIconNotification(WebIconDatabaseClient* client);
        static void UnregisterForIconNotification(WebIconDatabaseClient* client);
        static void DeliverNotifications(void*);

    private:
        // Deliver all the icon notifications
        void deliverNotifications();

        // List of clients and a mutex to protect it.
        Vector<WebIconDatabaseClient*> mClients;
        android::Mutex                 mClientsMutex;

        // Queue of page urls that have received an icon.
        Vector<WebCore::String> mNotifications;
        android::Mutex          mNotificationsMutex;
        // Flag to indicate that we have requested a delivery of notifications.
        bool                    mDeliveryRequested;
    };

    jobject webcoreImageToJavaBitmap(JNIEnv* env, WebCore::Image* icon);

};

#endif
