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

#ifndef MessageThread_h
#define MessageThread_h

#include <list>

#include "MessageTypes.h"

#include <utils/threads.h>

using std::list;

namespace android {

class MessageQueue {
public:
    MessageQueue() {}

    // Return true if the queue has messages with the given object and member
    // function.  If member is null, return true if the message has the same
    // object.
    template <class T>
    bool hasMessages(T* object, void (T::*member)(void));

    // Remove all messages with the given object and member function.  If
    // member is null, remove all messages with the given object.
    template <class T>
    void remove(T* object, void (T::*member)(void));

    // Post a new message to the queue.
    void post(Message* closure);

    // Post a new message at the front of the queue.
    void postAtFront(Message* closure);

    // Obtain the next message.  Blocks until either a new message arrives or
    // we reach the time of the next message.
    Message* next();

private:
    bool hasMessages(const Message& message);
    void remove(const Message& message);

    list<Message*> m_messages;
    Mutex          m_mutex;
    Condition      m_condition;
};

template <class T>
bool MessageQueue::hasMessages(T* object, void (T::*member)(void)) {
    MemberFunctionMessage<T, void> message(object, member);
    return hasMessages(message);
}

template <class T>
void MessageQueue::remove(T* object, void (T::*member)(void)) {
    MemberFunctionMessage<T, void> message(object, member);
    remove(message);
}

class MessageThread : public Thread {
public:
    MessageQueue& queue() { return m_queue; }

private:
    MessageThread() : Thread(true /* canCallJava */) {}

    virtual bool threadLoop();

    MessageQueue m_queue;
    // Used for thread initialization
    Mutex m_mutex;
    Condition m_condition;

    friend MessageThread* messageThread();
};

// Get (possibly creating) the global MessageThread object used to pass
// messages to WebCore.
MessageThread* messageThread();

}  // namespace android

#endif  // MessageThread_h
