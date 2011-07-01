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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
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

#include "ApplicationCacheStorage.h"
#include "ChromeClientAndroid.h"
#include "DatabaseTracker.h"
#include "Document.h"
#include "PlatformString.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "Geolocation.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "Icon.h"
#include "LayerAndroid.h"
#include "Page.h"
#include "PopupMenuAndroid.h"
#include "ScriptController.h"
#include "SearchPopupMenuAndroid.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreViewBridge.h"
#include "WebViewCore.h"
#include "WindowFeatures.h"
#include "Settings.h"
#include "UserGestureIndicator.h"
#include <wtf/text/CString.h>

namespace android {

#if ENABLE(DATABASE)
static unsigned long long tryToReclaimDatabaseQuota(SecurityOrigin* originNeedingQuota);
#endif

#if USE(ACCELERATED_COMPOSITING)

WebCore::GraphicsLayer* ChromeClientAndroid::layersSync()
{
    if (m_rootGraphicsLayer && m_needsLayerSync && m_webFrame) {
        if (FrameView* frameView = m_webFrame->page()->mainFrame()->view())
            frameView->syncCompositingStateIncludingSubframes();
    }
    m_needsLayerSync = false;
    return m_rootGraphicsLayer;
}

void ChromeClientAndroid::scheduleCompositingLayerSync()
{
    if (m_needsLayerSync)
        return;
    m_needsLayerSync = true;
    WebViewCore* webViewCore = WebViewCore::getWebViewCore(m_webFrame->page()->mainFrame()->view());
    if (webViewCore)
        webViewCore->layersDraw();
}

void ChromeClientAndroid::setNeedsOneShotDrawingSynchronization()
{
    // This should not be needed
}

void ChromeClientAndroid::attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer* layer)
{
    // frame is not used in Android as we should only get root graphics layer for the main frame
    m_rootGraphicsLayer = layer;
    if (!layer)
        return;
    scheduleCompositingLayerSync();
}

#endif

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
    ASSERT(m_webFrame);
    return m_webFrame->density();
}

void ChromeClientAndroid::focus()
{
    ASSERT(m_webFrame);
    bool isUserGesture = UserGestureIndicator::processingUserGesture();

    // Ask the application to focus this WebView if the action is intiated by the user
    if (isUserGesture)
        m_webFrame->requestFocus();
}
void ChromeClientAndroid::unfocus() { notImplemented(); }

bool ChromeClientAndroid::canTakeFocus(FocusDirection) { notImplemented(); return false; }
void ChromeClientAndroid::takeFocus(FocusDirection) { notImplemented(); }

void ChromeClientAndroid::focusedNodeChanged(Node* node)
{
    android::WebViewCore::getWebViewCore(m_webFrame->page()->mainFrame()->view())->focusNodeChanged(node);
}

void ChromeClientAndroid::focusedFrameChanged(Frame*) { notImplemented(); }

Page* ChromeClientAndroid::createWindow(Frame* frame, const FrameLoadRequest&,
        const WindowFeatures& features, const NavigationAction&)
{
    ASSERT(frame);
#ifdef ANDROID_MULTIPLE_WINDOWS
    if (frame->settings() && !(frame->settings()->supportMultipleWindows()))
        // If the client doesn't support multiple windows, just return the current page
        return frame->page();
#endif

    const WebCoreViewBridge* bridge = frame->view()->platformWidget();
    bool dialog = features.dialog || !features.resizable
            || (features.heightSet && features.height < bridge->height()
                    && features.widthSet && features.width < bridge->width())
            || (!features.menuBarVisible && !features.statusBarVisible
                    && !features.toolBarVisible && !features.locationBarVisible
                    && !features.scrollbarsVisible);
    // fullscreen definitely means no dialog
    if (features.fullscreen)
        dialog = false;
    WebCore::Frame* newFrame = m_webFrame->createWindow(dialog,
            ScriptController::processingUserGesture());
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

#if ENABLE(CONTEXT_MENUS)
void ChromeClientAndroid::showContextMenu() { notImplemented(); }
#endif

// This function is called by the JavaScript bindings to print usually an error to
// a message console. Pass the message to the java side so that the client can
// handle it as it sees fit.
void ChromeClientAndroid::addMessageToConsole(MessageSource, MessageType, MessageLevel msgLevel, const String& message, unsigned int lineNumber, const String& sourceID) {
    android::WebViewCore::getWebViewCore(m_webFrame->page()->mainFrame()->view())->addMessageToConsole(message, lineNumber, sourceID, msgLevel);
}

void ChromeClientAndroid::formDidBlur(const WebCore::Node* node)
{
    android::WebViewCore::getWebViewCore(m_webFrame->page()->mainFrame()->view())->formDidBlur(node);
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
    mainFrame->loader()->stopLoading(UnloadEventPolicyNone);
    // Cancel all pending loaders
    mainFrame->loader()->stopAllLoaders();
    // Remove all event listeners so that no javascript can execute as a result
    // of mouse/keyboard events.
    mainFrame->document()->removeAllEventListeners();
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
bool ChromeClientAndroid::shouldInterruptJavaScript() {
  FrameView* frameView = m_webFrame->page()->mainFrame()->view();
  return android::WebViewCore::getWebViewCore(frameView)->jsInterrupt();
}

KeyboardUIMode ChromeClientAndroid::keyboardUIMode()
{
    return KeyboardAccessDefault;
}

IntRect ChromeClientAndroid::windowResizerRect() const { return IntRect(0, 0, 0, 0); }

void ChromeClientAndroid::invalidateWindow(const IntRect&, bool)
{
    notImplemented();
}

void ChromeClientAndroid::invalidateContentsAndWindow(const IntRect& updateRect, bool /*immediate*/)
{
    notImplemented();
}

void ChromeClientAndroid::invalidateContentsForSlowScroll(const IntRect& updateRect, bool immediate)
{
    notImplemented();
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

PlatformPageClient ChromeClientAndroid::platformPageClient() const {
    Page* page = m_webFrame->page();
    Frame* mainFrame = page->mainFrame();
    FrameView* view = mainFrame->view();
    PlatformWidget viewBridge = view->platformWidget();
    return viewBridge;
}

void ChromeClientAndroid::contentsSizeChanged(Frame*, const IntSize&) const
{
    notImplemented();
}

void ChromeClientAndroid::scrollRectIntoView(const IntRect&, const ScrollView*) const
{
    notImplemented();
}

void ChromeClientAndroid::formStateDidChange(const Node*)
{
    notImplemented();
}

void ChromeClientAndroid::scrollbarsModeDidChange() const
{
    notImplemented();
}

void ChromeClientAndroid::dispatchViewportDataDidChange(const ViewportArguments& input) const {
#ifdef ANDROID_META_SUPPORT
    const ViewportArguments emptyArgument;
    if (input == emptyArgument) {
        // Empty Argument is for a page with no viewport meta tag; so reset everything.
        m_webFrame->page()->settings()->resetMetadataSettings();
    }
    Document* doc = m_webFrame->page()->mainFrame()->document();
    if (!doc->ownerElement()) {
        FrameView* view = doc->view();
        if (view)
            PlatformBridge::updateViewport(view);
    }
#endif
}

void ChromeClientAndroid::mouseDidMoveOverElement(const HitTestResult&, unsigned int) {}
void ChromeClientAndroid::setToolTip(const String&, TextDirection) {}
void ChromeClientAndroid::print(Frame*) {}

/*
 * This function is called on the main (webcore) thread by SQLTransaction::deliverQuotaIncreaseCallback.
 * The way that the callback mechanism is designed inside SQLTransaction means that there must be a new quota
 * (which may be equal to the old quota if the user did not allow more quota) when this function returns. As
 * we call into the browser thread to ask what to do with the quota, we block here and get woken up when the
 * browser calls the native WebViewCore::SetDatabaseQuota method with the new quota value.
 */
#if ENABLE(DATABASE)
void ChromeClientAndroid::exceededDatabaseQuota(Frame* frame, const String& name)
{
    SecurityOrigin* origin = frame->document()->securityOrigin();
    DatabaseTracker& tracker = WebCore::DatabaseTracker::tracker();

    m_isNewQuotaSet = false;

    // This origin is being tracked and has exceeded it's quota. Call into
    // the Java side of things to inform the user.
    unsigned long long currentQuota = 0;
    if (tracker.hasEntryForOrigin(origin))
        currentQuota = tracker.quotaForOrigin(origin);

    unsigned long long estimatedSize = 0;

    // Only update estimatedSize if we are trying to create a a new database, i.e. the usage for the database is 0.
    if (tracker.usageForDatabase(name, origin) == 0)
        estimatedSize = tracker.detailsForNameAndOrigin(name, origin).expectedUsage();

    if (android::WebViewCore::getWebViewCore(frame->view())->exceededDatabaseQuota(frame->document()->documentURI(), name, currentQuota, estimatedSize)) {
        // We've sent notification to the browser so now wait for it to come back.
        m_quotaThreadLock.lock();
        while (!m_isNewQuotaSet) {
            m_quotaThreadCondition.wait(m_quotaThreadLock);
        }
        m_quotaThreadLock.unlock();
    } else {
        // We failed to send the message to the UI thread to request a new quota,
        // so just use the current quota as a default.
        m_newQuota = currentQuota;
    }

    if (m_newQuota < currentQuota)
        m_newQuota = currentQuota;

    // If new quota is unavailable, we may be able to resolve the situation by
    // shrinking the quota of an origin that asked for a lot but is only using a
    // little. If we find such a site, shrink it's quota and ask Java to try
    // again.
    if (m_newQuota == currentQuota && !m_triedToReclaimDBQuota) {
        m_triedToReclaimDBQuota = true; // we should only try this once per quota overflow.
        unsigned long long reclaimedQuotaBytes = tryToReclaimDatabaseQuota(origin);

        // If we were able to free up enough space, try asking Java again.
        // Otherwise, give up and deny the new database. :(
        if (reclaimedQuotaBytes >= estimatedSize) {
            exceededDatabaseQuota(frame, name);
            return;
        }
    }

    // Update the DatabaseTracker with the new quota value (if the user declined
    // new quota, this may equal the old quota)
    tracker.setQuota(origin, m_newQuota);
    m_triedToReclaimDBQuota = false;
}

static unsigned long long tryToReclaimDatabaseQuota(SecurityOrigin* originNeedingQuota) {
    DatabaseTracker& tracker = WebCore::DatabaseTracker::tracker();
    Vector<RefPtr<SecurityOrigin> > origins;
    tracker.origins(origins);
    unsigned long long reclaimedQuotaBytes = 0;
    for (unsigned i = 0; i < origins.size(); i++) {
        SecurityOrigin* originToReclaimFrom = origins[i].get();

        // Don't try to reclaim from the origin that has exceeded its quota.
        if (originToReclaimFrom->equal(originNeedingQuota))
            continue;

        unsigned long long originUsage = tracker.usageForOrigin(originToReclaimFrom);
        unsigned long long originQuota = tracker.quotaForOrigin(originToReclaimFrom);
        // If the origin has a quota that is more than it's current usage +1MB, shrink it.
        static const int ONE_MB = 1 * 1024 * 1024;
        if (originUsage + ONE_MB < originQuota) {
            unsigned long long newQuota = originUsage + ONE_MB;
            tracker.setQuota(originToReclaimFrom, newQuota);
            reclaimedQuotaBytes += originQuota - newQuota;
        }
    }
    return reclaimedQuotaBytes;
}
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
void ChromeClientAndroid::reachedMaxAppCacheSize(int64_t spaceNeeded)
{
    m_isNewQuotaSet = false;
    Page* page = m_webFrame->page();
    Frame* mainFrame = page->mainFrame();
    FrameView* view = mainFrame->view();

    // If we fail to send the message to the UI thread to request a new quota,
    // there's nothing to do.
    if (!android::WebViewCore::getWebViewCore(view)->reachedMaxAppCacheSize(spaceNeeded))
        return;

    // We've sent notification to the browser so now wait for it to come back.
    m_quotaThreadLock.lock();
    while (!m_isNewQuotaSet) {
       m_quotaThreadCondition.wait(m_quotaThreadLock);
    }
    m_quotaThreadLock.unlock();
    if (m_newQuota > 0) {
        WebCore::cacheStorage().setMaximumSize(m_newQuota);
        // Now the app cache will retry the saving the previously failed cache.
    }
}
#endif

void ChromeClientAndroid::populateVisitedLinks()
{
    Page* page = m_webFrame->page();
    Frame* mainFrame = page->mainFrame();
    FrameView* view = mainFrame->view();
    android::WebViewCore::getWebViewCore(view)->populateVisitedLinks(&page->group());
}

void ChromeClientAndroid::requestGeolocationPermissionForFrame(Frame* frame, Geolocation* geolocation)
{
    ASSERT(geolocation);
    if (!m_geolocationPermissions) {
        m_geolocationPermissions = new GeolocationPermissions(android::WebViewCore::getWebViewCore(frame->view()),
                                                              m_webFrame->page()->mainFrame());
    }
    m_geolocationPermissions->queryPermissionState(frame);
}

void ChromeClientAndroid::cancelGeolocationPermissionRequestForFrame(Frame* frame, WebCore::Geolocation*)
{
    if (m_geolocationPermissions)
        m_geolocationPermissions->cancelPermissionStateQuery(frame);
}

void ChromeClientAndroid::provideGeolocationPermissions(const String &origin, bool allow, bool remember)
{
    ASSERT(m_geolocationPermissions);
    m_geolocationPermissions->providePermissionState(origin, allow, remember);
}

void ChromeClientAndroid::storeGeolocationPermissions()
{
    GeolocationPermissions::maybeStorePermanentPermissions();
}

void ChromeClientAndroid::onMainFrameLoadStarted()
{
    if (m_geolocationPermissions.get())
        m_geolocationPermissions->resetTemporaryPermissionStates();
}

void ChromeClientAndroid::runOpenPanel(Frame* frame,
        PassRefPtr<FileChooser> chooser)
{
    android::WebViewCore* core = android::WebViewCore::getWebViewCore(
            frame->view());
    core->openFileChooser(chooser);
}

void ChromeClientAndroid::chooseIconForFiles(const Vector<WTF::String>&, FileChooser*)
{
    notImplemented();
}

void ChromeClientAndroid::setCursor(const Cursor&)
{
    notImplemented(); 
}

void ChromeClientAndroid::wakeUpMainThreadWithNewQuota(long long newQuota) {
    MutexLocker locker(m_quotaThreadLock);
    m_newQuota = newQuota < 0 ? 0 : newQuota;
    m_isNewQuotaSet = true;
    m_quotaThreadCondition.signal();
}

#if ENABLE(TOUCH_EVENTS)
void ChromeClientAndroid::needTouchEvents(bool needTouchEvents)
{
    FrameView* frameView = m_webFrame->page()->mainFrame()->view();
    android::WebViewCore* core = android::WebViewCore::getWebViewCore(frameView);
    if (core)
        core->needTouchEvents(needTouchEvents);
}
#endif

bool ChromeClientAndroid::selectItemWritingDirectionIsNatural()
{
    return false;
}

bool ChromeClientAndroid::selectItemAlignmentFollowsMenuWritingDirection()
{
    return false;
}

PassRefPtr<PopupMenu> ChromeClientAndroid::createPopupMenu(PopupMenuClient* client) const
{
    return adoptRef(new PopupMenuAndroid(static_cast<ListPopupMenuClient*>(client)));
}

PassRefPtr<SearchPopupMenu> ChromeClientAndroid::createSearchPopupMenu(PopupMenuClient*) const
{
    return adoptRef(new SearchPopupMenuAndroid);
}

void ChromeClientAndroid::reachedApplicationCacheOriginQuota(SecurityOrigin*)
{
    notImplemented();
}

#if ENABLE(ANDROID_INSTALLABLE_WEB_APPS)
void ChromeClientAndroid::webAppCanBeInstalled()
{
    FrameView* frameView = m_webFrame->page()->mainFrame()->view();
    android::WebViewCore* core = android::WebViewCore::getWebViewCore(frameView);
    if (core)
        core->notifyWebAppCanBeInstalled();
}
#endif

#if ENABLE(VIDEO)
bool ChromeClientAndroid::supportsFullscreenForNode(const Node* node)
{
      return node->hasTagName(HTMLNames::videoTag);
}

void ChromeClientAndroid::enterFullscreenForNode(Node* node)
{
      if (!node->hasTagName(HTMLNames::videoTag))
          return;

      HTMLMediaElement* videoElement = static_cast<HTMLMediaElement*>(node);
      String url = videoElement->currentSrc();
      LayerAndroid* layer = videoElement->platformLayer();
      if (!layer)
          return;

      FrameView* frameView = m_webFrame->page()->mainFrame()->view();
      android::WebViewCore* core = android::WebViewCore::getWebViewCore(frameView);
      m_webFrame->page()->mainFrame()->document()->webkitWillEnterFullScreenForElement(videoElement);
      if (core)
          core->enterFullscreenForVideoLayer(layer->uniqueId(), url);
}

void ChromeClientAndroid::exitFullscreenForNode(Node* node)
{
}
#endif

#if ENABLE(FULLSCREEN_API)
void ChromeClientAndroid::exitFullScreenForElement(Element* element)
{
    if (!element)
        return;

    HTMLMediaElement* videoElement = static_cast<HTMLMediaElement*>(element);
    videoElement->exitFullscreen();
}
#endif

}
