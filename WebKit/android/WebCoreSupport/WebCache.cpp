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

#include "WebCache.h"
#include "WebRequestContext.h"
#include "WebUrlLoaderClient.h"

namespace android {

WebCache* WebCache::s_instance = 0;

void WebCache::clear()
{
     base::Thread* thread = WebUrlLoaderClient::ioThread();
     if (thread)
         thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(getInstance(), &WebCache::doClear));
}

WebCache::WebCache()
    : m_doomAllEntriesCallback(this, &WebCache::doomAllEntries)
    , m_doneCallback(this, &WebCache::done)
    , m_isClearInProgress(false)
{
}

WebCache* WebCache::getInstance()
{
    if (!s_instance) {
        s_instance = new WebCache();
        s_instance->AddRef();
    }
    return s_instance;
}

void WebCache::doClear()
{
    if (m_isClearInProgress)
        return;
    m_isClearInProgress = true;
    URLRequestContext* context = WebRequestContext::GetAndroidContext();
    net::HttpTransactionFactory* factory = context->http_transaction_factory();
    int code = factory->GetCache()->GetBackend(&m_cacheBackend, &m_doomAllEntriesCallback);
    // Code ERR_IO_PENDING indicates that the operation is still in progress and
    // the supplied callback will be invoked when it completes.
    if (code == net::ERR_IO_PENDING)
        return;
    else if (code != OK) {
        m_isClearInProgress = false;
        return;
    }
    doomAllEntries(0 /*unused*/);
}

void WebCache::doomAllEntries(int)
{
    if (!m_cacheBackend) {
        m_isClearInProgress = false;
        return;
    }

    int code = m_cacheBackend->DoomAllEntries(&m_doneCallback);
    // Code ERR_IO_PENDING indicates that the operation is still in progress and
    // the supplied callback will be invoked when it completes.
    if (code == net::ERR_IO_PENDING)
        return;
    else if (code != OK) {
        m_isClearInProgress = false;
        return;
    }
    done(0 /*unused*/);
}

void WebCache::done(int)
{
    m_isClearInProgress = false;
}

} // namespace android
