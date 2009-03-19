/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"
#include "JavaSharedClient.h"
#include "SharedBuffer.h"
#include "SharedBufferStream.h"

using namespace android;

namespace WebCore {

    static void CallDeref(void* buffer) {
        ((SharedBuffer*)buffer)->deref();
    }

    SharedBufferStream::SharedBufferStream(SharedBuffer* buffer)
            : SkMemoryStream(buffer->data(), buffer->size(), false) {
        fBuffer = buffer;
        buffer->ref();
    }

    SharedBufferStream::~SharedBufferStream() {
        // we can't necessarily call fBuffer->deref() here, as we may be
        // in a different thread from webkit, and SharedBuffer is not
        // threadsafe. Therefore we defer it until it can be executed in the
        // webkit thread.
        JavaSharedClient::EnqueueFunctionPtr(CallDeref, fBuffer);
    }

}

