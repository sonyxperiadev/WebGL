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

#ifndef WDS_COMMAND_H
#define WDS_COMMAND_H

#include "wtf/MainThread.h"
#include "wtf/Vector.h"

namespace WebCore {
class Frame;
}

using namespace WTF;
using namespace WebCore;

namespace android {

// WebCore Debug Server
namespace WDS {

class Connection;

// Command identifier length
#define COMMAND_LENGTH 4

// The dispatcher function called with a Frame for context and the established
// connection to the client. The connection can be used to read and write to the
// client application. Return true on successful completion of the command,
// return false to indicate failure.
typedef bool (*DispatchFunction)(const Frame*, const Connection*);

// Note: Although the type is named MainThreadFunction, it may not always be
// the main thread. The type is generic enough to reuse here but named
// something more appropriate.
typedef MainThreadFunction TargetThreadFunction;

// Helper class to dipatch functions on a particular thread.
class Handler {
public:
    virtual ~Handler() {}
    virtual void post(TargetThreadFunction, void*) const = 0;
};

// Class for containing information about particular commands.
class Command {
public:
    Command(const char* name, const char* desc, const DispatchFunction func,
            const Handler& handler)
        : m_name(name)
        , m_description(desc)
        , m_dispatch(func)
        , m_handler(handler) {}
    Command(const Command& comm)
        : m_name(comm.m_name)
        , m_description(comm.m_description)
        , m_dispatch(comm.m_dispatch)
        , m_handler(comm.m_handler) {}
    virtual ~Command() {}

    // Initialize the debug server commands
    static void Init();

    // Find the command specified by the client request.
    static Command* Find(const Connection* conn);

    // Dispatch this command
    void dispatch();

    const char* name() const { return m_name; }

protected:
    const char* m_name;
    const char* m_description;
    const DispatchFunction m_dispatch;

private:
    const Handler& m_handler;
    static Vector<const Command*>* s_commands;
};

} // end namespace WDS

} // end namespace android

#endif
