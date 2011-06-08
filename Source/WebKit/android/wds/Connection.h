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

#ifndef Connection_h
#define Connection_h

#include <unistd.h>
#include <sys/socket.h>

namespace android {

namespace WDS {

class Socket {
public:
    Socket(): m_fd(-1) {}
    Socket(int fd): m_fd(fd) {}
    ~Socket() {
        if (m_fd != -1) {
            shutdown(m_fd, SHUT_RDWR);
            close(m_fd);
        }
    }
    // Open a new socket using PF_INET and SOCK_STREAM
    bool open();
    int fd() const { return m_fd; }
private:
    int m_fd;
};

class Connection {
public:
    Connection(int conn): m_socket(conn) {}
    int read(char buf[], size_t length) const {
        return recv(m_socket.fd(), buf, length, 0);
    }
    int write(const char buf[], size_t length) const {
        return send(m_socket.fd(), buf, length, 0);
    }
    int write(const char buf[]) const {
        return write(buf, strlen(buf));
    }
private:
    Socket m_socket;
};

class ConnectionServer {
public:
    ConnectionServer() {}

    // Establish a connection to the local host on the given port.
    bool connect(short port);

    // Blocks on the established socket until a new connection arrives.
    Connection* accept() const;
private:
    Socket m_socket;
};

} // end namespace WDS

} // end namespace android

#endif
