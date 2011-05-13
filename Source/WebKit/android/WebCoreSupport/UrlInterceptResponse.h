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

#ifndef UrlInterceptResponse_h
#define UrlInterceptResponse_h

#include "PlatformString.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"

#include <jni.h>
#include <string>
#include <vector>

namespace android {

class JavaInputStreamWrapper;

class UrlInterceptResponse : public Noncopyable {
public:
    UrlInterceptResponse(JNIEnv* env, jobject response);
    ~UrlInterceptResponse();

    const std::string& mimeType() const {
        return m_mimeType;
    }

    const std::string& encoding() const {
        return m_encoding;
    }

    int status() const {
        return m_inputStream ? 200 : 404;
    }

    // Read from the input stream.  Returns false if reading failed.
    // A size of 0 indicates eof.
    bool readStream(std::vector<char>* out) const;

private:
    std::string m_mimeType;
    std::string m_encoding;
    OwnPtr<JavaInputStreamWrapper> m_inputStream;
};

}  // namespace android

#endif
