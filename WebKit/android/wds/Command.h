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
