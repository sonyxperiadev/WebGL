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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef INTERCEPT_H
#define INTERCEPT_H

#include "MyJavaVM.h"
#include "PlatformString.h"
#include "Timer.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreResourceLoader.h"
#include <JNIUtility.h>
#include <wtf/Vector.h>

namespace WebCore {
    class Page;
    class ResourceHandle;
    class ResourceRequest;
}

using namespace android;
using namespace WebCore;
using namespace WTF;

class MyResourceLoader : public WebCoreResourceLoader {
public:
    static PassRefPtr<WebCore::ResourceLoaderAndroid> create(
            ResourceHandle* handle, String url);
    void handleRequest();

private:
    MyResourceLoader(ResourceHandle* handle, String url)
        : WebCoreResourceLoader(JSC::Bindings::getJNIEnv(), MY_JOBJECT)
        , m_handle(handle)
        , m_url(url) {}

    void loadData(const String&);
    void loadFile(const String&);
    ResourceHandle* m_handle;
    String m_url;
};

class MyWebFrame : public WebFrame {
public:
    MyWebFrame(Page* page)
        : WebFrame(JSC::Bindings::getJNIEnv(), MY_JOBJECT, MY_JOBJECT, page)
        , m_timer(this, &MyWebFrame::timerFired) {}

    virtual PassRefPtr<WebCore::ResourceLoaderAndroid> startLoadingResource(
            ResourceHandle* handle, const ResourceRequest& req, bool, bool);

    virtual bool canHandleRequest(const ResourceRequest&) { return true; }

private:
    void timerFired(Timer<MyWebFrame>*);
    Vector<RefPtr<WebCore::ResourceLoaderAndroid> > m_requests;
    Timer<MyWebFrame> m_timer;
};

#endif
