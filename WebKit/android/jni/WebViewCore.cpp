/*
 * Copyright 2006, The Android Open Source Project
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

#define LOG_TAG "webcoreglue"

#include <config.h>
#include "WebViewCore.h"

#include "AtomicString.h"
#include "CachedNode.h"
#include "CachedRoot.h"
#include "ChromeClientAndroid.h"
#include "Color.h"
#include "DatabaseTracker.h"
#include "Document.h"
#include "DOMWindow.h"
#include "Element.h"
#include "Editor.h"
#include "EditorClientAndroid.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FocusController.h"
#include "Font.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClientAndroid.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "Geolocation.h"
#include "GraphicsContext.h"
#include "GraphicsJNI.h"
#include "HitTestResult.h"
#include "HTMLAnchorElement.h"
#include "HTMLAreaElement.h"
#include "HTMLElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "InlineTextBox.h"
#include <JNIHelp.h>
#include "KeyboardCodes.h"
#include "Navigator.h"
#include "Node.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformString.h"
#include "PluginWidgetAndroid.h"
#include "PluginView.h"
#include "Position.h"
#include "ProgressTracker.h"
#include "RenderBox.h"
#include "RenderLayer.h"
#include "RenderPart.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderThemeAndroid.h"
#include "RenderView.h"
#include "ResourceRequest.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SkANP.h"
#include "SkTemplates.h"
#include "SkTypes.h"
#include "SkCanvas.h"
#include "SkPicture.h"
#include "SkUtils.h"
#include "StringImpl.h"
#include "Text.h"
#include "TypingCommand.h"
#include "WebCoreFrameBridge.h"
#include "WebFrameView.h"
#include "HistoryItem.h"
#include "android_graphics.h"
#include <ui/KeycodeLabels.h>
#include "jni_utility.h"
#include <wtf/CurrentTime.h>

#if USE(V8)
#include "CString.h"
#include "ScriptController.h"
#endif

#if DEBUG_NAV_UI
#include "SkTime.h"
#endif

#if ENABLE(TOUCH_EVENTS) // Android
#include "PlatformTouchEvent.h"
#endif

#ifdef ANDROID_DOM_LOGGING
#include "AndroidLog.h"
#include "RenderTreeAsText.h"
#include "CString.h"

FILE* gDomTreeFile = 0;
FILE* gRenderTreeFile = 0;
#endif

#ifdef ANDROID_INSTRUMENT
#include "TimeCounter.h"
#endif

/*  We pass this flag when recording the actual content, so that we don't spend
    time actually regionizing complex path clips, when all we really want to do
    is record them.
 */
#define PICT_RECORD_FLAGS   SkPicture::kUsePathBoundsForClip_RecordingFlag

////////////////////////////////////////////////////////////////////////////////////////////////

namespace android {

bool WebViewCore::s_isPaused = false;

// ----------------------------------------------------------------------------

#define GET_NATIVE_VIEW(env, obj) ((WebViewCore*)env->GetIntField(obj, gWebViewCoreFields.m_nativeClass))

// Field ids for WebViewCore
struct WebViewCoreFields {
    jfieldID    m_nativeClass;
    jfieldID    m_viewportWidth;
    jfieldID    m_viewportHeight;
    jfieldID    m_viewportInitialScale;
    jfieldID    m_viewportMinimumScale;
    jfieldID    m_viewportMaximumScale;
    jfieldID    m_viewportUserScalable;
    jfieldID    m_viewportDensityDpi;
    jfieldID    m_webView;
} gWebViewCoreFields;

// ----------------------------------------------------------------------------

struct WebViewCore::JavaGlue {
    jobject     m_obj;
    jmethodID   m_spawnScrollTo;
    jmethodID   m_scrollTo;
    jmethodID   m_scrollBy;
    jmethodID   m_contentDraw;
    jmethodID   m_requestListBox;
    jmethodID   m_requestSingleListBox;
    jmethodID   m_jsAlert;
    jmethodID   m_jsConfirm;
    jmethodID   m_jsPrompt;
    jmethodID   m_jsUnload;
    jmethodID   m_jsInterrupt;
    jmethodID   m_didFirstLayout;
    jmethodID   m_updateViewport;
    jmethodID   m_sendNotifyProgressFinished;
    jmethodID   m_sendViewInvalidate;
    jmethodID   m_updateTextfield;
    jmethodID   m_updateTextSelection;
    jmethodID   m_clearTextEntry;
    jmethodID   m_restoreScale;
    jmethodID   m_restoreScreenWidthScale;
    jmethodID   m_needTouchEvents;
    jmethodID   m_requestKeyboard;
    jmethodID   m_exceededDatabaseQuota;
    jmethodID   m_reachedMaxAppCacheSize;
    jmethodID   m_populateVisitedLinks;
    jmethodID   m_geolocationPermissionsShowPrompt;
    jmethodID   m_geolocationPermissionsHidePrompt;
    jmethodID   m_addMessageToConsole;
    jmethodID   m_startFullScreenPluginActivity;
    jmethodID   m_createSurface;
    jmethodID   m_destroySurface;
    jmethodID   m_showRect;
    AutoJObject object(JNIEnv* env) {
        return getRealObject(env, m_obj);
    }
};

/*
 * WebViewCore Implementation
 */

static jmethodID GetJMethod(JNIEnv* env, jclass clazz, const char name[], const char signature[])
{
    jmethodID m = env->GetMethodID(clazz, name, signature);
    LOG_ASSERT(m, "Could not find method %s", name);
    return m;
}

Mutex WebViewCore::gFrameCacheMutex;
Mutex WebViewCore::gButtonMutex;
Mutex WebViewCore::gCursorBoundsMutex;
Mutex WebViewCore::m_contentMutex;

WebViewCore::WebViewCore(JNIEnv* env, jobject javaWebViewCore, WebCore::Frame* mainframe)
        : m_pluginInvalTimer(this, &WebViewCore::pluginInvalTimerFired)
{
    m_mainFrame = mainframe;

    m_popupReply = 0;
    m_moveGeneration = 0;
    m_generation = 0;
    m_lastGeneration = 0;
    m_touchGeneration = 0;
    m_blockTextfieldUpdates = false;
    // just initial values. These should be set by client
    m_maxXScroll = 320/4;
    m_maxYScroll = 240/4;
    m_textGeneration = 0;
    m_screenWidth = 320;
    m_scale = 1;
    m_screenWidthScale = 1;

    LOG_ASSERT(m_mainFrame, "Uh oh, somehow a frameview was made without an initial frame!");

    jclass clazz = env->GetObjectClass(javaWebViewCore);
    m_javaGlue = new JavaGlue;
    m_javaGlue->m_obj = adoptGlobalRef(env, javaWebViewCore);
    m_javaGlue->m_spawnScrollTo = GetJMethod(env, clazz, "contentSpawnScrollTo", "(II)V");
    m_javaGlue->m_scrollTo = GetJMethod(env, clazz, "contentScrollTo", "(II)V");
    m_javaGlue->m_scrollBy = GetJMethod(env, clazz, "contentScrollBy", "(IIZ)V");
    m_javaGlue->m_contentDraw = GetJMethod(env, clazz, "contentDraw", "()V");
    m_javaGlue->m_requestListBox = GetJMethod(env, clazz, "requestListBox", "([Ljava/lang/String;[Z[I)V");
    m_javaGlue->m_requestSingleListBox = GetJMethod(env, clazz, "requestListBox", "([Ljava/lang/String;[ZI)V");
    m_javaGlue->m_jsAlert = GetJMethod(env, clazz, "jsAlert", "(Ljava/lang/String;Ljava/lang/String;)V");
    m_javaGlue->m_jsConfirm = GetJMethod(env, clazz, "jsConfirm", "(Ljava/lang/String;Ljava/lang/String;)Z");
    m_javaGlue->m_jsPrompt = GetJMethod(env, clazz, "jsPrompt", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    m_javaGlue->m_jsUnload = GetJMethod(env, clazz, "jsUnload", "(Ljava/lang/String;Ljava/lang/String;)Z");
    m_javaGlue->m_jsInterrupt = GetJMethod(env, clazz, "jsInterrupt", "()Z");
    m_javaGlue->m_didFirstLayout = GetJMethod(env, clazz, "didFirstLayout", "(Z)V");
    m_javaGlue->m_updateViewport = GetJMethod(env, clazz, "updateViewport", "()V");
    m_javaGlue->m_sendNotifyProgressFinished = GetJMethod(env, clazz, "sendNotifyProgressFinished", "()V");
    m_javaGlue->m_sendViewInvalidate = GetJMethod(env, clazz, "sendViewInvalidate", "(IIII)V");
    m_javaGlue->m_updateTextfield = GetJMethod(env, clazz, "updateTextfield", "(IZLjava/lang/String;I)V");
    m_javaGlue->m_updateTextSelection = GetJMethod(env, clazz, "updateTextSelection", "(IIII)V");
    m_javaGlue->m_clearTextEntry = GetJMethod(env, clazz, "clearTextEntry", "()V");
    m_javaGlue->m_restoreScale = GetJMethod(env, clazz, "restoreScale", "(I)V");
    m_javaGlue->m_restoreScreenWidthScale = GetJMethod(env, clazz, "restoreScreenWidthScale", "(I)V");
    m_javaGlue->m_needTouchEvents = GetJMethod(env, clazz, "needTouchEvents", "(Z)V");
    m_javaGlue->m_requestKeyboard = GetJMethod(env, clazz, "requestKeyboard", "(Z)V");
    m_javaGlue->m_exceededDatabaseQuota = GetJMethod(env, clazz, "exceededDatabaseQuota", "(Ljava/lang/String;Ljava/lang/String;JJ)V");
    m_javaGlue->m_reachedMaxAppCacheSize = GetJMethod(env, clazz, "reachedMaxAppCacheSize", "(J)V");
    m_javaGlue->m_populateVisitedLinks = GetJMethod(env, clazz, "populateVisitedLinks", "()V");
    m_javaGlue->m_geolocationPermissionsShowPrompt = GetJMethod(env, clazz, "geolocationPermissionsShowPrompt", "(Ljava/lang/String;)V");
    m_javaGlue->m_geolocationPermissionsHidePrompt = GetJMethod(env, clazz, "geolocationPermissionsHidePrompt", "()V");
    m_javaGlue->m_addMessageToConsole = GetJMethod(env, clazz, "addMessageToConsole", "(Ljava/lang/String;ILjava/lang/String;)V");
    m_javaGlue->m_startFullScreenPluginActivity = GetJMethod(env, clazz, "startFullScreenPluginActivity", "(Ljava/lang/String;Ljava/lang/String;I)V");
    m_javaGlue->m_createSurface = GetJMethod(env, clazz, "createSurface", "(Ljava/lang/String;Ljava/lang/String;IIIII)Landroid/webkit/ViewManager$ChildView;");
    m_javaGlue->m_destroySurface = GetJMethod(env, clazz, "destroySurface", "(Landroid/webkit/ViewManager$ChildView;)V");
    m_javaGlue->m_showRect = GetJMethod(env, clazz, "showRect", "(IIIIIIFFFF)V");

    env->SetIntField(javaWebViewCore, gWebViewCoreFields.m_nativeClass, (jint)this);

    m_scrollOffsetX = m_scrollOffsetY = 0;

    PageGroup::setShouldTrackVisitedLinks(true);

    reset(true);
}

WebViewCore::~WebViewCore()
{
    // Release the focused view
    Release(m_popupReply);

    if (m_javaGlue->m_obj) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        env->DeleteGlobalRef(m_javaGlue->m_obj);
        m_javaGlue->m_obj = 0;
    }
    delete m_javaGlue;
    delete m_frameCacheKit;
    delete m_navPictureKit;
}

WebViewCore* WebViewCore::getWebViewCore(const WebCore::FrameView* view)
{
    return getWebViewCore(static_cast<const WebCore::ScrollView*>(view));
}

WebViewCore* WebViewCore::getWebViewCore(const WebCore::ScrollView* view)
{
    WebFrameView* webFrameView = static_cast<WebFrameView*>(view->platformWidget());
    if (!webFrameView)
        return 0;
    return webFrameView->webViewCore();
}

void WebViewCore::reset(bool fromConstructor)
{
    DBG_SET_LOG("");
    if (fromConstructor) {
        m_frameCacheKit = 0;
        m_navPictureKit = 0;
    } else {
        gFrameCacheMutex.lock();
        delete m_frameCacheKit;
        delete m_navPictureKit;
        m_frameCacheKit = 0;
        m_navPictureKit = 0;
        gFrameCacheMutex.unlock();
    }

    m_lastFocused = 0;
    m_lastFocusedBounds = WebCore::IntRect(0,0,0,0);
    m_lastFocusedSelStart = 0;
    m_lastFocusedSelEnd = 0;
    m_lastMoveGeneration = 0;
    clearContent();
    m_updatedFrameCache = true;
    m_frameCacheOutOfDate = true;
    m_skipContentDraw = false;
    m_findIsUp = false;
    m_domtree_version = 0;
    m_check_domtree_version = true;
    m_progressDone = false;
    m_hasCursorBounds = false;

    m_scrollOffsetX = 0;
    m_scrollOffsetY = 0;
    m_screenWidth = 0;
    m_screenHeight = 0;
    m_groupForVisitedLinks = NULL;
}

static bool layoutIfNeededRecursive(WebCore::Frame* f)
{
    if (!f)
        return true;

    WebCore::FrameView* v = f->view();
    if (!v)
        return true;

    if (v->needsLayout())
        v->layout(f->tree()->parent());

    WebCore::Frame* child = f->tree()->firstChild();
    bool success = true;
    while (child) {
        success &= layoutIfNeededRecursive(child);
        child = child->tree()->nextSibling();
    }

    return success && !v->needsLayout();
}

CacheBuilder& WebViewCore::cacheBuilder()
{
    return FrameLoaderClientAndroid::get(m_mainFrame)->getCacheBuilder();
}

WebCore::Node* WebViewCore::currentFocus()
{
    return cacheBuilder().currentFocus();
}

void WebViewCore::recordPicture(SkPicture* picture)
{
    // if there is no document yet, just return
    if (!m_mainFrame->document()) {
        DBG_NAV_LOG("no document");
        return;
    }
    // Call layout to ensure that the contentWidth and contentHeight are correct
    if (!layoutIfNeededRecursive(m_mainFrame)) {
        DBG_NAV_LOG("layout failed");
        return;
    }
    // draw into the picture's recording canvas
    WebCore::FrameView* view = m_mainFrame->view();
    DBG_NAV_LOGD("view=(w=%d,h=%d)", view->contentsWidth(),
        view->contentsHeight());
    SkAutoPictureRecord arp(picture, view->contentsWidth(),
                            view->contentsHeight(), PICT_RECORD_FLAGS);
    SkAutoMemoryUsageProbe mup(__FUNCTION__);

    // Copy m_buttons so we can pass it to our graphics context.
    gButtonMutex.lock();
    WTF::Vector<Container> buttons(m_buttons);
    gButtonMutex.unlock();

    WebCore::PlatformGraphicsContext pgc(arp.getRecordingCanvas(), &buttons);
    WebCore::GraphicsContext gc(&pgc);
    view->platformWidget()->draw(&gc, WebCore::IntRect(0, 0, INT_MAX, INT_MAX));

    gButtonMutex.lock();
    updateButtonList(&buttons);
    gButtonMutex.unlock();
}

void WebViewCore::recordPictureSet(PictureSet* content)
{
    // if there is no document yet, just return
    if (!m_mainFrame->document()) {
        DBG_SET_LOG("!m_mainFrame->document()");
        return;
    }
    if (m_addInval.isEmpty()) {
        DBG_SET_LOG("m_addInval.isEmpty()");
        return;
    }
    // Call layout to ensure that the contentWidth and contentHeight are correct
    // it's fine for layout to gather invalidates, but defeat sending a message
    // back to java to call webkitDraw, since we're already in the middle of
    // doing that
    m_skipContentDraw = true;
    bool success = layoutIfNeededRecursive(m_mainFrame);
    m_skipContentDraw = false;

    // We may be mid-layout and thus cannot draw.
    if (!success)
        return;

    {   // collect WebViewCoreRecordTimeCounter after layoutIfNeededRecursive
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreRecordTimeCounter);
#endif

    // if the webkit page dimensions changed, discard the pictureset and redraw.
    WebCore::FrameView* view = m_mainFrame->view();
    int width = view->contentsWidth();
    int height = view->contentsHeight();

    // Use the contents width and height as a starting point.
    SkIRect contentRect;
    contentRect.set(0, 0, width, height);
    SkIRect total(contentRect);

    // Traverse all the frames and add their sizes if they are in the visible
    // rectangle.
    for (WebCore::Frame* frame = m_mainFrame->tree()->traverseNext(); frame;
            frame = frame->tree()->traverseNext()) {
        // If the frame doesn't have an owner then it is the top frame and the
        // view size is the frame size.
        WebCore::RenderPart* owner = frame->ownerRenderer();
        if (owner && owner->style()->visibility() == VISIBLE) {
            int x = owner->x();
            int y = owner->y();

            // Traverse the tree up to the parent to find the absolute position
            // of this frame.
            WebCore::Frame* parent = frame->tree()->parent();
            while (parent) {
                WebCore::RenderPart* parentOwner = parent->ownerRenderer();
                if (parentOwner) {
                    x += parentOwner->x();
                    y += parentOwner->y();
                }
                parent = parent->tree()->parent();
            }
            // Use the owner dimensions so that padding and border are
            // included.
            int right = x + owner->width();
            int bottom = y + owner->height();
            SkIRect frameRect = {x, y, right, bottom};
            // Ignore a width or height that is smaller than 1. Some iframes
            // have small dimensions in order to be hidden. The iframe
            // expansion code does not expand in that case so we should ignore
            // them here.
            if (frameRect.width() > 1 && frameRect.height() > 1
                    && SkIRect::Intersects(total, frameRect))
                total.join(x, y, right, bottom);
        }
    }

    // If the new total is larger than the content, resize the view to include
    // all the content.
    if (!contentRect.contains(total)) {
        // Resize the view to change the overflow clip.
        view->resize(total.fRight, total.fBottom);

        // We have to force a layout in order for the clip to change.
        m_mainFrame->contentRenderer()->setNeedsLayoutAndPrefWidthsRecalc();
        view->forceLayout();

        // Relayout similar to above
        m_skipContentDraw = true;
        bool success = layoutIfNeededRecursive(m_mainFrame);
        m_skipContentDraw = false;
        if (!success)
            return;

        // Set the computed content width
        width = view->contentsWidth();
        height = view->contentsHeight();
    }

    content->checkDimensions(width, height, &m_addInval);

    // The inval region may replace existing pictures. The existing pictures
    // may have already been split into pieces. If reuseSubdivided() returns
    // true, the split pieces are the last entries in the picture already. They
    // are marked as invalid, and are rebuilt by rebuildPictureSet().

    // If the new region doesn't match a set of split pieces, add it to the end.
    if (!content->reuseSubdivided(m_addInval)) {
        const SkIRect& inval = m_addInval.getBounds();
        SkPicture* picture = rebuildPicture(inval);
        DBG_SET_LOGD("{%d,%d,w=%d,h=%d}", inval.fLeft,
            inval.fTop, inval.width(), inval.height());
        content->add(m_addInval, picture, 0, false);
        picture->safeUnref();
    }
    // Remove any pictures already in the set that are obscured by the new one,
    // and check to see if any already split pieces need to be redrawn.
    if (content->build())
        rebuildPictureSet(content);
    } // WebViewCoreRecordTimeCounter
    WebCore::Node* oldFocusNode = currentFocus();
    m_frameCacheOutOfDate = true;
    WebCore::IntRect oldBounds;
    int oldSelStart = 0;
    int oldSelEnd = 0;
    if (oldFocusNode) {
        oldBounds = oldFocusNode->getRect();
        RenderObject* renderer = oldFocusNode->renderer();
        if (renderer && (renderer->isTextArea() || renderer->isTextField())) {
            WebCore::RenderTextControl* rtc =
                static_cast<WebCore::RenderTextControl*>(renderer);
            oldSelStart = rtc->selectionStart();
            oldSelEnd = rtc->selectionEnd();
        }
    } else
        oldBounds = WebCore::IntRect(0,0,0,0);
    unsigned latestVersion = 0;
    if (m_check_domtree_version) {
        // as domTreeVersion only increment, we can just check the sum to see
        // whether we need to update the frame cache
        for (Frame* frame = m_mainFrame; frame; frame = frame->tree()->traverseNext()) {
            latestVersion += frame->document()->domTreeVersion();
        }
    }
    DBG_NAV_LOGD("m_lastFocused=%p oldFocusNode=%p"
        " m_lastFocusedBounds={%d,%d,%d,%d} oldBounds={%d,%d,%d,%d}"
        " m_lastFocusedSelection={%d,%d} oldSelection={%d,%d}"
        " m_check_domtree_version=%s latestVersion=%d m_domtree_version=%d",
        m_lastFocused, oldFocusNode,
        m_lastFocusedBounds.x(), m_lastFocusedBounds.y(),
        m_lastFocusedBounds.width(), m_lastFocusedBounds.height(),
        oldBounds.x(), oldBounds.y(), oldBounds.width(), oldBounds.height(),
        m_lastFocusedSelStart, m_lastFocusedSelEnd, oldSelStart, oldSelEnd,
        m_check_domtree_version ? "true" : "false",
        latestVersion, m_domtree_version);
    if (m_lastFocused == oldFocusNode && m_lastFocusedBounds == oldBounds
            && m_lastFocusedSelStart == oldSelStart
            && m_lastFocusedSelEnd == oldSelEnd
            && !m_findIsUp
            && (!m_check_domtree_version || latestVersion == m_domtree_version))
    {
        return;
    }
    m_lastFocused = oldFocusNode;
    m_lastFocusedBounds = oldBounds;
    m_lastFocusedSelStart = oldSelStart;
    m_lastFocusedSelEnd = oldSelEnd;
    m_domtree_version = latestVersion;
    DBG_NAV_LOG("call updateFrameCache");
    updateFrameCache();
}

void WebViewCore::updateButtonList(WTF::Vector<Container>* buttons)
{
    // All the entries in buttons are either updates of previous entries in
    // m_buttons or they need to be added to it.
    Container* end = buttons->end();
    for (Container* updatedContainer = buttons->begin();
            updatedContainer != end; updatedContainer++) {
        bool updated = false;
        // Search for a previous entry that references the same node as our new
        // data
        Container* lastPossibleMatch = m_buttons.end();
        for (Container* possibleMatch = m_buttons.begin();
                possibleMatch != lastPossibleMatch; possibleMatch++) {
            if (updatedContainer->matches(possibleMatch->node())) {
                // Update our record, and skip to the next one.
                possibleMatch->setRect(updatedContainer->rect());
                updated = true;
                break;
            }
        }
        if (!updated) {
            // This is a brand new button, so append it to m_buttons
            m_buttons.append(*updatedContainer);
        }
    }
    size_t i = 0;
    // count will decrease each time one is removed, so check count each time.
    while (i < m_buttons.size()) {
        if (m_buttons[i].canBeRemoved()) {
            m_buttons[i] = m_buttons.last();
            m_buttons.removeLast();
        } else {
            i++;
        }
    }
}

void WebViewCore::clearContent()
{
    DBG_SET_LOG("");
    m_contentMutex.lock();
    m_content.clear();
    m_contentMutex.unlock();
    m_addInval.setEmpty();
    m_rebuildInval.setEmpty();
}

void WebViewCore::copyContentToPicture(SkPicture* picture)
{
    DBG_SET_LOG("start");
    m_contentMutex.lock();
    PictureSet copyContent = PictureSet(m_content);
    m_contentMutex.unlock();

    int w = copyContent.width();
    int h = copyContent.height();
    copyContent.draw(picture->beginRecording(w, h, PICT_RECORD_FLAGS));
    picture->endRecording();
    DBG_SET_LOG("end");
}

bool WebViewCore::drawContent(SkCanvas* canvas, SkColor color)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewUIDrawTimeCounter);
#endif
    DBG_SET_LOG("start");
    m_contentMutex.lock();
    PictureSet copyContent = PictureSet(m_content);
    m_contentMutex.unlock();
    int sc = canvas->save(SkCanvas::kClip_SaveFlag);
    SkRect clip;
    clip.set(0, 0, copyContent.width(), copyContent.height());
    canvas->clipRect(clip, SkRegion::kDifference_Op);
    canvas->drawColor(color);
    canvas->restoreToCount(sc);
    bool tookTooLong = copyContent.draw(canvas);
    m_contentMutex.lock();
    m_content.setDrawTimes(copyContent);
    m_contentMutex.unlock();
    DBG_SET_LOG("end");
    return tookTooLong;
}

bool WebViewCore::pictureReady()
{
    bool done;
    m_contentMutex.lock();
    PictureSet copyContent = PictureSet(m_content);
    done = m_progressDone;
    m_contentMutex.unlock();
    DBG_NAV_LOGD("done=%s empty=%s", done ? "true" : "false",
        copyContent.isEmpty() ? "true" : "false");
    return done || !copyContent.isEmpty();
}

SkPicture* WebViewCore::rebuildPicture(const SkIRect& inval)
{
    WebCore::FrameView* view = m_mainFrame->view();
    int width = view->contentsWidth();
    int height = view->contentsHeight();
    SkPicture* picture = new SkPicture();
    SkAutoPictureRecord arp(picture, width, height);
    SkAutoMemoryUsageProbe mup(__FUNCTION__);
    SkCanvas* recordingCanvas = arp.getRecordingCanvas();

    gButtonMutex.lock();
    WTF::Vector<Container> buttons(m_buttons);
    gButtonMutex.unlock();

    WebCore::PlatformGraphicsContext pgc(recordingCanvas, &buttons);
    WebCore::GraphicsContext gc(&pgc);
    recordingCanvas->translate(-inval.fLeft, -inval.fTop);
    recordingCanvas->save();
    view->platformWidget()->draw(&gc, WebCore::IntRect(inval.fLeft,
        inval.fTop, inval.width(), inval.height()));
    m_rebuildInval.op(inval, SkRegion::kUnion_Op);
    DBG_SET_LOGD("m_rebuildInval={%d,%d,r=%d,b=%d}",
        m_rebuildInval.getBounds().fLeft, m_rebuildInval.getBounds().fTop,
        m_rebuildInval.getBounds().fRight, m_rebuildInval.getBounds().fBottom);

    gButtonMutex.lock();
    updateButtonList(&buttons);
    gButtonMutex.unlock();

    return picture;
}

void WebViewCore::rebuildPictureSet(PictureSet* pictureSet)
{
    WebCore::FrameView* view = m_mainFrame->view();
    size_t size = pictureSet->size();
    for (size_t index = 0; index < size; index++) {
        if (pictureSet->upToDate(index))
            continue;
        const SkIRect& inval = pictureSet->bounds(index);
        DBG_SET_LOGD("pictSet=%p [%d] {%d,%d,w=%d,h=%d}", pictureSet, index,
            inval.fLeft, inval.fTop, inval.width(), inval.height());
        pictureSet->setPicture(index, rebuildPicture(inval));
    }
    pictureSet->validate(__FUNCTION__);
}

bool WebViewCore::recordContent(SkRegion* region, SkIPoint* point)
{
    DBG_SET_LOG("start");
    float progress = (float) m_mainFrame->page()->progress()->estimatedProgress();
    m_contentMutex.lock();
    PictureSet contentCopy(m_content);
    m_progressDone = progress <= 0.0f || progress >= 1.0f;
    m_contentMutex.unlock();
    recordPictureSet(&contentCopy);
    if (!m_progressDone && contentCopy.isEmpty()) {
        DBG_SET_LOGD("empty (progress=%g)", progress);
        return false;
    }
    region->set(m_addInval);
    m_addInval.setEmpty();
    region->op(m_rebuildInval, SkRegion::kUnion_Op);
    m_rebuildInval.setEmpty();
    m_contentMutex.lock();
    contentCopy.setDrawTimes(m_content);
    m_content.set(contentCopy);
    point->fX = m_content.width();
    point->fY = m_content.height();
    m_contentMutex.unlock();
    DBG_SET_LOGD("region={%d,%d,r=%d,b=%d}", region->getBounds().fLeft,
        region->getBounds().fTop, region->getBounds().fRight,
        region->getBounds().fBottom);
    DBG_SET_LOG("end");
    return true;
}

void WebViewCore::splitContent()
{
    bool layoutSuceeded = layoutIfNeededRecursive(m_mainFrame);
    LOG_ASSERT(layoutSuceeded, "Can never be called recursively");
    PictureSet tempPictureSet;
    m_contentMutex.lock();
    m_content.split(&tempPictureSet);
    m_contentMutex.unlock();
    rebuildPictureSet(&tempPictureSet);
    m_contentMutex.lock();
    m_content.set(tempPictureSet);
    m_contentMutex.unlock();
}

void WebViewCore::scrollTo(int x, int y, bool animate)
{
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");

//    LOGD("WebViewCore::scrollTo(%d %d)\n", x, y);

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), animate ? m_javaGlue->m_spawnScrollTo : m_javaGlue->m_scrollTo, x, y);
    checkException(env);
}

void WebViewCore::sendNotifyProgressFinished()
{
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), m_javaGlue->m_sendNotifyProgressFinished);
    checkException(env);
}

void WebViewCore::viewInvalidate(const WebCore::IntRect& rect)
{
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(),
                        m_javaGlue->m_sendViewInvalidate,
                        rect.x(), rect.y(), rect.right(), rect.bottom());
    checkException(env);
}

void WebViewCore::scrollBy(int dx, int dy, bool animate)
{
    if (!(dx | dy))
        return;
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), m_javaGlue->m_scrollBy,
        dx, dy, animate);
    checkException(env);
}

void WebViewCore::contentDraw()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), m_javaGlue->m_contentDraw);
    checkException(env);
}

void WebViewCore::contentInvalidate(const WebCore::IntRect &r)
{
    DBG_SET_LOGD("rect={%d,%d,w=%d,h=%d}", r.x(), r.y(), r.width(), r.height());
    SkIRect rect(r);
    if (!rect.intersect(0, 0, INT_MAX, INT_MAX))
        return;
    m_addInval.op(rect, SkRegion::kUnion_Op);
    DBG_SET_LOGD("m_addInval={%d,%d,r=%d,b=%d}",
        m_addInval.getBounds().fLeft, m_addInval.getBounds().fTop,
        m_addInval.getBounds().fRight, m_addInval.getBounds().fBottom);
    if (!m_skipContentDraw)
        contentDraw();
}

void WebViewCore::offInvalidate(const WebCore::IntRect &r)
{
    // FIXME: these invalidates are offscreen, and can be throttled or
    // deferred until the area is visible. For now, treat them as
    // regular invals so that drawing happens (inefficiently) for now.
    contentInvalidate(r);
}

static int pin_pos(int x, int width, int targetWidth)
{
    if (x + width > targetWidth)
        x = targetWidth - width;
    if (x < 0)
        x = 0;
    return x;
}

void WebViewCore::didFirstLayout()
{
    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__);
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");

    WebCore::FrameLoader* loader = m_mainFrame->loader();
    const WebCore::KURL& url = loader->url();
    if (url.isEmpty())
        return;
    LOGV("::WebCore:: didFirstLayout %s", url.string().ascii().data());

    WebCore::FrameLoadType loadType = loader->loadType();

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), m_javaGlue->m_didFirstLayout,
            loadType == WebCore::FrameLoadTypeStandard
            // When redirect with locked history, we would like to reset the
            // scale factor. This is important for www.yahoo.com as it is
            // redirected to www.yahoo.com/?rs=1 on load.
            || loadType == WebCore::FrameLoadTypeRedirectWithLockedBackForwardList);
    checkException(env);
    DBG_NAV_LOG("call updateFrameCache");
    m_check_domtree_version = false;
    updateFrameCache();
    m_history.setDidFirstLayout(true);
}

void WebViewCore::updateViewport()
{
    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__);
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), m_javaGlue->m_updateViewport);
    checkException(env);
}

void WebViewCore::restoreScale(int scale)
{
    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__);
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), m_javaGlue->m_restoreScale, scale);
    checkException(env);
}

void WebViewCore::restoreScreenWidthScale(int scale)
{
    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__);
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), m_javaGlue->m_restoreScreenWidthScale, scale);
    checkException(env);
}

void WebViewCore::needTouchEvents(bool need)
{
    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__);
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");

#if ENABLE(TOUCH_EVENTS) // Android
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), m_javaGlue->m_needTouchEvents, need);
    checkException(env);
#endif
}

void WebViewCore::requestKeyboard(bool showKeyboard)
{
    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__);
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), m_javaGlue->m_requestKeyboard, showKeyboard);
    checkException(env);
}

void WebViewCore::notifyProgressFinished()
{
    DBG_NAV_LOG("call updateFrameCache");
    m_check_domtree_version = true;
    updateFrameCache();
    sendNotifyProgressFinished();
}

void WebViewCore::doMaxScroll(CacheBuilder::Direction dir)
{
    int dx = 0, dy = 0;

    switch (dir) {
    case CacheBuilder::LEFT:
        dx = -m_maxXScroll;
        break;
    case CacheBuilder::UP:
        dy = -m_maxYScroll;
        break;
    case CacheBuilder::RIGHT:
        dx = m_maxXScroll;
        break;
    case CacheBuilder::DOWN:
        dy = m_maxYScroll;
        break;
    case CacheBuilder::UNINITIALIZED:
    default:
        LOG_ASSERT(0, "unexpected focus selector");
    }
    this->scrollBy(dx, dy, true);
}

void WebViewCore::setScrollOffset(int moveGeneration, int dx, int dy)
{
    DBG_NAV_LOGD("{%d,%d} m_scrollOffset=(%d,%d)", dx, dy,
        m_scrollOffsetX, m_scrollOffsetY);
    if (m_scrollOffsetX != dx || m_scrollOffsetY != dy) {
        m_scrollOffsetX = dx;
        m_scrollOffsetY = dy;
        // The visible rect is located within our coordinate space so it
        // contains the actual scroll position. Setting the location makes hit
        // testing work correctly.
        m_mainFrame->view()->platformWidget()->setLocation(m_scrollOffsetX,
                m_scrollOffsetY);
        m_mainFrame->eventHandler()->sendScrollEvent();

        // update the currently visible screen
        sendPluginVisibleScreen();
    }
    gCursorBoundsMutex.lock();
    bool hasCursorBounds = m_hasCursorBounds;
    Frame* frame = (Frame*) m_cursorFrame;
    IntPoint location = m_cursorLocation;
    gCursorBoundsMutex.unlock();
    if (!hasCursorBounds)
        return;
    moveMouseIfLatest(moveGeneration, frame, location.x(), location.y());
}

void WebViewCore::setGlobalBounds(int x, int y, int h, int v)
{
    DBG_NAV_LOGD("{%d,%d}", x, y);
    m_mainFrame->view()->platformWidget()->setWindowBounds(x, y, h, v);
}

void WebViewCore::setSizeScreenWidthAndScale(int width, int height,
    int screenWidth, float scale, int realScreenWidth, int screenHeight,
    int anchorX, int anchorY, bool ignoreHeight)
{
    WebCoreViewBridge* window = m_mainFrame->view()->platformWidget();
    int ow = window->width();
    int oh = window->height();
    window->setSize(width, height);
    int osw = m_screenWidth;
    int orsw = m_screenWidth * m_screenWidthScale / m_scale;
    int osh = m_screenHeight;
    DBG_NAV_LOGD("old:(w=%d,h=%d,sw=%d,scale=%g) new:(w=%d,h=%d,sw=%d,scale=%g)",
        ow, oh, osw, m_scale, width, height, screenWidth, scale);
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    if (scale >= 0) { // negative means ignore
        m_scale = scale;
        if (screenWidth != realScreenWidth)
            m_screenWidthScale = realScreenWidth * scale / screenWidth;
        else
            m_screenWidthScale = m_scale;
    }
    m_maxXScroll = screenWidth >> 2;
    m_maxYScroll = (screenWidth * height / width) >> 2;
    if (ow != width || (!ignoreHeight && oh != height) || osw != screenWidth) {
        WebCore::RenderObject *r = m_mainFrame->contentRenderer();
        DBG_NAV_LOGD("renderer=%p view=(w=%d,h=%d)", r,
            realScreenWidth, screenHeight);
        if (r) {
            WebCore::Node* node = 0;
            WebCore::IntRect bounds;
            if (osw) {
                WebCore::IntPoint anchorPoint;
                if ((anchorX | anchorY) == 0)
                    // get current screen center position
                    anchorPoint = WebCore::IntPoint(m_scrollOffsetX
                            + (realScreenWidth >> 1), m_scrollOffsetY
                            + (screenHeight >> 1));
                else
                    anchorPoint = WebCore::IntPoint(anchorX, anchorY);
                WebCore::HitTestResult hitTestResult = m_mainFrame->eventHandler()->
                    hitTestResultAtPoint(anchorPoint, false);
                node = hitTestResult.innerNode();
                if (node) {
                    bounds = node->getRect();
                    DBG_NAV_LOGD("ob:(x=%d,y=%d,w=%d,h=%d)",
                        bounds.x(), bounds.y(), bounds.width(), bounds.height());
                    // sites like nytimes.com insert a non-standard tag <nyt_text>
                    // in the html. If it is the HitTestResult, it may have zero
                    // width and height. In this case, use its parent node.
                    if (bounds.width() == 0) {
                        node = node->parent();
                        if (node)
                            bounds = node->getRect();
                    }
                    if ((anchorX | anchorY) == 0) {
                        WebCore::IntPoint offset = WebCore::IntPoint(
                                anchorPoint.x() - bounds.x(), anchorPoint.y()
                                        - bounds.y());
                        if (offset.x() < 0 || offset.x() > realScreenWidth ||
                            offset.y() < 0 || offset.y() > screenHeight)
                        {
                            DBG_NAV_LOGD("offset out of bounds:(x=%d,y=%d)",
                                offset.x(), offset.y());
                            node = 0;
                        }
                    }
                }
            }
            r->setNeedsLayoutAndPrefWidthsRecalc();
            m_mainFrame->view()->forceLayout();
            // scroll to restore current screen center
            if (!node)
                return;
            const WebCore::IntRect& newBounds = node->getRect();
            DBG_NAV_LOGD("nb:(x=%d,y=%d,w=%d,"
                "h=%d,ns=%d)", newBounds.x(), newBounds.y(),
                newBounds.width(), newBounds.height());
            if ((anchorX | anchorY) == 0)
                scrollBy(newBounds.x() - bounds.x(),
                        newBounds.y() - bounds.y(), false);
            else if ((orsw && osh && bounds.width() && bounds.height())
                    && (bounds != newBounds)) {
                WebCore::FrameView* view = m_mainFrame->view();
                // force left align if width is not changed while height changed.
                // the anchorPoint is probably at some white space in the node
                // which is affected by text wrap around the screen width.
                bool leftAlign = (osw != m_screenWidth)
                        && (bounds.width() == newBounds.width())
                        && (bounds.height() != newBounds.height());
                showRect(newBounds.x(), newBounds.y(), newBounds.width(),
                        newBounds.height(), view->contentsWidth(),
                        view->contentsHeight(),
                        leftAlign ? 0.0 : (float) (anchorX - bounds.x()) / bounds.width(),
                        leftAlign ? 0.0 : (float) (anchorX - m_scrollOffsetX) / orsw,
                        (float) (anchorY - bounds.y()) / bounds.height(),
                        (float) (anchorY - m_scrollOffsetY) / osh);
            }
        }
    }

    // update the currently visible screen
    sendPluginVisibleScreen();
}

void WebViewCore::dumpDomTree(bool useFile)
{
#ifdef ANDROID_DOM_LOGGING
    if (useFile)
        gDomTreeFile = fopen(DOM_TREE_LOG_FILE, "w");
    m_mainFrame->document()->showTreeForThis();
    if (gDomTreeFile) {
        fclose(gDomTreeFile);
        gDomTreeFile = 0;
    }
#endif
}

void WebViewCore::dumpRenderTree(bool useFile)
{
#ifdef ANDROID_DOM_LOGGING
    if (useFile)
        gRenderTreeFile = fopen(RENDER_TREE_LOG_FILE, "w");
    WebCore::CString renderDump = WebCore::externalRepresentation(m_mainFrame->contentRenderer()).utf8();
    const char* data = renderDump.data();
    int length = renderDump.length();
    int last = 0;
    for (int i = 0; i < length; i++) {
        if (data[i] == '\n') {
            if (i != last) {
                char* chunk = new char[i - last + 1];
                strncpy(chunk, (data + last), i - last);
                chunk[i - last] = '\0';
                DUMP_RENDER_LOGD("%s", chunk);
            }
            last = i + 1;
        }
    }
    if (gRenderTreeFile) {
        fclose(gRenderTreeFile);
        gRenderTreeFile = 0;
    }
#endif
}

void WebViewCore::dumpNavTree()
{
#if DUMP_NAV_CACHE
    cacheBuilder().mDebug.print();
#endif
}

WebCore::String WebViewCore::retrieveHref(WebCore::Frame* frame, WebCore::Node* node)
{
    if (!CacheBuilder::validNode(m_mainFrame, frame, node))
        return WebCore::String();
    if (!node->hasTagName(WebCore::HTMLNames::aTag))
        return WebCore::String();
    WebCore::HTMLAnchorElement* anchor = static_cast<WebCore::HTMLAnchorElement*>(node);
    return anchor->href();
}

void WebViewCore::updateCacheOnNodeChange()
{
    gCursorBoundsMutex.lock();
    bool hasCursorBounds = m_hasCursorBounds;
    Frame* frame = (Frame*) m_cursorFrame;
    Node* node = (Node*) m_cursorNode;
    IntRect bounds = m_cursorHitBounds;
    gCursorBoundsMutex.unlock();
    if (!hasCursorBounds || !node)
        return;
    if (CacheBuilder::validNode(m_mainFrame, frame, node)) {
        RenderObject* renderer = node->renderer();
        if (renderer && renderer->style()->visibility() != HIDDEN) {
            IntRect absBox = renderer->absoluteBoundingBoxRect();
            int globalX, globalY;
            CacheBuilder::GetGlobalOffset(frame, &globalX, &globalY);
            absBox.move(globalX, globalY);
            if (absBox == bounds)
                return;
            DBG_NAV_LOGD("absBox=(%d,%d,%d,%d) bounds=(%d,%d,%d,%d)",
                absBox.x(), absBox.y(), absBox.width(), absBox.height(),
                bounds.x(), bounds.y(), bounds.width(), bounds.height());
        }
    }
    DBG_NAV_LOGD("updateFrameCache node=%p", node);
    updateFrameCache();
}

void WebViewCore::updateFrameCache()
{
    if (!m_frameCacheOutOfDate) {
        DBG_NAV_LOG("!m_frameCacheOutOfDate");
        return;
    }
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreBuildNavTimeCounter);
#endif
    m_frameCacheOutOfDate = false;
#if DEBUG_NAV_UI
    m_now = SkTime::GetMSecs();
#endif
    m_temp = new CachedRoot();
    m_temp->init(m_mainFrame, &m_history);
    CacheBuilder& builder = cacheBuilder();
    WebCore::Settings* settings = m_mainFrame->page()->settings();
    builder.allowAllTextDetection();
#ifdef ANDROID_META_SUPPORT
    if (settings) {
        if (!settings->formatDetectionAddress())
            builder.disallowAddressDetection();
        if (!settings->formatDetectionEmail())
            builder.disallowEmailDetection();
        if (!settings->formatDetectionTelephone())
            builder.disallowPhoneDetection();
    }
#endif
    builder.buildCache(m_temp);
    m_tempPict = new SkPicture();
    recordPicture(m_tempPict);
    m_temp->setPicture(m_tempPict);
    m_temp->setTextGeneration(m_textGeneration);
    WebCoreViewBridge* window = m_mainFrame->view()->platformWidget();
    m_temp->setVisibleRect(WebCore::IntRect(m_scrollOffsetX,
        m_scrollOffsetY, window->width(), window->height()));
    gFrameCacheMutex.lock();
    delete m_frameCacheKit;
    delete m_navPictureKit;
    m_frameCacheKit = m_temp;
    m_navPictureKit = m_tempPict;
    m_updatedFrameCache = true;
#if DEBUG_NAV_UI
    const CachedNode* cachedFocusNode = m_frameCacheKit->currentFocus();
    DBG_NAV_LOGD("cachedFocusNode=%d (nodePointer=%p)",
        cachedFocusNode ? cachedFocusNode->index() : 0,
        cachedFocusNode ? cachedFocusNode->nodePointer() : 0);
#endif
    gFrameCacheMutex.unlock();
}

void WebViewCore::updateFrameCacheIfLoading()
{
    if (!m_check_domtree_version)
        updateFrameCache();
}

///////////////////////////////////////////////////////////////////////////////

void WebViewCore::addPlugin(PluginWidgetAndroid* w)
{
//    SkDebugf("----------- addPlugin %p", w);
    *m_plugins.append() = w;
}

void WebViewCore::removePlugin(PluginWidgetAndroid* w)
{
//    SkDebugf("----------- removePlugin %p", w);
    int index = m_plugins.find(w);
    if (index < 0) {
        SkDebugf("--------------- pluginwindow not found! %p\n", w);
    } else {
        m_plugins.removeShuffle(index);
    }
}

void WebViewCore::invalPlugin(PluginWidgetAndroid* w)
{
    const double PLUGIN_INVAL_DELAY = 1.0 / 60;

    if (!m_pluginInvalTimer.isActive()) {
        m_pluginInvalTimer.startOneShot(PLUGIN_INVAL_DELAY);
    }
}

void WebViewCore::drawPlugins()
{
    SkRegion inval; // accumualte what needs to be redrawn
    PluginWidgetAndroid** iter = m_plugins.begin();
    PluginWidgetAndroid** stop = m_plugins.end();

    for (; iter < stop; ++iter) {
        PluginWidgetAndroid* w = *iter;
        SkIRect dirty;
        if (w->isDirty(&dirty)) {
            w->draw();
            w->localToDocumentCoords(&dirty);
            inval.op(dirty, SkRegion::kUnion_Op);
        }
    }

    if (!inval.isEmpty()) {
        // inval.getBounds() is our rectangle
        const SkIRect& bounds = inval.getBounds();
        WebCore::IntRect r(bounds.fLeft, bounds.fTop,
                           bounds.width(), bounds.height());
        this->viewInvalidate(r);
    }
}

void WebViewCore::notifyPluginsOnFrameLoad(const Frame* frame) {
    // if frame is the parent then notify all plugins
    if (!frame->tree()->parent()) {
        // trigger an event notifying the plugins that the page has loaded
        ANPEvent event;
        SkANP::InitEvent(&event, kLifecycle_ANPEventType);
        event.data.lifecycle.action = kOnLoad_ANPLifecycleAction;
        sendPluginEvent(event);
    }
    // else if frame's parent has completed
    else if (!frame->tree()->parent()->loader()->isLoading()) {
        // send to all plugins who have this frame in their heirarchy
        PluginWidgetAndroid** iter = m_plugins.begin();
        PluginWidgetAndroid** stop = m_plugins.end();
        for (; iter < stop; ++iter) {
            Frame* currentFrame = (*iter)->pluginView()->parentFrame();
            while (currentFrame) {
                if (frame == currentFrame) {
                    ANPEvent event;
                    SkANP::InitEvent(&event, kLifecycle_ANPEventType);
                    event.data.lifecycle.action = kOnLoad_ANPLifecycleAction;
                    (*iter)->sendEvent(event);
                    break;
                }
                currentFrame = currentFrame->tree()->parent();
            }
        }
    }
}

void WebViewCore::sendPluginVisibleScreen()
{
    ANPRectI visibleRect;
    visibleRect.left = m_scrollOffsetX;
    visibleRect.top = m_scrollOffsetY;
    visibleRect.right = m_scrollOffsetX + m_screenWidth;
    visibleRect.bottom = m_scrollOffsetY + m_screenHeight;

    PluginWidgetAndroid** iter = m_plugins.begin();
    PluginWidgetAndroid** stop = m_plugins.end();
    for (; iter < stop; ++iter) {
        (*iter)->setVisibleScreen(visibleRect, m_scale);
    }
}

void WebViewCore::sendPluginEvent(const ANPEvent& evt, ANPEventFlag flag)
{
    PluginWidgetAndroid** iter = m_plugins.begin();
    PluginWidgetAndroid** stop = m_plugins.end();
    for (; iter < stop; ++iter) {
        if((*iter)->isAcceptingEvent(flag))
            (*iter)->sendEvent(evt);
    }
}

void WebViewCore::sendPluginEvent(const ANPEvent& evt)
{
    PluginWidgetAndroid** iter = m_plugins.begin();
    PluginWidgetAndroid** stop = m_plugins.end();
    for (; iter < stop; ++iter) {
        (*iter)->sendEvent(evt);
    }
}

static PluginView* nodeIsPlugin(Node* node) {
    RenderObject* renderer = node->renderer();
    if (renderer && renderer->isWidget()) {
        Widget* widget = static_cast<RenderWidget*>(renderer)->widget();
        if (widget && widget->isPluginView())
            return static_cast<PluginView*>(widget);
    }
    return 0;
}

Node* WebViewCore::cursorNodeIsPlugin() {
    gCursorBoundsMutex.lock();
    bool hasCursorBounds = m_hasCursorBounds;
    Frame* frame = (Frame*) m_cursorFrame;
    Node* node = (Node*) m_cursorNode;
    gCursorBoundsMutex.unlock();
    if (hasCursorBounds && CacheBuilder::validNode(m_mainFrame, frame, node)
            && nodeIsPlugin(node)) {
        return node;
    }
    return 0;
}


void WebViewCore::updatePluginState(Frame* frame, Node* node, PluginState state) {

    // check that the node and frame pointers are (still) valid
    if (!frame || !node || !CacheBuilder::validNode(m_mainFrame, frame, node))
        return;

    // check that the node is a plugin view
    PluginView* pluginView = nodeIsPlugin(node);
    if (!pluginView)
        return;

    // create the event
    ANPEvent event;
    SkANP::InitEvent(&event, kLifecycle_ANPEventType);

    if (state == kLoseFocus_PluginState)
        event.data.lifecycle.action = kLoseFocus_ANPLifecycleAction;
    else if (state == kGainFocus_PluginState)
        event.data.lifecycle.action = kGainFocus_ANPLifecycleAction;
    else
        return;

    // send the event
    pluginView->platformPluginWidget()->sendEvent(event);
}

///////////////////////////////////////////////////////////////////////////////
void WebViewCore::moveMouseIfLatest(int moveGeneration,
    WebCore::Frame* frame, int x, int y)
{
    DBG_NAV_LOGD("m_moveGeneration=%d moveGeneration=%d"
        " frame=%p x=%d y=%d",
        m_moveGeneration, moveGeneration, frame, x, y);
    if (m_moveGeneration > moveGeneration) {
        DBG_NAV_LOGD("m_moveGeneration=%d > moveGeneration=%d",
            m_moveGeneration, moveGeneration);
        return; // short-circuit if a newer move has already been generated
    }
    m_lastGeneration = moveGeneration;
    moveMouse(frame, x, y);
}

// Update mouse position and may change focused node.
void WebViewCore::moveMouse(WebCore::Frame* frame, int x, int y)
{
    DBG_NAV_LOGD("frame=%p x=%d y=%d scrollOffset=(%d,%d)", frame,
        x, y, m_scrollOffsetX, m_scrollOffsetY);
    if (!frame || CacheBuilder::validNode(m_mainFrame, frame, NULL) == false)
        frame = m_mainFrame;
    // mouse event expects the position in the window coordinate
    m_mousePos = WebCore::IntPoint(x - m_scrollOffsetX, y - m_scrollOffsetY);
    // validNode will still return true if the node is null, as long as we have
    // a valid frame.  Do not want to make a call on frame unless it is valid.
    WebCore::PlatformMouseEvent mouseEvent(m_mousePos, m_mousePos,
        WebCore::NoButton, WebCore::MouseEventMoved, 1, false, false, false,
        false, WTF::currentTime());
    frame->eventHandler()->handleMouseMoveEvent(mouseEvent);
    updateCacheOnNodeChange();
}

static int findTextBoxIndex(WebCore::Node* node, const WebCore::IntPoint& pt)
{
    if (!node->isTextNode()) {
        DBG_NAV_LOGD("node=%p pt=(%d,%d) isText=false", node, pt.x(), pt.y());
        return -2; // error
    }
    WebCore::RenderText* renderText = (WebCore::RenderText*) node->renderer();
    if (!renderText) {
        DBG_NAV_LOGD("node=%p pt=(%d,%d) renderText=0", node, pt.x(), pt.y());
        return -3; // error
    }
    FloatPoint absPt = renderText->localToAbsolute();
    WebCore::InlineTextBox *textBox = renderText->firstTextBox();
    int globalX, globalY;
    CacheBuilder::GetGlobalOffset(node, &globalX, &globalY);
    int x = pt.x() - globalX;
    int y = pt.y() - globalY;
    do {
        int textBoxStart = textBox->start();
        int textBoxEnd = textBoxStart + textBox->len();
        if (textBoxEnd <= textBoxStart)
            continue;
        WebCore::IntRect bounds = textBox->selectionRect(absPt.x(), absPt.y(),
            textBoxStart, textBoxEnd);
        if (!bounds.contains(x, y))
            continue;
        int offset = textBox->offsetForPosition(x - absPt.x());
#if DEBUG_NAV_UI
        int prior = offset > 0 ? textBox->positionForOffset(offset - 1) : -1;
        int current = textBox->positionForOffset(offset);
        int next = textBox->positionForOffset(offset + 1);
        DBG_NAV_LOGD(
            "offset=%d pt.x=%d globalX=%d renderX=%d x=%d "
            "textBox->x()=%d textBox->start()=%d prior=%d current=%d next=%d",
            offset, pt.x(), globalX, absPt.x(), x,
            textBox->x(), textBox->start(), prior, current, next
            );
#endif
        return textBox->start() + offset;
    } while ((textBox = textBox->nextTextBox()));
    return -1; // couldn't find point, may have walked off end
}

static inline bool isPunctuation(UChar c)
{
    return WTF::Unicode::category(c) & (0
        | WTF::Unicode::Punctuation_Dash
        | WTF::Unicode::Punctuation_Open
        | WTF::Unicode::Punctuation_Close
        | WTF::Unicode::Punctuation_Connector
        | WTF::Unicode::Punctuation_Other
        | WTF::Unicode::Punctuation_InitialQuote
        | WTF::Unicode::Punctuation_FinalQuote
    );
}

static int centerX(const SkIRect& rect)
{
    return (rect.fLeft + rect.fRight) >> 1;
}

static int centerY(const SkIRect& rect)
{
    return (rect.fTop + rect.fBottom) >> 1;
}

WebCore::String WebViewCore::getSelection(SkRegion* selRgn)
{
    SkRegion::Iterator iter(*selRgn);
    // FIXME: switch this to use StringBuilder instead
    WebCore::String result;
    WebCore::Node* lastStartNode = 0;
    int lastStartEnd = -1;
    UChar lastChar = 0xffff;
    for (; !iter.done(); iter.next()) {
        const SkIRect& rect = iter.rect();
        DBG_NAV_LOGD("rect=(%d, %d, %d, %d)", rect.fLeft, rect.fTop,
            rect.fRight, rect.fBottom);
        int cy = centerY(rect);
        WebCore::IntPoint startPt = WebCore::IntPoint(rect.fLeft + 1, cy);
        WebCore::HitTestResult hitTestResult = m_mainFrame->eventHandler()->
            hitTestResultAtPoint(startPt, false);
        WebCore::Node* node = hitTestResult.innerNode();
        if (!node) {
            DBG_NAV_LOG("!node");
            return result;
        }
        WebCore::IntPoint endPt = WebCore::IntPoint(rect.fRight - 1, cy);
        hitTestResult = m_mainFrame->eventHandler()->hitTestResultAtPoint(endPt, false);
        WebCore::Node* endNode = hitTestResult.innerNode();
        if (!endNode) {
            DBG_NAV_LOG("!endNode (right-1)");
            endPt = WebCore::IntPoint(rect.fRight - 2, cy);
            hitTestResult = m_mainFrame->eventHandler()->hitTestResultAtPoint(endPt, false);
            endNode = hitTestResult.innerNode();
        }
        if (!endNode) {
            DBG_NAV_LOG("!endNode (right-2)");
            return result;
        }
        int start = findTextBoxIndex(node, startPt);
        if (start < 0)
            continue;
        int end = findTextBoxIndex(endNode, endPt);
        if (end < -1) // use node if endNode is not valid
            endNode = node;
        if (end <= 0)
            end = static_cast<WebCore::Text*>(endNode)->string()->length();
        DBG_NAV_LOGD("node=%p start=%d endNode=%p end=%d", node, start, endNode, end);
        WebCore::Node* startNode = node;
        do {
            if (!node->isTextNode())
                continue;
            if (node->getRect().isEmpty())
                continue;
            WebCore::Text* textNode = static_cast<WebCore::Text*>(node);
            WebCore::StringImpl* string = textNode->string();
            if (!string->length())
                continue;
            const UChar* chars = string->characters();
            int newLength = node == endNode ? end : string->length();
            if (node == startNode) {
 #if DEBUG_NAV_UI
                if (node == lastStartNode)
                    DBG_NAV_LOGD("start=%d last=%d", start, lastStartEnd);
 #endif
                if (node == lastStartNode && start < lastStartEnd)
                    break; // skip rect if text overlaps already written text
                lastStartNode = node;
                lastStartEnd = newLength - start;
            }
            if (newLength < start) {
                DBG_NAV_LOGD("newLen=%d < start=%d", newLength, start);
                break;
            }
            if (!isPunctuation(chars[start]))
                result.append(' ');
            result.append(chars + start, newLength - start);
            start = 0;
        } while (node != endNode && (node = node->traverseNextNode()));
    }
    result = result.simplifyWhiteSpace().stripWhiteSpace();
#if DUMP_NAV_CACHE
    {
        char buffer[256];
        CacheBuilder::Debug debug;
        debug.init(buffer, sizeof(buffer));
        debug.print("copy: ");
        debug.wideString(result);
        DUMP_NAV_LOGD("%s", buffer);
    }
#endif
    return result;
}

void WebViewCore::setSelection(int start, int end)
{
    WebCore::Node* focus = currentFocus();
    if (!focus)
        return;
    WebCore::RenderObject* renderer = focus->renderer();
    if (!renderer || (!renderer->isTextField() && !renderer->isTextArea()))
        return;
    WebCore::RenderTextControl* rtc = static_cast<WebCore::RenderTextControl*>(renderer);
    if (start > end) {
        int temp = start;
        start = end;
        end = temp;
    }
    // Tell our EditorClient that this change was generated from the UI, so it
    // does not need to echo it to the UI.
    EditorClientAndroid* client = static_cast<EditorClientAndroid*>(
            m_mainFrame->editor()->client());
    client->setUiGeneratedSelectionChange(true);
    rtc->setSelectionRange(start, end);
    client->setUiGeneratedSelectionChange(false);
    focus->document()->frame()->revealSelection();
    setFocusControllerActive(true);
}

void WebViewCore::deleteSelection(int start, int end, int textGeneration)
{
    setSelection(start, end);
    if (start == end)
        return;
    WebCore::Node* focus = currentFocus();
    if (!focus)
        return;
    WebCore::TypingCommand::deleteSelection(focus->document());
    m_textGeneration = textGeneration;
}

void WebViewCore::replaceTextfieldText(int oldStart,
        int oldEnd, const WebCore::String& replace, int start, int end,
        int textGeneration)
{
    WebCore::Node* focus = currentFocus();
    if (!focus)
        return;
    setSelection(oldStart, oldEnd);
    // Prevent our editor client from passing a message to change the
    // selection.
    EditorClientAndroid* client = static_cast<EditorClientAndroid*>(
            m_mainFrame->editor()->client());
    client->setUiGeneratedSelectionChange(true);
    WebCore::TypingCommand::insertText(focus->document(), replace,
        false);
    client->setUiGeneratedSelectionChange(false);
    setSelection(start, end);
    m_textGeneration = textGeneration;
}

void WebViewCore::passToJs(int generation, const WebCore::String& current,
    const PlatformKeyboardEvent& event)
{
    WebCore::Node* focus = currentFocus();
    if (!focus) {
        DBG_NAV_LOG("!focus");
        clearTextEntry();
        return;
    }
    WebCore::RenderObject* renderer = focus->renderer();
    if (!renderer || (!renderer->isTextField() && !renderer->isTextArea())) {
        DBG_NAV_LOGD("renderer==%p || not text", renderer);
        clearTextEntry();
        return;
    }
    // Block text field updates during a key press.
    m_blockTextfieldUpdates = true;
    // Also prevent our editor client from passing a message to change the
    // selection.
    EditorClientAndroid* client = static_cast<EditorClientAndroid*>(
            m_mainFrame->editor()->client());
    client->setUiGeneratedSelectionChange(true);
    key(event);
    client->setUiGeneratedSelectionChange(false);
    m_blockTextfieldUpdates = false;
    m_textGeneration = generation;
    setFocusControllerActive(true);
    WebCore::RenderTextControl* renderText =
        static_cast<WebCore::RenderTextControl*>(renderer);
    WebCore::String test = renderText->text();
    if (test == current) {
        DBG_NAV_LOG("test == current");
        return;
    }
    // If the text changed during the key event, update the UI text field.
    updateTextfield(focus, false, test);
}

void WebViewCore::scrollFocusedTextInput(float xPercent, int y)
{
    WebCore::Node* focus = currentFocus();
    if (!focus) {
        DBG_NAV_LOG("!focus");
        clearTextEntry();
        return;
    }
    WebCore::RenderObject* renderer = focus->renderer();
    if (!renderer || (!renderer->isTextField() && !renderer->isTextArea())) {
        DBG_NAV_LOGD("renderer==%p || not text", renderer);
        clearTextEntry();
        return;
    }
    WebCore::RenderTextControl* renderText =
        static_cast<WebCore::RenderTextControl*>(renderer);
    int x = (int) (xPercent * (renderText->scrollWidth() -
        renderText->clientWidth()));
    DBG_NAV_LOGD("x=%d y=%d xPercent=%g scrollW=%d clientW=%d", x, y,
        xPercent, renderText->scrollWidth(), renderText->clientWidth());
    renderText->setScrollLeft(x);
    renderText->setScrollTop(y);
}

void WebViewCore::setFocusControllerActive(bool active)
{
    m_mainFrame->page()->focusController()->setActive(active);
}

void WebViewCore::saveDocumentState(WebCore::Frame* frame)
{
    if (!CacheBuilder::validNode(m_mainFrame, frame, 0))
        frame = m_mainFrame;
    WebCore::HistoryItem *item = frame->loader()->currentHistoryItem();

    // item can be null when there is no offical URL for the current page. This happens
    // when the content is loaded using with WebCoreFrameBridge::LoadData() and there
    // is no failing URL (common case is when content is loaded using data: scheme)
    if (item) {
        item->setDocumentState(frame->document()->formElementsState());
    }
}

// Convert a WebCore::String into an array of characters where the first
// character represents the length, for easy conversion to java.
static uint16_t* stringConverter(const WebCore::String& text)
{
    size_t length = text.length();
    uint16_t* itemName = new uint16_t[length+1];
    itemName[0] = (uint16_t)length;
    uint16_t* firstChar = &(itemName[1]);
    memcpy((void*)firstChar, text.characters(), sizeof(UChar)*length);
    return itemName;
}

// Response to dropdown created for a listbox.
class ListBoxReply : public WebCoreReply {
public:
    ListBoxReply(WebCore::HTMLSelectElement* select, WebCore::Frame* frame, WebViewCore* view)
        : m_select(select)
        , m_frame(frame)
        , m_viewImpl(view)
    {}

    // Response used if the listbox only allows single selection.
    // index is listIndex of the selected item, or -1 if nothing is selected.
    virtual void replyInt(int index)
    {
        if (-2 == index) {
            // Special value for cancel. Do nothing.
            return;
        }
        // If the select element no longer exists, due to a page change, etc,
        // silently return.
        if (!m_select || !CacheBuilder::validNode(m_viewImpl->m_mainFrame,
                m_frame, m_select))
            return;
        // Use a pointer to HTMLSelectElement's superclass, where
        // listToOptionIndex is public.
        SelectElement* selectElement = m_select;
        int optionIndex = selectElement->listToOptionIndex(index);
        m_select->setSelectedIndex(optionIndex, true);
        m_select->dispatchFormControlChangeEvent();
        m_viewImpl->contentInvalidate(m_select->getRect());
    }

    // Response if the listbox allows multiple selection.  array stores the listIndices
    // of selected positions.
    virtual void replyIntArray(const int* array, int count)
    {
        // If the select element no longer exists, due to a page change, etc,
        // silently return.
        if (!m_select || !CacheBuilder::validNode(m_viewImpl->m_mainFrame,
                m_frame, m_select))
            return;

        // If count is 1 or 0, use replyInt.
        SkASSERT(count > 1);

        const WTF::Vector<Element*>& items = m_select->listItems();
        int totalItems = static_cast<int>(items.size());
        // Keep track of the position of the value we are comparing against.
        int arrayIndex = 0;
        // The value we are comparing against.
        int selection = array[arrayIndex];
        WebCore::HTMLOptionElement* option;
        for (int listIndex = 0; listIndex < totalItems; listIndex++) {
            if (items[listIndex]->hasLocalName(WebCore::HTMLNames::optionTag)) {
                option = static_cast<WebCore::HTMLOptionElement*>(
                        items[listIndex]);
                if (listIndex == selection) {
                    option->setSelectedState(true);
                    arrayIndex++;
                    if (arrayIndex == count)
                        selection = -1;
                    else
                        selection = array[arrayIndex];
                } else
                    option->setSelectedState(false);
            }
        }
        m_select->dispatchFormControlChangeEvent();
        m_viewImpl->contentInvalidate(m_select->getRect());
    }
private:
    // The select element associated with this listbox.
    WebCore::HTMLSelectElement* m_select;
    // The frame of this select element, to verify that it is valid.
    WebCore::Frame* m_frame;
    // For calling invalidate and checking the select element's validity
    WebViewCore* m_viewImpl;
};

// Create an array of java Strings.
static jobjectArray makeLabelArray(JNIEnv* env, const uint16_t** labels, size_t count)
{
    jclass stringClass = env->FindClass("java/lang/String");
    LOG_ASSERT(stringClass, "Could not find java/lang/String");
    jobjectArray array = env->NewObjectArray(count, stringClass, 0);
    LOG_ASSERT(array, "Could not create new string array");

    for (size_t i = 0; i < count; i++) {
        jobject newString = env->NewString(&labels[i][1], labels[i][0]);
        env->SetObjectArrayElement(array, i, newString);
        env->DeleteLocalRef(newString);
        checkException(env);
    }
    env->DeleteLocalRef(stringClass);
    return array;
}

void WebViewCore::listBoxRequest(WebCoreReply* reply, const uint16_t** labels, size_t count, const int enabled[], size_t enabledCount,
        bool multiple, const int selected[], size_t selectedCountOrSelection)
{
    // If m_popupReply is not null, then we already have a list showing.
    if (m_popupReply != 0)
        return;

    LOG_ASSERT(m_javaGlue->m_obj, "No java widget associated with this view!");

    // Create an array of java Strings for the drop down.
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    jobjectArray labelArray = makeLabelArray(env, labels, count);

    // Create an array determining whether each item is enabled.
    jbooleanArray enabledArray = env->NewBooleanArray(enabledCount);
    checkException(env);
    jboolean* ptrArray = env->GetBooleanArrayElements(enabledArray, 0);
    checkException(env);
    for (size_t i = 0; i < enabledCount; i++) {
        ptrArray[i] = enabled[i];
    }
    env->ReleaseBooleanArrayElements(enabledArray, ptrArray, 0);
    checkException(env);

    if (multiple) {
        // Pass up an array representing which items are selected.
        jintArray selectedArray = env->NewIntArray(selectedCountOrSelection);
        checkException(env);
        jint* selArray = env->GetIntArrayElements(selectedArray, 0);
        checkException(env);
        for (size_t i = 0; i < selectedCountOrSelection; i++) {
            selArray[i] = selected[i];
        }
        env->ReleaseIntArrayElements(selectedArray, selArray, 0);

        env->CallVoidMethod(obj.get(), m_javaGlue->m_requestListBox, labelArray, enabledArray, selectedArray);
        env->DeleteLocalRef(selectedArray);
    } else {
        // Pass up the single selection.
        env->CallVoidMethod(obj.get(), m_javaGlue->m_requestSingleListBox, labelArray, enabledArray, selectedCountOrSelection);
    }

    env->DeleteLocalRef(labelArray);
    env->DeleteLocalRef(enabledArray);
    checkException(env);

    Retain(reply);
    m_popupReply = reply;
}

bool WebViewCore::key(const PlatformKeyboardEvent& event)
{
    WebCore::EventHandler* eventHandler = m_mainFrame->eventHandler();
    WebCore::Node* focusNode = currentFocus();
    if (focusNode)
        eventHandler = focusNode->document()->frame()->eventHandler();
    DBG_NAV_LOGD("keyCode=%s unichar=%d focusNode=%p",
        event.keyIdentifier().utf8().data(), event.unichar(), focusNode);
    return eventHandler->keyEvent(event);
}

// For when the user clicks the trackball
void WebViewCore::click(WebCore::Frame* frame, WebCore::Node* node) {
    if (!node) {
        WebCore::IntPoint pt = m_mousePos;
        pt.move(m_scrollOffsetX, m_scrollOffsetY);
        WebCore::HitTestResult hitTestResult = m_mainFrame->eventHandler()->
                hitTestResultAtPoint(pt, false);
        node = hitTestResult.innerNode();
        frame = node->document()->frame();
        DBG_NAV_LOGD("m_mousePos=(%d,%d) m_scrollOffset=(%d,%d) pt=(%d,%d)"
            " node=%p", m_mousePos.x(), m_mousePos.y(),
            m_scrollOffsetX, m_scrollOffsetY, pt.x(), pt.y(), node);
    }
    if (node) {
        EditorClientAndroid* client
                = static_cast<EditorClientAndroid*>(
                m_mainFrame->editor()->client());
        client->setShouldChangeSelectedRange(false);
        handleMouseClick(frame, node);
        client->setShouldChangeSelectedRange(true);
    }
}

bool WebViewCore::handleTouchEvent(int action, int x, int y)
{
    bool preventDefault = false;

#if ENABLE(TOUCH_EVENTS) // Android
    WebCore::TouchEventType type = WebCore::TouchEventCancel;
    switch (action) {
    case 0: // MotionEvent.ACTION_DOWN
        type = WebCore::TouchEventStart;
        break;
    case 1: // MotionEvent.ACTION_UP
        type = WebCore::TouchEventEnd;
        break;
    case 2: // MotionEvent.ACTION_MOVE
        type = WebCore::TouchEventMove;
        break;
    case 3: // MotionEvent.ACTION_CANCEL
        type = WebCore::TouchEventCancel;
        break;
    }
    WebCore::IntPoint pt(x - m_scrollOffsetX, y - m_scrollOffsetY);
    WebCore::PlatformTouchEvent te(pt, pt, type);
    preventDefault = m_mainFrame->eventHandler()->handleTouchEvent(te);
#endif

    return preventDefault;
}

void WebViewCore::touchUp(int touchGeneration,
    WebCore::Frame* frame, WebCore::Node* node, int x, int y)
{
    if (m_touchGeneration > touchGeneration) {
        DBG_NAV_LOGD("m_touchGeneration=%d > touchGeneration=%d"
            " x=%d y=%d", m_touchGeneration, touchGeneration, x, y);
        return; // short circuit if a newer touch has been generated
    }
    moveMouse(frame, x, y);
    m_lastGeneration = touchGeneration;
    if (frame && CacheBuilder::validNode(m_mainFrame, frame, 0)) {
        frame->loader()->resetMultipleFormSubmissionProtection();
    }
    // If the click is on an unselected textfield/area we do not want to allow
    // the click to change the selection, because we will set it ourselves
    // elsewhere - beginning for textareas, end for textfields
    bool needToIgnoreChangesToSelectedRange = true;
    WebCore::Node* focusNode = currentFocus();
    if (focusNode) {
        WebCore::RenderObject* renderer = focusNode->renderer();
        if (renderer && (renderer->isTextField() || renderer->isTextArea())) {
            // Now check to see if the click is inside the focused textfield
            if (focusNode->getRect().contains(x, y))
                needToIgnoreChangesToSelectedRange = false;
        }
    }
    EditorClientAndroid* client = 0;
    if (needToIgnoreChangesToSelectedRange) {
        client = static_cast<EditorClientAndroid*>(
                m_mainFrame->editor()->client());
        client->setShouldChangeSelectedRange(false);
    }
    DBG_NAV_LOGD("touchGeneration=%d handleMouseClick frame=%p node=%p"
        " x=%d y=%d", touchGeneration, frame, node, x, y);
    handleMouseClick(frame, node);
    if (needToIgnoreChangesToSelectedRange)
        client->setShouldChangeSelectedRange(true);
}

// Common code for both clicking with the trackball and touchUp
bool WebViewCore::handleMouseClick(WebCore::Frame* framePtr, WebCore::Node* nodePtr)
{
    bool valid = framePtr == NULL
            || CacheBuilder::validNode(m_mainFrame, framePtr, nodePtr);
    WebFrame* webFrame = WebFrame::getWebFrame(m_mainFrame);
    if (valid && nodePtr) {
    // Need to special case area tags because an image map could have an area element in the middle
    // so when attempting to get the default, the point chosen would be follow the wrong link.
        if (nodePtr->hasTagName(WebCore::HTMLNames::areaTag)) {
            webFrame->setUserInitiatedClick(true);
            nodePtr->dispatchSimulatedClick(0, true, true);
            webFrame->setUserInitiatedClick(false);
            DBG_NAV_LOG("area");
            return true;
        }
        WebCore::RenderObject* renderer = nodePtr->renderer();
        if (renderer && (renderer->isMenuList() || renderer->isListBox())) {
            WebCore::HTMLSelectElement* select = static_cast<WebCore::HTMLSelectElement*>(nodePtr);
            const WTF::Vector<WebCore::Element*>& listItems = select->listItems();
            SkTDArray<const uint16_t*> names;
            SkTDArray<int> enabledArray;
            SkTDArray<int> selectedArray;
            int size = listItems.size();
            bool multiple = select->multiple();
            for (int i = 0; i < size; i++) {
                if (listItems[i]->hasTagName(WebCore::HTMLNames::optionTag)) {
                    WebCore::HTMLOptionElement* option = static_cast<WebCore::HTMLOptionElement*>(listItems[i]);
                    *names.append() = stringConverter(option->textIndentedToRespectGroupLabel());
                    *enabledArray.append() = option->disabled() ? 0 : 1;
                    if (multiple && option->selected())
                        *selectedArray.append() = i;
                } else if (listItems[i]->hasTagName(WebCore::HTMLNames::optgroupTag)) {
                    WebCore::HTMLOptGroupElement* optGroup = static_cast<WebCore::HTMLOptGroupElement*>(listItems[i]);
                    *names.append() = stringConverter(optGroup->groupLabelText());
                    *enabledArray.append() = 0;
                }
            }
            WebCoreReply* reply = new ListBoxReply(select, select->document()->frame(), this);
            // Use a pointer to HTMLSelectElement's superclass, where
            // optionToListIndex is public.
            SelectElement* selectElement = select;
            listBoxRequest(reply, names.begin(), size, enabledArray.begin(), enabledArray.count(),
                    multiple, selectedArray.begin(), multiple ? selectedArray.count() :
                    selectElement->optionToListIndex(select->selectedIndex()));
            DBG_NAV_LOG("menu list");
            return true;
        }
    }
    if (!valid || !framePtr)
        framePtr = m_mainFrame;
    webFrame->setUserInitiatedClick(true);
    WebCore::PlatformMouseEvent mouseDown(m_mousePos, m_mousePos, WebCore::LeftButton,
            WebCore::MouseEventPressed, 1, false, false, false, false,
            WTF::currentTime());
    // ignore the return from as it will return true if the hit point can trigger selection change
    framePtr->eventHandler()->handleMousePressEvent(mouseDown);
    WebCore::PlatformMouseEvent mouseUp(m_mousePos, m_mousePos, WebCore::LeftButton,
            WebCore::MouseEventReleased, 1, false, false, false, false,
            WTF::currentTime());
    bool handled = framePtr->eventHandler()->handleMouseReleaseEvent(mouseUp);
    webFrame->setUserInitiatedClick(false);

    // If the user clicked on a textfield, make the focusController active
    // so we show the blinking cursor.
    WebCore::Node* focusNode = currentFocus();
    DBG_NAV_LOGD("m_mousePos={%d,%d} focusNode=%p handled=%s", m_mousePos.x(),
        m_mousePos.y(), focusNode, handled ? "true" : "false");
    if (focusNode) {
        WebCore::RenderObject* renderer = focusNode->renderer();
        if (renderer && (renderer->isTextField() || renderer->isTextArea()))
            setFocusControllerActive(true);
    }
    return handled;
}

void WebViewCore::popupReply(int index)
{
    if (m_popupReply) {
        m_popupReply->replyInt(index);
        Release(m_popupReply);
        m_popupReply = 0;
    }
}

void WebViewCore::popupReply(const int* array, int count)
{
    if (m_popupReply) {
        m_popupReply->replyIntArray(array, count);
        Release(m_popupReply);
        m_popupReply = NULL;
    }
}

void WebViewCore::addMessageToConsole(const WebCore::String& message, unsigned int lineNumber, const WebCore::String& sourceID) {
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    jstring jMessageStr = env->NewString((unsigned short *)message.characters(), message.length());
    jstring jSourceIDStr = env->NewString((unsigned short *)sourceID.characters(), sourceID.length());
    env->CallVoidMethod(obj.get(), m_javaGlue->m_addMessageToConsole, jMessageStr, lineNumber, jSourceIDStr);
    env->DeleteLocalRef(jMessageStr);
    env->DeleteLocalRef(jSourceIDStr);
    checkException(env);
}

void WebViewCore::jsAlert(const WebCore::String& url, const WebCore::String& text)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    jstring jInputStr = env->NewString((unsigned short *)text.characters(), text.length());
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());
    env->CallVoidMethod(obj.get(), m_javaGlue->m_jsAlert, jUrlStr, jInputStr);
    env->DeleteLocalRef(jInputStr);
    env->DeleteLocalRef(jUrlStr);
    checkException(env);
}

void WebViewCore::exceededDatabaseQuota(const WebCore::String& url, const WebCore::String& databaseIdentifier, const unsigned long long currentQuota, unsigned long long estimatedSize)
{
#if ENABLE(DATABASE)
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    jstring jDatabaseIdentifierStr = env->NewString((unsigned short *)databaseIdentifier.characters(), databaseIdentifier.length());
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());
    env->CallVoidMethod(obj.get(), m_javaGlue->m_exceededDatabaseQuota, jUrlStr, jDatabaseIdentifierStr, currentQuota, estimatedSize);
    env->DeleteLocalRef(jDatabaseIdentifierStr);
    env->DeleteLocalRef(jUrlStr);
    checkException(env);
#endif
}

void WebViewCore::reachedMaxAppCacheSize(const unsigned long long spaceNeeded)
{
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), m_javaGlue->m_reachedMaxAppCacheSize, spaceNeeded);
    checkException(env);
#endif
}

void WebViewCore::populateVisitedLinks(WebCore::PageGroup* group)
{
    m_groupForVisitedLinks = group;
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), m_javaGlue->m_populateVisitedLinks);
    checkException(env);
}

void WebViewCore::geolocationPermissionsShowPrompt(const WebCore::String& origin)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    jstring originString = env->NewString((unsigned short *)origin.characters(), origin.length());
    env->CallVoidMethod(obj.get(),
                        m_javaGlue->m_geolocationPermissionsShowPrompt,
                        originString);
    env->DeleteLocalRef(originString);
    checkException(env);
}

void WebViewCore::geolocationPermissionsHidePrompt()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(),
                        m_javaGlue->m_geolocationPermissionsHidePrompt);
    checkException(env);
}

bool WebViewCore::jsConfirm(const WebCore::String& url, const WebCore::String& text)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return false;
    jstring jInputStr = env->NewString((unsigned short *)text.characters(), text.length());
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());
    jboolean result = env->CallBooleanMethod(obj.get(), m_javaGlue->m_jsConfirm, jUrlStr, jInputStr);
    env->DeleteLocalRef(jInputStr);
    env->DeleteLocalRef(jUrlStr);
    checkException(env);
    return result;
}

bool WebViewCore::jsPrompt(const WebCore::String& url, const WebCore::String& text, const WebCore::String& defaultValue, WebCore::String& result)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return false;

    jstring jInputStr = env->NewString((unsigned short *)text.characters(), text.length());
    jstring jDefaultStr = env->NewString((unsigned short *)defaultValue.characters(), defaultValue.length());
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());
    jstring returnVal = (jstring) env->CallObjectMethod(obj.get(), m_javaGlue->m_jsPrompt, jUrlStr, jInputStr, jDefaultStr);
    // If returnVal is null, it means that the user cancelled the dialog.
    if (!returnVal)
        return false;

    result = to_string(env, returnVal);
    env->DeleteLocalRef(jInputStr);
    env->DeleteLocalRef(jDefaultStr);
    env->DeleteLocalRef(jUrlStr);
    checkException(env);
    return true;
}

bool WebViewCore::jsUnload(const WebCore::String& url, const WebCore::String& message)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return false;
    jstring jInputStr = env->NewString((unsigned short *)message.characters(), message.length());
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());
    jboolean result = env->CallBooleanMethod(obj.get(), m_javaGlue->m_jsUnload, jUrlStr, jInputStr);
    env->DeleteLocalRef(jInputStr);
    env->DeleteLocalRef(jUrlStr);
    checkException(env);
    return result;
}

bool WebViewCore::jsInterrupt()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return true;    // default to interrupt
    jboolean result = env->CallBooleanMethod(obj.get(), m_javaGlue->m_jsInterrupt);
    checkException(env);
    return result;
}

AutoJObject
WebViewCore::getJavaObject()
{
    return getRealObject(JSC::Bindings::getJNIEnv(), m_javaGlue->m_obj);
}

jobject
WebViewCore::getWebViewJavaObject()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return 0;
    return env->GetObjectField(obj.get(), gWebViewCoreFields.m_webView);
}

void WebViewCore::updateTextSelection() {
    WebCore::Node* focusNode = currentFocus();
    if (!focusNode)
        return;
    RenderObject* renderer = focusNode->renderer();
    if (!renderer || (!renderer->isTextArea() && !renderer->isTextField()))
        return;
    RenderTextControl* rtc = static_cast<RenderTextControl*>(renderer);
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(),
            m_javaGlue->m_updateTextSelection, reinterpret_cast<int>(focusNode),
            rtc->selectionStart(), rtc->selectionEnd(), m_textGeneration);
    checkException(env);
}

void WebViewCore::updateTextfield(WebCore::Node* ptr, bool changeToPassword,
        const WebCore::String& text)
{
    if (m_blockTextfieldUpdates)
        return;
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;

    if (changeToPassword) {
        env->CallVoidMethod(obj.get(), m_javaGlue->m_updateTextfield,
                (int) ptr, true, 0, m_textGeneration);
        checkException(env);
        return;
    }
    int length = text.length();
    jstring string = env->NewString((unsigned short *) text.characters(), length);
    env->CallVoidMethod(obj.get(), m_javaGlue->m_updateTextfield,
            (int) ptr, false, string, m_textGeneration);
    env->DeleteLocalRef(string);
    checkException(env);
}

void WebViewCore::clearTextEntry()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;

    env->CallVoidMethod(obj.get(), m_javaGlue->m_clearTextEntry);
}

void WebViewCore::setBackgroundColor(SkColor c)
{
    WebCore::FrameView* view = m_mainFrame->view();
    if (!view)
        return;

    // need (int) cast to find the right constructor
    WebCore::Color bcolor((int)SkColorGetR(c), (int)SkColorGetG(c),
                          (int)SkColorGetB(c), (int)SkColorGetA(c));
    view->setBaseBackgroundColor(bcolor);
}

void WebViewCore::startFullScreenPluginActivity(const char* libName,
                                                const char* className, NPP npp)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();

    jstring libString = env->NewStringUTF(libName);
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;

    jstring classString = env->NewStringUTF(className);
    env->CallVoidMethod(obj.get(),
                        m_javaGlue->m_startFullScreenPluginActivity,
                        libString, classString, (int) npp);
    checkException(env);
}

jobject WebViewCore::createSurface(const char* libName, const char* className,
                                   NPP npp, int x, int y, int width, int height)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return 0;

    jstring libString = env->NewStringUTF(libName);
    jstring classString = env->NewStringUTF(className);
    jobject result = env->CallObjectMethod(obj.get(),
                                           m_javaGlue->m_createSurface, libString,
                                           classString,(int) npp, x, y, width, height);
    checkException(env);
    return result;

}

void WebViewCore::destroySurface(jobject childView)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;

    env->CallVoidMethod(obj.get(), m_javaGlue->m_destroySurface, childView);
    checkException(env);
}

void WebViewCore::showRect(int left, int top, int width, int height,
        int contentWidth, int contentHeight, float xPercentInDoc,
        float xPercentInView, float yPercentInDoc, float yPercentInView)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = m_javaGlue->object(env);
    // if it is called during DESTROY is handled, the real object of WebViewCore
    // can be gone. Check before using it.
    if (!obj.get())
        return;

    env->CallVoidMethod(obj.get(), m_javaGlue->m_showRect, left, top, width,
            height, contentWidth, contentHeight, xPercentInDoc, xPercentInView,
            yPercentInDoc, yPercentInView);
    checkException(env);
}

//----------------------------------------------------------------------
// Native JNI methods
//----------------------------------------------------------------------
static jstring WebCoreStringToJString(JNIEnv *env, WebCore::String string)
{
    int length = string.length();
    if (!length)
        return 0;
    jstring ret = env->NewString((jchar *)string.characters(), length);
    env->DeleteLocalRef(ret);
    return ret;
}

static void UpdateFrameCacheIfLoading(JNIEnv *env, jobject obj)
{
    GET_NATIVE_VIEW(env, obj)->updateFrameCacheIfLoading();
}

static void SetSize(JNIEnv *env, jobject obj, jint width, jint height,
        jint screenWidth, jfloat scale, jint realScreenWidth, jint screenHeight,
        jint anchorX, jint anchorY, jboolean ignoreHeight)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOGV("webviewcore::nativeSetSize(%u %u)\n viewImpl: %p", (unsigned)width, (unsigned)height, viewImpl);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeSetSize");
    viewImpl->setSizeScreenWidthAndScale(width, height, screenWidth, scale,
        realScreenWidth, screenHeight, anchorX, anchorY, ignoreHeight);
}

static void SetScrollOffset(JNIEnv *env, jobject obj, jint gen, jint x, jint y)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "need viewImpl");

    viewImpl->setScrollOffset(gen, x, y);
}

static void SetGlobalBounds(JNIEnv *env, jobject obj, jint x, jint y, jint h,
                            jint v)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "need viewImpl");

    viewImpl->setGlobalBounds(x, y, h, v);
}

static jboolean Key(JNIEnv *env, jobject obj, jint keyCode, jint unichar,
        jint repeatCount, jboolean isShift, jboolean isAlt, jboolean isSym,
        jboolean isDown)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    return GET_NATIVE_VIEW(env, obj)->key(PlatformKeyboardEvent(keyCode,
        unichar, repeatCount, isDown, isShift, isAlt, isSym));
}

static void Click(JNIEnv *env, jobject obj, int framePtr, int nodePtr)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in Click");

    viewImpl->click(reinterpret_cast<WebCore::Frame*>(framePtr),
        reinterpret_cast<WebCore::Node*>(nodePtr));
}

static void DeleteSelection(JNIEnv *env, jobject obj, jint start, jint end,
        jint textGeneration)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    viewImpl->deleteSelection(start, end, textGeneration);
}

static void SetSelection(JNIEnv *env, jobject obj, jint start, jint end)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    viewImpl->setSelection(start, end);
}


static void ReplaceTextfieldText(JNIEnv *env, jobject obj,
    jint oldStart, jint oldEnd, jstring replace, jint start, jint end,
    jint textGeneration)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    WebCore::String webcoreString = to_string(env, replace);
    viewImpl->replaceTextfieldText(oldStart,
            oldEnd, webcoreString, start, end, textGeneration);
}

static void PassToJs(JNIEnv *env, jobject obj,
    jint generation, jstring currentText, jint keyCode,
    jint keyValue, jboolean down, jboolean cap, jboolean fn, jboolean sym)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebCore::String current = to_string(env, currentText);
    GET_NATIVE_VIEW(env, obj)->passToJs(generation, current,
        PlatformKeyboardEvent(keyCode, keyValue, 0, down, cap, fn, sym));
}

static void ScrollFocusedTextInput(JNIEnv *env, jobject obj, jfloat xPercent,
    jint y)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    viewImpl->scrollFocusedTextInput(xPercent, y);
}

static void SetFocusControllerActive(JNIEnv *env, jobject obj, jboolean active)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    LOGV("webviewcore::nativeSetFocusControllerActive()\n");
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeSetFocusControllerActive");
    viewImpl->setFocusControllerActive(active);
}

static void SaveDocumentState(JNIEnv *env, jobject obj, jint frame)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    LOGV("webviewcore::nativeSaveDocumentState()\n");
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeSaveDocumentState");
    viewImpl->saveDocumentState((WebCore::Frame*) frame);
}

void WebViewCore::addVisitedLink(const UChar* string, int length)
{
    if (m_groupForVisitedLinks)
        m_groupForVisitedLinks->addVisitedLink(string, length);
}

static bool RecordContent(JNIEnv *env, jobject obj, jobject region, jobject pt)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    SkRegion* nativeRegion = GraphicsJNI::getNativeRegion(env, region);
    SkIPoint nativePt;
    bool result = viewImpl->recordContent(nativeRegion, &nativePt);
    GraphicsJNI::ipoint_to_jpoint(nativePt, env, pt);
    return result;
}

static void SplitContent(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    viewImpl->splitContent();
}

static void SendListBoxChoice(JNIEnv* env, jobject obj, jint choice)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeSendListBoxChoice");
    viewImpl->popupReply(choice);
}

// Set aside a predetermined amount of space in which to place the listbox
// choices, to avoid unnecessary allocations.
// The size here is arbitrary.  We want the size to be at least as great as the
// number of items in the average multiple-select listbox.
#define PREPARED_LISTBOX_STORAGE 10

static void SendListBoxChoices(JNIEnv* env, jobject obj, jbooleanArray jArray,
        jint size)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeSendListBoxChoices");
    jboolean* ptrArray = env->GetBooleanArrayElements(jArray, 0);
    SkAutoSTMalloc<PREPARED_LISTBOX_STORAGE, int> storage(size);
    int* array = storage.get();
    int count = 0;
    for (int i = 0; i < size; i++) {
        if (ptrArray[i]) {
            array[count++] = i;
        }
    }
    env->ReleaseBooleanArrayElements(jArray, ptrArray, JNI_ABORT);
    viewImpl->popupReply(array, count);
}

static jstring FindAddress(JNIEnv *env, jobject obj, jstring addr,
    jboolean caseInsensitive)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    if (!addr)
        return 0;
    int length = env->GetStringLength(addr);
    if (!length)
        return 0;
    const jchar* addrChars = env->GetStringChars(addr, 0);
    int start, end;
    bool success = CacheBuilder::FindAddress(addrChars, length,
        &start, &end, caseInsensitive) == CacheBuilder::FOUND_COMPLETE;
    jstring ret = 0;
    if (success) {
        ret = env->NewString((jchar*) addrChars + start, end - start);
        env->DeleteLocalRef(ret);
    }
    env->ReleaseStringChars(addr, addrChars);
    return ret;
}

static jboolean HandleTouchEvent(JNIEnv *env, jobject obj, jint action, jint x, jint y)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    return viewImpl->handleTouchEvent(action, x, y);
}

static void TouchUp(JNIEnv *env, jobject obj, jint touchGeneration,
        jint frame, jint node, jint x, jint y)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    viewImpl->touchUp(touchGeneration,
        (WebCore::Frame*) frame, (WebCore::Node*) node, x, y);
}

static jstring RetrieveHref(JNIEnv *env, jobject obj, jint frame,
        jint node)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    WebCore::String result = viewImpl->retrieveHref((WebCore::Frame*) frame,
            (WebCore::Node*) node);
    if (!result.isEmpty())
        return WebCoreStringToJString(env, result);
    return 0;
}

static void MoveMouse(JNIEnv *env, jobject obj, jint frame,
        jint x, jint y)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    viewImpl->moveMouse((WebCore::Frame*) frame, x, y);
}

static void MoveMouseIfLatest(JNIEnv *env, jobject obj, jint moveGeneration,
        jint frame, jint x, jint y)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    viewImpl->moveMouseIfLatest(moveGeneration,
        (WebCore::Frame*) frame, x, y);
}

static void UpdateFrameCache(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    viewImpl->updateFrameCache();
}

static jint GetContentMinPrefWidth(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);

    WebCore::Frame* frame = viewImpl->mainFrame();
    if (frame) {
        WebCore::Document* document = frame->document();
        if (document) {
            WebCore::RenderObject* renderer = document->renderer();
            if (renderer && renderer->isRenderView()) {
                return renderer->minPrefWidth();
            }
        }
    }
    return 0;
}

static void SetViewportSettingsFromNative(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);

    WebCore::Settings* s = viewImpl->mainFrame()->page()->settings();
    if (!s)
        return;

#ifdef ANDROID_META_SUPPORT
    env->SetIntField(obj, gWebViewCoreFields.m_viewportWidth, s->viewportWidth());
    env->SetIntField(obj, gWebViewCoreFields.m_viewportHeight, s->viewportHeight());
    env->SetIntField(obj, gWebViewCoreFields.m_viewportInitialScale, s->viewportInitialScale());
    env->SetIntField(obj, gWebViewCoreFields.m_viewportMinimumScale, s->viewportMinimumScale());
    env->SetIntField(obj, gWebViewCoreFields.m_viewportMaximumScale, s->viewportMaximumScale());
    env->SetBooleanField(obj, gWebViewCoreFields.m_viewportUserScalable, s->viewportUserScalable());
    env->SetIntField(obj, gWebViewCoreFields.m_viewportDensityDpi, s->viewportTargetDensityDpi());
#endif
}

static void SetBackgroundColor(JNIEnv *env, jobject obj, jint color)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);

    viewImpl->setBackgroundColor((SkColor) color);
}

static jstring GetSelection(JNIEnv *env, jobject obj, jobject selRgn)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    SkRegion* selectionRegion = GraphicsJNI::getNativeRegion(env, selRgn);
    WebCore::String result = viewImpl->getSelection(selectionRegion);
    if (!result.isEmpty())
        return WebCoreStringToJString(env, result);
    return 0;
}

static void DumpDomTree(JNIEnv *env, jobject obj, jboolean useFile)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);

    viewImpl->dumpDomTree(useFile);
}

static void DumpRenderTree(JNIEnv *env, jobject obj, jboolean useFile)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);

    viewImpl->dumpRenderTree(useFile);
}

static void DumpNavTree(JNIEnv *env, jobject obj)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);

    viewImpl->dumpNavTree();
}

static void SetJsFlags(JNIEnv *env, jobject obj, jstring flags)
{
#if USE(V8)
    WebCore::String flagsString = to_string(env, flags);
    WebCore::CString utf8String = flagsString.utf8();
    WebCore::ScriptController::setFlags(utf8String.data(), utf8String.length());
#endif
}


// Called from the Java side to set a new quota for the origin or new appcache
// max size in response to a notification that the original quota was exceeded or
// that the appcache has reached its maximum size.
static void SetNewStorageLimit(JNIEnv* env, jobject obj, jlong quota) {
#if ENABLE(DATABASE) || ENABLE(OFFLINE_WEB_APPLICATIONS)
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    Frame* frame = viewImpl->mainFrame();

    // The main thread is blocked awaiting this response, so now we can wake it
    // up.
    ChromeClientAndroid* chromeC = static_cast<ChromeClientAndroid*>(frame->page()->chrome()->client());
    chromeC->wakeUpMainThreadWithNewQuota(quota);
#endif
}

// Called from Java to provide a Geolocation permission state for the specified origin.
static void GeolocationPermissionsProvide(JNIEnv* env, jobject obj, jstring origin, jboolean allow, jboolean remember) {
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    Frame* frame = viewImpl->mainFrame();

    ChromeClientAndroid* chromeClient = static_cast<ChromeClientAndroid*>(frame->page()->chrome()->client());
    chromeClient->provideGeolocationPermissions(to_string(env, origin), allow, remember);
}

static void RegisterURLSchemeAsLocal(JNIEnv* env, jobject obj, jstring scheme) {
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebCore::SecurityOrigin::registerURLSchemeAsLocal(to_string(env, scheme));
}

static void ClearContent(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    viewImpl->clearContent();
}

static void CopyContentToPicture(JNIEnv *env, jobject obj, jobject pict)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    if (!viewImpl)
        return;
    SkPicture* picture = GraphicsJNI::getNativePicture(env, pict);
    viewImpl->copyContentToPicture(picture);
}

static bool DrawContent(JNIEnv *env, jobject obj, jobject canv, jint color)
{
    // Note: this is called from UI thread, don't count it for WebViewCoreTimeCounter
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    SkCanvas* canvas = GraphicsJNI::getNativeCanvas(env, canv);
    return viewImpl->drawContent(canvas, color);
}

static bool PictureReady(JNIEnv* env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->pictureReady();
}

static void UpdatePluginState(JNIEnv* env, jobject obj, jint framePtr, jint nodePtr, jint state)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeUpdatePluginState");
    viewImpl->updatePluginState((WebCore::Frame*) framePtr, (WebCore::Node*) nodePtr,
                                (PluginState) state);
}

static void Pause(JNIEnv* env, jobject obj)
{
    // This is called for the foreground tab when the browser is put to the
    // background (and also for any tab when it is put to the background of the
    // browser). The browser can only be killed by the system when it is in the
    // background, so saving the Geolocation permission state now ensures that
    // is maintained when the browser is killed.
    ChromeClient* chromeClient = GET_NATIVE_VIEW(env, obj)->mainFrame()->page()->chrome()->client();
    ChromeClientAndroid* chromeClientAndroid = static_cast<ChromeClientAndroid*>(chromeClient);
    chromeClientAndroid->storeGeolocationPermissions();

    Frame* mainFrame = GET_NATIVE_VIEW(env, obj)->mainFrame();
    for (Frame* frame = mainFrame; frame; frame = frame->tree()->traverseNext()) {
        Geolocation* geolocation = frame->domWindow()->navigator()->optionalGeolocation();
        if (geolocation)
            geolocation->suspend();
    }

    ANPEvent event;
    SkANP::InitEvent(&event, kLifecycle_ANPEventType);
    event.data.lifecycle.action = kPause_ANPLifecycleAction;
    GET_NATIVE_VIEW(env, obj)->sendPluginEvent(event);

    WebViewCore::setIsPaused(true);
}

static void Resume(JNIEnv* env, jobject obj)
{
    Frame* mainFrame = GET_NATIVE_VIEW(env, obj)->mainFrame();
    for (Frame* frame = mainFrame; frame; frame = frame->tree()->traverseNext()) {
        Geolocation* geolocation = frame->domWindow()->navigator()->optionalGeolocation();
        if (geolocation)
            geolocation->resume();
    }

    ANPEvent event;
    SkANP::InitEvent(&event, kLifecycle_ANPEventType);
    event.data.lifecycle.action = kResume_ANPLifecycleAction;
    GET_NATIVE_VIEW(env, obj)->sendPluginEvent(event);

    WebViewCore::setIsPaused(false);
}

static void FreeMemory(JNIEnv* env, jobject obj)
{
    ANPEvent event;
    SkANP::InitEvent(&event, kLifecycle_ANPEventType);
    event.data.lifecycle.action = kFreeMemory_ANPLifecycleAction;
    GET_NATIVE_VIEW(env, obj)->sendPluginEvent(event);
}

static void ProvideVisitedHistory(JNIEnv *env, jobject obj, jobject hist)
{
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);

    jobjectArray array = static_cast<jobjectArray>(hist);

    jsize len = env->GetArrayLength(array);
    for (jsize i = 0; i < len; i++) {
        jstring item = static_cast<jstring>(env->GetObjectArrayElement(array, i));
        const UChar* str = static_cast<const UChar*>(env->GetStringChars(item, NULL));
        jsize len = env->GetStringLength(item);
        viewImpl->addVisitedLink(str, len);
        env->ReleaseStringChars(item, str);
        env->DeleteLocalRef(item);
    }
    env->DeleteLocalRef(array);
}

// ----------------------------------------------------------------------------

/*
 * JNI registration.
 */
static JNINativeMethod gJavaWebViewCoreMethods[] = {
    { "nativeClearContent", "()V",
        (void*) ClearContent },
    { "nativeCopyContentToPicture", "(Landroid/graphics/Picture;)V",
        (void*) CopyContentToPicture },
    { "nativeDrawContent", "(Landroid/graphics/Canvas;I)Z",
        (void*) DrawContent } ,
    { "nativeKey", "(IIIZZZZ)Z",
        (void*) Key },
    { "nativeClick", "(II)V",
        (void*) Click },
    { "nativePictureReady", "()Z",
        (void*) PictureReady } ,
    { "nativeSendListBoxChoices", "([ZI)V",
        (void*) SendListBoxChoices },
    { "nativeSendListBoxChoice", "(I)V",
        (void*) SendListBoxChoice },
    { "nativeSetSize", "(IIIFIIIIZ)V",
        (void*) SetSize },
    { "nativeSetScrollOffset", "(III)V",
        (void*) SetScrollOffset },
    { "nativeSetGlobalBounds", "(IIII)V",
        (void*) SetGlobalBounds },
    { "nativeSetSelection", "(II)V",
        (void*) SetSelection } ,
    { "nativeDeleteSelection", "(III)V",
        (void*) DeleteSelection } ,
    { "nativeReplaceTextfieldText", "(IILjava/lang/String;III)V",
        (void*) ReplaceTextfieldText } ,
    { "nativeMoveMouse", "(III)V",
        (void*) MoveMouse },
    { "nativeMoveMouseIfLatest", "(IIII)V",
        (void*) MoveMouseIfLatest },
    { "passToJs", "(ILjava/lang/String;IIZZZZ)V",
        (void*) PassToJs },
    { "nativeScrollFocusedTextInput", "(FI)V",
        (void*) ScrollFocusedTextInput },
    { "nativeSetFocusControllerActive", "(Z)V",
        (void*) SetFocusControllerActive },
    { "nativeSaveDocumentState", "(I)V",
        (void*) SaveDocumentState },
    { "nativeFindAddress", "(Ljava/lang/String;Z)Ljava/lang/String;",
        (void*) FindAddress },
    { "nativeHandleTouchEvent", "(III)Z",
            (void*) HandleTouchEvent },
    { "nativeTouchUp", "(IIIII)V",
        (void*) TouchUp },
    { "nativeRetrieveHref", "(II)Ljava/lang/String;",
        (void*) RetrieveHref },
    { "nativeUpdateFrameCache", "()V",
        (void*) UpdateFrameCache },
    { "nativeGetContentMinPrefWidth", "()I",
        (void*) GetContentMinPrefWidth },
    { "nativeRecordContent", "(Landroid/graphics/Region;Landroid/graphics/Point;)Z",
        (void*) RecordContent },
    { "setViewportSettingsFromNative", "()V",
        (void*) SetViewportSettingsFromNative },
    { "nativeSplitContent", "()V",
        (void*) SplitContent },
    { "nativeSetBackgroundColor", "(I)V",
        (void*) SetBackgroundColor },
    { "nativeGetSelection", "(Landroid/graphics/Region;)Ljava/lang/String;",
        (void*) GetSelection },
    { "nativeRegisterURLSchemeAsLocal", "(Ljava/lang/String;)V",
        (void*) RegisterURLSchemeAsLocal },
    { "nativeDumpDomTree", "(Z)V",
        (void*) DumpDomTree },
    { "nativeDumpRenderTree", "(Z)V",
        (void*) DumpRenderTree },
    { "nativeDumpNavTree", "()V",
        (void*) DumpNavTree },
    { "nativeSetNewStorageLimit", "(J)V",
        (void*) SetNewStorageLimit },
    { "nativeGeolocationPermissionsProvide", "(Ljava/lang/String;ZZ)V",
        (void*) GeolocationPermissionsProvide },
    { "nativePause", "()V", (void*) Pause },
    { "nativeResume", "()V", (void*) Resume },
    { "nativeFreeMemory", "()V", (void*) FreeMemory },
    { "nativeSetJsFlags", "(Ljava/lang/String;)V", (void*) SetJsFlags },
    { "nativeUpdatePluginState", "(III)V", (void*) UpdatePluginState },
    { "nativeUpdateFrameCacheIfLoading", "()V",
        (void*) UpdateFrameCacheIfLoading },
    { "nativeProvideVisitedHistory", "([Ljava/lang/String;)V",
        (void*) ProvideVisitedHistory },
};

int register_webviewcore(JNIEnv* env)
{
    jclass widget = env->FindClass("android/webkit/WebViewCore");
    LOG_ASSERT(widget,
            "Unable to find class android/webkit/WebViewCore");
    gWebViewCoreFields.m_nativeClass = env->GetFieldID(widget, "mNativeClass",
            "I");
    LOG_ASSERT(gWebViewCoreFields.m_nativeClass,
            "Unable to find android/webkit/WebViewCore.mNativeClass");
    gWebViewCoreFields.m_viewportWidth = env->GetFieldID(widget,
            "mViewportWidth", "I");
    LOG_ASSERT(gWebViewCoreFields.m_viewportWidth,
            "Unable to find android/webkit/WebViewCore.mViewportWidth");
    gWebViewCoreFields.m_viewportHeight = env->GetFieldID(widget,
            "mViewportHeight", "I");
    LOG_ASSERT(gWebViewCoreFields.m_viewportHeight,
            "Unable to find android/webkit/WebViewCore.mViewportHeight");
    gWebViewCoreFields.m_viewportInitialScale = env->GetFieldID(widget,
            "mViewportInitialScale", "I");
    LOG_ASSERT(gWebViewCoreFields.m_viewportInitialScale,
            "Unable to find android/webkit/WebViewCore.mViewportInitialScale");
    gWebViewCoreFields.m_viewportMinimumScale = env->GetFieldID(widget,
            "mViewportMinimumScale", "I");
    LOG_ASSERT(gWebViewCoreFields.m_viewportMinimumScale,
            "Unable to find android/webkit/WebViewCore.mViewportMinimumScale");
    gWebViewCoreFields.m_viewportMaximumScale = env->GetFieldID(widget,
            "mViewportMaximumScale", "I");
    LOG_ASSERT(gWebViewCoreFields.m_viewportMaximumScale,
            "Unable to find android/webkit/WebViewCore.mViewportMaximumScale");
    gWebViewCoreFields.m_viewportUserScalable = env->GetFieldID(widget,
            "mViewportUserScalable", "Z");
    LOG_ASSERT(gWebViewCoreFields.m_viewportUserScalable,
            "Unable to find android/webkit/WebViewCore.mViewportUserScalable");
    gWebViewCoreFields.m_viewportDensityDpi = env->GetFieldID(widget,
            "mViewportDensityDpi", "I");
    LOG_ASSERT(gWebViewCoreFields.m_viewportDensityDpi,
            "Unable to find android/webkit/WebViewCore.mViewportDensityDpi");
    gWebViewCoreFields.m_webView = env->GetFieldID(widget,
            "mWebView", "Landroid/webkit/WebView;");
    LOG_ASSERT(gWebViewCoreFields.m_webView,
            "Unable to find android/webkit/WebViewCore.mWebView");

    return jniRegisterNativeMethods(env, "android/webkit/WebViewCore",
            gJavaWebViewCoreMethods, NELEM(gJavaWebViewCoreMethods));
}

} /* namespace android */
