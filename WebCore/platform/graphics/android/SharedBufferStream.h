/*
 * Copyright 2009, The Android Open Source Project
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

#ifndef WebCore_SharedBufferStream_DEFINED
#define WebCore_SharedBufferStream_DEFINED

#include "SkStream.h"
#include "SharedBuffer.h"

namespace WebCore {

    /** Subclass of SkStream that wrapps a webcore SharedBuffer object. To
        allow this object to be deleted from any thread, the impl will ensure
        that we unref the SharedBuffer object from the correct webcore thread.
     */
    class SharedBufferStream : public SkMemoryStream {
    public:
        SharedBufferStream(SharedBuffer* buffer);
        virtual ~SharedBufferStream();

    private:
        // don't allow this to change our data. should not get called, but we
        // override here just to be sure
        virtual void setMemory(const void* data, size_t length, bool copyData) {
            sk_throw();
        }

        // we share ownership of this with webkit
        SharedBuffer* fBuffer;
    };                           
}

#endif
