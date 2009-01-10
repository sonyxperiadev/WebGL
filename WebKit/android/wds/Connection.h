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

#ifndef WDS_CONNECTION_H
#define WDS_CONNECTION_H

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
