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

#include "android_graphics.h"
#include "CString.h"
#include "DocumentLoader.h"
#include "FrameAndroid.h"
#include "FrameLoader.h"
#include "FrameLoaderClientAndroid.h"
#include "FrameTree.h"
#include "GraphicsContext.h"
// HTMLFormElement needed for a bad include
#include "HTMLFormElement.h"
#include "HTMLFrameOwnerElement.h"
#include "IconDatabase.h"
#include "Page.h"
#include "PlatformGraphicsContext.h"
#include "PlatformString.h"
#include "PluginInfoStore.h"
#include "PluginDatabaseAndroid.h"
#include "PluginViewAndroid.h"
#include "ProgressTracker.h"
#include "RenderPart.h"
#include "ResourceError.h"
#include "SelectionController.h"
#include "SkCanvas.h"
#include "SkRect.h"
#include "Document.h"
#include "FrameView.h"
#include "WebCoreViewBridge.h"
#include "HistoryItem.h"
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"
#include "WebCoreResourceLoader.h"
#include "WebCoreViewBridge.h"
#include "WebHistory.h"
#include "WebIconDatabase.h"
#include "Settings.h"

#define LOG_TAG "WebCore"
#undef LOG
#include <utils/Log.h>
#include <utils/AssetManager.h>

//#define notImplemented() LOGV("%s\n", __PRETTY_FUNCTION__)
#define lowPriority_notImplemented() //printf("%s\n", __PRETTY_FUNCTION__)
#define verifiedOk()    // not a problem that it's not implemented
static inline void needsBridge(const char name[]) {LOGV("<%s> needs bridge", name);}

extern android::AssetManager* gGlobalAssetMgr;

namespace WebCore {

static const int EXTRA_LAYOUT_DELAY = 1000;

// FIXME: Need some data for how big this should be.
#define MAX_SESSION_HISTORY 50
static Vector<KURL> gSessionHistory;

bool historyContains(const UChar* chars, unsigned len) {
    const KURL url(DeprecatedString(reinterpret_cast<const DeprecatedChar*>(chars), len));
    Vector<KURL>::const_iterator end = gSessionHistory.end();
    for (Vector<KURL>::const_iterator i = gSessionHistory.begin(); i != end; ++i) {
        if (equalIgnoringRef(url, *i))
            return true;
    }
    return false;
}

void FrameLoaderClientAndroid::frameLoaderDestroyed() {
    registerForIconNotification(false);
    m_frame = 0;
    delete this;
}

bool FrameLoaderClientAndroid::hasWebView() const {
    // FIXME,
    // there is one web view per page, or top frame.
    // as android's view is created from Java side, it is always there.
    return true;
}

bool FrameLoaderClientAndroid::hasFrameView() const {
    // FIXME,
    // need to revisit for sub-frame case
    return true;
}

bool FrameLoaderClientAndroid::privateBrowsingEnabled() const {
    // FIXME, are we going to support private browsing?
    notImplemented();
    return false;
}

void FrameLoaderClientAndroid::makeRepresentation(DocumentLoader*) {
    // don't use representation
    verifiedOk();
}

void FrameLoaderClientAndroid::forceLayout() {
    ASSERT(m_frame);
    m_frame->forceLayout();
    // FIXME, should we adjust view size here?
    m_frame->view()->adjustViewSize();
}

void FrameLoaderClientAndroid::forceLayoutForNonHTML() {
    notImplemented();
}

void FrameLoaderClientAndroid::setCopiesOnScroll() {
    // this is a hint about whether we need to force redraws, or can
    // just copy the scrolled content. Since we always force a redraw
    // anyways, we can ignore this call.
    verifiedOk();
}

void FrameLoaderClientAndroid::detachedFromParent2() {
    // FIXME, ready to detach frame from view
}

void FrameLoaderClientAndroid::detachedFromParent3() {
    // FIXME, ready to release view
    notImplemented();
}

void FrameLoaderClientAndroid::detachedFromParent4() {
    // FIXME, ready to release view
    notImplemented();
}

void FrameLoaderClientAndroid::loadedFromPageCache() {
    // don't support page cache
    verifiedOk();
}

// This function is responsible for associating the "id" with a given
// subresource load.  The following functions that accept an "id" are
// called for each subresource, so they should not be dispatched to the m_frame.
void FrameLoaderClientAndroid::assignIdentifierToInitialRequest(unsigned long id,
                            DocumentLoader*, const ResourceRequest&) {
    lowPriority_notImplemented();
}

void FrameLoaderClientAndroid::dispatchWillSendRequest(DocumentLoader*, unsigned long id,
                            ResourceRequest&, const ResourceResponse&) {
    lowPriority_notImplemented();
}

void FrameLoaderClientAndroid::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*,
                            unsigned long id, const AuthenticationChallenge&) {
    lowPriority_notImplemented();
}

void FrameLoaderClientAndroid::dispatchDidCancelAuthenticationChallenge(DocumentLoader*,
                            unsigned long id, const AuthenticationChallenge&) {
    lowPriority_notImplemented();
}

void FrameLoaderClientAndroid::dispatchDidReceiveResponse(DocumentLoader*,
                            unsigned long id, const ResourceResponse&) {
    lowPriority_notImplemented();
}

void FrameLoaderClientAndroid::dispatchDidReceiveContentLength(DocumentLoader*,
                            unsigned long id, int lengthReceived) {
    lowPriority_notImplemented();
}

void FrameLoaderClientAndroid::dispatchDidFinishLoading(DocumentLoader*,
                            unsigned long id) {
    lowPriority_notImplemented();
}

void FrameLoaderClientAndroid::dispatchDidFailLoading(DocumentLoader* docLoader,
                            unsigned long id, const ResourceError&) {
    lowPriority_notImplemented();
}

bool FrameLoaderClientAndroid::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*,
                            const ResourceRequest&, const ResourceResponse&, int length) {
    notImplemented();
    return false;
}

void FrameLoaderClientAndroid::dispatchDidHandleOnloadEvents() {
}

void FrameLoaderClientAndroid::dispatchDidReceiveServerRedirectForProvisionalLoad() {
    ASSERT(m_frame);
    // Tell the load it was a redirect.
    m_frame->bridge()->loadStarted(m_frame);
}

void FrameLoaderClientAndroid::dispatchDidCancelClientRedirect() {
    notImplemented();
}

void FrameLoaderClientAndroid::dispatchWillPerformClientRedirect(const KURL&,
                                double interval, double fireDate) {
    notImplemented();
}

void FrameLoaderClientAndroid::dispatchDidChangeLocationWithinPage() {
    notImplemented();
}

void FrameLoaderClientAndroid::dispatchWillClose() {
    notImplemented();
}

void FrameLoaderClientAndroid::dispatchDidReceiveIcon() {
    ASSERT(m_frame);
    if (m_frame->tree() && m_frame->tree()->parent())
        return;
    WebCore::String url(m_frame->loader()->url().string());
    WebCore::Image* icon = WebCore::iconDatabase()->iconForPageURL(url, WebCore::IntSize(16,16));
    // There is a bug in webkit where cancelling an icon load is treated as a
    // failure. When this is fixed, we can ASSERT again that we have an icon.
    if (icon) {
        m_frame->bridge()->didReceiveIcon(icon);
        LOGV("Received icon (%p) for %s", icon,
                m_frame->loader()->url().deprecatedString().ascii());
    }
}

void FrameLoaderClientAndroid::dispatchDidStartProvisionalLoad() {
    notImplemented();
}

void FrameLoaderClientAndroid::dispatchDidReceiveTitle(const String& title) {
    ASSERT(m_frame);
    // Used to check for FrameLoadTypeStandard but we only want to send the title for
    // the top frame and not sub-frames.
    if (!m_frame->tree() || !m_frame->tree()->parent()) {
        m_frame->bridge()->setTitle(title);
    }
}

void FrameLoaderClientAndroid::dispatchDidCommitLoad() {
    ASSERT(m_frame);
    m_frame->view()->getWebCoreViewBridge()->updateFrameGeneration(m_frame);
}

void FrameLoaderClientAndroid::dispatchDidFailProvisionalLoad(const ResourceError& error) {
    ASSERT(m_frame);
    // Ignore ErrorInterrupted since it is due to a policy interruption. This
    // is caused by a decision to download the main resource rather than
    // display it.
    if (error.errorCode() == InternalErrorInterrupted
            || error.errorCode() == InternalErrorCancelled)
        return;

    if (!gGlobalAssetMgr) {
        gGlobalAssetMgr = new android::AssetManager();
        gGlobalAssetMgr->addDefaultAssets();
    }
    
    // Check to see if the error code was not generated internally
    const char* errorPage = "webkit/nodomain.html";
    if ((error.errorCode() == ErrorFile ||
            error.errorCode() == ErrorFileNotFound) &&
            (!error.localizedDescription().isEmpty())) {
        errorPage = "webkit/loaderror.html";
    }
    
    // Grab the error page from the asset manager
    android::Asset* a = gGlobalAssetMgr->open(errorPage,
            android::Asset::ACCESS_BUFFER);
    if (!a)
        return;

    // Take the failing url and encode html entities so javascript urls are not
    // executed.
    CString failingUrl = error.failingURL().utf8();
    Vector<char> url;
    int len = failingUrl.length();
    const char* data = failingUrl.data();
    for (int i = 0; i < len; i++) {
        char c = data[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9'))
            url.append(c);
        else {
            char buf[16];
            int res = sprintf(buf, "&#%d;", c);
            buf[res] = 0;
            url.append(buf, res);
        }
    }

    // Replace all occurances of %s with the failing url.
    String s = String((const char*)a->getBuffer(false), a->getLength());
    s = s.replace("%s", String(url.data(), url.size()));
    
    // Replace all occurances of %e with the error text
    s = s.replace("%e", error.localizedDescription());
    
    // Create the request and the substitute data and tell the FrameLoader to
    // load with the replacement data.
    ResourceRequest request(error.failingURL());
    CString cstr = s.utf8();
    RefPtr<SharedBuffer> buf = new SharedBuffer(cstr.data(), cstr.length());
    SubstituteData subData(buf, String("text/html"), String("utf-8"),
            request.url());
    m_frame->loader()->load(request, subData);

    // Delete the asset.
    delete a;
}

void FrameLoaderClientAndroid::dispatchDidFailLoad(const ResourceError&) {
    // called when page is completed with error
    lowPriority_notImplemented();
    ASSERT(m_frame);
    m_frame->document()->setExtraLayoutDelay(0);
}

void FrameLoaderClientAndroid::dispatchDidFinishDocumentLoad() {
    // called when finishedParsing
    lowPriority_notImplemented();
}

void FrameLoaderClientAndroid::dispatchDidFinishLoad() {
    ASSERT(m_frame);
    m_frame->document()->setExtraLayoutDelay(0);
    m_frame->bridge()->didFinishLoad(m_frame);
}

void FrameLoaderClientAndroid::dispatchDidFirstLayout() {
    ASSERT(m_frame);
    m_frame->document()->setExtraLayoutDelay(EXTRA_LAYOUT_DELAY);
    // FIXME: Need to figure out if we need didLayout or didFirstLayout
    // see WebViewCore::didLayout
    m_frame->view()->getWebCoreViewBridge()->didFirstLayout();
}

Frame* FrameLoaderClientAndroid::dispatchCreatePage() {
    ASSERT(m_frame);
    if (m_frame->settings()->supportMultipleWindows())
        // Always a user gesture since window.open maps to
        // ChromeClientAndroid::createWindow
        return m_frame->bridge()->createWindow(false, true);
    else
        // If the client doesn't support multiple windows, just replace the
        // current frame's contents.
        return m_frame;
}

void FrameLoaderClientAndroid::dispatchShow() {
    ASSERT(m_frame);
    m_frame->view()->invalidate();
}


static bool TreatAsAttachment(const String& content_disposition) {
    // Some broken sites just send
    // Content-Disposition: ; filename="file"
    // screen those out here.
    if (content_disposition.startsWith(";"))
        return false;

    if (content_disposition.startsWith("inline", false))
        return false;

    // Some broken sites just send
    // Content-Disposition: filename="file"
    // without a disposition token... screen those out.
    if (content_disposition.startsWith("filename", false))
        return false;

    // Also in use is Content-Disposition: name="file"
    if (content_disposition.startsWith("name", false))
        return false;

    // We have a content-disposition of "attachment" or unknown.
    // RFC 2183, section 2.8 says that an unknown disposition
    // value should be treated as "attachment"
    return true;
}

void FrameLoaderClientAndroid::dispatchDecidePolicyForMIMEType(FramePolicyFunction func,
                                const String& MIMEType, const ResourceRequest&) {
    ASSERT(m_frame);
    ASSERT(func);
    // Default to Use (display internally).
    PolicyAction action = PolicyUse;
    // Check if we should Download instead.
    const ResourceResponse& response = m_frame->loader()->activeDocumentLoader()->response();
    const String& content_disposition = response.httpHeaderField("Content-Disposition");
    if (!content_disposition.isEmpty()) {
        // Server wants to override our normal policy.
        if (TreatAsAttachment(content_disposition)) {
            // Check to see if we are a sub frame (main frame has no owner element)
            if (m_frame->ownerElement() != 0)
                action = PolicyIgnore;
            else
                action = PolicyDownload;
        }
    } else {
        // Ask if it can be handled internally.
        if (!canShowMIMEType(MIMEType)) {
            // Check to see if we are a sub frame (main frame has no owner element)
            if (m_frame->ownerElement() != 0)
                action = PolicyIgnore;
            else
                action = PolicyDownload;
        }
    }
    // A status code of 204 indicates no content change. Ignore the result.
    WebCore::DocumentLoader* docLoader = m_frame->loader()->activeDocumentLoader();
    if (docLoader->response().httpStatusCode() == 204)
        action = PolicyIgnore;
    (m_frame->loader()->*func)(action);
}

void FrameLoaderClientAndroid::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction func,
                                const NavigationAction&, const ResourceRequest& req, const String& frameName) {
    ASSERT(m_frame);
    // If we get to this point it means that a link has a target that was not
    // found by the frame tree. Instead of creating a new frame, return the
    // current frame in dispatchCreatePage.
    if (canHandleRequest(req))
        (m_frame->loader()->*func)(PolicyUse);
    else
        (m_frame->loader()->*func)(PolicyIgnore);
}

void FrameLoaderClientAndroid::cancelPolicyCheck() {
    lowPriority_notImplemented();
}

void FrameLoaderClientAndroid::dispatchUnableToImplementPolicy(const ResourceError&) {
    notImplemented();
}

void FrameLoaderClientAndroid::dispatchDecidePolicyForNavigationAction(FramePolicyFunction func,
                                const NavigationAction& action, const ResourceRequest& request) {
    ASSERT(m_frame);
    ASSERT(func);
    if (action.type() == NavigationTypeFormResubmitted) {
        m_frame->bridge()->decidePolicyForFormResubmission(func);
        return;
    } else {
        (m_frame->loader()->*func)(PolicyUse);
    }
}

void FrameLoaderClientAndroid::dispatchWillSubmitForm(FramePolicyFunction func, PassRefPtr<FormState>) {
    ASSERT(m_frame);
    ASSERT(func);
    (m_frame->loader()->*func)(PolicyUse);
}

void FrameLoaderClientAndroid::dispatchDidLoadMainResource(DocumentLoader*) {
    notImplemented();
}

void FrameLoaderClientAndroid::revertToProvisionalState(DocumentLoader*) {
    notImplemented();
}

void FrameLoaderClientAndroid::setMainDocumentError(DocumentLoader* docLoader, const ResourceError& error) {
    ASSERT(m_frame);
    if (!error.isNull() && error.errorCode() >= InternalErrorLast)
        m_frame->bridge()->reportError(error.errorCode(),
                error.localizedDescription(), error.failingURL());
}

void FrameLoaderClientAndroid::clearUnarchivingState(DocumentLoader*) {
    notImplemented();
}

// This function is called right before the progress is updated.
void FrameLoaderClientAndroid::willChangeEstimatedProgress() {
    verifiedOk();
}

// This function is called after the progress has been updated. The bad part
// about this is that when a page is completed, this function is called after
// the progress has been reset to 0.
void FrameLoaderClientAndroid::didChangeEstimatedProgress() {
    verifiedOk();
}

// This will give us the initial estimate when the page first starts to load.
void FrameLoaderClientAndroid::postProgressStartedNotification() {
    ASSERT(m_frame);
    if (m_frame->page())
        m_frame->bridge()->setProgress(m_frame->page()->progress()->estimatedProgress());
}

// This will give us any updated progress including the final progress.
void FrameLoaderClientAndroid::postProgressEstimateChangedNotification() {
    ASSERT(m_frame);
    if (m_frame->page())
        m_frame->bridge()->setProgress(m_frame->page()->progress()->estimatedProgress());
}

// This is just a notification that the progress has finished. Don't call
// setProgress(1) because postProgressEstimateChangedNotification will do so.
void FrameLoaderClientAndroid::postProgressFinishedNotification() {
    m_frame->view()->getWebCoreViewBridge()->notifyProgressFinished();
}

void FrameLoaderClientAndroid::setMainFrameDocumentReady(bool) {
    // this is only interesting once we provide an external API for the DOM
    notImplemented();
}

void FrameLoaderClientAndroid::startDownload(const ResourceRequest&) {
    notImplemented();
}

void FrameLoaderClientAndroid::willChangeTitle(DocumentLoader*) {
    verifiedOk();
}

void FrameLoaderClientAndroid::didChangeTitle(DocumentLoader* loader) {
    verifiedOk();
}

void FrameLoaderClientAndroid::finishedLoading(DocumentLoader* docLoader) {
    // Telling the frame we received some data and passing 0 as the data is our
    // way to get work done that is normally done when the first bit of data is
    // received, even for the case of a document with no data (like about:blank)
    committedLoad(docLoader, 0, 0);
}

void FrameLoaderClientAndroid::finalSetupForReplace(DocumentLoader*) {
    notImplemented();
}

void FrameLoaderClientAndroid::updateGlobalHistoryForStandardLoad(const KURL& url) {
    ASSERT(m_frame);
    const DeprecatedString dStr = url.deprecatedString();
    if (!historyContains(reinterpret_cast<const UChar*>(dStr.unicode()), dStr.length())) {
        if (gSessionHistory.size() == MAX_SESSION_HISTORY)
            gSessionHistory.remove(0);
        gSessionHistory.append(url);
    }
    m_frame->bridge()->updateVisitedHistory(url, false);
}

void FrameLoaderClientAndroid::updateGlobalHistoryForReload(const KURL& url) {
    ASSERT(m_frame);
    m_frame->bridge()->updateVisitedHistory(url, true);
}

bool FrameLoaderClientAndroid::shouldGoToHistoryItem(HistoryItem* item) const {
    // hmmm, seems like we might do a more thoughtful check
    ASSERT(m_frame);
    return item != NULL;
}

void FrameLoaderClientAndroid::committedLoad(DocumentLoader* loader, const char* data, int length) {
    ASSERT(m_frame);
    String encoding = loader->overrideEncoding();
    bool userChosen = !encoding.isNull();
    if (encoding.isNull())
        encoding = loader->response().textEncodingName();
    loader->frameLoader()->setEncoding(encoding, userChosen);
    Document *doc = m_frame->document();
    if (doc) {
        loader->frameLoader()->addData(data, length);
    }
}

ResourceError FrameLoaderClientAndroid::cancelledError(const ResourceRequest& request) {
    return ResourceError(String(), InternalErrorCancelled, String(), String());
}

ResourceError FrameLoaderClientAndroid::cannotShowURLError(const ResourceRequest& request) {
    return ResourceError(String(), InternalErrorCannotShowUrl, String(), String());
}

ResourceError FrameLoaderClientAndroid::interruptForPolicyChangeError(const ResourceRequest& request) {
    return ResourceError(String(), InternalErrorInterrupted, String(), String());
}

ResourceError FrameLoaderClientAndroid::cannotShowMIMETypeError(const ResourceResponse& request) {
    return ResourceError(String(), InternalErrorCannotShowMimeType, String(), String());
}

ResourceError FrameLoaderClientAndroid::fileDoesNotExistError(const ResourceResponse& request) {
    return ResourceError(String(), InternalErrorFileDoesNotExist, String(), String());
}

bool FrameLoaderClientAndroid::shouldFallBack(const ResourceError&) {
    notImplemented();
    return false;
}

void FrameLoaderClientAndroid::setDefersLoading(bool) {
    notImplemented();
}

bool FrameLoaderClientAndroid::willUseArchive(ResourceLoader*, const ResourceRequest&,
                                const KURL& originalURL) const {
    lowPriority_notImplemented();
    return false;
}

bool FrameLoaderClientAndroid::isArchiveLoadPending(ResourceLoader*) const {
    lowPriority_notImplemented();
    return false;
}

void FrameLoaderClientAndroid::cancelPendingArchiveLoad(ResourceLoader*) {
    notImplemented();
}

void FrameLoaderClientAndroid::clearArchivedResources() {
    notImplemented();
}

bool FrameLoaderClientAndroid::canHandleRequest(const ResourceRequest& request) const {
    ASSERT(m_frame);
    // Don't allow hijacking of intrapage navigation
    if (WebCore::equalIgnoringRef(request.url(), m_frame->loader()->url())) 
        return true;

    // Don't allow hijacking of iframe urls that are http or https
    if (request.url().protocol().startsWith("http", false) &&
            m_frame->tree() && m_frame->tree()->parent())
        return true;

    if (m_frame->bridge()->canHandleRequest(request)) {
#ifdef ANDROID_META_SUPPORT
        // reset metadata settings for the top frame as they are not preserved cross page
        if (!m_frame->tree()->parent())
            m_frame->settings()->resetMetadataSettings();
#endif
        return true;
    }
    return false;
}

bool FrameLoaderClientAndroid::canShowMIMEType(const String& mimeType) const {
    // FIXME: This looks like it has to do with whether or not a type can be
    // shown "internally" (i.e. inside the browser) regardless of whether
    // or not the browser is doing the rendering, e.g. a full page plugin.
    if (mimeType.startsWith("text/plain", false) ||
        mimeType.startsWith("image/jpeg", false) || // Be specific with supported image formats
        mimeType.startsWith("image/gif", false) ||
        mimeType.startsWith("image/png", false) ||
        mimeType.startsWith("image/bmp", false) ||
        mimeType.startsWith("image/x-icon", false) ||
        mimeType.startsWith("image/ico", false) ||
        mimeType.endsWith("/xml", false) ||  // text/xml & application/xml
        mimeType.endsWith("xhtml+xml", false) || // application/xhtml+xml
        // Checked Safari impl, it seems that the HTTP stack returns
        // multiple responses, the initial response, and then one for
        // multipart segment. Each response is sent to the same ResourceLoader
        // so for us to support this we would need to do the same.
        // mimeType.startsWith("multipart/x-mixed-replace", false) ||
        mimeType.endsWith("html", false))
        return true;
    return false;
}

bool FrameLoaderClientAndroid::representationExistsForURLScheme(const String&) const {
    // don't use representation
    verifiedOk();
    return false;
}

String FrameLoaderClientAndroid::generatedMIMETypeForURLScheme(const String& URLScheme) const {
    // FIXME, copy from Apple's port
    String mimetype("x-apple-web-kit/");
    mimetype.append(URLScheme.lower());
    return mimetype;
}

void FrameLoaderClientAndroid::frameLoadCompleted() {
    // copied from Apple port, without this back with sub-frame will trigger ASSERT
    ASSERT(m_frame);
    m_frame->loader()->setPreviousHistoryItem(0);
}

void FrameLoaderClientAndroid::saveViewStateToItem(HistoryItem* item) {
#ifdef ANDROID_HISTORY_CLIENT
    ASSERT(m_frame);
    ASSERT(item);
    // We should have added a bridge when the child item was added to its
    // parent.
    android::WebHistoryItem* bridge = item->bridge();
    ASSERT(bridge);
    // store the current scale
    bridge->setScale(m_frame->view()->getWebCoreViewBridge()->scale());

    // Store the location of the focus, based on how many traversals it
    // takes to reach the focus from the document.
    WebCore::Document* doc = m_frame->document();
    WebCore::Node* focus = doc->focusedNode();
    if (focus) {
        int focusNum = 0;
        WebCore::Node* node = doc;
        while (node != focus) {
            node = node->traverseNextNode(doc);
            ASSERT(node);
            focusNum++;
        }
        bridge->setTraversals(focusNum);
        DEBUG_NAV_UI_LOGD("%s focus=%p traversals=%d", __FUNCTION__, focus, focusNum);
    } else {
        bridge->setTraversals(-1);
        DEBUG_NAV_UI_LOGD("%s focus=NULL traversals=-1", __FUNCTION__);
    }
    WebCore::notifyHistoryItemChanged(item);
#endif
}

void FrameLoaderClientAndroid::restoreViewState() {
#ifdef ANDROID_HISTORY_CLIENT
    HistoryItem* item = m_frame->loader()->currentHistoryItem();
    // restore the scale
    m_frame->view()->getWebCoreViewBridge()->restoreScale(item->bridge()->scale());
    // restore the focus
    if (m_frame->loader()->firstLayoutDone() == false)
        return;
    int traversals = item->bridge()->traversals();
    if (traversals < 0)
        return;
    WebCore::Document* doc = m_frame->document();
    WebCore::Node* node = doc;
    while (--traversals >= 0) {
        if ((node = node->traverseNextNode(doc)) == NULL)
            return;
    }
    if (doc->focusedNode() == node)
        return;
    DEBUG_NAV_UI_LOGD("%s focus=%p", __FUNCTION__, node);
    doc->setFocusedNode(node);
    m_frame->view()->getWebCoreViewBridge()->notifyFocusSet();
#endif
}

#ifdef ANDROID_HISTORY_CLIENT
void FrameLoaderClientAndroid::dispatchDidAddHistoryItem(HistoryItem* item) const {
    ASSERT(m_frame);
    m_frame->bridge()->addHistoryItem(item);
}

void FrameLoaderClientAndroid::dispatchDidRemoveHistoryItem(HistoryItem* item, int index) const {
    ASSERT(m_frame);
    m_frame->bridge()->removeHistoryItem(index);
}

void FrameLoaderClientAndroid::dispatchDidChangeHistoryIndex(
        BackForwardList* list) const {
    ASSERT(m_frame);
    m_frame->bridge()->updateHistoryIndex(list->backListCount());
}
#endif

void FrameLoaderClientAndroid::provisionalLoadStarted() {
    ASSERT(m_frame);
    m_frame->bridge()->loadStarted(m_frame);
}

void FrameLoaderClientAndroid::didFinishLoad() {
    ASSERT(m_frame);
    m_frame->bridge()->didFinishLoad(m_frame);
}

void FrameLoaderClientAndroid::prepareForDataSourceReplacement() {
    ASSERT(m_frame);
    m_frame->loader()->detachChildren();
}

PassRefPtr<DocumentLoader> FrameLoaderClientAndroid::createDocumentLoader(
                    const ResourceRequest& request, const SubstituteData& data) {
    RefPtr<DocumentLoader> loader = new DocumentLoader(request, data);
    return loader.release();
}

void FrameLoaderClientAndroid::setTitle(const String& title, const KURL& url) {
    // Not needed. dispatchDidReceiveTitle is called immediately after this.
    // url is used to update the Apple port history items.
    verifiedOk();
}

String FrameLoaderClientAndroid::userAgent(const KURL& u) {
    ASSERT(m_frame);
    return m_frame->bridge()->userAgentForURL(&u);
}

bool FrameLoaderClientAndroid::canCachePage() const {
    return true;
}

void FrameLoaderClientAndroid::download(ResourceHandle* handle, const ResourceRequest&,
                                const ResourceRequest&, const ResourceResponse&) {
    // Get the C++ side of the load listener and tell it to handle the download
    android::WebCoreResourceLoader* loader = handle->getInternal()->m_loader;
    loader->downloadFile();
}

class ChildFrameViewBridge : public WebCoreViewBridge
{
public:
    ChildFrameViewBridge(WebCoreViewBridge* parent, WebCore::FrameAndroid* frame)
        : mFrame(frame)
    {
        setParent(parent);
        mFrame->ref();
    }

    virtual ~ChildFrameViewBridge()
    {
        mFrame->deref();
    }

    virtual void draw(WebCore::GraphicsContext* ctx, 
        const WebCore::IntRect& rect, bool)
    {
        // Create a new translated rect from the given rectangle.
        WebCore::IntRect transRect(rect);

        // Grab the intersection of transRect and the frame's bounds.
        transRect.intersect(this->getBounds());

        // Move the transRect into the frame's local coordinates.
        transRect.move(-this->locX(), -this->locY());

        // If the rect is non-empty, translate the canvas, add a clip and draw.
        SkRect r;
        android_setrect(&r, transRect);
        if (r.isEmpty())
            return;
        // In Frame::markAllMatchesForText(), it does a fake paint.  So we need
        // to handle the case where platformContext() is null.  However, we still
        // want to call paint, since WebKit must have called the paint for a reason.
        SkCanvas* canvas = ctx->platformContext() ? ctx->platformContext()->mCanvas : NULL;
        if (canvas) {
            canvas->save();
            canvas->translate(SkIntToScalar(this->locX()), SkIntToScalar(this->locY()));
            canvas->clipRect(r);
        }
        mFrame->paint(ctx, transRect);
        if (canvas)
            canvas->restore();
    }

    // Bubble up Javascript dialogs to the parent. The top level view will
    // take care of displaying them to the user.
    virtual void jsAlert(const WebCore::String& url, const WebCore::String& text)
    {
       getParent()->jsAlert(url, text);
    }
    virtual bool jsConfirm(const WebCore::String& url, const WebCore::String& text)
    {
        return getParent()->jsConfirm(url, text);
    }
    virtual bool jsPrompt(const WebCore::String& url,
                          const WebCore::String& message,
                          const WebCore::String& defaultValue,
                          WebCore::String& result)
    {
        return getParent()->jsPrompt(url, message, defaultValue, result);
    }

private:
    WebCore::FrameAndroid* mFrame;
    typedef WebCoreViewBridge INHERITED;
};

WTF::PassRefPtr<WebCore::Frame> FrameLoaderClientAndroid::createFrame(const KURL& url, const String& name,
                        HTMLFrameOwnerElement* ownerElement, const String& referrer,
                        bool allowsScrolling, int marginWidth, int marginHeight)
{
    Frame* parent = ownerElement->document()->frame();
    FrameLoaderClientAndroid* loaderC = new FrameLoaderClientAndroid;
    WebCore::FrameAndroid* newFrame = new WebCore::FrameAndroid(parent->page(), ownerElement, loaderC);
    loaderC->setFrame(newFrame);
    // Append the subframe to the parent and set the name of the subframe. The name must be set after
    // appending the child so that the name becomes unique.
    parent->tree()->appendChild(newFrame);
    newFrame->tree()->setName(name);
    newFrame->setBridge(Android(parent)->bridge());
    // Create a new FrameView and bridge for the child frame to draw into.
    WebCore::FrameView* frameView = new WebCore::FrameView(newFrame);
    ChildFrameViewBridge* view = new ChildFrameViewBridge(parent->view()->getWebCoreViewBridge(), newFrame);
    frameView->setWebCoreViewBridge(view);
    // Unref the viewBridge so that frameView is the only reference holder.
    Release(view);
    newFrame->setView(frameView);
    // Frames are initiliazed with a ref count of 1 but since our view bridge is holding onto the frame, deref
    // it here. setView also refs the frameView so call deref on the frameView as well.
    newFrame->deref();
    frameView->deref();
    newFrame->init();
    newFrame->selectionController()->setFocused(true);
    LOGV("::WebCore:: createSubFrame returning %p", newFrame);

    HistoryItem* item = parent->loader()->currentHistoryItem();
    KURL childUrl = url;
    FrameLoadType loadType = parent->loader()->loadType();
    FrameLoadType childLoadType = FrameLoadTypeRedirectWithLockedHistory;
    // If we are moving in the back/forward list, we might want to replace the
    // content of this child frame with whatever was there at that point. Reload
    // will maintain the frame contents, LoadSame will not.
    HistoryItem* childItem = NULL;
    if (item && item->children().size() &&
            (isBackForwardLoadType(loadType) ||
             loadType == FrameLoadTypeReload ||
             loadType == FrameLoadTypeReloadAllowingStaleData)) {
        childItem = item->childItemWithName(newFrame->tree()->name());
        if (childItem) {
            // Use the original url to ensure we get all the side-effects, such
            // as onLoad handlers or redirects.
            childUrl = childItem->originalURL();
            childLoadType = loadType;
            if (isBackForwardLoadType(loadType))
                // For back/forward, remember this item so we can traverse any
                // child items as child frames load.
                newFrame->loader()->setProvisionalHistoryItem(childItem);
            else
                // For reload, just reinstall the current item, since a new
                // child frame was created but we won't be creating a new
                // history item.
                newFrame->loader()->setCurrentHistoryItem(childItem);
        }
    }
    RefPtr<Frame> autoFrame = newFrame;
#ifdef ANDROID_FIX
    if (childItem && childItem->originalFormData())
        autoFrame->loader()->postFromHistory(childUrl, childItem->originalFormData(),
                childItem->originalFormContentType(), childItem->originalFormReferrer(), childLoadType);
    else
#endif
#ifdef ANDROID_USER_GESTURE
    autoFrame->loader()->load(childUrl, referrer, childLoadType,
            String(), NULL, NULL, false);
#else
    autoFrame->loader()->load(childUrl, referrer, childLoadType,
            String(), NULL, NULL);
#endif

    // onLoad may cuase the frame to be removed from the document. Allow the RefPtr to delete the child frame.
    if (!autoFrame->tree()->parent())
        return NULL;

    return autoFrame.release();
}

// YouTube flash url path starts with /v/
static const char slash_v_slash[] = { '/', 'v', '/' };

static bool isValidYouTubeVideo(const DeprecatedString& path)
{
    if (!path.isAllASCII())
        return false;
    unsigned int len = path.length();
    if (len <= sizeof(slash_v_slash)) // check for more than just /v/
        return false;
    DeprecatedString str = path.lower();
    const char* data = str.ascii();
    if (memcmp(data, slash_v_slash, sizeof(slash_v_slash)) != 0)
        return false;
    // Start after /v/
    for (unsigned int i = sizeof(slash_v_slash); i < len; i++) {
        char c = data[i];
        // Check for alpha-numeric characters only.
        if (WTF::isASCIIAlphanumeric(c))
            continue;
        // The url can have more parameters such as &hl=en after the video id.
        // Once we start seeing extra parameters we can return true.
        return c == '&' && i > sizeof(slash_v_slash);
    }
    return true;
}

static bool isYouTubeUrl(const KURL& url, const String& mimeType)
{
    return url.host().endsWith("youtube.com") && isValidYouTubeVideo(url.path())
            && equalIgnoringCase(mimeType, "application/x-shockwave-flash");
}

Widget* FrameLoaderClientAndroid::createPlugin(
        const IntSize& size,
        Element* element,
        const KURL& url,
        const WTF::Vector<String, 0u>& names,
        const WTF::Vector<String, 0u>& values,
        const String& mimeType,
        bool loadManually) {
    // Create an iframe for youtube urls.
    if (isYouTubeUrl(url, mimeType)) {
        DeprecatedString str("file:///android_asset/webkit/youtube.html?v=");
        str.append(url.path().mid(sizeof(slash_v_slash))); // grab everything after /v/
        RefPtr<Frame> frame = createFrame(KURL(str), String(), static_cast<HTMLFrameOwnerElement*>(element),
                String(), false, 0, 0);
        return frame ? frame->view() : NULL;
    }
    PluginViewAndroid* v =
            PluginDatabaseAndroid::installedPlugins()->createPluginView(
                    m_frame,
                    size,
                    element,
                    url,
                    names,
                    values,
                    mimeType,
                    loadManually);
    return v;
}

void FrameLoaderClientAndroid::redirectDataToPlugin(Widget* pluginWidget) {
    // don't support plugin yet
    notImplemented();
}

Widget* FrameLoaderClientAndroid::createJavaAppletWidget(const IntSize&, Element*,
                                        const KURL& baseURL, const Vector<String>& paramNames,
                                        const Vector<String>& paramValues) {
    // don't support widget yet
    notImplemented();
    return 0;
}

// This function is used by the <OBJECT> element to determine the type of
// the contents and work out if it can render it.
ObjectContentType FrameLoaderClientAndroid::objectContentType(const KURL& url,
                                        const String& mimeType) {
    if (mimeType.length() == 0)
    {
        // Guess the mimeType from the extension
        if (url.hasPath())
        {
            DeprecatedString path = url.path();
            const char* ascii = path.ascii();
            const char* lastIndex = strrchr(ascii, '.');
            static const String image("image/");
            if (lastIndex)
            {
                String mime(lastIndex + 1);
                mime.insert(image, 0);
                if (Image::supportsType(mime))
                    return ObjectContentImage;
            }
        }
        return ObjectContentFrame;
    }
    if (equalIgnoringCase(mimeType, "text/html") ||
        equalIgnoringCase(mimeType, "text/xml") ||
        equalIgnoringCase(mimeType, "text/") ||
        equalIgnoringCase(mimeType, "application/xml") ||
        equalIgnoringCase(mimeType, "application/xhtml+xml") ||
        equalIgnoringCase(mimeType, "application/x-javascript"))
        return ObjectContentFrame;
    if (Image::supportsType(mimeType))
        return ObjectContentImage;
    return ObjectContentNone;
}

// This function allows the application to set the correct CSS media
// style. Android could use it to set the media style 'handheld'. Safari
// may use it to set the media style to 'print' when the user wants to print
// a particular web page.
String FrameLoaderClientAndroid::overrideMediaType() const {
    lowPriority_notImplemented();
    return String();
}

// This function is used to re-attach Javascript<->native code classes.
void FrameLoaderClientAndroid::windowObjectCleared() {
    ASSERT(m_frame);
    LOGV("::WebCore:: windowObjectCleared called on frame %p for %s\n", 
    		m_frame, m_frame->loader()->url().string().ascii().data());
    m_frame->bridge()->windowObjectCleared(m_frame);
}

// functions new to Jun-07 tip of tree merge:
ResourceError FrameLoaderClientAndroid::blockedError(ResourceRequest const& request) {
    return ResourceError(String(), InternalErrorFileDoesNotExist, String(), String());
}

// functions new to Nov-07 tip of tree merge:
void FrameLoaderClientAndroid::didPerformFirstNavigation() const {
    // This seems to be just a notification that the UI can listen to, to 
    // know if the user has performed first navigation action. 
    // It is called from 
    // void FrameLoader::addBackForwardItemClippedAtTarget(bool doClip)
    // "Navigation" here means a transition from one page to another that 
    // ends up in the back/forward list.
}

void FrameLoaderClientAndroid::registerForIconNotification(bool listen) {
    if (listen)
        android::WebIconDatabase::RegisterForIconNotification(this);
    else
        android::WebIconDatabase::UnregisterForIconNotification(this);
}

// This is the WebIconDatabaseClient method for receiving a notification when we
// get the icon for the page.
void FrameLoaderClientAndroid::didAddIconForPageUrl(const String& pageUrl) {
    registerForIconNotification(false);
    KURL u(pageUrl.deprecatedString());
    if (equalIgnoringRef(u, m_frame->loader()->url())) {
        dispatchDidReceiveIcon();
    }
}

// functions new to Feb-19 tip of tree merge:
// According to the changelog:
// The very Mac-centric "makeDocumentView", "setDocumentViewFromCachedPage",
// and "saveDocumentViewToCachedPage" become "transitionToCommittedForNewPage", 
// "transitionToCommittedFromCachedPage", and "savePlatformDataToCachedPage" 
// accordingly
void FrameLoaderClientAndroid::savePlatformDataToCachedPage(CachedPage*) {
    // don't support page cache
    verifiedOk();
}

void FrameLoaderClientAndroid::transitionToCommittedFromCachedPage(CachedPage*) {
    // don't support page cache
    verifiedOk();
}

void FrameLoaderClientAndroid::transitionToCommittedForNewPage() {
    ASSERT(m_frame);
    if (!m_frame->page()->settings()->usesPageCache()) {
        m_frame->bridge()->transitionToCommitted(m_frame);
        return;
    }

    // Remember the old bridge
    WebCoreViewBridge* bridge = m_frame->view()->getWebCoreViewBridge();
    Retain(bridge);

    // Remove the old FrameView
    m_frame->setView(NULL);

    // Create a new one and set the bridge
    FrameView* view = new FrameView(m_frame);
    view->setWebCoreViewBridge(bridge);
    Release(bridge);
    bridge->setView(view);

    // Give the new FrameView to the Frame
    m_frame->setView(view);

    // Deref since FrameViews are created with a ref of 1
    view->deref();

    if (m_frame->ownerRenderer())
        m_frame->ownerRenderer()->setWidget(view);

    m_frame->view()->initScrollbars();

    m_frame->bridge()->transitionToCommitted(m_frame);
}

}
