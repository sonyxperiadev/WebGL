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

#ifndef ANDROID_WEBKIT_MESSAGETYPES_H_
#define ANDROID_WEBKIT_MESSAGETYPES_H_

#include <wtf/CurrentTime.h>

// TODO(phanna): autogenerate these types!

namespace android {

// Forward declared for friendship!
class MessageQueue;

// Removes the reference from the typename so we store the actual value in the
// closure.
template <typename T> struct remove_reference { typedef T type; };
template <typename T> struct remove_reference<T&> { typedef T type; };

// Prevent the compiler from inferring the type.
template <typename T> struct identity { typedef T type; };

// Message base class.  Defines the public run() method and contains generic
// object and member function variables for use in MessageQueue.
//
// Note: The template subclass MemberFunctionMessage casts its object and
// member function to the generic void* and Message::* types.  During run(),
// each template specialization downcasts to the original type and invokes the
// correct function.  This may seem dangerous but the compiler enforces
// correctness in NewMessage and in the template constructor.
class Message {
public:
    typedef void (Message::*GenericMemberFunction)(void);

    virtual ~Message() {}
    virtual void run() = 0;

    // The wall time that the message is supposed to run.
    double m_when;

    void* object() const { return m_object; }
    GenericMemberFunction member() const { return m_member; }

protected:
    Message(void* object, GenericMemberFunction member, long delay = 0)
        : m_object(object)
        , m_member(member) {
        m_when = WTF::currentTimeMS() + delay;
    }

    // Downcast back to the original template params in run().  Also accessed
    // by MessageQueue to compare messages.
    void*                 m_object;
    GenericMemberFunction m_member;

private:
    // Disallow copy
    Message(const Message&);
};

// Forward declaration for partial specialization.
template <class T, typename A1>
class MemberFunctionMessage;

template <class T>
class MemberFunctionMessage<T, void>  : public Message {
private:
    typedef void (T::*MemberSignature)();

public:
    inline MemberFunctionMessage(T* object,
                                 MemberSignature member,
                                 long delay = 0)
        : Message(reinterpret_cast<void*>(object),
                  reinterpret_cast<GenericMemberFunction>(member),
                  delay) {}

    virtual void run() {
        MemberSignature member = reinterpret_cast<MemberSignature>(m_member);
        (reinterpret_cast<T*>(m_object)->*member)();
        delete this;
    }
};

template <class T>
inline Message* NewMessage(T* object, void (T::*member)()) {
    return new MemberFunctionMessage<T, void>(object, member);
}

template <class T>
inline Message* NewDelayedMessage(T* object, void (T::*member)(), long delay) {
    return new MemberFunctionMessage<T, void>(object, member, delay);
}

template <class T, typename A1>
class MemberFunctionMessage : public Message {
private:
    typedef void (T::*MemberSignature)(A1);

public:
    inline MemberFunctionMessage(T* object,
                                 MemberSignature member,
                                 A1 arg1,
                                 long delay = 0)
        : Message(reinterpret_cast<void*>(object),
                  reinterpret_cast<GenericMemberFunction>(member),
                  delay)
        , m_arg1(arg1) {}

    virtual void run() {
        MemberSignature member = reinterpret_cast<MemberSignature>(m_member);
        (reinterpret_cast<T*>(m_object)->*member)(m_arg1);
        delete this;
    }

private:
    typename remove_reference<A1>::type m_arg1;
};

template <class T, typename A1>
inline Message* NewMessage(T* object, void (T::*member)(A1),
        typename identity<A1>::type arg1) {
    return new MemberFunctionMessage<T, A1>(
            object, member, arg1);
}

template <class T, typename A1>
inline Message* NewDelayedMessage(T* object, void (T::*member)(A1),
        typename identity<A1>::type arg1, long delay) {
    return new MemberFunctionMessage<T, A1>(object, member, arg1, delay);
}

}  // namespace android


#endif  // ANDROID_WEBKIT_MESSAGETYPES_H_
