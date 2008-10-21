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

#include "config.h"
#include "JavaSharedClient.h"
#define LOG_TAG "JavaSharedClient"
#include "utils/Log.h"
#include "SkDeque.h"
#include "SkThread.h"

namespace android {
    void AndroidSignalServiceFuncPtrQueue();
}

namespace WebCore {

    TimerClient* JavaSharedClient::GetTimerClient()
    {
        //LOG_ASSERT(gTimerClient != NULL, "gTimerClient not initialized!!!");
        return gTimerClient;
    }

    CookieClient* JavaSharedClient::GetCookieClient()
    {
        //LOG_ASSERT(gCookieClient != NULL, "gCookieClient not initialized!!!");
        return gCookieClient;
    }

    void JavaSharedClient::SetTimerClient(TimerClient* client)
    {
        //LOG_ASSERT(gTimerClient == NULL || client == NULL, "gTimerClient already set, aborting...");
        gTimerClient = client;
    }

    void JavaSharedClient::SetCookieClient(CookieClient* client)
    {
        //LOG_ASSERT(gCookieClient == NULL || client == NULL, "gCookieClient already set, aborting...");
        gCookieClient = client;
    }

    TimerClient*    JavaSharedClient::gTimerClient = NULL;
    CookieClient*   JavaSharedClient::gCookieClient = NULL;

    ///////////////////////////////////////////////////////////////////////////
    
    struct FuncPtrRec {
        void (*fProc)(void* payload);
        void* fPayload;
    };
    
    static SkMutex gFuncPtrQMutex;
    static SkDeque gFuncPtrQ(sizeof(FuncPtrRec));

    void JavaSharedClient::EnqueueFunctionPtr(void (*proc)(void* payload),
                                              void* payload)
    {
        gFuncPtrQMutex.acquire();

        FuncPtrRec* rec = (FuncPtrRec*)gFuncPtrQ.push_back();
        rec->fProc = proc;
        rec->fPayload = payload;
        
        gFuncPtrQMutex.release();
        
        android::AndroidSignalServiceFuncPtrQueue();
    }

    void JavaSharedClient::ServiceFunctionPtrQueue()
    {
        for (;;) {
            void (*proc)(void*);
            void* payload;
            const FuncPtrRec* rec;
            
            // we have to copy the proc/payload (if present). we do this so we
            // don't call the proc inside the mutex (possible deadlock!)
            gFuncPtrQMutex.acquire();
            rec = (const FuncPtrRec*)gFuncPtrQ.front();
            if (NULL != rec) {
                proc = rec->fProc;
                payload = rec->fPayload;
                gFuncPtrQ.pop_front();
            }
            gFuncPtrQMutex.release();
            
            if (NULL == rec) {
                break;
            }
            proc(payload);
        }
    }
}
