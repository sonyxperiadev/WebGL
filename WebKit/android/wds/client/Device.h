/*
 * Copyright 2009, The Android Open Source Project
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

#ifndef WDS_DEVICE_H
#define WDS_DEVICE_H

#include <stdlib.h>

class AdbConnection;

class Device {
public:
    // Type of device.
    // TODO: Add simulator support
    enum DeviceType {
        NONE = -1,
        EMULATOR,
        DEVICE
    };

    // Takes ownership of name
    Device(char* name, DeviceType type, const AdbConnection* conn)
        : m_connection(conn)
        , m_name(strdup(name))
        , m_type(type) {}
    ~Device() { free(m_name); }

    const char* name() const  { return m_name; }
    DeviceType type() const   { return m_type; }

    // Send a request to this device.
    bool sendRequest(const char* req) const;

private:
    const AdbConnection* m_connection;
    char*      m_name;
    DeviceType m_type;
};

#endif
