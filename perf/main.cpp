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

#define LOG_TAG "webcore_test"

#include "config.h"

#include "BackForwardList.h"
#include "ChromeClientAndroid.h"
#include "ContextMenuClientAndroid.h"
#include "CookieClient.h"
#include "DragClientAndroid.h"
#include "EditorClientAndroid.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClientAndroid.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HistoryItem.h"
#if USE(JSC)
#include "InitializeThreading.h"
#elif USE(V8)
#include "V8InitializeThreading.h"
#endif
#include "InspectorClientAndroid.h"
#include "Intercept.h"
#include "IntRect.h"
#include "JavaSharedClient.h"
#include "jni_utility.h"
#include "MyJavaVM.h"
#include "Page.h"
#include "PlatformGraphicsContext.h"
#include "ResourceRequest.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SubstituteData.h"
#include "TimerClient.h"
#include "TextEncoding.h"
#include "WebCoreViewBridge.h"
#include "WebFrameView.h"
#include "WebViewCore.h"

#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkImageEncoder.h"

#include <getopt.h>
#include <utils/Log.h>

using namespace android;
using namespace WebCore;

class MyJavaSharedClient : public TimerClient, public CookieClient {
public:
    MyJavaSharedClient() : m_hasTimer(false) {}
    virtual void setSharedTimer(long long timemillis) { m_hasTimer = true; }
    virtual void stopSharedTimer() { m_hasTimer = false; }
    virtual void setSharedTimerCallback(void (*f)()) { m_func = f; }
    virtual void signalServiceFuncPtrQueue() {}

    // Cookie methods that do nothing.
    virtual void setCookies(const KURL&, const KURL&, const String&) {}
    virtual String cookies(const KURL&) { return ""; }
    virtual bool cookiesEnabled() { return false; }

    bool m_hasTimer;
    void (*m_func)();
};

static void historyItemChanged(HistoryItem* i) {
    if (i->bridge())
        i->bridge()->updateHistoryItem(i);
}

int main(int argc, char** argv) {
    int width = 800;
    int height = 600;
    int reloadCount = 0;
    while (true) {
        int c = getopt(argc, argv, "d:r:");
        if (c == -1)
            break;
        else if (c == 'd') {
            char* x = strchr(optarg, 'x');
            if (x) {
                width = atoi(optarg);
                height = atoi(x + 1);
                LOGD("Rendering page at %dx%d", width, height);
            }
        } else if (c == 'r') {
            reloadCount = atoi(optarg);
            if (reloadCount < 0)
                reloadCount = 0;
            LOGD("Reloading %d times", reloadCount);
        }
    }
    if (optind >= argc) {
        LOGE("Please supply a file to read\n");
        return 1;
    }
#if USE(JSC)
    JSC::initializeThreading();
#elif USE(V8)
    V8::initializeThreading();
#endif

    // Setting this allows data: urls to load from a local file.
    FrameLoader::setLocalLoadPolicy(FrameLoader::AllowLocalLoadsForAll);

    // Create the fake JNIEnv and JavaVM
    InitializeJavaVM();

    // The real function is private to libwebcore but we know what it does.
    notifyHistoryItemChanged = historyItemChanged;

    // Implement the shared timer callback
    MyJavaSharedClient client;
    JavaSharedClient::SetTimerClient(&client);
    JavaSharedClient::SetCookieClient(&client);

    // Create the page with all the various clients
    ChromeClientAndroid* chrome = new ChromeClientAndroid;
    EditorClientAndroid* editor = new EditorClientAndroid;
    Page* page = new Page(chrome, new ContextMenuClientAndroid, editor,
            new DragClientAndroid, new InspectorClientAndroid);
    editor->setPage(page);

    // Create MyWebFrame that intercepts network requests
    MyWebFrame* webFrame = new MyWebFrame(page);
    webFrame->setUserAgent("Performance testing"); // needs to be non-empty
    chrome->setWebFrame(webFrame);
    // ChromeClientAndroid maintains the reference.
    Release(webFrame);

    // Create the Frame and the FrameLoaderClient
    FrameLoaderClientAndroid* loader = new FrameLoaderClientAndroid(webFrame);
    RefPtr<Frame> frame = Frame::create(page, NULL, loader);
    loader->setFrame(frame.get());

    // Build our View system, resize it to the given dimensions and release our
    // references. Note: We keep a referenec to frameView so we can layout and
    // draw later without risk of it being deleted.
    WebViewCore* webViewCore = new WebViewCore(JSC::Bindings::getJNIEnv(),
            MY_JOBJECT, frame.get());
    FrameView* frameView = new FrameView(frame.get());
    WebFrameView* webFrameView = new WebFrameView(frameView, webViewCore);
    frame->setView(frameView);
    frameView->resize(width, height);
    Release(webViewCore);
    Release(webFrameView);

    // Initialize the frame and turn of low-bandwidth display (it fails an
    // assertion in the Cache code)
    frame->init();
    frame->selection()->setFocused(true);

    // Set all the default settings the Browser normally uses.
    Settings* s = frame->settings();
    s->setLayoutAlgorithm(Settings::kLayoutNormal); // Normal layout for now
    s->setStandardFontFamily("sans-serif");
    s->setFixedFontFamily("monospace");
    s->setSansSerifFontFamily("sans-serif");
    s->setSerifFontFamily("serif");
    s->setCursiveFontFamily("cursive");
    s->setFantasyFontFamily("fantasy");
    s->setMinimumFontSize(8);
    s->setMinimumLogicalFontSize(8);
    s->setDefaultFontSize(16);
    s->setDefaultFixedFontSize(13);
    s->setLoadsImagesAutomatically(true);
    s->setJavaScriptEnabled(true);
    s->setDefaultTextEncodingName("latin1");
    s->setPluginsEnabled(false);
    s->setShrinksStandaloneImagesToFit(false);
    s->setUseWideViewport(false);

    // Finally, load the actual data
    ResourceRequest req(argv[optind]);
    frame->loader()->load(req, false);

    do {
        // Layout the page and service the timer
        frameView->layout();
        while (client.m_hasTimer) {
            client.m_func();
            JavaSharedClient::ServiceFunctionPtrQueue();
        }
        JavaSharedClient::ServiceFunctionPtrQueue();

        // Layout more if needed.
        while (frameView->needsLayout())
            frameView->layout();
        JavaSharedClient::ServiceFunctionPtrQueue();

        if (reloadCount)
            frame->loader()->reload(true);
    } while (reloadCount--);

    // Draw into an offscreen bitmap
    SkBitmap bmp;
    bmp.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    bmp.allocPixels();
    SkCanvas canvas(bmp);
    PlatformGraphicsContext ctx(&canvas, NULL);
    GraphicsContext gc(&ctx);
    frameView->paintContents(&gc, IntRect(0, 0, width, height));

    // Write the bitmap to the sdcard
    SkImageEncoder* enc = SkImageEncoder::Create(SkImageEncoder::kPNG_Type);
    enc->encodeFile("/sdcard/webcore_test.png", bmp, 100);
    delete enc;

    // Tear down the world.
    frameView->deref();
    frame->loader()->detachFromParent();
    delete page;

    return 0;
}
