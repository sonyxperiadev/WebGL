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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "webcoreglue"

#include "config.h"

#include "BackForwardList.h"
#include "ChromeClientAndroid.h"
#include "ContextMenuClientAndroid.h"
#include "CookieClient.h"
#include "DeviceMotionClientAndroid.h"
#include "DeviceOrientationClientAndroid.h"
#include "DragClientAndroid.h"
#include "EditorClientAndroid.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClientAndroid.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HistoryItem.h"
#include "InspectorClientAndroid.h"
#include "IntRect.h"
#include "JavaSharedClient.h"
#include "Page.h"
#include "PlatformGraphicsContext.h"
#include "ResourceRequest.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkImageEncoder.h"
#include "SubstituteData.h"
#include "TimerClient.h"
#include "TextEncoding.h"
#include "WebCoreViewBridge.h"
#include "WebFrameView.h"
#include "WebViewCore.h"
#include "benchmark/Intercept.h"
#include "benchmark/MyJavaVM.h"

#include <JNIUtility.h>
#include <jni.h>
#include <utils/Log.h>

#define EXPORT __attribute__((visibility("default")))

namespace android {

extern int registerWebFrame(JNIEnv*);
extern int registerJavaBridge(JNIEnv*);
extern int registerJniUtil(JNIEnv*);
extern int registerResourceLoader(JNIEnv*);
extern int registerWebViewCore(JNIEnv*);
extern int registerWebHistory(JNIEnv*);
extern int registerWebIconDatabase(JNIEnv*);
extern int registerWebSettings(JNIEnv*);
extern int registerWebView(JNIEnv*);
extern int registerViewStateSerializer(JNIEnv*);
#if ENABLE(DATABASE)
extern int registerWebStorage(JNIEnv*);
#endif
extern int registerGeolocationPermissions(JNIEnv*);
extern int registerMockGeolocation(JNIEnv*);
#if ENABLE(VIDEO)
extern int registerMediaPlayerAudio(JNIEnv*);
extern int registerMediaPlayerVideo(JNIEnv*);
#endif
extern int registerDeviceMotionAndOrientationManager(JNIEnv*);
extern int registerCookieManager(JNIEnv*);
#if USE(CHROME_NETWORK_STACK)
extern int registerCacheManager(JNIEnv*);
#endif

}

struct RegistrationMethod {
    const char* name;
    int (*func)(JNIEnv*);
};

static RegistrationMethod gWebCoreRegMethods[] = {
    { "JavaBridge", android::registerJavaBridge },
    { "JniUtil", android::registerJniUtil },
    { "WebFrame", android::registerWebFrame },
    { "WebCoreResourceLoader", android::registerResourceLoader },
    { "WebViewCore", android::registerWebViewCore },
    { "WebHistory", android::registerWebHistory },
    { "WebIconDatabase", android::registerWebIconDatabase },
    { "WebSettings", android::registerWebSettings },
#if ENABLE(DATABASE)
    { "WebStorage", android::registerWebStorage },
#endif
    { "WebView", android::registerWebView },
    { "ViewStateSerializer", android::registerViewStateSerializer },
    { "GeolocationPermissions", android::registerGeolocationPermissions },
    { "MockGeolocation", android::registerMockGeolocation },
#if ENABLE(VIDEO)
    { "HTML5Audio", android::registerMediaPlayerAudio },
    { "HTML5VideoViewProxy", android::registerMediaPlayerVideo },
#endif
    { "DeviceMotionAndOrientationManager", android::registerDeviceMotionAndOrientationManager },
    { "CookieManager", android::registerCookieManager },
#if USE(CHROME_NETWORK_STACK)
    { "CacheManager", android::registerCacheManager },
#endif
};

EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    // Save the JavaVM pointer for use globally.
    JSC::Bindings::setJavaVM(vm);

    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        LOGE("GetEnv failed!");
        return result;
    }
    LOG_ASSERT(env, "Could not retrieve the env!");

    const RegistrationMethod* method = gWebCoreRegMethods;
    const RegistrationMethod* end = method + sizeof(gWebCoreRegMethods)/sizeof(RegistrationMethod);
    while (method != end) {
        if (method->func(env) < 0) {
            LOGE("%s registration failed!", method->name);
            return result;
        }
        method++;
    }

    // Initialize rand() function. The rand() function is used in
    // FileSystemAndroid to create a random temporary filename.
    srand(time(NULL));

    return JNI_VERSION_1_4;
}

class MyJavaSharedClient : public TimerClient, public CookieClient {
public:
    MyJavaSharedClient() : m_hasTimer(false) {}
    virtual void setSharedTimer(long long timemillis) { m_hasTimer = true; }
    virtual void stopSharedTimer() { m_hasTimer = false; }
    virtual void setSharedTimerCallback(void (*f)()) { m_func = f; }
    virtual void signalServiceFuncPtrQueue() {}

    // Cookie methods that do nothing.
    virtual void setCookies(const KURL&, const String&) {}
    virtual String cookies(const KURL&) { return ""; }
    virtual bool cookiesEnabled() { return false; }

    bool m_hasTimer;
    void (*m_func)();
};

static void historyItemChanged(HistoryItem* i) {
    if (i->bridge())
        i->bridge()->updateHistoryItem(i);
}

namespace android {

EXPORT void benchmark(const char* url, int reloadCount, int width, int height) {
    ScriptController::initializeThreading();

    // Setting this allows data: urls to load from a local file.
    SecurityOrigin::setLocalLoadPolicy(SecurityOrigin::AllowLocalLoadsForAll);

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
    DeviceMotionClientAndroid* deviceMotion = new DeviceMotionClientAndroid;
    DeviceOrientationClientAndroid* deviceOrientation = new DeviceOrientationClientAndroid;
    WebCore::Page::PageClients pageClients;
    pageClients.chromeClient = chrome;
    pageClients.contextMenuClient = new ContextMenuClientAndroid;
    pageClients.editorClient = editor;
    pageClients.dragClient = new DragClientAndroid;
    pageClients.inspectorClient = new InspectorClientAndroid;
    pageClients.deviceMotionClient = deviceMotion;
    pageClients.deviceOrientationClient = deviceOrientation;
    WebCore::Page* page = new WebCore::Page(pageClients);
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
    RefPtr<FrameView> frameView = FrameView::create(frame.get());
    WebFrameView* webFrameView = new WebFrameView(frameView.get(), webViewCore);
    frame->setView(frameView);
    frameView->resize(width, height);
    Release(webViewCore);
    Release(webFrameView);

    // Initialize the frame and turn of low-bandwidth display (it fails an
    // assertion in the Cache code)
    frame->init();
    frame->selection()->setFocused(true);
    frame->page()->focusController()->setFocused(true);

    deviceMotion->setWebViewCore(webViewCore);
    deviceOrientation->setWebViewCore(webViewCore);

    // Set all the default settings the Browser normally uses.
    Settings* s = frame->settings();
#ifdef ANDROID_LAYOUT
    s->setLayoutAlgorithm(Settings::kLayoutNormal); // Normal layout for now
#endif
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
#ifdef ANDROID_LAYOUT
    s->setUseWideViewport(false);
#endif

    // Finally, load the actual data
    ResourceRequest req(url);
    frame->loader()->load(req, false);

    do {
        // Layout the page and service the timer
        frame->view()->layout();
        while (client.m_hasTimer) {
            client.m_func();
            JavaSharedClient::ServiceFunctionPtrQueue();
        }
        JavaSharedClient::ServiceFunctionPtrQueue();

        // Layout more if needed.
        while (frame->view()->needsLayout())
            frame->view()->layout();
        JavaSharedClient::ServiceFunctionPtrQueue();

        if (reloadCount)
            frame->loader()->reload(true);
    } while (reloadCount--);

    // Draw into an offscreen bitmap
    SkBitmap bmp;
    bmp.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    bmp.allocPixels();
    SkCanvas canvas(bmp);
    PlatformGraphicsContext ctx(&canvas);
    GraphicsContext gc(&ctx);
    frame->view()->paintContents(&gc, IntRect(0, 0, width, height));

    // Write the bitmap to the sdcard
    SkImageEncoder* enc = SkImageEncoder::Create(SkImageEncoder::kPNG_Type);
    enc->encodeFile("/sdcard/webcore_test.png", bmp, 100);
    delete enc;

    // Tear down the world.
    frame->loader()->detachFromParent();
    delete page;
}

}  // namespace android
