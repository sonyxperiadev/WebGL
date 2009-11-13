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

#define LOG_TAG "wdsclient"

#include "AdbConnection.h"
#include "ClientUtils.h"
#include "Device.h"
#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <utils/Log.h>

#define DEFAULT_WDS_PORT 9999
#define STR(x) #x
#define XSTR(x) STR(x)
#define PORT_STR XSTR(DEFAULT_WDS_PORT)

int wds_open() {
    // Create the structure for connecting to the forwarded 9999 port
    sockaddr_in addr;
    createTcpSocket(addr, DEFAULT_WDS_PORT);

    // Create our socket
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        log_errno("Failed to create file descriptor");
        return -1;
    }
    // Connect to the remote wds server thread
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        log_errno("Failed to connect to remote debug server");
        return -1;
    }
    return fd;
}

// Clean up the file descriptor and connections
void wds_close(int fd) {
    if (fd != -1) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
}

int main(int argc, char** argv) {

    Device::DeviceType type = Device::NONE;

    if (argc <= 1) {
        LOGE("wdsclient takes at least 1 argument");
        return 1;
    } else {
        // Parse the options, look for -e or -d to choose a device.
        while (true) {
            int c = getopt(argc, argv, "ed");
            if (c == -1)
                break;
            switch (c) {
                case 'e':
                    type = Device::EMULATOR;
                    break;
                case 'd':
                    type = Device::DEVICE;
                    break;
                default:
                    break;
            }
        }
        if (optind == argc) {
            LOGE("No command specified");
            return 1;
        }
    }

    // Do the initial connection.
    AdbConnection conn;
    conn.connect();

    const DeviceList& devices = conn.getDeviceList();
    // host:devices closes the connection, reconnect
    conn.connect();

    // No device specified and more than one connected, bail
    if (type == Device::NONE && devices.size() > 1) {
        LOGE("More than one device/emulator, please specify with -e or -d");
        return 1;
    } else if (devices.size() == 0) {
        LOGE("No devices connected");
        return 1;
    }

    // Find the correct device
    const Device* device = NULL;
    if (type == Device::NONE)
        device = devices[0]; // grab the only one
    else {
        // Search for a matching device type
        for (unsigned i = 0; i < devices.size(); i++) {
            if (devices[i]->type() == type) {
                device = devices[i];
                break;
            }
        }
    }

    if (!device) {
        LOGE("No device found!");
        return 1;
    }

    // Forward tcp:9999
    if (!device->sendRequest("forward:tcp:" PORT_STR ";tcp:" PORT_STR)) {
        LOGE("Failed to send forwarding request");
        return 1;
    }

    LOGV("Connecting to localhost port " PORT_STR);

    const char* command = argv[optind];
    int commandLen = strlen(command);
#define WDS_COMMAND_LENGTH 4
    if (commandLen != WDS_COMMAND_LENGTH) {
        LOGE("Commands must be 4 characters '%s'", command);
        return 1;
    }

    // Open the wds connection
    int wdsFd = wds_open();
    if (wdsFd == -1)
        return 1;

    // Send the command specified
    send(wdsFd, command, WDS_COMMAND_LENGTH, 0); // commands are 4 bytes

    // Read and display the response
    char response[256];
    int res = 0;
    while ((res = recv(wdsFd, response, sizeof(response), 0)) > 0)
        printf("%.*s", res, response);
    printf("\n\n");

    // Shutdown
    wds_close(wdsFd);

    return 0;
}
