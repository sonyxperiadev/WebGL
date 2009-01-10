/*
 * Copyright 2008, The Android Open Source Project
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

#define LOG_TAG "wds"
#include "config.h"

#include "Command.h"
#include "Connection.h"
#include "DebugServer.h"
#include "wtf/MainThread.h"
#include "wtf/Threading.h"
#include <arpa/inet.h>
#include <cutils/properties.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <utils/Log.h>

#if ENABLE(WDS)

#define DEFAULT_PORT 9999
#define log_errno(x) LOGE("%s: %d", x, strerror(errno))

namespace android {

namespace WDS {

static DebugServer* s_server = NULL;

// Main thread function for createThread
static void* mainThread(void* v) {
    DebugServer* server = static_cast<DebugServer*>(v);
    server->start();
    delete server;
    s_server = NULL;
    return NULL;
}

DebugServer* server() {
    if (s_server == NULL)
        s_server = new DebugServer();
    return s_server;
}

DebugServer::DebugServer() {
    // Read webcore.wds.enable to determine if the debug server should run
    char buf[PROPERTY_VALUE_MAX];
    int ret = property_get("webcore.wds.enable", buf, NULL);
    if (ret != -1 && strcmp(buf, "1") == 0) {
        LOGD("WDS Enabled");
        m_threadId = createThread(mainThread, this, "WDS");
    }
    // Initialize the available commands.
    Command::Init();
}

void DebugServer::start() {
    LOGD("DebugServer thread started");

    ConnectionServer cs;
    if (!cs.connect(DEFAULT_PORT)) {
        LOGE("Failed to start the server socket connection");
        return;
    }

    while (true ) {
        LOGD("Waiting for incoming connections...");
        Connection* conn = cs.accept();
        if (!conn) {
            log_errno("Failed to accept new connections");
            return;
        }
        LOGD("...Connection established");

        Command* c = Command::Find(conn);
        if (!c) {
            LOGE("Could not find matching command");
            delete conn;
        } else {
            // Dispatch the command, it will handle cleaning up the connection
            // when finished.
            c->dispatch();
        }
    }

    LOGD("DebugServer thread finished");
}

} // end namespace WDS

} // end namespace android

#endif
