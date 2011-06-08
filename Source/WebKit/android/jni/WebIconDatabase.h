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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebIconDatabase_h
#define WebIconDatabase_h

#include "IconDatabaseClient.h"
#include "PlatformString.h"
#include "utils/threads.h"
#include <jni.h>
#include <wtf/Vector.h>

namespace WebCore {
    class Image;
}

namespace android {

    class WebIconDatabaseClient {
    public:
        virtual ~WebIconDatabaseClient() {}
        virtual void didAddIconForPageUrl(const WTF::String& pageUrl) = 0;
    };

    class WebIconDatabase : public WebCore::IconDatabaseClient {
    public:
        WebIconDatabase() : mDeliveryRequested(false) {}
        // IconDatabaseClient methods
        virtual bool performImport();
        virtual void didRemoveAllIcons();
        virtual void didImportIconURLForPageURL(const WTF::String&);
        virtual void didImportIconDataForPageURL(const WTF::String&);
        virtual void didChangeIconForPageURL(const WTF::String&);
        virtual void didFinishURLImport();

        static void RegisterForIconNotification(WebIconDatabaseClient* client);
        static void UnregisterForIconNotification(WebIconDatabaseClient* client);
        static void DeliverNotifications(void*);

    private:
        // Deliver all the icon notifications
        void deliverNotifications();

        // List of clients.
        Vector<WebIconDatabaseClient*> mClients;

        // Queue of page urls that have received an icon.
        Vector<WTF::String> mNotifications;
        android::Mutex          mNotificationsMutex;
        // Flag to indicate that we have requested a delivery of notifications.
        bool                    mDeliveryRequested;
    };

    jobject webcoreImageToJavaBitmap(JNIEnv* env, WebCore::Image* icon);

};

#endif
