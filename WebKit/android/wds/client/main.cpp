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
