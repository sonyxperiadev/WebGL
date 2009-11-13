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
