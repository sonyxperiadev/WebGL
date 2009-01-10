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

#include "DebugServer.h" // used for ENABLE_WDS
#include "Connection.h"
#include <arpa/inet.h>
#include <string.h>
#include <utils/Log.h>

#if ENABLE(WDS)

#define MAX_CONNECTION_QUEUE 5
#define log_errno(x) LOGE("%s: %d", x, strerror(errno))

namespace android {

namespace WDS {

bool Socket::open() {
    m_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (m_fd < 0) {
        log_errno("Failed to create file descriptor");
        return false;
    }
    return true;
}

bool ConnectionServer::connect(short port) {
    if (!m_socket.open())
        return false;
    int fd = m_socket.fd();

    // Build our sockaddr_in structure use to listen to incoming connections
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    // Try to bind to the given port
    if (bind(fd, (sockaddr*) &addr, sizeof(addr)) < 0) {
        log_errno("Failed to bind to local host");
        return false;
    }

    // Try to listen
    if (listen(fd, MAX_CONNECTION_QUEUE) < 0) {
        log_errno("Failed to listen");
        return false;
    }

    return true;
}

Connection* ConnectionServer::accept() const {
    int conn = ::accept(m_socket.fd(), NULL, NULL);
    if (conn < 0) {
        log_errno("Accept failed");
        return NULL;
    }
    return new Connection(conn);
}

} // end namespace WDS

} // end namespace android

#endif
