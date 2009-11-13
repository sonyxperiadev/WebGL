/*
 * Copyright 2008, The Android Open Source Project
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
