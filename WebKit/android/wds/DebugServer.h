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
    Vector<Frame*> m_frames;
    ThreadIdentifier m_threadId;
    friend DebugServer* server();
};

DebugServer* server();

} // end namespace WDS

} // end namespace android

#endif
