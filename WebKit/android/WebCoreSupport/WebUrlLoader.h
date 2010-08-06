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

#ifndef WebUrlLoader_h
#define WebUrlLoader_h

#include "ResourceLoaderAndroid.h"

using namespace WebCore;

namespace android {
class WebUrlLoaderClient;

class WebUrlLoader : public ResourceLoaderAndroid {
public:
    virtual ~WebUrlLoader();
    static PassRefPtr<WebUrlLoader> start(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, bool sync, bool isPrivateBrowsing);

    virtual void cancel();
    virtual void downloadFile() {} // Not implemented yet
    virtual void pauseLoad(bool pause) {} // Android method, does nothing for now

private:
    WebUrlLoader(WebCore::ResourceHandle*, const WebCore::ResourceRequest&);
    static PassRefPtr<WebUrlLoader> create(WebCore::ResourceHandle*, const WebCore::ResourceRequest&);

    OwnPtr<WebUrlLoaderClient> m_loaderClient;
};

} // namespace android

#endif
