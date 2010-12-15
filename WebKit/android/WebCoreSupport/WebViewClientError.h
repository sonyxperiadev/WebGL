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

#ifndef WebViewClientError_h
#define WebViewClientError_h

#include "ChromiumIncludes.h"

namespace android {

// This enum must be kept in sync with WebViewClient.java
enum WebViewClientError {
    /** Success */
    ERROR_OK = 0,
    /** Generic error */
    ERROR_UNKNOWN = -1,
    /** Server or proxy hostname lookup failed */
    ERROR_HOST_LOOKUP = -2,
    /** Unsupported authentication scheme (not basic or digest) */
    ERROR_UNSUPPORTED_AUTH_SCHEME = -3,
    /** User authentication failed on server */
    ERROR_AUTHENTICATION = -4,
    /** User authentication failed on proxy */
    ERROR_PROXY_AUTHENTICATION = -5,
    /** Failed to connect to the server */
    ERROR_CONNECT = -6,
    /** Failed to read or write to the server */
    ERROR_IO = -7,
    /** Connection timed out */
    ERROR_TIMEOUT = -8,
    /** Too many redirects */
    ERROR_REDIRECT_LOOP = -9,
    /** Unsupported URI scheme */
    ERROR_UNSUPPORTED_SCHEME = -10,
    /** Failed to perform SSL handshake */
    ERROR_FAILED_SSL_HANDSHAKE = -11,
    /** Malformed URL */
    ERROR_BAD_URL = -12,
    /** Generic file error */
    ERROR_FILE = -13,
    /** File not found */
    ERROR_FILE_NOT_FOUND = -14,
    /** Too many requests during this load */
    ERROR_TOO_MANY_REQUESTS = -15,
};

// Get the closest WebViewClient match to the given Chrome error code.
WebViewClientError ToWebViewClientError(net::Error);

} // namespace android

#endif // WebViewClientError_h
