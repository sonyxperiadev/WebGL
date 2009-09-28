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

#include "android_graphics.h"
#include "CString.h"
#include "DocumentLoader.h"
#include "DOMImplementation.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClientAndroid.h"
#include "FrameTree.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLPlugInElement.h"
#include "IconDatabase.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformGraphicsContext.h"
#include "PlatformString.h"
#include "PluginDatabase.h"
#include "PluginView.h"
#include "ProgressTracker.h"
#include "RenderPart.h"
#include "ResourceError.h"
#include "SelectionController.h"
#include "SkCanvas.h"
#include "SkRect.h"
#include "TextEncoding.h"
#include "Document.h"
#include "FrameView.h"
#include "HistoryItem.h"
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreResourceLoader.h"
#include "WebHistory.h"
#include "WebIconDatabase.h"
#include "WebFrameView.h"
#include "WebViewCore.h"
#include "Settings.h"

#include <utils/AssetManager.h>

extern android::AssetManager* globalAssetManager();

namespace android {

static const int EXTRA_LAYOUT_DELAY = 1000;

FrameLoaderClientAndroid::FrameLoaderClientAndroid(WebFrame* webframe)
    : m_frame(NULL)
    , m_webFrame(webframe) {
    Retain(m_webFrame);
}

FrameLoaderClientAndroid* FrameLoaderClientAndroid::get(const WebCore::Frame* frame)
{
    return static_cast<FrameLoaderClientAndroid*> (frame->loader()->client());
}

void FrameLoaderClientAndroid::frameLoaderDestroyed() {
    registerForIconNotification(false);
    m_frame = 0;
    Release(m_webFrame);
    delete this;
}

bool FrameLoaderClientAndroid::hasWebView() const {
    // FIXME,
    // there is one web view per page, or top frame.
    // as android's view is created from Java side, it is always there.
    return true;
}

void FrameLoaderClientAndroid::makeRepresentation(DocumentLoader*) {
    // don't use representation
    verifiedOk();
}

void FrameLoaderClientAndroid::forceLayout() {
    ASSERT(m_frame);
    m_frame->view()->forceLayout();
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

bool FrameLoaderClientAndroid::shouldUseCredentialStorage(DocumentLoader*, unsigned long  identifier)
{
    notImplemented();
    return false;
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

void FrameLoaderClientAndroid::dispatchDidLoadResourceByXMLHttpRequest(unsigned long identifier, const ScriptString&) {
    return;
}

void FrameLoaderClientAndroid::dispatchDidHandleOnloadEvents() {
}

void FrameLoaderClientAndroid::dispatchDidReceiveServerRedirectForProvisionalLoad() {
    ASSERT(m_frame);
    // Tell the load it was a redirect.
    m_webFrame->loadStarted(m_frame);
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
    // Try to obtain the icon image.
    WebCore::Image* icon = WebCore::iconDatabase()->iconForPageURL(
            url, WebCore::IntSize(16, 16));
    // If the request fails, try the original request url.
    if (!icon) {
        DocumentLoader* docLoader = m_frame->loader()->activeDocumentLoader();
        KURL originalURL = docLoader->originalRequest().url();
        icon = WebCore::iconDatabase()->iconForPageURL(
                   originalURL, WebCore::IntSize(16, 16));
    }
    // There is a bug in webkit where cancelling an icon load is treated as a
    // failure. When this is fixed, we can ASSERT again that we have an icon.
    if (icon) {
        LOGV("Received icon (%p) for %s", icon,
                url.utf8().data());
        m_webFrame->didReceiveIcon(icon);
    } else {
        LOGV("Icon data for %s unavailable, registering for notification...",
                url.utf8().data());
        registerForIconNotification();
    }
}

void FrameLoaderClientAndroid::dispatchDidReceiveTouchIconURL(const String& url, bool precomposed) {
    ASSERT(m_frame);
    // Do not report sub frame touch icons
    if (m_frame->tree() && m_frame->tree()->parent())
        return;
    m_webFrame->didReceiveTouchIconURL(url, precomposed);
}

void FrameLoaderClientAndroid::dispatchDidStartProvisionalLoad() {
    notImplemented();
}

void FrameLoaderClientAndroid::dispatchDidReceiveTitle(const String& title) {
    ASSERT(m_frame);
    // Used to check for FrameLoadTypeStandard but we only want to send the title for
    // the top frame and not sub-frames.
    if (!m_frame->tree() || !m_frame->tree()->parent()) {
        m_webFrame->setTitle(title);
    }
}

void FrameLoaderClientAndroid::dispatchDidCommitLoad() {
    verifiedOk();
}

static void loadDataIntoFrame(Frame* frame, KURL baseUrl, const String& url,
        const String& data) {
    if (baseUrl.isEmpty()) {
        baseUrl = blankURL();
    }
    ResourceRequest request(baseUrl);
    CString cstr = data.utf8();
    RefPtr<WebCore::SharedBuffer> buf = WebCore::SharedBuffer::create(cstr.data(), cstr.length());
    SubstituteData subData(buf, String("text/html"), String("utf-8"),
            KURL(KURL(), url));
    frame->loader()->load(request, subData, false);
}

void FrameLoaderClientAndroid::dispatchDidFailProvisionalLoad(const ResourceError& error) {
    ASSERT(m_frame);
    // Ignore ErrorInterrupted since it is due to a policy interruption. This
    // is caused by a decision to download the main resource rather than
    // display it.
    if (error.errorCode() == InternalErrorInterrupted
            || error.errorCode() == InternalErrorCancelled) {
        // If we decided to download the main resource or if the user cancelled
        // it, make sure we report that the load is done.
        didFinishLoad();
        return;
    }

    AssetManager* am = globalAssetManager();

    // Check to see if the error code was not generated internally
    WebFrame::RAW_RES_ID id = WebFrame::NODOMAIN;
    if ((error.errorCode() == ErrorFile ||
            error.errorCode() == ErrorFileNotFound) &&
            (!error.localizedDescription().isEmpty())) {
        id = WebFrame::LOADERROR;
    }
    String filename = m_webFrame->getRawResourceFilename(id);
    if (filename.isEmpty())
        return;

    // Grab the error page from the asset manager
    Asset* a = am->openNonAsset(
            filename.utf8().data(), Asset::ACCESS_BUFFER);
    if (!a)
        return;

    // Take the failing url and encode html entities so javascript urls are not
    // executed.
    CString failingUrl = error.failingURL().utf8();
    WTF::Vector<char> url;
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
    String s = UTF8Encoding().decode((const char*)a->getBuffer(false), a->getLength());
    s = s.replace("%s", String(url.data(), url.size()));

    // Replace all occurances of %e with the error text
    s = s.replace("%e", error.localizedDescription());

    // Create the request and the substitute data and tell the FrameLoader to
    // load with the replacement data.
    // use KURL(const char*) as KURL(const String& url) can trigger ASSERT for
    // invalidate URL string.
    loadDataIntoFrame(m_frame, KURL(data), error.failingURL(), s);

    // Delete the asset.
    delete a;
}

void FrameLoaderClientAndroid::dispatchDidFailLoad(const ResourceError&) {
    // called when page is completed with error
    didFinishLoad();
}

void FrameLoaderClientAndroid::dispatchDidFinishDocumentLoad() {
    // called when finishedParsing
    lowPriority_notImplemented();
}

void FrameLoaderClientAndroid::dispatchDidFinishLoad() {
    didFinishLoad();
}

void FrameLoaderClientAndroid::dispatchDidFirstLayout() {
    ASSERT(m_frame);
    m_frame->document()->setExtraLayoutDelay(EXTRA_LAYOUT_DELAY);
    // we need to do this here instead of dispatchDidFirstVisuallyNonEmptyLayout
    // so that about:blank will update the screen.
    if (!m_frame->tree()->parent()) {
        // Only need to notify Java side for the top frame
        WebViewCore::getWebViewCore(m_frame->view())->didFirstLayout();
    }
}

void FrameLoaderClientAndroid::dispatchDidFirstVisuallyNonEmptyLayout()
{
    notImplemented();
}

Frame* FrameLoaderClientAndroid::dispatchCreatePage() {
    ASSERT(m_frame);
#ifdef ANDROID_MULTIPLE_WINDOWS
    if (m_frame->settings() && m_frame->settings()->supportMultipleWindows())
        // Always a user gesture since window.open maps to
        // ChromeClientAndroid::createWindow
        return m_webFrame->createWindow(false, true);
    else
#endif
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
                                const String& MIMEType, const ResourceRequest& request) {
    ASSERT(m_frame);
    ASSERT(func);
    if (!func)
        return;
    if (request.isNull()) {
        (m_frame->loader()->*func)(PolicyIgnore);
        return;
    }
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
                                const NavigationAction& action, const ResourceRequest& request,
                                PassRefPtr<FormState> formState, const String& frameName) {
    ASSERT(m_frame);
    ASSERT(func);
    if (!func)
        return;

    if (request.isNull()) {
        (m_frame->loader()->*func)(PolicyIgnore);
        return;
    }

    if (action.type() == NavigationTypeFormSubmitted || action.type() == NavigationTypeFormResubmitted)
        m_frame->loader()->resetMultipleFormSubmissionProtection();

    // If we get to this point it means that a link has a target that was not
    // found by the frame tree. Instead of creating a new frame, return the
    // current frame in dispatchCreatePage.
    if (canHandleRequest(request))
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
                                const NavigationAction& action, const ResourceRequest& request,
                                PassRefPtr<FormState> formState) {
    ASSERT(m_frame);
    ASSERT(func);
    if (!func)
        return;
    if (request.isNull()) {
        (m_frame->loader()->*func)(PolicyIgnore);
        return;
    }

    // Reset multiple form submission protection. If this is a resubmission, we check with the
    // user and reset the protection if they choose to resubmit the form (see WebCoreFrameBridge.cpp)
    if (action.type() == NavigationTypeFormSubmitted)
        m_frame->loader()->resetMultipleFormSubmissionProtection();

    if (action.type() == NavigationTypeFormResubmitted) {
        m_webFrame->decidePolicyForFormResubmission(func);
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
        m_webFrame->reportError(error.errorCode(),
                error.localizedDescription(), error.failingURL());
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
        m_webFrame->setProgress(m_frame->page()->progress()->estimatedProgress());
}

// This will give us any updated progress including the final progress.
void FrameLoaderClientAndroid::postProgressEstimateChangedNotification() {
    ASSERT(m_frame);
    if (m_frame->page())
        m_webFrame->setProgress(m_frame->page()->progress()->estimatedProgress());
}

// This is just a notification that the progress has finished. Don't call
// setProgress(1) because postProgressEstimateChangedNotification will do so.
void FrameLoaderClientAndroid::postProgressFinishedNotification() {
    WebViewCore* core =  WebViewCore::getWebViewCore(m_frame->view());
    if (!m_frame->tree()->parent()) {
        // only need to notify Java for the top frame
        core->notifyProgressFinished();
    }
    // notify plugins that the frame has loaded
    core->notifyPluginsOnFrameLoad(m_frame);
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

void FrameLoaderClientAndroid::updateGlobalHistory() {
    ASSERT(m_frame);

    DocumentLoader* docLoader = m_frame->loader()->documentLoader();
    ASSERT(docLoader);

    // Code copied from FrameLoader.cpp:createHistoryItem
    // Only add this URL to the database if it is a valid page
    if (docLoader->unreachableURL().isEmpty()
            && docLoader->response().httpStatusCode() < 400) {
        m_webFrame->updateVisitedHistory(docLoader->urlForHistory(), false);
        if (!docLoader->serverRedirectSourceForHistory().isNull())
            m_webFrame->updateVisitedHistory(KURL(docLoader->serverRedirectDestinationForHistory()), false);
    }
}

void FrameLoaderClientAndroid::updateGlobalHistoryRedirectLinks() {
    // Note, do we need to do anything where there is no HistoryItem? If we call
    // updateGlobalHistory(), we will add bunch of "data:xxx" urls for gmail.com
    // which is not what we want. Opt to do nothing now.
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
    return ResourceError(String(), InternalErrorCancelled, request.url(), String());
}

ResourceError FrameLoaderClientAndroid::cannotShowURLError(const ResourceRequest& request) {
    return ResourceError(String(), InternalErrorCannotShowUrl, request.url(), String());
}

ResourceError FrameLoaderClientAndroid::interruptForPolicyChangeError(const ResourceRequest& request) {
    return ResourceError(String(), InternalErrorInterrupted, request.url(), String());
}

ResourceError FrameLoaderClientAndroid::cannotShowMIMETypeError(const ResourceResponse& request) {
    return ResourceError(String(), InternalErrorCannotShowMimeType, request.url(), String());
}

ResourceError FrameLoaderClientAndroid::fileDoesNotExistError(const ResourceResponse& request) {
    return ResourceError(String(), InternalErrorFileDoesNotExist, request.url(), String());
}

ResourceError FrameLoaderClientAndroid::pluginWillHandleLoadError(const ResourceResponse& request) {
    return ResourceError(String(), InternalErrorPluginWillHandleLoadError, request.url(), String());
}

bool FrameLoaderClientAndroid::shouldFallBack(const ResourceError&) {
    notImplemented();
    return false;
}

bool FrameLoaderClientAndroid::canHandleRequest(const ResourceRequest& request) const {
    ASSERT(m_frame);
    // Don't allow hijacking of intrapage navigation
    if (WebCore::equalIgnoringFragmentIdentifier(request.url(), m_frame->loader()->url()))
        return true;

    // Don't allow hijacking of iframe urls that are http or https
    if (request.url().protocol().startsWith("http", false) &&
            m_frame->tree() && m_frame->tree()->parent())
        return true;

    return m_webFrame->canHandleRequest(request);
}

bool FrameLoaderClientAndroid::canShowMIMEType(const String& mimeType) const {
    // FIXME: This looks like it has to do with whether or not a type can be
    // shown "internally" (i.e. inside the browser) regardless of whether
    // or not the browser is doing the rendering, e.g. a full page plugin.
    if (MIMETypeRegistry::isSupportedImageResourceMIMEType(mimeType) ||
            MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType) ||
            MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType) ||
            PluginDatabase::installedPlugins()->isMIMETypeRegistered(mimeType) ||
            DOMImplementation::isTextMIMEType(mimeType) ||
            DOMImplementation::isXMLMIMEType(mimeType))
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
}

void FrameLoaderClientAndroid::saveViewStateToItem(HistoryItem* item) {
#ifdef ANDROID_HISTORY_CLIENT
    ASSERT(m_frame);
    ASSERT(item);
    // We should have added a bridge when the child item was added to its
    // parent.
    WebHistoryItem* bridge = item->bridge();
    ASSERT(bridge);
    // store the current scale (only) for the top frame
    if (!m_frame->tree()->parent()) {
        WebViewCore* webViewCore = WebViewCore::getWebViewCore(m_frame->view());
        bridge->setScale((int)(webViewCore->scale() * 100));
        bridge->setScreenWidthScale((int)(webViewCore->screenWidthScale() * 100));
    }

    WebCore::notifyHistoryItemChanged(item);
#endif
}

void FrameLoaderClientAndroid::restoreViewState() {
#ifdef ANDROID_HISTORY_CLIENT
    WebViewCore* webViewCore = WebViewCore::getWebViewCore(m_frame->view());
    HistoryItem* item = m_frame->loader()->currentHistoryItem();
    // restore the scale (only) for the top frame
    if (!m_frame->tree()->parent()) {
        int scale = item->bridge()->scale();
        webViewCore->restoreScale(scale);
        int screenWidthScale = item->bridge()->screenWidthScale();
        if (screenWidthScale != scale)
            webViewCore->restoreScreenWidthScale(screenWidthScale);
    }
#endif
}

#ifdef ANDROID_HISTORY_CLIENT
void FrameLoaderClientAndroid::dispatchDidAddHistoryItem(HistoryItem* item) const {
    ASSERT(m_frame);
    m_webFrame->addHistoryItem(item);
}

void FrameLoaderClientAndroid::dispatchDidRemoveHistoryItem(HistoryItem* item, int index) const {
    ASSERT(m_frame);
    m_webFrame->removeHistoryItem(index);
}

void FrameLoaderClientAndroid::dispatchDidChangeHistoryIndex(
        BackForwardList* list) const {
    ASSERT(m_frame);
    m_webFrame->updateHistoryIndex(list->backListCount());
}
#endif

void FrameLoaderClientAndroid::provisionalLoadStarted() {
    ASSERT(m_frame);
    m_webFrame->loadStarted(m_frame);
}

void FrameLoaderClientAndroid::didFinishLoad() {
    ASSERT(m_frame);
    m_frame->document()->setExtraLayoutDelay(0);
    m_webFrame->didFinishLoad(m_frame);
}

void FrameLoaderClientAndroid::prepareForDataSourceReplacement() {
    verifiedOk();
}

PassRefPtr<DocumentLoader> FrameLoaderClientAndroid::createDocumentLoader(
                    const ResourceRequest& request, const SubstituteData& data) {
    RefPtr<DocumentLoader> loader = DocumentLoader::create(request, data);
    return loader.release();
}

void FrameLoaderClientAndroid::setTitle(const String& title, const KURL& url) {
    // Not needed. dispatchDidReceiveTitle is called immediately after this.
    // url is used to update the Apple port history items.
    verifiedOk();
}

String FrameLoaderClientAndroid::userAgent(const KURL& u) {
    return m_webFrame->userAgentForURL(&u);
}

void FrameLoaderClientAndroid::savePlatformDataToCachedFrame(WebCore::CachedFrame*) {
    notImplemented();
}

void FrameLoaderClientAndroid::transitionToCommittedFromCachedFrame(WebCore::CachedFrame*) {
    notImplemented();
}

void FrameLoaderClientAndroid::transitionToCommittedForNewPage() {
    ASSERT(m_frame);

#ifdef ANDROID_META_SUPPORT
    // reset metadata settings for the main frame as they are not preserved cross page
    if (m_frame == m_frame->page()->mainFrame() && m_frame->settings())
        m_frame->settings()->resetMetadataSettings();
#endif

    if (m_frame->settings() && !m_frame->settings()->usesPageCache()) {
        m_webFrame->transitionToCommitted(m_frame);
        return;
    }

    // Remember the old WebFrameView
    WebFrameView* webFrameView = static_cast<WebFrameView*> (
            m_frame->view()->platformWidget());
    Retain(webFrameView);

    // Remove the old FrameView
    m_frame->setView(NULL);

    // Create a new FrameView and associate it with the saved webFrameView
    RefPtr<FrameView> view = FrameView::create(m_frame);
    webFrameView->setView(view.get());

    Release(webFrameView);

    // Give the new FrameView to the Frame
    m_frame->setView(view);

    if (m_frame->ownerRenderer())
        m_frame->ownerRenderer()->setWidget(view.get());

    m_frame->view()->initScrollbars();

    m_webFrame->transitionToCommitted(m_frame);
}

bool FrameLoaderClientAndroid::canCachePage() const {
    return true;
}

void FrameLoaderClientAndroid::download(ResourceHandle* handle, const ResourceRequest&,
                                const ResourceRequest&, const ResourceResponse&) {
    // Get the C++ side of the load listener and tell it to handle the download
    WebCoreResourceLoader* loader = handle->getInternal()->m_loader;
    loader->downloadFile();
}

WTF::PassRefPtr<WebCore::Frame> FrameLoaderClientAndroid::createFrame(const KURL& url, const String& name,
                        HTMLFrameOwnerElement* ownerElement, const String& referrer,
                        bool allowsScrolling, int marginWidth, int marginHeight)
{
    Frame* parent = ownerElement->document()->frame();
    FrameLoaderClientAndroid* loaderC = new FrameLoaderClientAndroid(m_webFrame);
    RefPtr<Frame> pFrame = Frame::create(parent->page(), ownerElement, loaderC);
    Frame* newFrame = pFrame.get();
    loaderC->setFrame(newFrame);
    // Append the subframe to the parent and set the name of the subframe. The name must be set after
    // appending the child so that the name becomes unique.
    parent->tree()->appendChild(newFrame);
    newFrame->tree()->setName(name);
    // Create a new FrameView and WebFrameView for the child frame to draw into.
    RefPtr<FrameView> frameView = FrameView::create(newFrame);
    WebFrameView* webFrameView = new WebFrameView(frameView.get(),
            WebViewCore::getWebViewCore(parent->view()));
    // frameView Retains webFrameView, so call Release for webFrameView
    Release(webFrameView);
    // Attach the frameView to the newFrame.
    newFrame->setView(frameView);
    newFrame->init();
    newFrame->selection()->setFocused(true);
    LOGV("::WebCore:: createSubFrame returning %p", newFrame);

    // The creation of the frame may have run arbitrary JavaScript that removed it from the page already.
    if (!pFrame->page())
        return 0;

    parent->loader()->loadURLIntoChildFrame(url, referrer, pFrame.get());

    // onLoad may cuase the frame to be removed from the document. Allow the RefPtr to delete the child frame.
    if (!pFrame->tree()->parent())
        return NULL;

    return pFrame.release();
}

// YouTube flash url path starts with /v/
static const char slash_v_slash[] = { '/', 'v', '/' };

static bool isValidYouTubeVideo(const String& path)
{
    if (!charactersAreAllASCII(path.characters(), path.length()))
        return false;
    unsigned int len = path.length();
    if (len <= sizeof(slash_v_slash)) // check for more than just /v/
        return false;
    CString str = path.lower().utf8();
    const char* data = str.data();
    if (memcmp(data, slash_v_slash, sizeof(slash_v_slash)) != 0)
        return false;
    // Start after /v/
    for (unsigned int i = sizeof(slash_v_slash); i < len; i++) {
        char c = data[i];
        // Check for alpha-numeric characters only.
        if (WTF::isASCIIAlphanumeric(c) || c == '_' || c == '-')
            continue;
        // The url can have more parameters such as &hl=en after the video id.
        // Once we start seeing extra parameters we can return true.
        return c == '&' && i > sizeof(slash_v_slash);
    }
    return true;
}

static bool isYouTubeUrl(const KURL& url, const String& mimeType)
{
    String host = url.host();
    bool youtube = host.endsWith("youtube.com")
            || host.endsWith("youtube-nocookie.com");
    return youtube && isValidYouTubeVideo(url.path())
            && equalIgnoringCase(mimeType, "application/x-shockwave-flash");
}

WTF::PassRefPtr<Widget> FrameLoaderClientAndroid::createPlugin(
        const IntSize& size,
        HTMLPlugInElement* element,
        const KURL& url,
        const WTF::Vector<String>& names,
        const WTF::Vector<String>& values,
        const String& mimeType,
        bool loadManually) {
    // Create an iframe for youtube urls.
    if (isYouTubeUrl(url, mimeType)) {
        WTF::RefPtr<Frame> frame = createFrame(blankURL(), String(), element,
                String(), false, 0, 0);
        if (frame) {
            // grab everything after /v/
            String videoId = url.path().substring(sizeof(slash_v_slash));
            // Extract just the video id
            unsigned videoIdEnd = 0;
            for (; videoIdEnd < videoId.length(); videoIdEnd++) {
                if (videoId[videoIdEnd] == '&') {
                    videoId = videoId.left(videoIdEnd);
                    break;
                }
            }
            AssetManager* am = globalAssetManager();
            Asset* a = am->open("webkit/youtube.html",
                    Asset::ACCESS_BUFFER);
            if (!a)
                return NULL;
            String s = String((const char*)a->getBuffer(false), a->getLength());
            s = s.replace("VIDEO_ID", videoId);
            delete a;
            loadDataIntoFrame(frame.get(),
                    KURL("file:///android_asset/webkit/"), String(), s);
            // Transfer ownership to a local refptr.
            WTF::RefPtr<Widget> widget(frame->view());
            return widget.release();
        }
        return NULL;
    }
#ifdef ANDROID_PLUGINS
    return PluginView::create(m_frame,
                              size,
                              element,
                              url,
                              names,
                              values,
                              mimeType,
                              loadManually);
#else
    return NULL;
#endif
}

void FrameLoaderClientAndroid::redirectDataToPlugin(Widget* pluginWidget) {
    // don't support plugin yet
    notImplemented();
}

WTF::PassRefPtr<Widget> FrameLoaderClientAndroid::createJavaAppletWidget(const IntSize&, HTMLAppletElement*,
                                        const KURL& baseURL, const WTF::Vector<String>& paramNames,
                                        const WTF::Vector<String>& paramValues) {
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
            String path = url.path();
            int lastIndex = path.reverseFind('.');
            static const String image("image/");
            if (lastIndex >= 0)
            {
                String mime(path.substring(lastIndex + 1));
                mime.insert(image, 0);
                if (Image::supportsType(mime))
                    return ObjectContentImage;
            }
        }
        return ObjectContentFrame;
    }

    if (Image::supportsType(mimeType))
        return ObjectContentImage;

    if (PluginDatabase::installedPlugins()->isMIMETypeRegistered(mimeType))
        return ObjectContentOtherPlugin;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return ObjectContentFrame;

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
    m_webFrame->windowObjectCleared(m_frame);
}

void FrameLoaderClientAndroid::documentElementAvailable() {
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
        WebIconDatabase::RegisterForIconNotification(this);
    else
        WebIconDatabase::UnregisterForIconNotification(this);
}

// This is the WebIconDatabaseClient method for receiving a notification when we
// get the icon for the page.
void FrameLoaderClientAndroid::didAddIconForPageUrl(const String& pageUrl) {
    // This call must happen before dispatchDidReceiveIcon since that method
    // may register for icon notifications again since the icon data may have
    // to be read from disk.
    registerForIconNotification(false);
    KURL u(pageUrl);
    if (equalIgnoringFragmentIdentifier(u, m_frame->loader()->url())) {
        dispatchDidReceiveIcon();
    }
}

}
