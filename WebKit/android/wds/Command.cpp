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

#include "AndroidLog.h"
#include "CString.h"
#include "Command.h"
#include "Connection.h"
#include "DebugServer.h"
#include "Frame.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "WebViewCore.h"
#include <utils/Log.h>

#if ENABLE(WDS)

using namespace WebCore;

namespace android {

namespace WDS {

//------------------------------------------------------------------------------
// Actual commands -- XXX should be moved somewhere else
//------------------------------------------------------------------------------
static bool callDumpRenderTree(const Frame* frame, const Connection* conn) {
    CString str = externalRepresentation(frame->contentRenderer()).latin1();
    conn->write(str.data(), str.length());
    return true;
}

static bool callDumpDomTree(const Frame* frame, const Connection* conn) {
    WebViewCore::getWebViewCore(frame->view())->dumpDomTree(true);

    FILE* f = fopen(DOM_TREE_LOG_FILE, "r");
    if (!f) {
        conn->write("Dom tree written to logcat\n");
    } else {
        char buf[512];
        while (true) {
            int nread = fread(buf, 1, sizeof(buf), f);
            if (nread <= 0)
                break;
            conn->write(buf, nread);
        }
        fclose(f);
    }
    return true;
}

class WebCoreHandler : public Handler {
public:
    virtual void post(TargetThreadFunction func, void* v) const {
        callOnMainThread(func, v);
    }
};
static WebCoreHandler s_webcoreHandler;

//------------------------------------------------------------------------------
// End command section
//------------------------------------------------------------------------------

class InternalCommand : public Command {
public:
    InternalCommand(const Command* comm, const Frame* frame,
            const Connection* connection)
        : Command(*comm)
        , m_frame(frame)
        , m_connection(connection) {}
    virtual ~InternalCommand() { delete m_connection; }

    void doCommand() const {
        LOGD("Executing command '%s' (%s)", m_name, m_description);
        if (!m_dispatch(m_frame, m_connection))
            // XXX: Have useful failure messages
            m_connection->write("EPIC FAIL!\n", 11);
    }

private:
    const Frame* m_frame;
    const Connection* m_connection;
};

static void commandDispatcher(void* v) {
    InternalCommand* c = static_cast<InternalCommand*>(v);
    c->doCommand();
    delete c;
}

void Command::dispatch() {
    m_handler.post(commandDispatcher, this);
}

Vector<const Command*>* Command::s_commands;

void Command::Init() {
    // Do not initialize twice.
    if (s_commands)
        return;
    // XXX: Move this somewhere else.
    s_commands = new Vector<const Command*>();
    s_commands->append(new Command("DDOM", "Dump Dom Tree",
                callDumpDomTree, s_webcoreHandler));
    s_commands->append(new Command("DDRT", "Dump Render Tree",
                callDumpRenderTree, s_webcoreHandler));
}

Command* Command::Find(const Connection* conn) {
    char buf[COMMAND_LENGTH];
    if (conn->read(buf, sizeof(buf)) != COMMAND_LENGTH)
        return NULL;

    // Linear search of commands. TODO: binary search when more commands are
    // added.
    Vector<const Command*>::const_iterator i = s_commands->begin();
    Vector<const Command*>::const_iterator end = s_commands->end();
    while (i != end) {
        if (strncmp(buf, (*i)->name(), sizeof(buf)) == 0)
            return new InternalCommand(*i, server()->getFrame(0), conn);
        i++;
    }
    return NULL;
}

} // end namespace WDS

} // end namespace android

#endif
