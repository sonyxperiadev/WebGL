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

#ifndef FrameLoaderClientAndroid_h
#define FrameLoaderClientAndroid_h

#include "CacheBuilder.h"
#include "FrameLoaderClient.h"
#include "ResourceResponse.h"
#include "WebIconDatabase.h"

namespace WebCore {
class PluginManualLoader;
}

using namespace WebCore;

namespace android {
    class WebFrame;

    class FrameLoaderClientAndroid : public FrameLoaderClient,
            WebIconDatabaseClient {
    public:
        FrameLoaderClientAndroid(WebFrame* webframe);

        Frame* getFrame() { return m_frame; }
        static FrameLoaderClientAndroid* get(const Frame* frame);

        void setFrame(Frame* frame) { m_frame = frame; }
        WebFrame* webFrame() const { return m_webFrame; }
        
        virtual void frameLoaderDestroyed();
        
        virtual bool hasWebView() const; // mainly for assertions

        virtual void makeRepresentation(DocumentLoader*);
        virtual void forceLayout();
        virtual void forceLayoutForNonHTML();

        virtual void setCopiesOnScroll();

        virtual void detachedFromParent2();
        virtual void detachedFromParent3();

        virtual void assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&);

        virtual void dispatchWillSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse);
        virtual bool shouldUseCredentialStorage(DocumentLoader*, unsigned long identifier);
        virtual void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&);
        virtual void dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&);        
        virtual void dispatchDidReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&);
        virtual void dispatchDidReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived);
        virtual void dispatchDidFinishLoading(DocumentLoader*, unsigned long identifier);
        virtual void dispatchDidFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError&);
        virtual bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length);
        virtual void dispatchDidLoadResourceByXMLHttpRequest(unsigned long identifier, const ScriptString&);

        virtual void dispatchDidHandleOnloadEvents();
        virtual void dispatchDidReceiveServerRedirectForProvisionalLoad();
        virtual void dispatchDidCancelClientRedirect();
        virtual void dispatchWillPerformClientRedirect(const KURL&, double interval, double fireDate);
        virtual void dispatchDidChangeLocationWithinPage();
        virtual void dispatchDidPushStateWithinPage();
        virtual void dispatchDidReplaceStateWithinPage();
        virtual void dispatchDidPopStateWithinPage();
        virtual void dispatchWillClose();
        virtual void dispatchDidReceiveIcon();
        virtual void dispatchDidStartProvisionalLoad();
        virtual void dispatchDidReceiveTitle(const String& title);
        virtual void dispatchDidCommitLoad();
        virtual void dispatchDidFailProvisionalLoad(const ResourceError&);
        virtual void dispatchDidFailLoad(const ResourceError&);
        virtual void dispatchDidFinishDocumentLoad();
        virtual void dispatchDidFinishLoad();
        virtual void dispatchDidFirstLayout();
        virtual void dispatchDidFirstVisuallyNonEmptyLayout();

        virtual Frame* dispatchCreatePage();
        virtual void dispatchShow();

        virtual void dispatchDecidePolicyForMIMEType(FramePolicyFunction, const String& MIMEType, const ResourceRequest&);
        virtual void dispatchDecidePolicyForNewWindowAction(FramePolicyFunction, const NavigationAction&, const ResourceRequest&, PassRefPtr<FormState>, const String& frameName);
        virtual void dispatchDecidePolicyForNavigationAction(FramePolicyFunction, const NavigationAction&, const ResourceRequest&, PassRefPtr<FormState>);
        virtual void cancelPolicyCheck();

        virtual void dispatchUnableToImplementPolicy(const ResourceError&);

        virtual void dispatchWillSubmitForm(FramePolicyFunction, PassRefPtr<FormState>);

        virtual void dispatchDidLoadMainResource(DocumentLoader*);
        virtual void revertToProvisionalState(DocumentLoader*);
        virtual void setMainDocumentError(DocumentLoader*, const ResourceError&);

        virtual void willChangeEstimatedProgress();
        virtual void didChangeEstimatedProgress();
        virtual void postProgressStartedNotification();
        virtual void postProgressEstimateChangedNotification();
        virtual void postProgressFinishedNotification();
        
        virtual void setMainFrameDocumentReady(bool);

        virtual void startDownload(const ResourceRequest&);

        virtual void willChangeTitle(DocumentLoader*);
        virtual void didChangeTitle(DocumentLoader*);

        virtual void committedLoad(DocumentLoader*, const char*, int);
        virtual void finishedLoading(DocumentLoader*);
        
        virtual void updateGlobalHistory();
        virtual void updateGlobalHistoryRedirectLinks();

        virtual bool shouldGoToHistoryItem(HistoryItem*) const;

        virtual void didDisplayInsecureContent();
        virtual void didRunInsecureContent(SecurityOrigin*);

        virtual void dispatchDidAddBackForwardItem(HistoryItem*) const;
        virtual void dispatchDidRemoveBackForwardItem(HistoryItem*) const;
        virtual void dispatchDidChangeBackForwardIndex() const;

        virtual ResourceError cancelledError(const ResourceRequest&);
        virtual ResourceError blockedError(const ResourceRequest&);
        virtual ResourceError cannotShowURLError(const ResourceRequest&);
        virtual ResourceError interruptForPolicyChangeError(const ResourceRequest&);

        virtual ResourceError cannotShowMIMETypeError(const ResourceResponse&);
        virtual ResourceError fileDoesNotExistError(const ResourceResponse&);
        virtual ResourceError pluginWillHandleLoadError(const ResourceResponse&);

        virtual bool shouldFallBack(const ResourceError&);

        virtual bool canHandleRequest(const ResourceRequest&) const;
        virtual bool canShowMIMEType(const String& MIMEType) const;
        virtual bool representationExistsForURLScheme(const String& URLScheme) const;
        virtual String generatedMIMETypeForURLScheme(const String& URLScheme) const;

        virtual void frameLoadCompleted();
        virtual void saveViewStateToItem(HistoryItem*);
        virtual void restoreViewState();
        virtual void provisionalLoadStarted();
        virtual void didFinishLoad();
        virtual void prepareForDataSourceReplacement();

        virtual PassRefPtr<DocumentLoader> createDocumentLoader(const ResourceRequest&, const SubstituteData&);
        virtual void setTitle(const String& title, const KURL&);

        virtual String userAgent(const KURL&);
        
        virtual void savePlatformDataToCachedFrame(WebCore::CachedFrame*);
        virtual void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*);
        virtual void transitionToCommittedForNewPage();

        virtual bool canCachePage() const;
        virtual void download(ResourceHandle*, const ResourceRequest&, const ResourceRequest&, const ResourceResponse&);

        virtual WTF::PassRefPtr<Frame> createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                   const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight);
       virtual WTF::PassRefPtr<Widget> createPlugin(const IntSize&, HTMLPlugInElement*, const KURL&,
               const WTF::Vector<String>&, const WTF::Vector<String>&,
               const String&, bool loadManually);
        virtual void redirectDataToPlugin(Widget* pluginWidget);

        virtual WTF::PassRefPtr<Widget> createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const KURL& baseURL, const WTF::Vector<String>& paramNames, const WTF::Vector<String>& paramValues);

        virtual ObjectContentType objectContentType(const KURL& url, const String& mimeType);
        virtual String overrideMediaType() const;

        virtual void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld*);
        virtual void documentElementAvailable();
        virtual void didPerformFirstNavigation() const;

#if USE(V8)
        // TODO(benm): Implement
        virtual void didCreateScriptContextForFrame() { }
        virtual void didDestroyScriptContextForFrame() { }
        virtual void didCreateIsolatedScriptContext() { }
#endif
        
        virtual void registerForIconNotification(bool listen = true);

        virtual void dispatchDidReceiveTouchIconURL(const String& url, bool precomposed);

        // WebIconDatabaseClient api
        virtual void didAddIconForPageUrl(const String& pageUrl);
        
        // FIXME: this doesn't really go here, but it's better than Frame
        CacheBuilder& getCacheBuilder() { return m_cacheBuilder; }

        void enableOnDemandPlugins() { m_onDemandPluginsEnabled = true; }
    private:
        CacheBuilder        m_cacheBuilder;
        Frame*              m_frame;
        WebFrame*           m_webFrame;
        PluginManualLoader* m_manualLoader;
        bool                m_hasSentResponseToPlugin;
        bool                m_onDemandPluginsEnabled;

        enum ResourceErrors {
            InternalErrorCancelled = -99,
            InternalErrorCannotShowUrl,
            InternalErrorInterrupted,
            InternalErrorCannotShowMimeType,
            InternalErrorFileDoesNotExist,
            InternalErrorPluginWillHandleLoadError,
            InternalErrorLast
        };

        /* XXX: These must match android.net.http.EventHandler */
        enum EventHandlerErrors {
            Error                      = -1,
            ErrorLookup                = -2,
            ErrorUnsupportedAuthScheme = -3,
            ErrorAuth                  = -4,
            ErrorProxyAuth             = -5,
            ErrorConnect               = -6,
            ErrorIO                    = -7,
            ErrorTimeout               = -8,
            ErrorRedirectLoop          = -9,
            ErrorUnsupportedScheme     = -10,
            ErrorFailedSslHandshake    = -11,
            ErrorBadUrl                = -12,
            ErrorFile                  = -13,
            ErrorFileNotFound          = -14,
            ErrorTooManyRequests       = -15
        };
        friend class CacheBuilder;
    };

}

#endif
