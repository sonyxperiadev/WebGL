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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
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
#include "Timer.h"

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace WebCore {
    class Frame;
    class Geolocation;
    class SQLiteDatabase;
}

namespace android {

    class WebViewCore;

    // The GeolocationPermissions class manages Geolocation permissions for the
    // browser. Permissions are managed on a per-origin basis, as required by
    // the Geolocation spec - http://dev.w3.org/geo/api/spec-source.html. An
    // origin specifies the scheme, host and port of particular frame.  An
    // origin is represented here as a string, using the output of
    // WebCore::SecurityOrigin::toString.
    //
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
        void cancelPermissionStateQuery(WebCore::Frame*);

        // Provides this object with a permission state set by the user. The
        // permission is specified by 'allow' and applied to 'origin'. If
        // 'remember' is set, the permission state is remembered permanently.
        // The new permission state is recorded and will trigger callbacks to
        // geolocation objects as described above. If any other permission
        // requests are queued, the next is started.
        void providePermissionState(WTF::String origin, bool allow, bool remember);

        // Clears the temporary permission state and any pending requests. Used
        // when the main frame is refreshed or navigated to a new URL.
        void resetTemporaryPermissionStates();

        // Static methods for use from Java. These are used to interact with the
        // browser settings menu and to update the permanent permissions when
        // system settings are changed.
        // Gets the list of all origins for which permanent permissions are
        // recorded.
        typedef HashSet<WTF::String> OriginSet;
        static OriginSet getOrigins();
        // Gets whether the specified origin is allowed.
        static bool getAllowed(WTF::String origin);
        // Clears the permission state for the specified origin.
        static void clear(WTF::String origin);
        // Sets the permission state for the specified origin to allowed.
        static void allow(WTF::String origin);
        // Clears the permission state for all origins.
        static void clearAll();
        // Sets whether the GeolocationPermissions object should always deny
        // permission requests, irrespective of previously recorded permission
        // states.
        static void setAlwaysDeny(bool deny);

        static void setDatabasePath(WTF::String path);
        static bool openDatabase(WebCore::SQLiteDatabase*);

        // Saves the permanent permissions to the DB if required.
        static void maybeStorePermanentPermissions();

      private:
        // Records the permission state for the specified origin and whether
        // this should be remembered.
        void recordPermissionState(WTF::String origin, bool allow, bool remember);

        // Used to make an asynchronous callback to the Geolocation objects.
        void makeAsynchronousCallbackToGeolocation(WTF::String origin, bool allow);
        void timerFired(WebCore::Timer<GeolocationPermissions>* timer);

        // Calls back to the Geolocation objects in all frames from the
        // specified origin. There may be no such objects, as the frames using
        // Geolocation from the specified origin may no longer use Geolocation,
        // or may have been navigated to a different origin..
        void maybeCallbackFrames(WTF::String origin, bool allow);

        // Cancels pending permission requests for the specified origin in
        // other main frames (ie browser tabs). This is used when the user
        // specifies permission to be remembered.
        static void cancelPendingRequestsInOtherTabs(WTF::String origin);
        void cancelPendingRequests(WTF::String origin);

        static void maybeLoadPermanentPermissions();

        const WTF::String& nextOriginInQueue();

        WebViewCore* m_webViewCore;
        WebCore::Frame* m_mainFrame;
        // A vector of the origins queued to make a permission request.
        // The first in the vector is the origin currently making the request.
        typedef Vector<WTF::String> OriginVector;
        OriginVector m_queuedOrigins;
        // A map from a queued origin to the set of frames that have requested
        // permission for that origin.
        typedef HashSet<WebCore::Frame*> FrameSet;
        typedef HashMap<WTF::String, FrameSet> OriginToFramesMap;
        OriginToFramesMap m_queuedOriginsToFramesMap;

        typedef WTF::HashMap<WTF::String, bool> PermissionsMap;
        PermissionsMap m_temporaryPermissions;
        static PermissionsMap s_permanentPermissions;

        typedef WTF::Vector<GeolocationPermissions*> GeolocationPermissionsVector;
        static GeolocationPermissionsVector s_instances;

        WebCore::Timer<GeolocationPermissions> m_timer;

        struct CallbackData {
            WTF::String origin;
            bool allow;
        };
        CallbackData m_callbackData;

        static bool s_alwaysDeny;

        static bool s_permanentPermissionsLoaded;
        static bool s_permanentPermissionsModified;
        static WTF::String s_databasePath;
    };

}  // namespace android

#endif
