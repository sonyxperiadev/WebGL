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
#include "Color.h"
#include "Document.h"
#include "Element.h"
#include "Editor.h"
#include "EditorClientAndroid.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Font.h"
#include "FrameLoader.h"
#include "FrameLoaderClientAndroid.h"
#include "FrameTree.h"
#include "FrameView.h"
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
#include "Node.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformString.h"
#include "PluginInfoStore.h"
#include "PluginWidgetAndroid.h"
#include "Position.h"
#include "ProgressTracker.h"
#include "RenderLayer.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderThemeAndroid.h"
#include "RenderView.h"
#include "ResourceRequest.h"
#include "SelectionController.h"
#include "Settings.h"
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
    jmethodID   m_didFirstLayout;
    jmethodID   m_sendMarkNodeInvalid;
    jmethodID   m_sendNotifyFocusSet;
    jmethodID   m_sendNotifyProgressFinished;
    jmethodID   m_sendRecomputeFocus;
    jmethodID   m_sendViewInvalidate;
    jmethodID   m_updateTextfield;
    jmethodID   m_restoreScale;
    jmethodID   m_needTouchEvents;
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
Mutex WebViewCore::gFrameGenerationMutex;
Mutex WebViewCore::gRecomputeFocusMutex;
Mutex WebViewCore::gButtonMutex;
Mutex WebViewCore::m_contentMutex;

WebViewCore::WebViewCore(JNIEnv* env, jobject javaWebViewCore, WebCore::Frame* mainframe)
        : m_pluginInvalTimer(this, &WebViewCore::pluginInvalTimerFired)
{
    m_mainFrame = mainframe;

    m_popupReply = 0;
    m_buildGeneration = 0;
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
    m_scale = 100;

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
    m_javaGlue->m_didFirstLayout = GetJMethod(env, clazz, "didFirstLayout", "()V");
    m_javaGlue->m_sendMarkNodeInvalid = GetJMethod(env, clazz, "sendMarkNodeInvalid", "(I)V");
    m_javaGlue->m_sendNotifyFocusSet = GetJMethod(env, clazz, "sendNotifyFocusSet", "()V");
    m_javaGlue->m_sendNotifyProgressFinished = GetJMethod(env, clazz, "sendNotifyProgressFinished", "()V");
    m_javaGlue->m_sendRecomputeFocus = GetJMethod(env, clazz, "sendRecomputeFocus", "()V");
    m_javaGlue->m_sendViewInvalidate = GetJMethod(env, clazz, "sendViewInvalidate", "(IIII)V");
    m_javaGlue->m_updateTextfield = GetJMethod(env, clazz, "updateTextfield", "(IZLjava/lang/String;I)V");
    m_javaGlue->m_restoreScale = GetJMethod(env, clazz, "restoreScale", "(I)V");
    m_javaGlue->m_needTouchEvents = GetJMethod(env, clazz, "needTouchEvents", "(Z)V");

    env->SetIntField(javaWebViewCore, gWebViewCoreFields.m_nativeClass, (jint)this);

    m_scrollOffsetX = m_scrollOffsetY = 0;

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
    clearContent();
    m_updatedFrameCache = true;
    m_frameCacheOutOfDate = true;
    m_blockFocusChange = false;
    m_snapAnchorNode = 0;
    m_useReplay = false;
    m_skipContentDraw = false;
    m_findIsUp = false;
    m_domtree_version = 0;
    m_check_domtree_version = true;
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

void WebViewCore::recordPicture(SkPicture* picture)
{
    // if there is no document yet, just return
    if (!m_mainFrame->document())
        return;
    // Call layout to ensure that the contentWidth and contentHeight are correct
    if (!layoutIfNeededRecursive(m_mainFrame))
        return;
    // draw into the picture's recording canvas
    WebCore::FrameView* view = m_mainFrame->view();
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
    CacheBuilder& builder = FrameLoaderClientAndroid::get(m_mainFrame)->getCacheBuilder();
    WebCore::Node* oldFocusNode = builder.currentFocus();
    m_frameCacheOutOfDate = true;
    WebCore::IntRect oldBounds = oldFocusNode ?
        oldFocusNode->getRect() : WebCore::IntRect(0,0,0,0);
    DBG_NAV_LOGD("m_lastFocused=%p oldFocusNode=%p"
        " m_lastFocusedBounds={%d,%d,%d,%d} oldBounds={%d,%d,%d,%d}",
        m_lastFocused, oldFocusNode,
        m_lastFocusedBounds.x(), m_lastFocusedBounds.y(), m_lastFocusedBounds.width(), m_lastFocusedBounds.height(),
        oldBounds.x(), oldBounds.y(), oldBounds.width(), oldBounds.height());
    unsigned latestVersion = 0;
    if (m_check_domtree_version) {
        // as domTreeVersion only increment, we can just check the sum to see 
        // whether we need to update the frame cache
        for (Frame* frame = m_mainFrame; frame; frame = frame->tree()->traverseNext()) {
            latestVersion += frame->document()->domTreeVersion();
        }
    }
    if (m_lastFocused != oldFocusNode || m_lastFocusedBounds != oldBounds || m_findIsUp 
            || (m_check_domtree_version && latestVersion != m_domtree_version)) {
        m_lastFocused = oldFocusNode;
        m_lastFocusedBounds = oldBounds;
        DBG_NAV_LOGD("call updateFrameCache m_domtree_version=%d latest=%d",
            m_domtree_version, latestVersion);
        m_domtree_version = latestVersion;
        updateFrameCache();
    }
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
    m_contentMutex.lock();
    PictureSet contentCopy(m_content);
    m_contentMutex.unlock();
    recordPictureSet(&contentCopy);
    float progress = (float) m_mainFrame->page()->progress()->estimatedProgress();
    if (progress > 0.0f && progress < 1.0f && contentCopy.isEmpty()) {
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
    env->CallVoidMethod(m_javaGlue->object(env).get(), animate ? m_javaGlue->m_spawnScrollTo : m_javaGlue->m_scrollTo, x, y);
    checkException(env);
}

void WebViewCore::sendMarkNodeInvalid(WebCore::Node* node)
{
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue->object(env).get(), m_javaGlue->m_sendMarkNodeInvalid, (int) node);
    checkException(env);
}

void WebViewCore::sendNotifyFocusSet()
{
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue->object(env).get(), m_javaGlue->m_sendNotifyFocusSet);
    checkException(env);
}

void WebViewCore::sendNotifyProgressFinished()
{
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue->object(env).get(), m_javaGlue->m_sendNotifyProgressFinished);
    checkException(env);
}

void WebViewCore::sendRecomputeFocus()
{
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue->object(env).get(), m_javaGlue->m_sendRecomputeFocus);
    checkException(env);
}

void WebViewCore::viewInvalidate(const WebCore::IntRect& rect)
{    
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue->object(env).get(),
                        m_javaGlue->m_sendViewInvalidate,
                        rect.x(), rect.y(), rect.right(), rect.bottom());
    checkException(env);
}

void WebViewCore::scrollBy(int dx, int dy, bool animate)
{
    if (!(dx | dy))
        return;
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue->object(env).get(), m_javaGlue->m_scrollBy, 
        dx, dy, animate);
    checkException(env);
}

void WebViewCore::contentDraw()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue->object(env).get(), m_javaGlue->m_contentDraw);
    checkException(env);
}

void WebViewCore::contentInvalidate(const WebCore::IntRect &r)
{
    DBG_SET_LOGD("rect={%d,%d,w=%d,h=%d}", r.x(), r.y(), r.width(), r.height());
    SkIRect rect, max;
    android_setrect(&rect, r);
    max.set(0, 0, INT_MAX, INT_MAX);
    if (!rect.intersect(max))
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

    const WebCore::KURL& url = m_mainFrame->loader()->url();
    if (url.isEmpty())
        return;
    LOGV("::WebCore:: didFirstLayout %s", url.string().ascii().data());

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue->object(env).get(), m_javaGlue->m_didFirstLayout);
    checkException(env);

    DBG_NAV_LOG("call updateFrameCache");
    m_check_domtree_version = false;
    updateFrameCache();
    m_history.setDidFirstLayout(true);
}

void WebViewCore::restoreScale(int scale)
{
    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__);
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue->object(env).get(), m_javaGlue->m_restoreScale, scale);
    checkException(env);
}

void WebViewCore::needTouchEvents(bool need)
{
    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__);
    LOG_ASSERT(m_javaGlue->m_obj, "A Java widget was not associated with this view bridge!");

#if ENABLE(TOUCH_EVENTS) // Android
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue->object(env).get(), m_javaGlue->m_needTouchEvents, need);
    checkException(env);
#endif
}

void WebViewCore::notifyFocusSet()
{
    sendNotifyFocusSet();
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

void WebViewCore::setScrollOffset(int dx, int dy)
{
    DBG_NAV_LOGD("{%d,%d}", dx, dy);
    if (m_scrollOffsetX != dx || m_scrollOffsetY != dy) {
        m_scrollOffsetX = dx;
        m_scrollOffsetY = dy;
        // The visible rect is located within our coordinate space so it
        // contains the actual scroll position. Setting the location makes hit
        // testing work correctly.
        m_mainFrame->view()->platformWidget()->setLocation(m_scrollOffsetX,
                m_scrollOffsetY);
        m_mainFrame->sendScrollEvent();
    }
}

void WebViewCore::setGlobalBounds(int x, int y, int h, int v)
{
    DBG_NAV_LOGD("{%d,%d}", x, y);
    m_mainFrame->view()->platformWidget()->setWindowBounds(x, y, h, v);
}

void WebViewCore::setSizeScreenWidthAndScale(int width, int height, 
    int screenWidth, int scale, int realScreenWidth, int screenHeight)
{
    WebCoreViewBridge* window = m_mainFrame->view()->platformWidget();
    int ow = window->width();
    int oh = window->height();
    window->setSize(width, height);
    int osw = m_screenWidth;
    DBG_NAV_LOGD("old:(w=%d,h=%d,sw=%d,scale=%d) new:(w=%d,h=%d,sw=%d,scale=%d)",
        ow, oh, osw, m_scale, width, height, screenWidth, scale);
    m_screenWidth = screenWidth;
    m_scale = scale;
    m_maxXScroll = screenWidth >> 2;
    m_maxYScroll = (screenWidth * height / width) >> 2;
    if (ow != width || oh != height || osw != screenWidth) {
        WebCore::RenderObject *r = m_mainFrame->contentRenderer();
        DBG_NAV_LOGD("renderer=%p view=(w=%d,h=%d)", r, 
            realScreenWidth, screenHeight);
        if (r) {
            // get current screen center position
            WebCore::IntPoint screenCenter = WebCore::IntPoint(
                m_scrollOffsetX + (realScreenWidth >> 1),
                m_scrollOffsetY + (screenHeight >> 1));
            WebCore::HitTestResult hitTestResult = m_mainFrame->eventHandler()->
                hitTestResultAtPoint(screenCenter, false);
            WebCore::Node* node = hitTestResult.innerNode();
            WebCore::IntRect bounds;
            WebCore::IntPoint offset;
            if (node) {
                bounds = node->getRect();
                DBG_NAV_LOGD("ob:(x=%d,y=%d,w=%d,h=%d)",
                    bounds.x(), bounds.y(), bounds.width(), bounds.height());
                offset = WebCore::IntPoint(screenCenter.x() - bounds.x(),
                    screenCenter.y() - bounds.y());
                if (offset.x() < 0 || offset.x() > realScreenWidth ||
                    offset.y() < 0 || offset.y() > screenHeight) 
                {
                    DBG_NAV_LOGD("offset out of bounds:(x=%d,y=%d)",
                        offset.x(), offset.y());
                    node = 0;
                }
            }
            r->setNeedsLayoutAndPrefWidthsRecalc();
            m_mainFrame->forceLayout();
            // scroll to restore current screen center
            if (!node)
                return;
            const WebCore::IntRect& newBounds = node->getRect();
            DBG_NAV_LOGD("nb:(x=%d,y=%d,w=%d,"
                "h=%d,ns=%d)", newBounds.x(), newBounds.y(), 
                newBounds.width(), newBounds.height());
            scrollBy(newBounds.x() - bounds.x(), newBounds.y() - bounds.y(),
                false);
        }
    }
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
    FrameLoaderClientAndroid::get(m_mainFrame)->getCacheBuilder().mDebug.print();
#endif
}

WebCore::String WebViewCore::retrieveHref(WebCore::Frame* frame, WebCore::Node* node)
{
    CacheBuilder& builder = FrameLoaderClientAndroid::get(m_mainFrame)->getCacheBuilder();
    if (!builder.validNode(frame, node))
        return WebCore::String();
    if (!node->hasTagName(WebCore::HTMLNames::aTag))
        return WebCore::String();
    WebCore::HTMLAnchorElement* anchor = static_cast<WebCore::HTMLAnchorElement*>(node);
    return anchor->href();
}

bool WebViewCore::prepareFrameCache()
{
    if (!m_frameCacheOutOfDate) {
        DBG_NAV_LOG("!m_frameCacheOutOfDate");
        return false;
    }
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreBuildNavTimeCounter);
#endif
    m_frameCacheOutOfDate = false;
#if DEBUG_NAV_UI
    DBG_NAV_LOG("m_frameCacheOutOfDate was true");
    m_now = SkTime::GetMSecs();
#endif
    m_temp = new CachedRoot();
    m_temp->init(m_mainFrame, &m_history);
    m_temp->setGeneration(++m_buildGeneration);
    CacheBuilder& builder = FrameLoaderClientAndroid::get(m_mainFrame)->getCacheBuilder();
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
    return true;
}

void WebViewCore::releaseFrameCache(bool newCache)
{
    if (!newCache) {
        DBG_NAV_LOG("!newCache");
        return;
    }
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
    notifyFocusSet();
    // it's tempting to send an invalidate here, but it's a bad idea
    // the cache is now up to date, but the focus is not -- the event
    // may need to be recomputed from the prior history. An invalidate
    // will draw the stale location causing the ring to flash at the wrong place.
}

void WebViewCore::updateFrameCache()
{
    m_useReplay = false;
    releaseFrameCache(prepareFrameCache());
}

void WebViewCore::removeFrameGeneration(WebCore::Frame* frame)
{
    DBG_NAV_LOGD("frame=%p m_generation=%d", frame, m_generation);
    gFrameGenerationMutex.lock();
    int last = m_frameGenerations.size() - 1;
    for (int index = 0; index <= last; index++) {
        if (m_frameGenerations[index].m_frame == frame) {
            DBG_NAV_LOGD("index=%d last=%d", index, last);
            if (index != last)
                m_frameGenerations[index] = m_frameGenerations[last];
            m_frameGenerations.removeLast();
            break;
        }
    }
    gFrameGenerationMutex.unlock();
}

void WebViewCore::updateFrameGeneration(WebCore::Frame* frame)
{
    DBG_NAV_LOGD("frame=%p m_generation=%d", frame, m_generation);
    gFrameGenerationMutex.lock();
    ++m_buildGeneration;
    for (size_t index = 0; index < m_frameGenerations.size(); index++) {
        if (m_frameGenerations[index].m_frame == frame) {
            DBG_NAV_LOG("replace");
            m_frameGenerations[index].m_generation = m_buildGeneration;
            goto done;
        }
    }
    {
        FrameGen frameGen = {frame, m_buildGeneration};
        m_frameGenerations.append(frameGen);
        DBG_NAV_LOG("append");
    }
done:
    gFrameGenerationMutex.unlock();
}

int WebViewCore::retrieveFrameGeneration(WebCore::Frame* frame)
{
    int result = INT_MAX;
    gFrameGenerationMutex.lock();
    for (size_t index = 0; index < m_frameGenerations.size(); index++) {
        if (m_frameGenerations[index].m_frame == frame) {
            result = m_frameGenerations[index].m_generation;
            break;
        }
    }
    gFrameGenerationMutex.unlock();
    DBG_NAV_LOGD("frame=%p m_generation=%d result=%d", frame, m_generation, result);
    return result;
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
    const double PLUGIN_INVAL_DELAY = 0;    // should this be non-zero?

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
            w->localToPageCoords(&dirty);
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

///////////////////////////////////////////////////////////////////////////////

void WebViewCore::setFinalFocus(WebCore::Frame* frame, WebCore::Node* node,
    int x, int y, bool block)
{
    DBG_NAV_LOGD("frame=%p node=%p x=%d y=%d", frame, node, x, y);
    bool result = finalKitFocus(frame, node, x, y, false);
    if (block) {
        m_blockFocusChange = true;
        if (!result && node)
            touchUp(m_touchGeneration, 0, 0, 0, x, y, 0, true, true);
    }
}

void WebViewCore::setKitFocus(int moveGeneration, int buildGeneration,
    WebCore::Frame* frame, WebCore::Node* node, int x, int y,
    bool ignoreNullFocus)
{
    DBG_NAV_LOGD("m_moveGeneration=%d moveGeneration=%d"
        " buildGeneration=%d frame=%p node=%p x=%d y=%d",
        m_moveGeneration, moveGeneration, buildGeneration, frame, node, x, y);
    if (m_blockFocusChange) {
        DBG_NAV_LOG("m_blockFocusChange");
        return;
    }
    if (m_moveGeneration > moveGeneration) {
        DBG_NAV_LOGD("m_moveGeneration=%d > moveGeneration=%d",
            m_moveGeneration, moveGeneration);
        return; // short-circuit if a newer move has already been generated
    }
    if (!commonKitFocus(moveGeneration, buildGeneration, frame, node, x, y,
            ignoreNullFocus))
        return;
    m_lastGeneration = moveGeneration;
}

bool WebViewCore::commonKitFocus(int generation, int buildGeneration,
    WebCore::Frame* frame, WebCore::Node* node, int x, int y,
    bool ignoreNullFocus)
{
    DBG_NAV_LOGD("generation=%d buildGeneration=%d frame=%p"
        " node=%p x=%d y=%d", generation, buildGeneration, frame, node, x, y);
    m_useReplay = true;
    bool newCache = prepareFrameCache(); // must wait for possible recompute before using
    if (m_moveGeneration > generation) {
        DBG_NAV_LOGD("m_moveGeneration=%d > generation=%d",
            m_moveGeneration, generation);
        releaseFrameCache(newCache);
        return false; // short-circuit if a newer move has already been generated
    }
    // if the nav cache has been rebuilt since this focus request was generated,
    // send a request back to the UI side to recompute the kit-side focus
    if (m_buildGeneration > buildGeneration || (node && !FrameLoaderClientAndroid::get(m_mainFrame)->getCacheBuilder().validNode(frame, node))) {
        DBG_NAV_LOGD("m_buildGeneration=%d > buildGeneration=%d",
           m_buildGeneration, buildGeneration);
        gRecomputeFocusMutex.lock();
        bool first = !m_recomputeEvents.size();
        m_recomputeEvents.append(generation);
        gRecomputeFocusMutex.unlock();
        releaseFrameCache(newCache);
        if (first)
            sendRecomputeFocus();
        return false;
    }
    releaseFrameCache(newCache);
    if (!node && ignoreNullFocus)
        return true;
    finalKitFocus(frame, node, x, y, false);
    return true;
}

// Update mouse position and may change focused node.
// If donotChangeDOMFocus is true, the function does not changed focused node
// in the DOM tree. Changing the focus in DOM may trigger onblur event
// handler on the current focused node before firing mouse up and down events.
bool WebViewCore::finalKitFocus(WebCore::Frame* frame, WebCore::Node* node,
    int x, int y, bool donotChangeDOMFocus)
{
    CacheBuilder& builder = FrameLoaderClientAndroid::
        get(m_mainFrame)->getCacheBuilder();
    if (!frame || builder.validNode(frame, NULL) == false)
        frame = m_mainFrame;
    WebCore::Node* oldFocusNode = builder.currentFocus();
    // mouse event expects the position in the window coordinate
    m_mousePos = WebCore::IntPoint(x - m_scrollOffsetX, y - m_scrollOffsetY);
    // validNode will still return true if the node is null, as long as we have
    // a valid frame.  Do not want to make a call on frame unless it is valid.
    WebCore::PlatformMouseEvent mouseEvent(m_mousePos, m_mousePos,
        WebCore::NoButton, WebCore::MouseEventMoved, 1, false, false, false,
        false, WTF::currentTime());
    frame->eventHandler()->handleMouseMoveEvent(mouseEvent);
    bool valid = builder.validNode(frame, node);
    if (!donotChangeDOMFocus) {
        WebCore::Document* oldDoc = oldFocusNode ? oldFocusNode->document() : 0;
        if (!node) {
            if (oldFocusNode)
                oldDoc->setFocusedNode(0);
            return false;
        } else if (!valid) {
            DBG_NAV_LOGD("sendMarkNodeInvalid node=%p", node);
            sendMarkNodeInvalid(node);
            if (oldFocusNode)
                oldDoc->setFocusedNode(0);
            return false;
        }
        // If we jump frames (docs), kill the focus on the old doc
        if (oldFocusNode && node->document() != oldDoc) {
            oldDoc->setFocusedNode(0);
        }
        if (!node->isTextNode())
            static_cast<WebCore::Element*>(node)->focus(false);
        if (node->document()->focusedNode() != node) {
            // This happens when Element::focus() fails as we may try to set the
            // focus to a node which WebCore doesn't recognize as a focusable node.
            // So we need to do some extra work, as it does in Element::focus(),
            // besides calling Document::setFocusedNode.
            if (oldFocusNode) {
                // copied from clearSelectionIfNeeded in FocusController.cpp
                WebCore::SelectionController* s = oldDoc->frame()->selection();
                if (!s->isNone())
                    s->clear();
            }
            //setFocus on things that WebCore doesn't recognize as supporting focus
            //for instance, if there is an onclick element that does not support focus
            node->document()->setFocusedNode(node);
        }
    } else {   // !donotChangeDOMFocus
        if (!node || !valid)
            return false;
    }

    DBG_NAV_LOGD("setFocusedNode node=%p", node);
    builder.setLastFocus(node);
    m_lastFocused = node;
    m_lastFocusedBounds = node->getRect();
    return true;
}

// helper function to find the frame that has focus
static WebCore::Frame* FocusedFrame(WebCore::Frame* frame)
{
    if (!frame)
        return 0;
    WebCore::Node* focusNode = FrameLoaderClientAndroid::get(frame)->getCacheBuilder().currentFocus();
    if (!focusNode)
        return 0;
    WebCore::Document* doc = focusNode->document();
    if (!doc)
        return 0;
    return doc->frame();
}

static WebCore::RenderTextControl* FocusedTextControl(WebCore::Frame* frame)
{
    WebCore::Node* focusNode = FrameLoaderClientAndroid::get(frame)->getCacheBuilder().currentFocus();
    WebCore::RenderObject* renderer = focusNode->renderer();
    if (renderer && (renderer->isTextField() || renderer->isTextArea())) {
        return static_cast<WebCore::RenderTextControl*>(renderer);
    }
    return 0;
}

WebCore::Frame* WebViewCore::changedKitFocus(WebCore::Frame* frame,
    WebCore::Node* node, int x, int y)
{
    if (!frame || !node)
        return m_mainFrame;
    WebCore::Node* current = FrameLoaderClientAndroid::get(m_mainFrame)->getCacheBuilder().currentFocus();
    if (current == node)
        return frame;
    return finalKitFocus(frame, node, x, y, false) ? frame : m_mainFrame;
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
            textBox->xPos(), textBox->start(), prior, current, next
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
        WebCore::IntPoint endPt = WebCore::IntPoint(rect.fRight - 2, cy);
        hitTestResult = m_mainFrame->eventHandler()->hitTestResultAtPoint(endPt, false);
        WebCore::Node* endNode = hitTestResult.innerNode();
        if (!endNode) {
            DBG_NAV_LOG("!endNode");
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

static void selectInFrame(WebCore::Frame* frame, int start, int end)
{
    WebCore::Document* doc = frame->document();
    if (!doc)
        return;

    WebCore::Node* focus = doc->focusedNode();
    if (!focus)
        return;

    WebCore::RenderObject* renderer = focus->renderer();
    if (renderer && (renderer->isTextField() || renderer->isTextArea())) {
        WebCore::RenderTextControl* rtc = static_cast<WebCore::RenderTextControl*>(renderer);
        if (start > end) {
            int temp = start;
            start = end;
            end = temp;
        }
        rtc->setSelectionRange(start, end);
        frame->revealSelection();
    }
}

WebCore::Frame* WebViewCore::setSelection(WebCore::Frame* frame, WebCore::Node* node,
    int x, int y, int start, int end)
{
    // FIXME: Consider using a generation number to avoid doing this many more times than necessary.
    frame = changedKitFocus(frame, node, x, y);
    if (!frame)
        return 0;
    selectInFrame(frame, start, end);
    return frame;
}

// Shortcut for no modifier keys
#define NO_MODIFIER_KEYS (static_cast<WebCore::PlatformKeyboardEvent::ModifierKey>(0))

WebCore::Frame* WebViewCore::deleteSelection(WebCore::Frame* frame, WebCore::Node* node,
    int x, int y, int start, int end)
{
    frame = setSelection(frame, node, x, y, start, end);
    if (start != end) {
        WebCore::PlatformKeyboardEvent downEvent(kKeyCodeDel, WebCore::VK_BACK,
                WebCore::PlatformKeyboardEvent::KeyDown, 0, NO_MODIFIER_KEYS);
        frame->eventHandler()->keyEvent(downEvent);
        WebCore::PlatformKeyboardEvent upEvent(kKeyCodeDel, WebCore::VK_BACK,
                WebCore::PlatformKeyboardEvent::KeyUp, 0, NO_MODIFIER_KEYS);
        frame->eventHandler()->keyEvent(upEvent);
    }
    return frame;
}

void WebViewCore::replaceTextfieldText(WebCore::Frame* frame, WebCore::Node* node, int x, int y,
        int oldStart, int oldEnd, jstring replace, int start, int end)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();

    WebCore::String webcoreString = to_string(env, replace);
    frame = setSelection(frame, node, x, y, oldStart, oldEnd);
    WebCore::TypingCommand::insertText(frame->document(), webcoreString, false);
    selectInFrame(frame, start, end);
}

void WebViewCore::passToJs(WebCore::Frame* frame, WebCore::Node* node, int x, int y, int generation,
     jstring currentText, int keyCode, int keyValue, bool down, bool cap, bool fn, bool sym)
{
    frame = changedKitFocus(frame, node, x, y);
    // Construct the ModifierKey value
    int mods = 0;
    if (cap) {
        mods |= WebCore::PlatformKeyboardEvent::ShiftKey;
    }
    if (fn) {
        mods |= WebCore::PlatformKeyboardEvent::AltKey;
    }
    if (sym) {
        mods |= WebCore::PlatformKeyboardEvent::CtrlKey;
    }
    WebCore::PlatformKeyboardEvent event(keyCode, keyValue,
            down ? WebCore::PlatformKeyboardEvent::KeyDown :
                   WebCore::PlatformKeyboardEvent::KeyUp,
            0, static_cast<WebCore::PlatformKeyboardEvent::ModifierKey>(mods));
    // Block text field updates during a key press.
    m_blockTextfieldUpdates = true;
    frame->eventHandler()->keyEvent(event);
    m_blockTextfieldUpdates = false;
    m_textGeneration = generation;

    WebCore::Node* currentFocus = FrameLoaderClientAndroid::get(m_mainFrame)->getCacheBuilder().currentFocus();
    // Make sure we have the same focus and it is a text field.
    if (node == currentFocus && currentFocus) {
        WebCore::RenderObject* renderer = currentFocus->renderer();
        if (renderer && (renderer->isTextField() || renderer->isTextArea())) {
            WebCore::RenderTextControl* renderText = static_cast<WebCore::RenderTextControl*>(renderer);
            JNIEnv* env = JSC::Bindings::getJNIEnv();
            WebCore::String current = to_string(env, currentText);
            WebCore::String test = renderText->text();
            // If the text changed during the key event, update the UI text field.
            if (test != current)
                updateTextfield(currentFocus, false, test);
        }
    }
}

void WebViewCore::saveDocumentState(WebCore::Frame* frame)
{
    if (!FrameLoaderClientAndroid::get(m_mainFrame)->getCacheBuilder()
            .validNode(frame, 0))
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
        // If the select element no longer exists, do to a page change, etc, silently return.
        if (!m_select || !FrameLoaderClientAndroid::get(m_viewImpl->m_mainFrame)->getCacheBuilder().validNode(m_frame, m_select))
            return;
        if (-1 == index) {
            if (m_select->selectedIndex() != -1) {
#ifdef ANDROID_DESELECT_SELECT
                m_select->deselectItems();
#endif
                m_select->onChange();
                m_viewImpl->contentInvalidate(m_select->getRect());
            }
            return;
        }
        WebCore::HTMLOptionElement* option = static_cast<WebCore::HTMLOptionElement*>(
                m_select->item(m_select->listToOptionIndex(index)));
        if (!option->selected()) {
            option->setSelected(true);
            m_select->onChange();
			m_viewImpl->contentInvalidate(m_select->getRect());
        }
    }

    // Response if the listbox allows multiple selection.  array stores the listIndices
    // of selected positions.
    virtual void replyIntArray(const int* array, int count)
    {
        // If the select element no longer exists, do to a page change, etc, silently return.
        if (!m_select || !FrameLoaderClientAndroid::get(m_viewImpl->m_mainFrame)->getCacheBuilder().validNode(m_frame, m_select))
            return;
#ifdef ANDROID_DESELECT_SELECT
        m_select->deselectItems();
#endif
        WebCore::HTMLOptionElement* option;
        for (int i = 0; i < count; i++) {
            option = static_cast<WebCore::HTMLOptionElement*>(
                    m_select->item(m_select->listToOptionIndex(array[i])));
            option->setSelected(true);
        }
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

        env->CallVoidMethod(m_javaGlue->object(env).get(), m_javaGlue->m_requestListBox, labelArray, enabledArray, selectedArray);
        env->DeleteLocalRef(selectedArray);
    } else {
        // Pass up the single selection.
        env->CallVoidMethod(m_javaGlue->object(env).get(), m_javaGlue->m_requestSingleListBox, labelArray, enabledArray, selectedCountOrSelection);
    }

    env->DeleteLocalRef(labelArray);
    env->DeleteLocalRef(enabledArray);
    checkException(env);

    Retain(reply);
    m_popupReply = reply;
}

bool WebViewCore::key(int keyCode, UChar32 unichar, int repeatCount, bool isShift, bool isAlt, bool isDown)
{
    DBG_NAV_LOGD("key: keyCode=%d", keyCode);

    WebCore::EventHandler* eventHandler = m_mainFrame->eventHandler();
    WebCore::Node* focusNode = FrameLoaderClientAndroid::get(m_mainFrame)->getCacheBuilder().currentFocus();
    if (focusNode) {
        eventHandler = focusNode->document()->frame()->eventHandler();
    }

    int mods = 0; // PlatformKeyboardEvent::ModifierKey
    if (isShift) {
        mods |= WebCore::PlatformKeyboardEvent::ShiftKey;
    }
    if (isAlt) {
        mods |= WebCore::PlatformKeyboardEvent::AltKey;
    }
    WebCore::PlatformKeyboardEvent evt(keyCode, unichar,
            isDown ? WebCore::PlatformKeyboardEvent::KeyDown : WebCore::PlatformKeyboardEvent::KeyUp,
            repeatCount, static_cast<WebCore::PlatformKeyboardEvent::ModifierKey> (mods));
    return eventHandler->keyEvent(evt);
}

bool WebViewCore::click() {
    bool keyHandled = false;
    WebCore::Node* focusNode = FrameLoaderClientAndroid::get(m_mainFrame)->getCacheBuilder().currentFocus();
    if (focusNode) {
        WebFrame::getWebFrame(m_mainFrame)->setUserInitiatedClick(true);
        keyHandled = handleMouseClick(focusNode->document()->frame(), focusNode);
        WebFrame::getWebFrame(m_mainFrame)->setUserInitiatedClick(false);
    }
    // match in setFinalFocus()
    m_blockFocusChange = false;
    return keyHandled;
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

void WebViewCore::touchUp(int touchGeneration, int buildGeneration,
    WebCore::Frame* frame, WebCore::Node* node, int x, int y, int size,
    bool isClick, bool retry)
{
    if (m_touchGeneration > touchGeneration) {
        DBG_NAV_LOGD("m_touchGeneration=%d > touchGeneration=%d"
            " x=%d y=%d", m_touchGeneration, touchGeneration, x, y);
        return; // short circuit if a newer touch has been generated
    }
    if (retry || isClick)
        finalKitFocus(frame, node, x, y, true);  // don't change DOM focus
    else if (!commonKitFocus(touchGeneration, buildGeneration,
            frame, node, x, y, false)) {
        return;
    }
    m_lastGeneration = touchGeneration;
    // If this is just a touch and not a click, we have already done the change in focus,
    // so just leave the function now.
    if (!isClick)
        return;
    if (frame && FrameLoaderClientAndroid::get(m_mainFrame)->getCacheBuilder().validNode(frame, 0)) {
        frame->loader()->resetMultipleFormSubmissionProtection();
    }
    EditorClientAndroid* client = static_cast<EditorClientAndroid*>(m_mainFrame->editor()->client());
    client->setFromClick(true);
    DBG_NAV_LOGD("touchGeneration=%d handleMouseClick frame=%p node=%p"
        " x=%d y=%d", touchGeneration, frame, node, x, y);
    handleMouseClick(frame, node);
    client->setFromClick(false);
}

bool WebViewCore::handleMouseClick(WebCore::Frame* framePtr, WebCore::Node* nodePtr)
{
    bool valid = framePtr == NULL || FrameLoaderClientAndroid::get(
            m_mainFrame)->getCacheBuilder().validNode(framePtr, nodePtr);
    WebFrame* webFrame = WebFrame::getWebFrame(m_mainFrame);
    if (valid && nodePtr) {
    // Need to special case area tags because an image map could have an area element in the middle
    // so when attempting to get the default, the point chosen would be follow the wrong link.
        if (nodePtr->hasTagName(WebCore::HTMLNames::areaTag)) {
            webFrame->setUserInitiatedClick(true);
            WebCore::EventTargetNodeCast(nodePtr)->dispatchSimulatedClick(0,
                true, true);
            webFrame->setUserInitiatedClick(false);
            return true;
        }
        WebCore::RenderObject* renderer = nodePtr->renderer();
        if (renderer && renderer->isMenuList()) {
            WebCore::HTMLSelectElement* select = static_cast<WebCore::HTMLSelectElement*>(nodePtr);
            const WTF::Vector<WebCore::HTMLElement*>& listItems = select->listItems();
            SkTDArray<const uint16_t*> names;
            SkTDArray<int> enabledArray;
            SkTDArray<int> selectedArray;
            int size = listItems.size();
            bool multiple = select->multiple();
            for (int i = 0; i < size; i++) {
                if (listItems[i]->hasTagName(WebCore::HTMLNames::optionTag)) {
                    WebCore::HTMLOptionElement* option = static_cast<WebCore::HTMLOptionElement*>(listItems[i]);
                    *names.append() = stringConverter(option->text());
                    *enabledArray.append() = option->disabled() ? 0 : 1;
                    if (multiple && option->selected())
                        *selectedArray.append() = i;
                } else if (listItems[i]->hasTagName(WebCore::HTMLNames::optgroupTag)) {
                    WebCore::HTMLOptGroupElement* optGroup = static_cast<WebCore::HTMLOptGroupElement*>(listItems[i]);
                    *names.append() = stringConverter(optGroup->groupLabelText());
                    *enabledArray.append() = 0;
                    if (multiple)
                        *selectedArray.append() = 0;
                }
            }
            WebCoreReply* reply = new ListBoxReply(select, select->document()->frame(), this);
            listBoxRequest(reply, names.begin(), size, enabledArray.begin(), enabledArray.count(),
                    multiple, selectedArray.begin(), multiple ? selectedArray.count() :
                    select->optionToListIndex(select->selectedIndex()));
            return true;
        }
    }
    if (!valid || !framePtr)
        framePtr = m_mainFrame;
    webFrame->setUserInitiatedClick(true);
    DBG_NAV_LOGD("m_mousePos={%d,%d}", m_mousePos.x(), m_mousePos.y());
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

void WebViewCore::jsAlert(const WebCore::String& url, const WebCore::String& text)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jstring jInputStr = env->NewString((unsigned short *)text.characters(), text.length());
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());
    env->CallVoidMethod(m_javaGlue->object(env).get(), m_javaGlue->m_jsAlert, jUrlStr, jInputStr);
    env->DeleteLocalRef(jInputStr);
    env->DeleteLocalRef(jUrlStr);
    checkException(env);
}

bool WebViewCore::jsConfirm(const WebCore::String& url, const WebCore::String& text)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jstring jInputStr = env->NewString((unsigned short *)text.characters(), text.length());
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());
    jboolean result = env->CallBooleanMethod(m_javaGlue->object(env).get(), m_javaGlue->m_jsConfirm, jUrlStr, jInputStr);
    env->DeleteLocalRef(jInputStr);
    env->DeleteLocalRef(jUrlStr);
    checkException(env);
    return result;
}

bool WebViewCore::jsPrompt(const WebCore::String& url, const WebCore::String& text, const WebCore::String& defaultValue, WebCore::String& result)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jstring jInputStr = env->NewString((unsigned short *)text.characters(), text.length());
    jstring jDefaultStr = env->NewString((unsigned short *)defaultValue.characters(), defaultValue.length());
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());
    jstring returnVal = (jstring) env->CallObjectMethod(m_javaGlue->object(env).get(), m_javaGlue->m_jsPrompt, jUrlStr, jInputStr, jDefaultStr);
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
    jstring jInputStr = env->NewString((unsigned short *)message.characters(), message.length());
    jstring jUrlStr = env->NewString((unsigned short *)url.characters(), url.length());
    jboolean result = env->CallBooleanMethod(m_javaGlue->object(env).get(), m_javaGlue->m_jsUnload, jUrlStr, jInputStr);
    env->DeleteLocalRef(jInputStr);
    env->DeleteLocalRef(jUrlStr);
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
    return env->GetObjectField(m_javaGlue->object(env).get(), gWebViewCoreFields.m_webView);
}

void WebViewCore::updateTextfield(WebCore::Node* ptr, bool changeToPassword,
        const WebCore::String& text)
{
    if (m_blockTextfieldUpdates)
        return;
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (changeToPassword) {
        env->CallVoidMethod(m_javaGlue->object(env).get(), m_javaGlue->m_updateTextfield,
                (int) ptr, true, 0, m_textGeneration);
        checkException(env);
        return;
    }
    int length = text.length();
    jstring string = env->NewString((unsigned short *) text.characters(), length);
    env->CallVoidMethod(m_javaGlue->object(env).get(), m_javaGlue->m_updateTextfield,
            (int) ptr, false, string, m_textGeneration);
    env->DeleteLocalRef(string);
    checkException(env);
}

void WebViewCore::setSnapAnchor(int x, int y)
{
    m_snapAnchorNode = 0;
    if (!x && !y) {
        return;
    }

    WebCore::IntPoint point = WebCore::IntPoint(x, y);
    WebCore::Node* node = m_mainFrame->eventHandler()->hitTestResultAtPoint(point, false).innerNode();
    if (node) {
//        LOGD("found focus node name: %s, type %d\n", node->nodeName().utf8().data(), node->nodeType());
        while (node) {
            if (node->hasTagName(WebCore::HTMLNames::divTag) ||
                    node->hasTagName(WebCore::HTMLNames::tableTag)) {
                m_snapAnchorNode = node;
                return;
            }
//            LOGD("parent node name: %s, type %d\n", node->nodeName().utf8().data(), node->nodeType());
            node = node->parentNode();
        }
    }
}

void WebViewCore::snapToAnchor()
{
    if (m_snapAnchorNode) {
        if (m_snapAnchorNode->inDocument()) {
            FloatPoint pt = m_snapAnchorNode->renderer()->localToAbsolute();
            scrollTo(pt.x(), pt.y());
        } else {
            m_snapAnchorNode = 0;
        }
    }
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

static void SetSize(JNIEnv *env, jobject obj, jint width, jint height,
        jint screenWidth, jfloat scale, jint realScreenWidth, jint screenHeight)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOGV("webviewcore::nativeSetSize(%u %u)\n viewImpl: %p", (unsigned)width, (unsigned)height, viewImpl);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeSetSize");
    // convert the scale to an int
    int s = (int) (scale * 100);
    // a negative value indicates that we should not change the scale
    if (scale < 0)
        s = viewImpl->scale();

    viewImpl->setSizeScreenWidthAndScale(width, height, screenWidth, s,
        realScreenWidth, screenHeight);
}

static void SetScrollOffset(JNIEnv *env, jobject obj, jint dx, jint dy)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "need viewImpl");

    viewImpl->setScrollOffset(dx, dy);
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
        jint repeatCount, jboolean isShift, jboolean isAlt, jboolean isDown)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in Key");

    return viewImpl->key(keyCode, unichar, repeatCount, isShift, isAlt, isDown);
}

static jboolean Click(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in Click");

    return viewImpl->click();
}

static void DeleteSelection(JNIEnv *env, jobject obj,
                    jint frame, jint node, jint x, jint y, jint start, jint end)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    LOGV("webviewcore::nativeDeleteSelection()\n");
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeDeleteSelection");
    viewImpl->deleteSelection((WebCore::Frame*) frame, (WebCore::Node*) node,
        x, y, start, end);
}

static void SetSelection(JNIEnv *env, jobject obj,
    jint frame, jint node, jint x, jint y, jint start, jint end)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    LOGV("webviewcore::nativeSetSelection()\n");
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeDeleteSelection");
    viewImpl->setSelection((WebCore::Frame*) frame, (WebCore::Node*) node,
        x, y, start, end);
}


static void ReplaceTextfieldText(JNIEnv *env, jobject obj,
        jint framePtr, jint nodePtr, jint x, jint y, jint oldStart, jint oldEnd,
        jstring replace, jint start, jint end)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    LOGV("webviewcore::nativeReplaceTextfieldText()\n");
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativeReplaceTextfieldText");
    viewImpl->replaceTextfieldText((WebCore::Frame*) framePtr, (WebCore::Node*) nodePtr, x, y, oldStart,
            oldEnd, replace, start, end);
}

static void PassToJs(JNIEnv *env, jobject obj, jint frame, jint node,
    jint x, jint y, jint generation, jstring currentText, jint keyCode,
    jint keyValue, jboolean down, jboolean cap, jboolean fn, jboolean sym)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    LOGV("webviewcore::nativePassToJs()\n");
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in nativePassToJs");
    viewImpl->passToJs((WebCore::Frame*) frame, (WebCore::Node*) node,
        x, y, generation, currentText, keyCode, keyValue, down, cap, fn, sym);
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

static jstring FindAddress(JNIEnv *env, jobject obj, jstring addr)
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
        &start, &end) == CacheBuilder::FOUND_COMPLETE;
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
        jint buildGeneration, jint frame, jint node, jint x, jint y, jint size,
        jboolean isClick, jboolean retry)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    viewImpl->touchUp(touchGeneration, buildGeneration,
        (WebCore::Frame*) frame, (WebCore::Node*) node, x, y, size, isClick, retry);
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

static void SetFinalFocus(JNIEnv *env, jobject obj, jint frame, jint node,
        jint x, jint y, jboolean block)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    viewImpl->setFinalFocus((WebCore::Frame*) frame, (WebCore::Node*) node, x,
        y, block);
}

static void SetKitFocus(JNIEnv *env, jobject obj, jint moveGeneration,
        jint buildGeneration, jint frame, jint node, jint x, jint y,
        jboolean ignoreNullFocus)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    viewImpl->setKitFocus(moveGeneration, buildGeneration,
        (WebCore::Frame*) frame, (WebCore::Node*) node, x, y,
        ignoreNullFocus);
}

static void UnblockFocus(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);
    viewImpl->unblockFocus();
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
#endif
}

static void SetSnapAnchor(JNIEnv *env, jobject obj, jint x, jint y)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);

    viewImpl->setSnapAnchor(x, y);
}

static void SnapToAnchor(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebViewCore* viewImpl = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(viewImpl, "viewImpl not set in %s", __FUNCTION__);

    viewImpl->snapToAnchor();
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

static void RefreshPlugins(JNIEnv *env,
                                 jobject obj,
                                 jboolean reloadOpenPages)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    // Refresh the list of plugins, optionally reloading all open
    // pages.
    WebCore::refreshPlugins(reloadOpenPages);
}

static void RegisterURLSchemeAsLocal(JNIEnv* env, jobject obj, jstring scheme) {
#ifdef ANDROID_INSTRUMENT
    TimeCounterAuto counter(TimeCounter::WebViewCoreTimeCounter);
#endif
    WebCore::FrameLoader::registerURLSchemeAsLocal(to_string(env, scheme));
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
    { "nativeKey", "(IIIZZZ)Z",
        (void*) Key },
    { "nativeClick", "()Z",
        (void*) Click },
    { "nativeSendListBoxChoices", "([ZI)V",
        (void*) SendListBoxChoices },
    { "nativeSendListBoxChoice", "(I)V",
        (void*) SendListBoxChoice },
    { "nativeSetSize", "(IIIFII)V",
        (void*) SetSize },
    { "nativeSetScrollOffset", "(II)V",
        (void*) SetScrollOffset },
    { "nativeSetGlobalBounds", "(IIII)V",
        (void*) SetGlobalBounds },
    { "nativeSetSelection", "(IIIIII)V",
        (void*) SetSelection } ,
    { "nativeDeleteSelection", "(IIIIII)V",
        (void*) DeleteSelection } ,
    { "nativeReplaceTextfieldText", "(IIIIIILjava/lang/String;II)V",
        (void*) ReplaceTextfieldText } ,
    { "passToJs", "(IIIIILjava/lang/String;IIZZZZ)V",
        (void*) PassToJs } ,
    { "nativeSaveDocumentState", "(I)V",
        (void*) SaveDocumentState },
    { "nativeFindAddress", "(Ljava/lang/String;)Ljava/lang/String;",
        (void*) FindAddress },
    { "nativeHandleTouchEvent", "(III)Z",
            (void*) HandleTouchEvent },
    { "nativeTouchUp", "(IIIIIIIZZ)V",
        (void*) TouchUp },
    { "nativeRetrieveHref", "(II)Ljava/lang/String;",
        (void*) RetrieveHref },
    { "nativeSetFinalFocus", "(IIIIZ)V",
        (void*) SetFinalFocus },
    { "nativeSetKitFocus", "(IIIIIIZ)V",
        (void*) SetKitFocus },
    { "nativeUnblockFocus", "()V",
        (void*) UnblockFocus },
    { "nativeUpdateFrameCache", "()V",
        (void*) UpdateFrameCache },
    { "nativeGetContentMinPrefWidth", "()I",
        (void*) GetContentMinPrefWidth },
    { "nativeRecordContent", "(Landroid/graphics/Region;Landroid/graphics/Point;)Z",
        (void*) RecordContent },
    { "setViewportSettingsFromNative", "()V",
        (void*) SetViewportSettingsFromNative },
    { "nativeSetSnapAnchor", "(II)V",
        (void*) SetSnapAnchor },
    { "nativeSnapToAnchor", "()V",
        (void*) SnapToAnchor },
    { "nativeSplitContent", "()V",
        (void*) SplitContent },
    { "nativeSetBackgroundColor", "(I)V",
        (void*) SetBackgroundColor },
    { "nativeGetSelection", "(Landroid/graphics/Region;)Ljava/lang/String;",
        (void*) GetSelection },
    { "nativeRefreshPlugins", "(Z)V",
        (void*) RefreshPlugins },
    { "nativeRegisterURLSchemeAsLocal", "(Ljava/lang/String;)V",
        (void*) RegisterURLSchemeAsLocal },
    { "nativeDumpDomTree", "(Z)V",
        (void*) DumpDomTree },
    { "nativeDumpRenderTree", "(Z)V",
        (void*) DumpRenderTree },
    { "nativeDumpNavTree", "()V",
        (void*) DumpNavTree }
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
    gWebViewCoreFields.m_webView = env->GetFieldID(widget,
            "mWebView", "Landroid/webkit/WebView;");
    LOG_ASSERT(gWebViewCoreFields.m_webView,
            "Unable to find android/webkit/WebViewCore.mWebView");

    return jniRegisterNativeMethods(env, "android/webkit/WebViewCore",
            gJavaWebViewCoreMethods, NELEM(gJavaWebViewCoreMethods));
}

} /* namespace android */
