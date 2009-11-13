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

#ifndef DEBUGSERVER_H
#define DEBUGSERVER_H

// Turn on the wds feature in webkit
#define ENABLE_WDS 0

#include "wtf/Threading.h"
#include "wtf/Vector.h"

// Forward declarations.
namespace WebCore {
    class Frame;
}

using namespace WTF;
using namespace WebCore;

namespace android {

// WebCore Debug Server
namespace WDS {

class DebugServer : WTFNoncopyable::Noncopyable {
public:
    void start();
    void addFrame(Frame* frame) {
        m_frames.append(frame);
    }
    void removeFrame(Frame* frame) {
        size_t i = m_frames.find(frame);
        if (i != notFound)
            m_frames.remove(i);
    }
    Frame* getFrame(unsigned idx) {
        if (idx < m_frames.size())
            return m_frames.at(idx);
        return NULL;
    }
private:
    DebugServer();
    WTF::Vector<Frame*> m_frames;
    ThreadIdentifier m_threadId;
    friend DebugServer* server();
};

DebugServer* server();

} // end namespace WDS

} // end namespace android

#endif
