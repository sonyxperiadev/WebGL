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

#define LOG_TAG "wdsclient"

#include "AdbConnection.h"
#include "ClientUtils.h"
#include "Device.h"
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <utils/Log.h>

void AdbConnection::close() {
    if (m_fd != -1) {
        shutdown(m_fd, SHUT_RDWR);
        ::close(m_fd);
        m_fd = -1;
    }
}

// Default adb port
#define ADB_PORT 5037

bool AdbConnection::connect() {
    // Some commands (host:devices for example) close the connection so we call
    // connect after the response.
    close();

    m_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (m_fd < 0) {
        log_errno("Failed to create socket for connecting to adb");
        return false;
    }

    // Create the socket address struct
    sockaddr_in adb;
    createTcpSocket(adb, ADB_PORT);

    // Connect to adb
    if (::connect(m_fd, (sockaddr*) &adb, sizeof(adb)) < 0) {
        log_errno("Failed to connect to adb");
        return false;
    }

    // Connected
    return true;
}

// Adb protocol stuff
#define MAX_COMMAND_LENGTH 1024
#define PAYLOAD_LENGTH 4
#define PAYLOAD_FORMAT "%04X"

bool AdbConnection::sendRequest(const char* fmt, ...) const {
    if (m_fd == -1) {
        LOGE("Connection is closed");
        return false;
    }

    // Build the command (service)
    char buf[MAX_COMMAND_LENGTH];
    va_list args;
    va_start(args, fmt);
    int res = vsnprintf(buf, MAX_COMMAND_LENGTH, fmt, args);
    va_end(args);

    LOGV("Sending command: %04X%.*s", res, res, buf);

    // Construct the payload length
    char payloadLen[PAYLOAD_LENGTH + 1];
    snprintf(payloadLen, sizeof(payloadLen), PAYLOAD_FORMAT, res);

    // First, send the payload length
    if (send(m_fd, payloadLen, PAYLOAD_LENGTH, 0) < 0) {
        log_errno("Failure when sending payload");
        return false;
    }

    // Send the actual command
    if (send(m_fd, buf, res, 0) < 0) {
        log_errno("Failure when sending command");
        return false;
    }

    // Check for the OKAY from adb
    return checkOkayResponse();
}

static void printFailureMessage(int fd) {
    // Grab the payload length
    char lenStr[PAYLOAD_LENGTH + 1];
    int payloadLen = recv(fd, lenStr, sizeof(lenStr) - 1, 0);
    LOG_ASSERT(payloadLen == PAYLOAD_LENGTH, "Incorrect payload size");
    lenStr[PAYLOAD_LENGTH] = 0;

    // Parse the hex payload
    payloadLen = strtol(lenStr, NULL, 16);
    if (payloadLen < 0)
        return;

    // Grab the message
    char* msg = new char[payloadLen + 1]; // include null-terminator
    int res = recv(fd, msg, payloadLen, 0);
    if (res < 0) {
        log_errno("Failure reading failure message from adb");
        return;
    } else if (res != payloadLen) {
        LOGE("Incorrect payload length %d - expected %d", res, payloadLen);
        return;
    }
    msg[res] = 0;

    // Tell somebody about it
    LOGE("Received failure from adb: %s", msg);

    // Cleanup
    delete[] msg;
}

#define ADB_RESPONSE_LENGTH 4

bool AdbConnection::checkOkayResponse() const {
    LOG_ASSERT(m_fd != -1, "Connection has been closed!");

    char buf[ADB_RESPONSE_LENGTH];
    int res = recv(m_fd, buf, sizeof(buf), 0);
    if (res < 0) {
        log_errno("Failure reading response from adb");
        return false;
    }

    // Check for a response other than OKAY/FAIL
    if ((res == ADB_RESPONSE_LENGTH) && (strncmp(buf, "OKAY", res) == 0)) {
        LOGV("Command OKAY");
        return true;
    } else if (strncmp(buf, "FAIL", ADB_RESPONSE_LENGTH) == 0) {
        // Something happened, print out the reason for failure
        printFailureMessage(m_fd);
        return false;
    }
    LOGE("Incorrect response from adb - '%.*s'", res, buf);
    return false;
}

void AdbConnection::clearDevices() {
    for (unsigned i = 0; i < m_devices.size(); i++)
        delete m_devices.editItemAt(i);
    m_devices.clear();
}

const DeviceList& AdbConnection::getDeviceList() {
    // Clear the current device list
    clearDevices();

    if (m_fd == -1) {
        LOGE("Connection is closed");
        return m_devices;
    }

    // Try to send the device list request
    if (!sendRequest("host:devices")) {
        LOGE("Failed to get device list from adb");
        return m_devices;
    }

    // Get the payload length
    char lenStr[PAYLOAD_LENGTH + 1];
    int res = recv(m_fd, lenStr, sizeof(lenStr) - 1, 0);
    if (res < 0) {
        log_errno("Failure to read payload size of device list");
        return m_devices;
    }
    lenStr[PAYLOAD_LENGTH] = 0;

    // Parse the hex payload
    int payloadLen = strtol(lenStr, NULL, 16);
    if (payloadLen < 0)
        return m_devices;

    // Grab the list of devices. The format is as follows:
    // <serialno><tab><state><newline>
    char* msg = new char[payloadLen + 1];
    res = recv(m_fd, msg, payloadLen, 0);
    if (res < 0) {
        log_errno("Failure reading the device list");
        return m_devices;
    } else if (res != payloadLen) {
        LOGE("Incorrect payload length %d - expected %d", res, payloadLen);
        return m_devices;
    }
    msg[res] = 0;

    char serial[32];
    char state[32];
    int numRead;
    char* ptr = msg;
    while (sscanf(ptr, "%31s\t%31s\n%n", serial, state, &numRead) > 1) {
        Device::DeviceType t = Device::DEVICE;
        static const char emulator[] = "emulator-";
        if (strncmp(serial, emulator, sizeof(emulator) - 1) == 0)
            t = Device::EMULATOR;
        LOGV("Adding device %s (%s)", serial, state);
        m_devices.add(new Device(serial, t, this));

        // Reset for the next line
        ptr += numRead;
    }
    // Cleanup
    delete[] msg;

    return m_devices;
}
