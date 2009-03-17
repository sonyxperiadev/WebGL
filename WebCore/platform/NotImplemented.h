/*
 * Copyright (C) 2007 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NotImplemented_h
#define NotImplemented_h

#include "Logging.h"
#include <wtf/Assertions.h>

#if PLATFORM(GTK)
    #define supressNotImplementedWarning() getenv("DISABLE_NI_WARNING")
#elif PLATFORM(QT)
    #include <QByteArray>
    #define supressNotImplementedWarning() !qgetenv("DISABLE_NI_WARNING").isEmpty()
#else
    #define supressNotImplementedWarning() false
#endif

#if defined ANDROID

    #if 1 && defined LOG_TAG
        #ifndef _LIBS_UTILS_LOG_H
            #undef LOG
            #include <utils/Log.h>
        #endif
        #define notImplemented() LOGV("%s: notImplemented\n", __PRETTY_FUNCTION__)
        #define lowPriority_notImplemented() //printf("%s\n", __PRETTY_FUNCTION__)
        #define verifiedOk()    // not a problem that it's not implemented
    #else
        #define notImplemented() fprintf(stderr, "%s\n", __PRETTY_FUNCTION__)
    #endif
#elif defined(NDEBUG)
    #define notImplemented() ((void)0)
#else

#define notImplemented() do { \
        static bool havePrinted = false; \
        if (!havePrinted && !supressNotImplementedWarning()) { \
            WTFLogVerbose(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, &::WebCore::LogNotYetImplemented, "UNIMPLEMENTED: "); \
            havePrinted = true; \
        } \
    } while (0)

#endif // NDEBUG

#endif // NotImplemented_h
