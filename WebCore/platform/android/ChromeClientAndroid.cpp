/* 
**
** Copyright 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#include "config.h"

#include "ChromeClientAndroid.h"
#include "CString.h"
#include "Document.h"
#include "PlatformString.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameAndroid.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "kjs_proxy.h"
#include "Page.h"
#include "Screen.h"
#include "WebCoreViewBridge.h"
#include "WindowFeatures.h"
#include "Settings.h"

#define LOG_TAG "WebCore"
#undef LOG
#include <utils/Log.h>

namespace WebCore {

void ChromeClientAndroid::chromeDestroyed()
{
    delete this;
}

void ChromeClientAndroid::setWindowRect(const FloatRect&) { notImplemented(); }

static WebCoreViewBridge* rootViewForFrame(const Frame* frame)
{
    if (!frame)
        return NULL;
    FrameView* frameView = frame->view();
    return frameView ? frameView->getWebCoreViewBridge() : NULL;
}

FloatRect ChromeClientAndroid::windowRect() { 
    ASSERT(m_frame);
    WebCoreViewBridge* view = rootViewForFrame(m_frame);
    if (!view)
        return FloatRect();   
    const WebCore::IntRect& rect = view->getBounds();
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
    ASSERT(m_frame);
    // Ask the application to focus this WebView.
    m_frame->bridge()->requestFocus();
}
void ChromeClientAndroid::unfocus() { notImplemented(); }

bool ChromeClientAndroid::canTakeFocus(FocusDirection) { notImplemented(); return false; }
void ChromeClientAndroid::takeFocus(FocusDirection) { notImplemented(); }

Page* ChromeClientAndroid::createWindow(Frame* frame, const FrameLoadRequest&,
        const WindowFeatures& features)
{
    ASSERT(frame);
    if (!(frame->settings()->supportMultipleWindows()))
        // If the client doesn't support multiple windows, just return the current page
        return frame->page();

    FrameAndroid* frameAndroid = (FrameAndroid*) frame;
    WebCore::Screen screen(frame);
    bool dialog = features.dialog || !features.resizable
            || (features.heightSet && features.height < screen.height()
                    && features.widthSet && features.width < screen.width())
            || (!features.menuBarVisible && !features.statusBarVisible
                    && !features.toolBarVisible && !features.locationBarVisible
                    && !features.scrollbarsVisible);
    // fullscreen definitely means no dialog
    if (features.fullscreen)
        dialog = false;
    WebCore::Frame* newFrame = frameAndroid->bridge()->createWindow(dialog,
            frame->scriptProxy()->processingUserGesture());
    if (newFrame) {
        WebCore::Page* page = newFrame->page();
        page->setGroupName(frameAndroid->page()->groupName());
        return page;
    }
    return NULL;
}

Page* ChromeClientAndroid::createModalDialog(Frame* , const FrameLoadRequest&) { notImplemented(); return 0; }
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
    return frame->view()->getWebCoreViewBridge()->jsUnload(url, message);
}

void ChromeClientAndroid::closeWindowSoon() 
{
    ASSERT(m_frame);
    // This will prevent javascript cross-scripting during unload
    m_frame->page()->setGroupName(String());
    // Stop loading but do not send the unload event
    m_frame->loader()->stopLoading(false);
    // Cancel all pending loaders
    m_frame->loader()->stopAllLoaders();
    // Remove all event listeners so that no javascript can execute as a result
    // of mouse/keyboard events.
    m_frame->document()->removeAllEventListenersFromAllNodes();
    // Close the window.
    m_frame->bridge()->closeWindow(m_frame->view()->getWebCoreViewBridge());
}

void ChromeClientAndroid::runJavaScriptAlert(Frame* frame, const String& message) 
{
    String url = frame->document()->documentURI();

    frame->view()->getWebCoreViewBridge()->jsAlert(url, message);
}

bool ChromeClientAndroid::runJavaScriptConfirm(Frame* frame, const String& message) 
{ 
    String url = frame->document()->documentURI();

    return frame->view()->getWebCoreViewBridge()->jsConfirm(url, message);
}

/* This function is called for the javascript method Window.prompt(). A dialog should be shown on
 * the screen with an input put box. First param is the text, the second is the default value for
 * the input box, third is return param. If the function returns true, the value set in the third parameter
 * is provided to javascript, else null is returned to the script.
 */
bool ChromeClientAndroid::runJavaScriptPrompt(Frame* frame, const String& message, const String& defaultValue, String& result) 
{ 
    String url = frame->document()->documentURI();
    
    return frame->view()->getWebCoreViewBridge()->jsPrompt(url, message, defaultValue, result);
}
void ChromeClientAndroid::setStatusbarText(const String&) { notImplemented(); }

// This is called by the JavaScript interpreter when a script has been running for a long
// time. A dialog should be shown to the user asking them if they would like to cancel the
// Javascript. If true is returned, the script is cancelled.
// To make a device more responsive, we default to return true to disallow long running script.
// This implies that some of scripts will not be completed.
bool ChromeClientAndroid::shouldInterruptJavaScript() { return true; }

// functions new to Jun-07 tip of tree merge:
void ChromeClientAndroid::addToDirtyRegion(IntRect const&) {}
void ChromeClientAndroid::scrollBackingStore(int, int, IntRect const&, IntRect const&) {}
void ChromeClientAndroid::updateBackingStore() {}
bool ChromeClientAndroid::tabsToLinks() const { return false; }
IntRect ChromeClientAndroid::windowResizerRect() const { return IntRect(0, 0, 0, 0); }

// functions new to the Nov-16-08 tip of tree merge:
void ChromeClientAndroid::mouseDidMoveOverElement(const HitTestResult&, unsigned int) {}
void ChromeClientAndroid::setToolTip(const String&) {}
void ChromeClientAndroid::print(Frame*) {}
bool ChromeClientAndroid::runDatabaseSizeLimitPrompt(Frame*, const String&) { return false; }

// functions new to Feb-19 tip of tree merge:
void ChromeClientAndroid::exceededDatabaseQuota(Frame*, const String&) {}

}
