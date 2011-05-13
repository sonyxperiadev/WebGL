/*
 * Copyright 2010, The Android Open Source Project
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

#define LOG_TAG "MessageThread"

#include "config.h"

#include <sys/time.h>
#include <time.h>

#include "MessageThread.h"
#include "ScriptController.h"

#include <utils/Log.h>

namespace android {

static bool compareMessages(const Message& msg1,
                            const Message& msg2,
                            bool memberIsNull) {
    return (msg1.object() == msg2.object() &&
            (memberIsNull || msg1.member() == msg2.member()));
}

bool MessageQueue::hasMessages(const Message& message) {
    AutoMutex lock(m_mutex);

    static const Message::GenericMemberFunction nullMember = NULL;
    const bool memberIsNull = message.member() == nullMember;

    for (list<Message*>::iterator it = m_messages.begin();
         it != m_messages.end(); ++it) {
        Message* m = *it;
        if (compareMessages(message, *m, memberIsNull))
            return true;
    }
    return false;
}

void MessageQueue::remove(const Message& message) {
    AutoMutex lock(m_mutex);

    static const Message::GenericMemberFunction nullMember = NULL;
    const bool memberIsNull = message.member() == nullMember;

    for (list<Message*>::iterator it = m_messages.begin();
         it != m_messages.end(); ++it) {
        Message* m = *it;
        if (compareMessages(message, *m, memberIsNull)) {
            it = m_messages.erase(it);
            delete m;
        }
    }
}

void MessageQueue::post(Message* message) {
    AutoMutex lock(m_mutex);

    double when = message->m_when;
    LOG_ASSERT(when > 0, "Message time may not be 0");

    list<Message*>::iterator it;
    for (it = m_messages.begin(); it != m_messages.end(); ++it) {
        Message* m = *it;
        if (when < m->m_when) {
            break;
        }
    }
    m_messages.insert(it, message);
    m_condition.signal();
}

void MessageQueue::postAtFront(Message* message) {
    AutoMutex lock(m_mutex);
    message->m_when = 0;
    m_messages.push_front(message);
}

Message* MessageQueue::next() {
    AutoMutex lock(m_mutex);
    while (true) {
        if (m_messages.empty()) {
            // No messages, wait until another arrives
            m_condition.wait(m_mutex);
        }
        Message* next = m_messages.front();
        double now = WTF::currentTimeMS();
        double diff = next->m_when - now;
        if (diff > 0) {
            // Not time for this message yet, wait the difference in nanos
            m_condition.waitRelative(m_mutex,
                    static_cast<nsecs_t>(diff * 1000000) /* nanos */);
        } else {
            // Time for this message to run.
            m_messages.pop_front();
            return next;
        }
    }
}

bool MessageThread::threadLoop() {
    WebCore::ScriptController::initializeThreading();

    while (true) {
        Message* message = m_queue.next();
        if (message != NULL) {
            message->run();
        }
    }
    return false;
}

// Global thread object obtained by messageThread().
static sp<MessageThread> gMessageThread;

MessageThread* messageThread() {
    if (gMessageThread == NULL) {
        gMessageThread = new MessageThread();
        gMessageThread->run("WebCoreThread");
    }
    return gMessageThread.get();
}

}  // namespace android
