/*
 * Copyright 2009, The Android Open Source Project
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

#ifndef GeolocationPermissions_h
#define GeolocationPermissions_h

#include "PlatformString.h"
// We must include this before before HashMap.h, as it provides specalizations
// for String hash types instantiated there.
#include "StringHash.h"
#include "HashMap.h"
#include "HashSet.h"
#include "Timer.h"
#include "Vector.h"
#include "wtf/RefCounted.h"

namespace WebCore {
    class Frame;
    class Geolocation;
}

namespace android {

    class WebViewCore;

    // The GeolocationPermissions class manages permissions for the browser.
    // Each instance handles permissions for a given main frame. The class
    // enforces the following policy.
    // - Non-remembered permissions last for the dureation of the main frame.
    // - Remembered permissions last indefinitely.
    // - All permissions are shared between child frames of a main frame.
    // - Only remembered permissions are shared between main frames.
    // - Remembered permissions are made available for use in the browser
    //   settings menu.
    class GeolocationPermissions : public RefCounted<GeolocationPermissions> {
      public:
        // Creates the GeolocationPermissions object to manage permissions for
        // the specified main frame (i.e. tab). The WebViewCore is used to
        // communicate with the browser to display UI.
        GeolocationPermissions(WebViewCore* webViewCore, WebCore::Frame* mainFrame);
        virtual ~GeolocationPermissions();

        // Queries the permission state for the specified frame. If the
        // permission state has not yet been set, prompts the user. Once the
        // permission state has been determined, asynchronously calls back to
        // the Geolocation objects in all frames in this WebView that are from
        // the same origin as the requesting frame.
        void queryPermissionState(WebCore::Frame* frame);

        // Provides this object the given permission state from the user. The
        // new permission state is recorded and will trigger callbacks to
        // geolocation objects as described above. If any other permission
        // requests are queued, the next is started.
        void providePermissionState(WebCore::String origin, bool allow, bool remember);

        // Clears the temporary permission state and any pending requests. Used
        // when the main frame is refreshed or navigated to a new URL.
        void resetTemporaryPermissionStates();

        // Static methods for use from Java. These are used to interact with the
        // browser settings menu and to update the permanent permissions when
        // system settings are changed.
        typedef HashSet<WebCore::String> OriginSet;
        static OriginSet getOrigins();
        static bool getAllowed(WebCore::String origin);
        static void clear(WebCore::String origin);
        static void allow(WebCore::String origin);
        static void clearAll();
        static void setAlwaysDeny(bool deny);

        static void setDatabasePath(WebCore::String path);

        // Saves the permanent permissions to the DB if required.
        static void maybeStorePermanentPermissions();

      private:
        // Records the permission state for the specified origin.
        void recordPermissionState(WebCore::String origin, bool allow, bool remember);

        // Used to make an asynchronous callback to the Geolocation objects.
        void makeAsynchronousCallbackToGeolocation(WebCore::String origin, bool allow);
        void timerFired(WebCore::Timer<GeolocationPermissions>* timer);

        // Calls back to the Geolocation objects in all frames from the
        // specified origin. There may be no such objects, as the frames using
        // Geolocation from the specified origin may no longer use Geolocation,
        // or may have been navigated to a different origin..
        void maybeCallbackFrames(WebCore::String origin, bool allow);

        // Cancels pending permission requests for the specified origin in
        // other main frames (ie browser tabs). This is used when the user
        // specifies permission to be remembered.
        static void cancelPendingRequestsInOtherTabs(WebCore::String origin);
        void cancelPendingRequests(WebCore::String origin);

        static void maybeLoadPermanentPermissions();

        WebViewCore* m_webViewCore;
        WebCore::Frame* m_mainFrame;
        WebCore::String m_originInProgress;
        typedef Vector<WebCore::String> OriginVector;
        OriginVector m_queuedOrigins;

        typedef WTF::HashMap<WebCore::String, bool> PermissionsMap;
        PermissionsMap m_temporaryPermissions;
        static PermissionsMap s_permanentPermissions;

        typedef WTF::Vector<GeolocationPermissions*> GeolocationPermissionsVector;
        static GeolocationPermissionsVector s_instances;

        WebCore::Timer<GeolocationPermissions> m_timer;

        struct CallbackData {
            WebCore::String origin;
            bool allow;
        };
        CallbackData m_callbackData;

        static bool s_alwaysDeny;

        static bool s_permanentPermissionsLoaded;
        static WebCore::String s_databasePath;
    };

}  // namespace android

#endif
