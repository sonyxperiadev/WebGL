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
#include "WebViewClientError.h"

using namespace net;

namespace android {

WebViewClientError ToWebViewClientError(net::Error error) {
    // Note: many net::Error constants don't have an obvious mapping.
    // These will be handled by the default case, ERROR_UNKNOWN.
    switch(error) {
        case ERR_UNSUPPORTED_AUTH_SCHEME:
            return ERROR_UNSUPPORTED_AUTH_SCHEME;

        case ERR_INVALID_AUTH_CREDENTIALS:
        case ERR_MISSING_AUTH_CREDENTIALS:
        case ERR_MISCONFIGURED_AUTH_ENVIRONMENT:
            return ERROR_AUTHENTICATION;

        case ERR_TOO_MANY_REDIRECTS:
            return ERROR_REDIRECT_LOOP;

        case ERR_UPLOAD_FILE_CHANGED:
            return ERROR_FILE_NOT_FOUND;

        case ERR_INVALID_URL:
            return ERROR_BAD_URL;

        case ERR_DISALLOWED_URL_SCHEME:
        case ERR_UNKNOWN_URL_SCHEME:
            return ERROR_UNSUPPORTED_SCHEME;

        case ERR_IO_PENDING:
        case ERR_NETWORK_IO_SUSPENDED:
            return ERROR_IO;

        case ERR_CONNECTION_TIMED_OUT:
        case ERR_TIMED_OUT:
            return ERROR_TIMEOUT;

        case ERR_FILE_TOO_BIG:
            return ERROR_FILE;

        case ERR_HOST_RESOLVER_QUEUE_TOO_LARGE:
        case ERR_INSUFFICIENT_RESOURCES:
        case ERR_OUT_OF_MEMORY:
            return ERROR_TOO_MANY_REQUESTS;

        case ERR_CONNECTION_CLOSED:
        case ERR_CONNECTION_RESET:
        case ERR_CONNECTION_REFUSED:
        case ERR_CONNECTION_ABORTED:
        case ERR_CONNECTION_FAILED:
        case ERR_SOCKET_NOT_CONNECTED:
            return ERROR_CONNECT;

        case ERR_ADDRESS_INVALID:
        case ERR_ADDRESS_UNREACHABLE:
        case ERR_NAME_NOT_RESOLVED:
        case ERR_NAME_RESOLUTION_FAILED:
            return ERROR_HOST_LOOKUP;

        case ERR_SSL_PROTOCOL_ERROR:
        case ERR_SSL_CLIENT_AUTH_CERT_NEEDED:
        case ERR_TUNNEL_CONNECTION_FAILED:
        case ERR_NO_SSL_VERSIONS_ENABLED:
        case ERR_SSL_VERSION_OR_CIPHER_MISMATCH:
        case ERR_SSL_RENEGOTIATION_REQUESTED:
        case ERR_CERT_ERROR_IN_SSL_RENEGOTIATION:
        case ERR_BAD_SSL_CLIENT_AUTH_CERT:
        case ERR_SSL_NO_RENEGOTIATION:
        case ERR_SSL_DECOMPRESSION_FAILURE_ALERT:
        case ERR_SSL_BAD_RECORD_MAC_ALERT:
        case ERR_SSL_UNSAFE_NEGOTIATION:
        case ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY:
        case ERR_SSL_SNAP_START_NPN_MISPREDICTION:
        case ERR_SSL_CLIENT_AUTH_PRIVATE_KEY_ACCESS_DENIED:
        case ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY:
            return ERROR_FAILED_SSL_HANDSHAKE;

        case ERR_PROXY_AUTH_UNSUPPORTED:
        case ERR_PROXY_AUTH_REQUESTED:
        case ERR_PROXY_CONNECTION_FAILED:
        case ERR_UNEXPECTED_PROXY_AUTH:
            return ERROR_PROXY_AUTHENTICATION;

        /* The certificate errors are handled by their own dialog
         * and don't need to be reported to the framework again.
         */
        case ERR_CERT_COMMON_NAME_INVALID:
        case ERR_CERT_DATE_INVALID:
        case ERR_CERT_AUTHORITY_INVALID:
        case ERR_CERT_CONTAINS_ERRORS:
        case ERR_CERT_NO_REVOCATION_MECHANISM:
        case ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
        case ERR_CERT_REVOKED:
        case ERR_CERT_INVALID:
        case ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
        case ERR_CERT_NOT_IN_DNS:
            return ERROR_OK;

        default:
            return ERROR_UNKNOWN;
    }
}

}
