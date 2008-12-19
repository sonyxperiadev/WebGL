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

#ifndef JAVA_SHARED_CLIENT_H
#define JAVA_SHARED_CLIENT_H

namespace WebCore {

    class TimerClient;
    class CookieClient;

    class JavaSharedClient
    {
    public:
        static TimerClient* GetTimerClient(); 
        static CookieClient* GetCookieClient();

        static void SetTimerClient(TimerClient* client);
        static void SetCookieClient(CookieClient* client);

        // can be called from any thread, to be executed in webkit thread
        static void EnqueueFunctionPtr(void (*proc)(void*), void* payload);
        // only call this from webkit thread
        static void ServiceFunctionPtrQueue();
        
    private:
        static TimerClient* gTimerClient;
        static CookieClient* gCookieClient;
    };
}
#endif
