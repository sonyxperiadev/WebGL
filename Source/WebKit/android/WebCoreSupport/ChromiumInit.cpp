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

#include "config.h"
#include "ChromiumInit.h"

#include "ChromiumIncludes.h"
#include "JNIUtility.h"
#include "jni.h"

#include <cutils/log.h>
#include <string>

namespace android {

bool logMessageHandler(int severity, const char* file, int line, size_t message_start, const std::string& str) {
    int androidSeverity = ANDROID_LOG_VERBOSE;
    switch(severity) {
    case logging::LOG_FATAL:
        androidSeverity = ANDROID_LOG_FATAL;
        break;
    case logging::LOG_ERROR_REPORT:
    case logging::LOG_ERROR:
        androidSeverity = ANDROID_LOG_ERROR;
        break;
    case logging::LOG_WARNING:
        androidSeverity = ANDROID_LOG_WARN;
        break;
    default:
        androidSeverity = ANDROID_LOG_VERBOSE;
        break;
    }
    android_printLog(androidSeverity, "chromium", "%s:%d: %s", file, line, str.c_str());
    return false;
}

namespace {
    scoped_ptr<net::NetworkChangeNotifier> networkChangeNotifier;
}

void initChromium()
{
    static base::Lock lock;
    base::AutoLock aLock(lock);
    static bool initCalled = false;
    if (!initCalled) {
        logging::SetLogMessageHandler(logMessageHandler);
        networkChangeNotifier.reset(net::NetworkChangeNotifier::Create());
        net::HttpNetworkLayer::EnableSpdy("npn");
        initCalled = true;
        jni::SetJavaVM(JSC::Bindings::getJavaVM());
    }
}

} // namespace android
