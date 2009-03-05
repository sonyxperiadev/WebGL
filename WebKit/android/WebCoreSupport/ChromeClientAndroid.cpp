/*
 * Copyright 2007, The Android Open Source Project
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

#define LOG_TAG "WebCore"

#include "config.h"

#include "ChromeClientAndroid.h"
#include "CString.h"
#include "Document.h"
#include "PlatformString.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "Page.h"
#include "Screen.h"
#include "ScriptController.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreViewBridge.h"
#include "WebViewCore.h"
#include "WindowFeatures.h"
#include "Settings.h"

namespace android {

void ChromeClientAndroid::setWebFrame(android::WebFrame* webframe)
{
    Release(m_webFrame);
    m_webFrame = webframe;
    Retain(m_webFrame);
}

void ChromeClientAndroid::chromeDestroyed()
{
    Release(m_webFrame);
    delete this;
}

void ChromeClientAndroid::setWindowRect(const FloatRect&) { notImplemented(); }

FloatRect ChromeClientAndroid::windowRect() { 
    ASSERT(m_webFrame);
    if (!m_webFrame)
        return FloatRect();
    FrameView* frameView = m_webFrame->page()->mainFrame()->view();
    if (!frameView)
        return FloatRect();
    const WebCoreViewBridge* bridge = frameView->platformWidget();
    const IntRect& rect = bridge->getWindowBounds();
    FloatRect fRect(rect.x(), rect.y(), rect.width(), rect.height());
    return fRect;
}

FloatRect ChromeClientAndroid::pageRect() { notImplemented(); return FloatRect(); }

float ChromeClientAndroid::scaleFactor()
{
    // only seems to be used for dashboard regions, so just return 1
    return 1;
}

void ChromeClientAndroid::focus() {
    ASSERT(m_webFrame);
    // Ask the application to focus this WebView.
    m_webFrame->requestFocus();
}
void ChromeClientAndroid::unfocus() { notImplemented(); }

bool ChromeClientAndroid::canTakeFocus(FocusDirection) { notImplemented(); return false; }
void ChromeClientAndroid::takeFocus(FocusDirection) { notImplemented(); }

Page* ChromeClientAndroid::createWindow(Frame* frame, const FrameLoadRequest&,
        const WindowFeatures& features)
{
    ASSERT(frame);
#ifdef ANDROID_MULTIPLE_WINDOWS
    if (frame->settings() && !(frame->settings()->supportMultipleWindows()))
        // If the client doesn't support multiple windows, just return the current page
        return frame->page();
#endif

    WTF::PassRefPtr<WebCore::Screen> screen = WebCore::Screen::create(frame);
    bool dialog = features.dialog || !features.resizable
            || (features.heightSet && features.height < screen.get()->height()
                    && features.widthSet && features.width < screen.get()->width())
            || (!features.menuBarVisible && !features.statusBarVisible
                    && !features.toolBarVisible && !features.locationBarVisible
                    && !features.scrollbarsVisible);
    // fullscreen definitely means no dialog
    if (features.fullscreen)
        dialog = false;
    WebCore::Frame* newFrame = m_webFrame->createWindow(dialog,
            frame->script()->processingUserGesture());
    if (newFrame) {
        WebCore::Page* page = newFrame->page();
        page->setGroupName(frame->page()->groupName());
        return page;
    }
    return NULL;
}

void ChromeClientAndroid::show() { notImplemented(); }

bool ChromeClientAndroid::canRunModal() { notImplemented(); return false; }
void ChromeClientAndroid::runModal() { notImplemented(); }

void ChromeClientAndroid::setToolbarsVisible(bool) { notImplemented(); }
bool ChromeClientAndroid::toolbarsVisible() { notImplemented(); return false; }

void ChromeClientAndroid::setStatusbarVisible(bool) { notImplemented(); }
bool ChromeClientAndroid::statusbarVisible() { notImplemented(); return false; }

void ChromeClientAndroid::setScrollbarsVisible(bool) { notImplemented(); }
bool ChromeClientAndroid::scrollbarsVisible() { notImplemented(); return false; }

void ChromeClientAndroid::setMenubarVisible(bool) { notImplemented(); }
bool ChromeClientAndroid::menubarVisible() { notImplemented(); return false; }

void ChromeClientAndroid::setResizable(bool) { notImplemented(); }

// This function is called by the JavaScript bindings to print usually an error to
// a message console.
void ChromeClientAndroid::addMessageToConsole(const String& message, unsigned int lineNumber, const String& sourceID) {
    notImplemented();
    LOGD("Console: %s line: %d source: %s\n", message.latin1().data(), lineNumber, sourceID.latin1().data());
}

bool ChromeClientAndroid::canRunBeforeUnloadConfirmPanel() { return true; }
bool ChromeClientAndroid::runBeforeUnloadConfirmPanel(const String& message, Frame* frame) {
    String url = frame->document()->documentURI();
    return android::WebViewCore::getWebViewCore(frame->view())->jsUnload(url, message);
}

void ChromeClientAndroid::closeWindowSoon() 
{
    ASSERT(m_webFrame);
    Page* page = m_webFrame->page();
    Frame* mainFrame = page->mainFrame();
    // This will prevent javascript cross-scripting during unload
    page->setGroupName(String());
    // Stop loading but do not send the unload event
    mainFrame->loader()->stopLoading(false);
    // Cancel all pending loaders
    mainFrame->loader()->stopAllLoaders();
    // Remove all event listeners so that no javascript can execute as a result
    // of mouse/keyboard events.
    mainFrame->document()->removeAllEventListenersFromAllNodes();
    // Close the window.
    m_webFrame->closeWindow(android::WebViewCore::getWebViewCore(mainFrame->view()));
}

void ChromeClientAndroid::runJavaScriptAlert(Frame* frame, const String& message) 
{
    String url = frame->document()->documentURI();

    android::WebViewCore::getWebViewCore(frame->view())->jsAlert(url, message);
}

bool ChromeClientAndroid::runJavaScriptConfirm(Frame* frame, const String& message) 
{ 
    String url = frame->document()->documentURI();

    return android::WebViewCore::getWebViewCore(frame->view())->jsConfirm(url, message);
}

/* This function is called for the javascript method Window.prompt(). A dialog should be shown on
 * the screen with an input put box. First param is the text, the second is the default value for
 * the input box, third is return param. If the function returns true, the value set in the third parameter
 * is provided to javascript, else null is returned to the script.
 */
bool ChromeClientAndroid::runJavaScriptPrompt(Frame* frame, const String& message, const String& defaultValue, String& result) 
{ 
    String url = frame->document()->documentURI();
    return android::WebViewCore::getWebViewCore(frame->view())->jsPrompt(url, message, defaultValue, result);
}
void ChromeClientAndroid::setStatusbarText(const String&) { notImplemented(); }

// This is called by the JavaScript interpreter when a script has been running for a long
// time. A dialog should be shown to the user asking them if they would like to cancel the
// Javascript. If true is returned, the script is cancelled.
// To make a device more responsive, we default to return true to disallow long running script.
// This implies that some of scripts will not be completed.
bool ChromeClientAndroid::shouldInterruptJavaScript() { return true; }

bool ChromeClientAndroid::tabsToLinks() const { return false; }

IntRect ChromeClientAndroid::windowResizerRect() const { return IntRect(0, 0, 0, 0); }

// new to change 38068 (Nov 6, 2008)
void ChromeClientAndroid::repaint(const IntRect& rect, bool contentChanged, 
        bool immediate, bool repaintContentOnly) { 
    notImplemented(); 
// was in ScrollViewAndroid::update() : needs to be something like:
//    android::WebViewCore::getWebViewCore(this)->contentInvalidate(rect);
}

// new to change 38068 (Nov 6, 2008)
void ChromeClientAndroid::scroll(const IntSize& scrollDelta, 
        const IntRect& rectToScroll, const IntRect& clipRect) { 
    notImplemented(); 
}

// new to change 38068 (Nov 6, 2008)
IntPoint ChromeClientAndroid::screenToWindow(const IntPoint&) const { 
    notImplemented(); 
    return IntPoint();
}

// new to change 38068 (Nov 6, 2008)
IntRect ChromeClientAndroid::windowToScreen(const IntRect&) const { 
    notImplemented(); 
    return IntRect();
}

// new to change 38068 (Nov 6, 2008)
// in place of view()->containingWindow(), webkit now uses view()->hostWindow()->platformWindow()
PlatformWidget ChromeClientAndroid::platformWindow() const {
    Page* page = m_webFrame->page();
    Frame* mainFrame = page->mainFrame();
    FrameView* view = mainFrame->view();
    PlatformWidget viewBridge = view->platformWidget();
    return viewBridge;
}

// new to webkit4 (Feb 27, 2009)
void ChromeClientAndroid::contentsSizeChanged(Frame*, const IntSize&) const
{
    notImplemented();
}

// new to webkit4 (Feb 27, 2009)
void ChromeClientAndroid::formStateDidChange(const Node*)
{
    notImplemented();
}

void ChromeClientAndroid::mouseDidMoveOverElement(const HitTestResult&, unsigned int) {}
void ChromeClientAndroid::setToolTip(const String&) {}
void ChromeClientAndroid::print(Frame*) {}

void ChromeClientAndroid::exceededDatabaseQuota(Frame*, const String&) {}

// new to change 38068 (Nov 6, 2008)
void ChromeClientAndroid::runOpenPanel(Frame*, PassRefPtr<FileChooser>) { notImplemented(); }

}
