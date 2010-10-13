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

#define LOG_TAG "webviewglue"

#include "config.h"

#include "AndroidAnimation.h"
#include "AndroidLog.h"
#include "AtomicString.h"
#include "CachedFrame.h"
#include "CachedNode.h"
#include "CachedRoot.h"
#include "CString.h"
#include "DrawExtra.h"
#include "FindCanvas.h"
#include "Frame.h"
#include "GraphicsJNI.h"
#include "HTMLInputElement.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "LayerAndroid.h"
#include "Node.h"
#include "PlatformGraphicsContext.h"
#include "PlatformString.h"
#include "SelectText.h"
#include "SkCanvas.h"
#include "SkDumpCanvas.h"
#include "SkPicture.h"
#include "SkRect.h"
#include "SkTime.h"
#ifdef ANDROID_INSTRUMENT
#include "TimeCounter.h"
#endif
#include "WebCoreJni.h"
#include "WebViewCore.h"
#include "android_graphics.h"

#ifdef GET_NATIVE_VIEW
#undef GET_NATIVE_VIEW
#endif

#define GET_NATIVE_VIEW(env, obj) ((WebView*)env->GetIntField(obj, gWebViewField))

#include <JNIUtility.h>
#include <JNIHelp.h>
#include <jni.h>
#include <ui/KeycodeLabels.h>

namespace android {

static jfieldID gWebViewField;

//-------------------------------------

static jmethodID GetJMethod(JNIEnv* env, jclass clazz, const char name[], const char signature[])
{
    jmethodID m = env->GetMethodID(clazz, name, signature);
    LOG_ASSERT(m, "Could not find method %s", name);
    return m;
}

//-------------------------------------
// This class provides JNI for making calls into native code from the UI side
// of the multi-threaded WebView.
class WebView
{
public:
enum FrameCachePermission {
    DontAllowNewer,
    AllowNewer,
    AllowNewest
};

enum DrawExtras { // keep this in sync with WebView.java
    DrawExtrasNone = 0,
    DrawExtrasFind = 1,
    DrawExtrasSelection = 2,
    DrawExtrasCursorRing = 3
};

struct JavaGlue {
    jweak       m_obj;
    jmethodID   m_calcOurContentVisibleRectF;
    jmethodID   m_clearTextEntry;
    jmethodID   m_overrideLoading;
    jmethodID   m_scrollBy;
    jmethodID   m_sendMoveFocus;
    jmethodID   m_sendMoveMouse;
    jmethodID   m_sendMoveMouseIfLatest;
    jmethodID   m_sendMotionUp;
    jmethodID   m_domChangedFocus;
    jmethodID   m_getScaledMaxXScroll;
    jmethodID   m_getScaledMaxYScroll;
    jmethodID   m_getVisibleRect;
    jmethodID   m_rebuildWebTextView;
    jmethodID   m_viewInvalidate;
    jmethodID   m_viewInvalidateRect;
    jmethodID   m_postInvalidateDelayed;
    jfieldID    m_rectLeft;
    jfieldID    m_rectTop;
    jmethodID   m_rectWidth;
    jmethodID   m_rectHeight;
    jfieldID    m_rectFLeft;
    jfieldID    m_rectFTop;
    jmethodID   m_rectFWidth;
    jmethodID   m_rectFHeight;
    AutoJObject object(JNIEnv* env) {
        return getRealObject(env, m_obj);
    }
} m_javaGlue;

WebView(JNIEnv* env, jobject javaWebView, int viewImpl) :
    m_ring((WebViewCore*) viewImpl)
{
    jclass clazz = env->FindClass("android/webkit/WebView");
 //   m_javaGlue = new JavaGlue;
    m_javaGlue.m_obj = env->NewWeakGlobalRef(javaWebView);
    m_javaGlue.m_scrollBy = GetJMethod(env, clazz, "setContentScrollBy", "(IIZ)Z");
    m_javaGlue.m_calcOurContentVisibleRectF = GetJMethod(env, clazz, "calcOurContentVisibleRectF", "(Landroid/graphics/RectF;)V");
    m_javaGlue.m_clearTextEntry = GetJMethod(env, clazz, "clearTextEntry", "(Z)V");
    m_javaGlue.m_overrideLoading = GetJMethod(env, clazz, "overrideLoading", "(Ljava/lang/String;)V");
    m_javaGlue.m_sendMoveFocus = GetJMethod(env, clazz, "sendMoveFocus", "(II)V");
    m_javaGlue.m_sendMoveMouse = GetJMethod(env, clazz, "sendMoveMouse", "(IIII)V");
    m_javaGlue.m_sendMoveMouseIfLatest = GetJMethod(env, clazz, "sendMoveMouseIfLatest", "(Z)V");
    m_javaGlue.m_sendMotionUp = GetJMethod(env, clazz, "sendMotionUp", "(IIIII)V");
    m_javaGlue.m_domChangedFocus = GetJMethod(env, clazz, "domChangedFocus", "()V");
    m_javaGlue.m_getScaledMaxXScroll = GetJMethod(env, clazz, "getScaledMaxXScroll", "()I");
    m_javaGlue.m_getScaledMaxYScroll = GetJMethod(env, clazz, "getScaledMaxYScroll", "()I");
    m_javaGlue.m_getVisibleRect = GetJMethod(env, clazz, "sendOurVisibleRect", "()Landroid/graphics/Rect;");
    m_javaGlue.m_rebuildWebTextView = GetJMethod(env, clazz, "rebuildWebTextView", "()V");
    m_javaGlue.m_viewInvalidate = GetJMethod(env, clazz, "viewInvalidate", "()V");
    m_javaGlue.m_viewInvalidateRect = GetJMethod(env, clazz, "viewInvalidate", "(IIII)V");
    m_javaGlue.m_postInvalidateDelayed = GetJMethod(env, clazz,
        "viewInvalidateDelayed", "(JIIII)V");
    jclass rectClass = env->FindClass("android/graphics/Rect");
    LOG_ASSERT(rectClass, "Could not find Rect class");
    m_javaGlue.m_rectLeft = env->GetFieldID(rectClass, "left", "I");
    m_javaGlue.m_rectTop = env->GetFieldID(rectClass, "top", "I");
    m_javaGlue.m_rectWidth = GetJMethod(env, rectClass, "width", "()I");
    m_javaGlue.m_rectHeight = GetJMethod(env, rectClass, "height", "()I");
    jclass rectClassF = env->FindClass("android/graphics/RectF");
    LOG_ASSERT(rectClassF, "Could not find RectF class");
    m_javaGlue.m_rectFLeft = env->GetFieldID(rectClassF, "left", "F");
    m_javaGlue.m_rectFTop = env->GetFieldID(rectClassF, "top", "F");
    m_javaGlue.m_rectFWidth = GetJMethod(env, rectClassF, "width", "()F");
    m_javaGlue.m_rectFHeight = GetJMethod(env, rectClassF, "height", "()F");
    env->SetIntField(javaWebView, gWebViewField, (jint)this);
    m_viewImpl = (WebViewCore*) viewImpl;
    m_frameCacheUI = 0;
    m_navPictureUI = 0;
    m_generation = 0;
    m_heightCanMeasure = false;
    m_ring.m_followedLink = false;
    m_lastDx = 0;
    m_lastDxTime = 0;
    m_ringAnimationEnd = 0;
    m_rootLayer = 0;
}

~WebView()
{
    if (m_javaGlue.m_obj)
    {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        env->DeleteWeakGlobalRef(m_javaGlue.m_obj);
        m_javaGlue.m_obj = 0;
    }
    delete m_frameCacheUI;
    delete m_navPictureUI;
    delete m_rootLayer;
}

WebViewCore* getWebViewCore() const {
    return m_viewImpl;
}

// removes the cursor altogether (e.g., when going to a new page)
void clearCursor()
{
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root)
        return;
    DBG_NAV_LOG("");
    m_viewImpl->m_hasCursorBounds = false;
    root->clearCursor();
    viewInvalidate();
}

// leaves the cursor where it is, but suppresses drawing it
void hideCursor()
{
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root)
        return;
    DBG_NAV_LOG("");
    m_viewImpl->m_hasCursorBounds = false;
    root->hideCursor();
    viewInvalidate();
}

void clearTextEntry()
{
    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__);
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(),
        m_javaGlue.m_clearTextEntry, true);
    checkException(env);
}

#if DUMP_NAV_CACHE
void debugDump()
{
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (root)
        root->mDebug.print();
}
#endif

// Traverse our stored array of buttons that are in our picture, and update
// their subpictures according to their current state.
// Called from the UI thread.  This is the one place in the UI thread where we
// access the buttons stored in the WebCore thread.
// hasFocus keeps track of whether the WebView has focus && windowFocus.
// If not, we do not want to draw the button in a selected or pressed state
void nativeRecordButtons(bool hasFocus, bool pressed, bool invalidate)
{
    bool cursorIsOnButton = false;
    const CachedFrame* cachedFrame;
    const CachedNode* cachedCursor = 0;
    // Lock the mutex, since we now share with the WebCore thread.
    m_viewImpl->gButtonMutex.lock();
    if (m_viewImpl->m_buttons.size()) {
        // FIXME: In a future change, we should keep track of whether the selection
        // has changed to short circuit (note that we would still need to update
        // if we received new buttons from the WebCore thread).
        WebCore::Node* cursor = 0;
        CachedRoot* root = getFrameCache(DontAllowNewer);
        if (root) {
            cachedCursor = root->currentCursor(&cachedFrame);
            if (cachedCursor)
                cursor = (WebCore::Node*) cachedCursor->nodePointer();
        }

        // Traverse the array, and update each button, depending on whether it
        // is selected.
        Container* end = m_viewImpl->m_buttons.end();
        for (Container* ptr = m_viewImpl->m_buttons.begin(); ptr != end; ptr++) {
            WebCore::RenderSkinAndroid::State state;
            if (ptr->matches(cursor)) {
                cursorIsOnButton = true;
                // If the WebView is out of focus/window focus, set the state to
                // normal, but still keep track of the fact that the selected is a
                // button
                if (!hasFocus) {
                    state = WebCore::RenderSkinAndroid::kNormal;
                } else if (m_ring.m_followedLink || pressed) {
                    state = WebCore::RenderSkinAndroid::kPressed;
                } else {
                    state = WebCore::RenderSkinAndroid::kFocused;
                }
            } else {
                state = WebCore::RenderSkinAndroid::kNormal;
            }
            ptr->updateFocusState(state);
        }
    }
    m_viewImpl->gButtonMutex.unlock();
    if (invalidate && cachedCursor && cursorIsOnButton) {
        const WebCore::IntRect& b = cachedCursor->bounds(cachedFrame);
        viewInvalidateRect(b.x(), b.y(), b.right(), b.bottom());
    }
}

// The caller has already determined that the desired document rect corresponds
// to the main picture, and not a layer
void scrollRectOnScreen(const IntRect& rect)
{
    if (rect.isEmpty())
        return;
    SkRect visible;
    calcOurContentVisibleRect(&visible);
#if USE(ACCELERATED_COMPOSITING)
    if (m_rootLayer) {
        m_rootLayer->updateFixedLayersPositions(visible);
        m_rootLayer->updatePositions();
        visible = m_rootLayer->subtractLayers(visible);
    }
#endif
    int dx = 0;
    int left = rect.x();
    int right = rect.right();
    if (left < visible.fLeft) {
        dx = left - visible.fLeft;
    // Only scroll right if the entire width can fit on screen.
    } else if (right > visible.fRight && right - left < visible.width()) {
        dx = right - visible.fRight;
    }
    int dy = 0;
    int top = rect.y();
    int bottom = rect.bottom();
    if (top < visible.fTop) {
        dy = top - visible.fTop;
    // Only scroll down if the entire height can fit on screen
    } else if (bottom > visible.fBottom && bottom - top < visible.height()) {
        dy = bottom - visible.fBottom;
    }
    if ((dx|dy) == 0 || !scrollBy(dx, dy))
        return;
    viewInvalidate();
}

void calcOurContentVisibleRect(SkRect* r)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jclass rectClass = env->FindClass("android/graphics/RectF");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(FFFF)V");
    jobject jRect = env->NewObject(rectClass, init, 0, 0, 0, 0);
    env->CallVoidMethod(m_javaGlue.object(env).get(),
        m_javaGlue.m_calcOurContentVisibleRectF, jRect);
    r->fLeft = env->GetFloatField(jRect, m_javaGlue.m_rectFLeft);
    r->fTop = env->GetFloatField(jRect, m_javaGlue.m_rectFTop);
    r->fRight = r->fLeft + env->CallFloatMethod(jRect, m_javaGlue.m_rectFWidth);
    r->fBottom = r->fTop + env->CallFloatMethod(jRect, m_javaGlue.m_rectFHeight);
    env->DeleteLocalRef(jRect);
    checkException(env);
}

void resetCursorRing()
{
    m_ring.m_followedLink = false;
    m_viewImpl->m_hasCursorBounds = false;
}

bool drawCursorPreamble(CachedRoot* root)
{
    const CachedFrame* frame;
    const CachedNode* node = root->currentCursor(&frame);
    if (!node) {
        DBG_NAV_LOGV("%s", "!node");
        resetCursorRing();
        return false;
    }
    if (node->isHidden()) {
        DBG_NAV_LOG("node->isHidden()");
        m_viewImpl->m_hasCursorBounds = false;
        return false;
    }
    setVisibleRect(root);
    m_ring.m_root = root;
    m_ring.m_frame = frame;
    m_ring.m_node = node;
    return true;
}

void drawCursorPostamble()
{
    if (!m_ring.m_isButton && m_ring.m_flavor < CursorRing::NORMAL_ANIMATING)
        return;
    SkMSec time = SkTime::GetMSecs();
    if (time < m_ringAnimationEnd) {
        // views assume that inval bounds coordinates are non-negative
        WebCore::IntRect invalBounds(0, 0, INT_MAX, INT_MAX);
        invalBounds.intersect(m_ring.m_bounds);
        postInvalidateDelayed(m_ringAnimationEnd - time, invalBounds);
    } else {
        if (m_ring.m_followedLink)
            hideCursor();
        m_ring.m_followedLink = false;
        m_ring.m_flavor = static_cast<CursorRing::Flavor>
                (m_ring.m_flavor - CursorRing::NORMAL_ANIMATING);
    }
}

void drawExtras(SkCanvas* canvas, int extras)
{
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        if (extras == DrawExtrasCursorRing)
            resetCursorRing();
        return;
    }
    LayerAndroid mainPicture(m_navPictureUI);
    DrawExtra* extra = 0;
    switch (extras) {
        case DrawExtrasFind:
            extra = &m_findOnPage;
            break;
        case DrawExtrasSelection:
            extra = &m_selectText;
            break;
        case DrawExtrasCursorRing:
            if (drawCursorPreamble(root) && m_ring.setup()) {
                if (!m_ring.m_isButton)
                    extra = &m_ring;
                drawCursorPostamble();
            }
            break;
        default:
            ;
    }
    if (extra)
        extra->draw(canvas, &mainPicture);
#if USE(ACCELERATED_COMPOSITING)
    if (!m_rootLayer)
        return;
    m_rootLayer->setExtra(extra);
    SkRect visible;
    calcOurContentVisibleRect(&visible);
    // call this to be sure we've adjusted for any scrolling or animations
    // before we actually draw
    m_rootLayer->updateFixedLayersPositions(visible);
    m_rootLayer->updatePositions();
    // We have to set the canvas' matrix on the root layer
    // (to have fixed layers work as intended)
    SkAutoCanvasRestore restore(canvas, true);
    m_rootLayer->setMatrix(canvas->getTotalMatrix());
    canvas->resetMatrix();
    m_rootLayer->draw(canvas);
#endif
}


bool cursorIsTextInput(FrameCachePermission allowNewer)
{
    CachedRoot* root = getFrameCache(allowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        return false;
    }
    const CachedNode* cursor = root->currentCursor();
    if (!cursor) {
        DBG_NAV_LOG("!cursor");
        return false;
    }
    DBG_NAV_LOGD("%s", cursor->isTextInput() ? "true" : "false");
    return cursor->isTextInput();
}

void cursorRingBounds(WebCore::IntRect* bounds)
{
    DBG_NAV_LOGD("%s", "");
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (root) {
        const CachedFrame* cachedFrame;
        const CachedNode* cachedNode = root->currentCursor(&cachedFrame);
        if (cachedNode) {
            *bounds = cachedNode->cursorRingBounds(cachedFrame);
            DBG_NAV_LOGD("bounds={%d,%d,%d,%d}", bounds->x(), bounds->y(),
                bounds->width(), bounds->height());
            return;
        }
    }
    *bounds = WebCore::IntRect(0, 0, 0, 0);
}

void fixCursor()
{
    m_viewImpl->gCursorBoundsMutex.lock();
    bool hasCursorBounds = m_viewImpl->m_hasCursorBounds;
    IntRect bounds = m_viewImpl->m_cursorBounds;
    m_viewImpl->gCursorBoundsMutex.unlock();
    if (!hasCursorBounds)
        return;
    int x, y;
    const CachedFrame* frame;
    const CachedNode* node = m_frameCacheUI->findAt(bounds, &frame, &x, &y, false);
    if (!node)
        return;
    // require that node have approximately the same bounds (+/- 4) and the same
    // center (+/- 2)
    IntPoint oldCenter = IntPoint(bounds.x() + (bounds.width() >> 1),
        bounds.y() + (bounds.height() >> 1));
    IntRect newBounds = node->bounds(frame);
    IntPoint newCenter = IntPoint(newBounds.x() + (newBounds.width() >> 1),
        newBounds.y() + (newBounds.height() >> 1));
    DBG_NAV_LOGD("oldCenter=(%d,%d) newCenter=(%d,%d)"
        " bounds=(%d,%d,w=%d,h=%d) newBounds=(%d,%d,w=%d,h=%d)",
        oldCenter.x(), oldCenter.y(), newCenter.x(), newCenter.y(),
        bounds.x(), bounds.y(), bounds.width(), bounds.height(),
        newBounds.x(), newBounds.y(), newBounds.width(), newBounds.height());
    if (abs(oldCenter.x() - newCenter.x()) > 2)
        return;
    if (abs(oldCenter.y() - newCenter.y()) > 2)
        return;
    if (abs(bounds.x() - newBounds.x()) > 4)
        return;
    if (abs(bounds.y() - newBounds.y()) > 4)
        return;
    if (abs(bounds.right() - newBounds.right()) > 4)
        return;
    if (abs(bounds.bottom() - newBounds.bottom()) > 4)
        return;
    DBG_NAV_LOGD("node=%p frame=%p x=%d y=%d bounds=(%d,%d,w=%d,h=%d)",
        node, frame, x, y, bounds.x(), bounds.y(), bounds.width(),
        bounds.height());
    m_frameCacheUI->setCursor(const_cast<CachedFrame*>(frame),
        const_cast<CachedNode*>(node));
}

CachedRoot* getFrameCache(FrameCachePermission allowNewer)
{
    if (!m_viewImpl->m_updatedFrameCache) {
        DBG_NAV_LOGV("%s", "!m_viewImpl->m_updatedFrameCache");
        return m_frameCacheUI;
    }
    if (allowNewer == DontAllowNewer && m_viewImpl->m_lastGeneration < m_generation) {
        DBG_NAV_LOGD("allowNewer==DontAllowNewer m_viewImpl->m_lastGeneration=%d"
            " < m_generation=%d", m_viewImpl->m_lastGeneration, m_generation);
        return m_frameCacheUI;
    }
    DBG_NAV_LOGD("%s", "m_viewImpl->m_updatedFrameCache == true");
    const CachedFrame* oldCursorFrame;
    const CachedNode* oldCursorNode = m_frameCacheUI ?
        m_frameCacheUI->currentCursor(&oldCursorFrame) : 0;
#if USE(ACCELERATED_COMPOSITING)
    int layerId = -1;
    if (oldCursorNode && oldCursorNode->isInLayer()) {
        const LayerAndroid* cursorLayer = oldCursorFrame->layer(oldCursorNode)
            ->layer(m_frameCacheUI->rootLayer());
        if (cursorLayer)
            layerId = cursorLayer->uniqueId();
    }
#endif
    // get id from old layer and use to find new layer
    bool oldFocusIsTextInput = false;
    void* oldFocusNodePointer = 0;
    if (m_frameCacheUI) {
        const CachedNode* oldFocus = m_frameCacheUI->currentFocus();
        if (oldFocus) {
            oldFocusIsTextInput = oldFocus->isTextInput();
            oldFocusNodePointer = oldFocus->nodePointer();
        }
    }
    m_viewImpl->gFrameCacheMutex.lock();
    delete m_frameCacheUI;
    delete m_navPictureUI;
    m_viewImpl->m_updatedFrameCache = false;
    m_frameCacheUI = m_viewImpl->m_frameCacheKit;
    m_navPictureUI = m_viewImpl->m_navPictureKit;
    m_viewImpl->m_frameCacheKit = 0;
    m_viewImpl->m_navPictureKit = 0;
    m_viewImpl->gFrameCacheMutex.unlock();
    if (m_frameCacheUI)
        m_frameCacheUI->setRootLayer(m_rootLayer);
#if USE(ACCELERATED_COMPOSITING)
    if (layerId >= 0) {
        SkRect visible;
        calcOurContentVisibleRect(&visible);
        LayerAndroid* layer = const_cast<LayerAndroid*>(
                                                m_frameCacheUI->rootLayer());
        if (layer) {
            layer->updateFixedLayersPositions(visible);
            layer->updatePositions();
        }
    }
#endif
    fixCursor();
    if (oldFocusIsTextInput) {
        const CachedNode* newFocus = m_frameCacheUI->currentFocus();
        if (newFocus && oldFocusNodePointer != newFocus->nodePointer()
                && newFocus->isTextInput()
                && newFocus != m_frameCacheUI->currentCursor()) {
            // The focus has changed.  We may need to update things.
            LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
            JNIEnv* env = JSC::Bindings::getJNIEnv();
            env->CallVoidMethod(m_javaGlue.object(env).get(),
                    m_javaGlue.m_domChangedFocus);
            checkException(env);
        }
    }
    if (oldCursorNode && (!m_frameCacheUI || !m_frameCacheUI->currentCursor()))
        viewInvalidate(); // redraw in case cursor ring is still visible
    return m_frameCacheUI;
}

int getScaledMaxXScroll()
{
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    int result = env->CallIntMethod(m_javaGlue.object(env).get(), m_javaGlue.m_getScaledMaxXScroll);
    checkException(env);
    return result;
}

int getScaledMaxYScroll()
{
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    int result = env->CallIntMethod(m_javaGlue.object(env).get(), m_javaGlue.m_getScaledMaxYScroll);
    checkException(env);
    return result;
}

void getVisibleRect(WebCore::IntRect* rect)
{
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jobject jRect = env->CallObjectMethod(m_javaGlue.object(env).get(), m_javaGlue.m_getVisibleRect);
    checkException(env);
    int left = (int) env->GetIntField(jRect, m_javaGlue.m_rectLeft);
    checkException(env);
    rect->setX(left);
    int top = (int) env->GetIntField(jRect, m_javaGlue.m_rectTop);
    checkException(env);
    rect->setY(top);
    int width = (int) env->CallIntMethod(jRect, m_javaGlue.m_rectWidth);
    checkException(env);
    rect->setWidth(width);
    int height = (int) env->CallIntMethod(jRect, m_javaGlue.m_rectHeight);
    checkException(env);
    rect->setHeight(height);
    env->DeleteLocalRef(jRect);
    checkException(env);
}

static CachedFrame::Direction KeyToDirection(int32_t keyCode)
{
    switch (keyCode) {
        case AKEYCODE_DPAD_RIGHT:
            DBG_NAV_LOGD("keyCode=%s", "right");
            return CachedFrame::RIGHT;
        case AKEYCODE_DPAD_LEFT:
            DBG_NAV_LOGD("keyCode=%s", "left");
            return CachedFrame::LEFT;
        case AKEYCODE_DPAD_DOWN:
            DBG_NAV_LOGD("keyCode=%s", "down");
            return CachedFrame::DOWN;
        case AKEYCODE_DPAD_UP:
            DBG_NAV_LOGD("keyCode=%s", "up");
            return CachedFrame::UP;
        default:
            DBG_NAV_LOGD("bad key %d sent", keyCode);
            return CachedFrame::UNINITIALIZED;
    }
}

WebCore::String imageURI(int x, int y)
{
    const CachedRoot* root = getFrameCache(DontAllowNewer);
    return root ? root->imageURI(x, y) : WebCore::String();
}

bool cursorWantsKeyEvents()
{
    const CachedRoot* root = getFrameCache(DontAllowNewer);
    if (root) {
        const CachedNode* focus = root->currentCursor();
        if (focus)
            return focus->wantsKeyEvents();
    }
    return false;
}


/* returns true if the key had no effect (neither scrolled nor changed cursor) */
bool moveCursor(int keyCode, int count, bool ignoreScroll)
{
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        return true;
    }

    m_viewImpl->m_moveGeneration++;
    CachedFrame::Direction direction = KeyToDirection(keyCode);
    const CachedFrame* cachedFrame, * oldFrame = 0;
    const CachedNode* cursor = root->currentCursor(&oldFrame);
    WebCore::IntPoint cursorLocation = root->cursorLocation();
    DBG_NAV_LOGD("old cursor %d (nativeNode=%p) cursorLocation={%d, %d}",
        cursor ? cursor->index() : 0,
        cursor ? cursor->nodePointer() : 0, cursorLocation.x(), cursorLocation.y());
    WebCore::IntRect visibleRect = setVisibleRect(root);
    int xMax = getScaledMaxXScroll();
    int yMax = getScaledMaxYScroll();
    root->setMaxScroll(xMax, yMax);
    const CachedNode* cachedNode = 0;
    int dx = 0;
    int dy = 0;
    int counter = count;
    if (!cursor || !m_ring.m_followedLink)
        root->setScrollOnly(m_ring.m_followedLink);
    while (--counter >= 0) {
        WebCore::IntPoint scroll = WebCore::IntPoint(0, 0);
        cachedNode = root->moveCursor(direction, &cachedFrame, &scroll);
        dx += scroll.x();
        dy += scroll.y();
    }
    DBG_NAV_LOGD("new cursor %d (nativeNode=%p) cursorLocation={%d, %d}"
        "bounds={%d,%d,w=%d,h=%d}", cachedNode ? cachedNode->index() : 0,
        cachedNode ? cachedNode->nodePointer() : 0,
            root->cursorLocation().x(), root->cursorLocation().y(),
            cachedNode ? cachedNode->bounds(cachedFrame).x() : 0,
            cachedNode ? cachedNode->bounds(cachedFrame).y() : 0,
            cachedNode ? cachedNode->bounds(cachedFrame).width() : 0,
            cachedNode ? cachedNode->bounds(cachedFrame).height() : 0);
    // If !m_heightCanMeasure (such as in the browser), we want to scroll no
    // matter what
    if (!ignoreScroll && (!m_heightCanMeasure ||
            !cachedNode ||
            (cursor && cursor->nodePointer() == cachedNode->nodePointer())))
    {
        if (count == 1 && dx != 0 && dy == 0 && -m_lastDx == dx &&
                SkTime::GetMSecs() - m_lastDxTime < 1000)
            root->checkForJiggle(&dx);
        DBG_NAV_LOGD("scrollBy %d,%d", dx, dy);
        if ((dx | dy))
            this->scrollBy(dx, dy);
        m_lastDx = dx;
        m_lastDxTime = SkTime::GetMSecs();
    }
    bool result = false;
    if (cachedNode) {
        m_viewImpl->updateCursorBounds(root, cachedFrame, cachedNode);
        root->setCursor(const_cast<CachedFrame*>(cachedFrame),
                const_cast<CachedNode*>(cachedNode));
        bool disableFocusController = cachedNode != root->currentFocus()
                && cachedNode->wantsKeyEvents();
        sendMoveMouseIfLatest(disableFocusController);
        viewInvalidate();
    } else {
        int docHeight = root->documentHeight();
        int docWidth = root->documentWidth();
        if (visibleRect.bottom() + dy > docHeight)
            dy = docHeight - visibleRect.bottom();
        else if (visibleRect.y() + dy < 0)
            dy = -visibleRect.y();
        if (visibleRect.right() + dx > docWidth)
            dx = docWidth - visibleRect.right();
        else if (visibleRect.x() < 0)
            dx = -visibleRect.x();
        result = direction == CachedFrame::LEFT ? dx >= 0 :
            direction == CachedFrame::RIGHT ? dx <= 0 :
            direction == CachedFrame::UP ? dy >= 0 : dy <= 0;
    }
    return result;
}

void notifyProgressFinished()
{
    DBG_NAV_LOGD("cursorIsTextInput=%d", cursorIsTextInput(DontAllowNewer));
    rebuildWebTextView();
#if DEBUG_NAV_UI
    if (m_frameCacheUI) {
        const CachedNode* focus = m_frameCacheUI->currentFocus();
        DBG_NAV_LOGD("focus %d (nativeNode=%p)",
            focus ? focus->index() : 0,
            focus ? focus->nodePointer() : 0);
    }
#endif
}

const CachedNode* findAt(CachedRoot* root, const WebCore::IntRect& rect,
    const CachedFrame** framePtr, int* rxPtr, int* ryPtr)
{
    *rxPtr = 0;
    *ryPtr = 0;
    *framePtr = 0;
    if (!root)
        return 0;
    setVisibleRect(root);
    return root->findAt(rect, framePtr, rxPtr, ryPtr, true);
}

IntRect setVisibleRect(CachedRoot* root)
{
    IntRect visibleRect;
    getVisibleRect(&visibleRect);
    DBG_NAV_LOGD("getVisibleRect %d,%d,%d,%d",
        visibleRect.x(), visibleRect.y(), visibleRect.width(), visibleRect.height());
    root->setVisibleRect(visibleRect);
    return visibleRect;
}

void selectBestAt(const WebCore::IntRect& rect)
{
    const CachedFrame* frame;
    int rx, ry;
    CachedRoot* root = getFrameCache(DontAllowNewer);
    const CachedNode* node = findAt(root, rect, &frame, &rx, &ry);

    if (!node) {
        DBG_NAV_LOGD("no nodes found root=%p", root);
        m_viewImpl->m_hasCursorBounds = false;
        if (root)
            root->setCursor(0, 0);
    } else {
        DBG_NAV_LOGD("CachedNode:%p (%d)", node, node->index());
        root->rootHistory()->setMouseBounds(node->bounds(frame));
        m_viewImpl->updateCursorBounds(root, frame, node);
        root->setCursor(const_cast<CachedFrame*>(frame),
                const_cast<CachedNode*>(node));
    }
    sendMoveMouseIfLatest(false);
    viewInvalidate();
}

WebCore::IntRect getNavBounds()
{
    CachedRoot* root = getFrameCache(DontAllowNewer);
    return root ? root->rootHistory()->navBounds() :
        WebCore::IntRect(0, 0, 0, 0);
}

void setNavBounds(const WebCore::IntRect& rect)
{
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root)
        return;
    root->rootHistory()->setNavBounds(rect);
}



const CachedNode* m_cacheHitNode;
const CachedFrame* m_cacheHitFrame;

bool pointInNavCache(int x, int y, int slop)
{
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root)
        return false;
    IntRect rect = IntRect(x - slop, y - slop, slop * 2, slop * 2);
    int rx, ry;
    return (m_cacheHitNode = findAt(root, rect, &m_cacheHitFrame, &rx, &ry));
}

bool motionUp(int x, int y, int slop)
{
    bool pageScrolled = false;
    m_ring.m_followedLink = false;
    IntRect rect = IntRect(x - slop, y - slop, slop * 2, slop * 2);
    int rx, ry;
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root)
        return 0;
    const CachedFrame* frame = 0;
    const CachedNode* result = findAt(root, rect, &frame, &rx, &ry);
    if (!result) {
        DBG_NAV_LOGD("no nodes found root=%p", root);
        setNavBounds(rect);
        m_viewImpl->m_hasCursorBounds = false;
        root->hideCursor();
        int dx = root->checkForCenter(x, y);
        if (dx) {
            scrollBy(dx, 0);
            pageScrolled = true;
        }
        sendMotionUp(frame ? (WebCore::Frame*) frame->framePointer() : 0,
            0, x, y);
        viewInvalidate();
        clearTextEntry();
        return pageScrolled;
    }
    DBG_NAV_LOGD("CachedNode:%p (%d) x=%d y=%d rx=%d ry=%d", result,
        result->index(), x, y, rx, ry);
    WebCore::IntRect navBounds = WebCore::IntRect(rx, ry, 1, 1);
    setNavBounds(navBounds);
    root->rootHistory()->setMouseBounds(navBounds);
    m_viewImpl->updateCursorBounds(root, frame, result);
    root->setCursor(const_cast<CachedFrame*>(frame),
        const_cast<CachedNode*>(result));
    bool syntheticLink = result->isSyntheticLink();
    if (!syntheticLink) {
        sendMotionUp(
            (WebCore::Frame*) frame->framePointer(),
            (WebCore::Node*) result->nodePointer(), rx, ry);
    }
    viewInvalidate();
    if (!result->isTextInput()) {
        clearTextEntry();
        setFollowedLink(true);
        if (syntheticLink)
            overrideUrlLoading(result->getExport());
    }
    return pageScrolled;
}

int getBlockLeftEdge(int x, int y, float scale)
{
    CachedRoot* root = getFrameCache(AllowNewer);
    if (root)
        return root->getBlockLeftEdge(x, y, scale);
    return -1;
}

void overrideUrlLoading(const WebCore::String& url)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jstring jName = env->NewString((jchar*) url.characters(), url.length());
    env->CallVoidMethod(m_javaGlue.object(env).get(),
            m_javaGlue.m_overrideLoading, jName);
    env->DeleteLocalRef(jName);
}

void setFindIsUp(bool up)
{
    DBG_NAV_LOGD("up=%d", up);
    m_viewImpl->m_findIsUp = up;
}

void setFindIsEmpty()
{
    DBG_NAV_LOG("");
    m_findOnPage.clearCurrentLocation();
}

void setFollowedLink(bool followed)
{
    if ((m_ring.m_followedLink = followed) != false) {
        m_ringAnimationEnd = SkTime::GetMSecs() + 500;
        viewInvalidate();
    }
}

void setHeightCanMeasure(bool measure)
{
    m_heightCanMeasure = measure;
}

String getSelection()
{
    return m_selectText.getSelection();
}

void moveSelection(int x, int y)
{
    const CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root)
        return;
    SkPicture* picture = root->pictureAt(x, y);
    // FIXME: use the visibleRect only for the main picture
    // for layer pictures, use the equivalent of the canvas clipping rect
    IntRect visibleRect;
    getVisibleRect(&visibleRect);
    m_selectText.setVisibleRect(visibleRect);
    m_selectText.moveSelection(picture, x, y);
}

void selectAll()
{
    const CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root)
        return;
    SkPicture* picture = root->pictureAt(0, 0);
    m_selectText.selectAll(picture);
}

int selectionX()
{
    return m_selectText.selectionX();
}

int selectionY()
{
    return m_selectText.selectionY();
}

void resetSelection()
{
    m_selectText.reset();
}

bool startSelection(int x, int y)
{
    return m_selectText.startSelection(x, y);
}

bool wordSelection(int x, int y)
{
    startSelection(x, y);
    if (!extendSelection(x, y))
        return false;
    m_selectText.setDrawPointer(false);
    SkPicture* picture = getFrameCache(DontAllowNewer)->pictureAt(x, y);
    return m_selectText.wordSelection(picture);
}

bool extendSelection(int x, int y)
{
    const CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root)
        return false;
    SkPicture* picture = root->pictureAt(x, y);
    IntRect visibleRect;
    getVisibleRect(&visibleRect);
    m_selectText.setVisibleRect(visibleRect);
    m_selectText.extendSelection(picture, x, y);
    return true;
}

bool hitSelection(int x, int y)
{
    return m_selectText.hitSelection(x, y);
}

void setExtendSelection()
{
    m_selectText.setExtendSelection(true);
}

void setSelectionPointer(bool set, float scale, int x, int y)
{
    m_selectText.setDrawPointer(set);
    if (!set)
        return;
    m_selectText.m_inverseScale = scale;
    m_selectText.m_selectX = x;
    m_selectText.m_selectY = y;
}

void sendMoveFocus(WebCore::Frame* framePtr, WebCore::Node* nodePtr)
{
    DBG_NAV_LOGD("framePtr=%p nodePtr=%p", framePtr, nodePtr);
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(),
        m_javaGlue.m_sendMoveFocus, (jint) framePtr, (jint) nodePtr);
    checkException(env);
}

void sendMoveMouse(WebCore::Frame* framePtr, WebCore::Node* nodePtr, int x, int y)
{
    DBG_NAV_LOGD("framePtr=%p nodePtr=%p x=%d y=%d", framePtr, nodePtr, x, y);
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(), m_javaGlue.m_sendMoveMouse,
        (jint) framePtr, (jint) nodePtr, x, y);
    checkException(env);
}

void sendMoveMouseIfLatest(bool disableFocusController)
{
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(),
            m_javaGlue.m_sendMoveMouseIfLatest, disableFocusController);
    checkException(env);
}

void sendMotionUp(
    WebCore::Frame* framePtr, WebCore::Node* nodePtr, int x, int y)
{
    m_viewImpl->m_touchGeneration = ++m_generation;
    DBG_NAV_LOGD("m_generation=%d framePtr=%p nodePtr=%p x=%d y=%d",
        m_generation, framePtr, nodePtr, x, y);
    LOG_ASSERT(m_javaGlue.m_obj, "A WebView was not associated with this WebViewNative!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(), m_javaGlue.m_sendMotionUp,
        m_generation, (jint) framePtr, (jint) nodePtr, x, y);
    checkException(env);
}

void findNext(bool forward)
{
    m_findOnPage.findNext(forward);
    if (!m_findOnPage.currentMatchIsInLayer())
        scrollRectOnScreen(m_findOnPage.currentMatchBounds());
    viewInvalidate();
}

// With this call, WebView takes ownership of matches, and is responsible for
// deleting it.
void setMatches(WTF::Vector<MatchInfo>* matches)
{
    m_findOnPage.setMatches(matches);
    if (!m_findOnPage.currentMatchIsInLayer())
        scrollRectOnScreen(m_findOnPage.currentMatchBounds());
    viewInvalidate();
}

int currentMatchIndex()
{
    return m_findOnPage.currentMatchIndex();
}

bool scrollBy(int dx, int dy)
{
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    bool result = env->CallBooleanMethod(m_javaGlue.object(env).get(),
        m_javaGlue.m_scrollBy, dx, dy, true);
    checkException(env);
    return result;
}

bool hasCursorNode()
{
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        return false;
    }
    const CachedNode* cursorNode = root->currentCursor();
    DBG_NAV_LOGD("cursorNode=%d (nodePointer=%p)",
        cursorNode ? cursorNode->index() : -1,
        cursorNode ? cursorNode->nodePointer() : 0);
    return cursorNode;
}

bool hasFocusNode()
{
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        return false;
    }
    const CachedNode* focusNode = root->currentFocus();
    DBG_NAV_LOGD("focusNode=%d (nodePointer=%p)",
        focusNode ? focusNode->index() : -1,
        focusNode ? focusNode->nodePointer() : 0);
    return focusNode;
}

void rebuildWebTextView()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(),
            m_javaGlue.m_rebuildWebTextView);
    checkException(env);
}

void viewInvalidate()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(), m_javaGlue.m_viewInvalidate);
    checkException(env);
}

void viewInvalidateRect(int l, int t, int r, int b)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(), m_javaGlue.m_viewInvalidateRect, l, r, t, b);
    checkException(env);
}

void postInvalidateDelayed(int64_t delay, const WebCore::IntRect& bounds)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(), m_javaGlue.m_postInvalidateDelayed,
        delay, bounds.x(), bounds.y(), bounds.right(), bounds.bottom());
    checkException(env);
}

int moveGeneration()
{
    return m_viewImpl->m_moveGeneration;
}

LayerAndroid* rootLayer() const
{
    return m_rootLayer;
}

void setRootLayer(LayerAndroid* layer)
{
    delete m_rootLayer;
    m_rootLayer = layer;
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root)
        return;
    root->resetLayers();
    root->setRootLayer(m_rootLayer);
}

private: // local state for WebView
    // private to getFrameCache(); other functions operate in a different thread
    CachedRoot* m_frameCacheUI; // navigation data ready for use
    WebViewCore* m_viewImpl;
    int m_generation; // associate unique ID with sent kit focus to match with ui
    SkPicture* m_navPictureUI;
    SkMSec m_ringAnimationEnd;
    // Corresponds to the same-named boolean on the java side.
    bool m_heightCanMeasure;
    int m_lastDx;
    SkMSec m_lastDxTime;
    SelectText m_selectText;
    FindOnPage m_findOnPage;
    CursorRing m_ring;
    LayerAndroid* m_rootLayer;
}; // end of WebView class

/*
 * Native JNI methods
 */
static jstring WebCoreStringToJString(JNIEnv *env, WebCore::String string)
{
    int length = string.length();
    if (!length)
        return 0;
    jstring ret = env->NewString((jchar *)string.characters(), length);
    env->DeleteLocalRef(ret);
    return ret;
}

static int nativeCacheHitFramePointer(JNIEnv *env, jobject obj)
{
    return reinterpret_cast<int>(GET_NATIVE_VIEW(env, obj)
            ->m_cacheHitFrame->framePointer());
}

static jobject nativeCacheHitNodeBounds(JNIEnv *env, jobject obj)
{
    WebCore::IntRect bounds = GET_NATIVE_VIEW(env, obj)
        ->m_cacheHitNode->originalAbsoluteBounds();
    jclass rectClass = env->FindClass("android/graphics/Rect");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    jobject rect = env->NewObject(rectClass, init, bounds.x(),
        bounds.y(), bounds.right(), bounds.bottom());
    return rect;
}

static int nativeCacheHitNodePointer(JNIEnv *env, jobject obj)
{
    return reinterpret_cast<int>(GET_NATIVE_VIEW(env, obj)
        ->m_cacheHitNode->nodePointer());
}

static void nativeClearCursor(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->clearCursor();
}

static void nativeCreate(JNIEnv *env, jobject obj, int viewImpl)
{
    WebView* webview = new WebView(env, obj, viewImpl);
    // NEED THIS OR SOMETHING LIKE IT!
    //Release(obj);
}

static jint nativeCursorFramePointer(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return 0;
    const CachedFrame* frame = 0;
    (void) root->currentCursor(&frame);
    return reinterpret_cast<int>(frame ? frame->framePointer() : 0);
}

static const CachedNode* getCursorNode(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    return root ? root->currentCursor() : 0;
}

static const CachedNode* getCursorNode(JNIEnv *env, jobject obj,
    const CachedFrame** frame)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    return root ? root->currentCursor(frame) : 0;
}

static const CachedNode* getFocusCandidate(JNIEnv *env, jobject obj,
    const CachedFrame** frame)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return 0;
    const CachedNode* cursor = root->currentCursor(frame);
    if (cursor && cursor->wantsKeyEvents())
        return cursor;
    return root->currentFocus();
}

static bool focusCandidateHasNextTextfield(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return false;
    const CachedNode* cursor = root->currentCursor();
    if (!cursor || !cursor->isTextInput())
        cursor = root->currentFocus();
    if (!cursor || !cursor->isTextInput()) return false;
    return root->nextTextField(cursor, 0);
}

static const CachedNode* getFocusNode(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    return root ? root->currentFocus() : 0;
}

static const CachedNode* getFocusNode(JNIEnv *env, jobject obj,
    const CachedFrame** frame)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    return root ? root->currentFocus(frame) : 0;
}

static const CachedInput* getInputCandidate(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return 0;
    const CachedFrame* frame;
    const CachedNode* cursor = root->currentCursor(&frame);
    if (!cursor || !cursor->wantsKeyEvents())
        cursor = root->currentFocus(&frame);
    return cursor ? frame->textInput(cursor) : 0;
}

static jboolean nativeCursorMatchesFocus(JNIEnv *env, jobject obj)
{
    const CachedNode* cursor = getCursorNode(env, obj);
    const CachedNode* focus = getFocusNode(env, obj);
    return cursor && focus && cursor->nodePointer() == focus->nodePointer();
}

static jobject nativeCursorNodeBounds(JNIEnv *env, jobject obj)
{
    const CachedFrame* frame;
    const CachedNode* node = getCursorNode(env, obj, &frame);
    WebCore::IntRect bounds = node ? node->bounds(frame)
        : WebCore::IntRect(0, 0, 0, 0);
    jclass rectClass = env->FindClass("android/graphics/Rect");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    jobject rect = env->NewObject(rectClass, init, bounds.x(),
        bounds.y(), bounds.right(), bounds.bottom());
    return rect;
}

static jint nativeCursorNodePointer(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getCursorNode(env, obj);
    return reinterpret_cast<int>(node ? node->nodePointer() : 0);
}

static jobject nativeCursorPosition(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    const CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    WebCore::IntPoint pos = WebCore::IntPoint(0, 0);
    if (root)
        root->getSimulatedMousePosition(&pos);
    jclass pointClass = env->FindClass("android/graphics/Point");
    jmethodID init = env->GetMethodID(pointClass, "<init>", "(II)V");
    jobject point = env->NewObject(pointClass, init, pos.x(), pos.y());
    return point;
}

static WebCore::IntRect jrect_to_webrect(JNIEnv* env, jobject obj)
{
    int L, T, R, B;
    GraphicsJNI::get_jrect(env, obj, &L, &T, &R, &B);
    return WebCore::IntRect(L, T, R - L, B - T);
}

static bool nativeCursorIntersects(JNIEnv *env, jobject obj, jobject visRect)
{
    const CachedFrame* frame;
    const CachedNode* node = getCursorNode(env, obj, &frame);
    return node ? node->bounds(frame).intersects(
        jrect_to_webrect(env, visRect)) : false;
}

static bool nativeCursorIsAnchor(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getCursorNode(env, obj);
    return node ? node->isAnchor() : false;
}

static bool nativeCursorIsTextInput(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getCursorNode(env, obj);
    return node ? node->isTextInput() : false;
}

static jobject nativeCursorText(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getCursorNode(env, obj);
    if (!node)
        return 0;
    WebCore::String value = node->getExport();
    return !value.isEmpty() ? env->NewString((jchar *)value.characters(),
        value.length()) : 0;
}

static void nativeDebugDump(JNIEnv *env, jobject obj)
{
#if DUMP_NAV_CACHE
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->debugDump();
#endif
}

static void nativeDrawExtras(JNIEnv *env, jobject obj, jobject canv, jint extras)
{
    SkCanvas* canvas = GraphicsJNI::getNativeCanvas(env, canv);
    GET_NATIVE_VIEW(env, obj)->drawExtras(canvas, extras);
}

static bool nativeEvaluateLayersAnimations(JNIEnv *env, jobject obj)
{
#if USE(ACCELERATED_COMPOSITING)
    const LayerAndroid* root = GET_NATIVE_VIEW(env, obj)->rootLayer();
    if (root)
        return root->evaluateAnimations();
#endif
    return false;
}

static void nativeSetRootLayer(JNIEnv *env, jobject obj, jint layer)
{
#if USE(ACCELERATED_COMPOSITING)
    LayerAndroid* layerImpl = reinterpret_cast<LayerAndroid*>(layer);
    GET_NATIVE_VIEW(env, obj)->setRootLayer(layerImpl);
#endif
}

static jobject nativeImageURI(JNIEnv *env, jobject obj, jint x, jint y)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    WebCore::String uri = view->imageURI(x, y);
    jstring ret = 0;
    unsigned len = uri.length();
    if (len) {
        ret = env->NewString((jchar*) uri.characters(), len);
        env->DeleteLocalRef(ret);
    }
    return ret;
}

static jint nativeFocusCandidateFramePointer(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return 0;
    const CachedFrame* frame = 0;
    const CachedNode* cursor = root->currentCursor(&frame);
    if (!cursor || !cursor->wantsKeyEvents())
        (void) root->currentFocus(&frame);
    return reinterpret_cast<int>(frame ? frame->framePointer() : 0);
}

static bool nativeFocusCandidateIsPassword(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    return input && input->inputType() == WebCore::HTMLInputElement::PASSWORD;
}

static bool nativeFocusCandidateIsRtlText(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    return input ? input->isRtlText() : false;
}

static bool nativeFocusCandidateIsTextInput(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getFocusCandidate(env, obj, 0);
    return node ? node->isTextInput() : false;
}

static jint nativeFocusCandidateMaxLength(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    return input ? input->maxLength() : false;
}

static jobject nativeFocusCandidateName(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    if (!input)
        return 0;
    const WebCore::String& name = input->name();
    return env->NewString((jchar*)name.characters(), name.length());
}

static jobject createJavaRect(JNIEnv* env, int x, int y, int right, int bottom)
{
    jclass rectClass = env->FindClass("android/graphics/Rect");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    jobject rect = env->NewObject(rectClass, init, x, y, right, bottom);
    return rect;
}

static jobject nativeFocusCandidateNodeBounds(JNIEnv *env, jobject obj)
{
    const CachedFrame* frame;
    const CachedNode* node = getFocusCandidate(env, obj, &frame);
    WebCore::IntRect bounds = node ? node->bounds(frame)
        : WebCore::IntRect(0, 0, 0, 0);
    return createJavaRect(env, bounds.x(), bounds.y(), bounds.right(), bounds.bottom());
}

static jobject nativeFocusCandidatePaddingRect(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    if (!input)
        return 0;
    // Note that the Java Rect is being used to pass four integers, rather than
    // being used as an actual rectangle.
    return createJavaRect(env, input->paddingLeft(), input->paddingTop(),
            input->paddingRight(), input->paddingBottom());
}

static jint nativeFocusCandidatePointer(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getFocusCandidate(env, obj, 0);
    return reinterpret_cast<int>(node ? node->nodePointer() : 0);
}

static jobject nativeFocusCandidateText(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getFocusCandidate(env, obj, 0);
    if (!node)
        return 0;
    WebCore::String value = node->getExport();
    return !value.isEmpty() ? env->NewString((jchar *)value.characters(),
        value.length()) : 0;
}

static jint nativeFocusCandidateTextSize(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    return input ? input->textSize() : 0;
}

enum type {
    NONE = -1,
    NORMAL_TEXT_FIELD = 0,
    TEXT_AREA = 1,
    PASSWORD = 2,
    SEARCH = 3,
    EMAIL = 4,
    NUMBER = 5,
    TELEPHONE = 6,
    URL = 7
};

static int nativeFocusCandidateType(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    if (!input) return NONE;
    if (!input->isTextField()) return TEXT_AREA;
    switch (input->inputType()) {
    case HTMLInputElement::PASSWORD:
        return PASSWORD;
    case HTMLInputElement::SEARCH:
        return SEARCH;
    case HTMLInputElement::EMAIL:
        return EMAIL;
    case HTMLInputElement::NUMBER:
        return NUMBER;
    case HTMLInputElement::TELEPHONE:
        return TELEPHONE;
    case HTMLInputElement::URL:
        return URL;
    default:
        return NORMAL_TEXT_FIELD;
    }
}

static bool nativeFocusIsPlugin(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getFocusNode(env, obj);
    return node ? node->isPlugin() : false;
}

static jobject nativeFocusNodeBounds(JNIEnv *env, jobject obj)
{
    const CachedFrame* frame;
    const CachedNode* node = getFocusNode(env, obj, &frame);
    WebCore::IntRect bounds = node ? node->bounds(frame)
        : WebCore::IntRect(0, 0, 0, 0);
    jclass rectClass = env->FindClass("android/graphics/Rect");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    jobject rect = env->NewObject(rectClass, init, bounds.x(),
        bounds.y(), bounds.right(), bounds.bottom());
    return rect;
}

static jint nativeFocusNodePointer(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getFocusNode(env, obj);
    return node ? reinterpret_cast<int>(node->nodePointer()) : 0;
}

static bool nativeCursorWantsKeyEvents(JNIEnv* env, jobject jwebview) {
    WebView* view = GET_NATIVE_VIEW(env, jwebview);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return view->cursorWantsKeyEvents();
}

static void nativeHideCursor(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->hideCursor();
}

static void nativeInstrumentReport(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounter::reportNow();
#endif
}

static void nativeSelectBestAt(JNIEnv *env, jobject obj, jobject jrect)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    WebCore::IntRect rect = jrect_to_webrect(env, jrect);
    view->selectBestAt(rect);
}

static jobject nativeSubtractLayers(JNIEnv* env, jobject obj, jobject jrect)
{
    SkIRect irect = jrect_to_webrect(env, jrect);
#if USE(ACCELERATED_COMPOSITING)
    LayerAndroid* root = GET_NATIVE_VIEW(env, obj)->rootLayer();
    if (root) {
        SkRect rect;
        rect.set(irect);
        rect = root->subtractLayers(rect);
        rect.round(&irect);
    }
#endif
    jclass rectClass = env->FindClass("android/graphics/Rect");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    return env->NewObject(rectClass, init, irect.fLeft, irect.fTop,
        irect.fRight, irect.fBottom);
}

static jint nativeTextGeneration(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    return root ? root->textGeneration() : 0;
}

static bool nativePointInNavCache(JNIEnv *env, jobject obj,
    int x, int y, int slop)
{
    return GET_NATIVE_VIEW(env, obj)->pointInNavCache(x, y, slop);
}

static bool nativeMotionUp(JNIEnv *env, jobject obj,
    int x, int y, int slop)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return view->motionUp(x, y, slop);
}

static bool nativeHasCursorNode(JNIEnv *env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->hasCursorNode();
}

static bool nativeHasFocusNode(JNIEnv *env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->hasFocusNode();
}

static bool nativeMoveCursor(JNIEnv *env, jobject obj,
    int key, int count, bool ignoreScroll)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    DBG_NAV_LOGD("env=%p obj=%p view=%p", env, obj, view);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return view->moveCursor(key, count, ignoreScroll);
}

static void nativeRecordButtons(JNIEnv* env, jobject obj, bool hasFocus,
        bool pressed, bool invalidate)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->nativeRecordButtons(hasFocus, pressed, invalidate);
}

static void nativeSetFindIsUp(JNIEnv *env, jobject obj, jboolean isUp)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->setFindIsUp(isUp);
}

static void nativeSetFindIsEmpty(JNIEnv *env, jobject obj)
{
    GET_NATIVE_VIEW(env, obj)->setFindIsEmpty();
}

static void nativeSetFollowedLink(JNIEnv *env, jobject obj, bool followed)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->setFollowedLink(followed);
}

static void nativeSetHeightCanMeasure(JNIEnv *env, jobject obj, bool measure)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in nativeSetHeightCanMeasure");
    view->setHeightCanMeasure(measure);
}

static jobject nativeGetCursorRingBounds(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    jclass rectClass = env->FindClass("android/graphics/Rect");
    LOG_ASSERT(rectClass, "Could not find Rect class!");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    LOG_ASSERT(init, "Could not find constructor for Rect");
    WebCore::IntRect webRect;
    view->cursorRingBounds(&webRect);
    jobject rect = env->NewObject(rectClass, init, webRect.x(),
        webRect.y(), webRect.right(), webRect.bottom());
    return rect;
}

static int nativeFindAll(JNIEnv *env, jobject obj, jstring findLower,
        jstring findUpper)
{
    // If one or the other is null, do not search.
    if (!(findLower && findUpper))
        return 0;
    // Obtain the characters for both the lower case string and the upper case
    // string representing the same word.
    const jchar* findLowerChars = env->GetStringChars(findLower, 0);
    const jchar* findUpperChars = env->GetStringChars(findUpper, 0);
    // If one or the other is null, do not search.
    if (!(findLowerChars && findUpperChars)) {
        if (findLowerChars)
            env->ReleaseStringChars(findLower, findLowerChars);
        if (findUpperChars)
            env->ReleaseStringChars(findUpper, findUpperChars);
        checkException(env);
        return 0;
    }
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in nativeFindAll");
    CachedRoot* root = view->getFrameCache(WebView::AllowNewer);
    if (!root) {
        env->ReleaseStringChars(findLower, findLowerChars);
        env->ReleaseStringChars(findUpper, findUpperChars);
        checkException(env);
        return 0;
    }
    int length = env->GetStringLength(findLower);
    // If the lengths of the strings do not match, then they are not the same
    // word, so do not search.
    if (!length || env->GetStringLength(findUpper) != length) {
        env->ReleaseStringChars(findLower, findLowerChars);
        env->ReleaseStringChars(findUpper, findUpperChars);
        checkException(env);
        return 0;
    }
    int width = root->documentWidth();
    int height = root->documentHeight();
    // Create a FindCanvas, which allows us to fake draw into it so we can
    // figure out where our search string is rendered (and how many times).
    FindCanvas canvas(width, height, (const UChar*) findLowerChars,
            (const UChar*) findUpperChars, length << 1);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    canvas.setBitmapDevice(bitmap);
    root->draw(canvas);
    WTF::Vector<MatchInfo>* matches = canvas.detachMatches();
    // With setMatches, the WebView takes ownership of matches
    view->setMatches(matches);

    env->ReleaseStringChars(findLower, findLowerChars);
    env->ReleaseStringChars(findUpper, findUpperChars);
    checkException(env);
    return canvas.found();
}

static void nativeFindNext(JNIEnv *env, jobject obj, bool forward)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in nativeFindNext");
    view->findNext(forward);
}

static int nativeFindIndex(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in nativeFindIndex");
    return view->currentMatchIndex();
}

static void nativeUpdateCachedTextfield(JNIEnv *env, jobject obj, jstring updatedText, jint generation)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in nativeUpdateCachedTextfield");
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return;
    const CachedNode* cachedFocusNode = root->currentFocus();
    if (!cachedFocusNode || !cachedFocusNode->isTextInput())
        return;
    WebCore::String webcoreString = to_string(env, updatedText);
    (const_cast<CachedNode*>(cachedFocusNode))->setExport(webcoreString);
    root->setTextGeneration(generation);
    checkException(env);
}

static jint nativeGetBlockLeftEdge(JNIEnv *env, jobject obj, jint x, jint y,
        jfloat scale)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    if (!view)
        return -1;
    return view->getBlockLeftEdge(x, y, scale);
}

static void nativeDestroy(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOGD("nativeDestroy view: %p", view);
    LOG_ASSERT(view, "view not set in nativeDestroy");
    delete view;
}

static bool nativeMoveCursorToNextTextInput(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return false;
    const CachedNode* current = root->currentCursor();
    if (!current || !current->isTextInput())
        current = root->currentFocus();
    if (!current || !current->isTextInput())
        return false;
    const CachedFrame* frame;
    const CachedNode* next = root->nextTextField(current, &frame);
    if (!next)
        return false;
    const WebCore::IntRect& bounds = next->bounds(frame);
    root->rootHistory()->setMouseBounds(bounds);
    view->getWebViewCore()->updateCursorBounds(root, frame, next);
    root->setCursor(const_cast<CachedFrame*>(frame),
            const_cast<CachedNode*>(next));
    view->sendMoveFocus(static_cast<WebCore::Frame*>(frame->framePointer()),
            static_cast<WebCore::Node*>(next->nodePointer()));
    if (!next->isInLayer())
        view->scrollRectOnScreen(bounds);
    view->getWebViewCore()->m_moveGeneration++;
    return true;
}

static int nativeMoveGeneration(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    if (!view)
        return 0;
    return view->moveGeneration();
}

static void nativeMoveSelection(JNIEnv *env, jobject obj, int x, int y)
{
    GET_NATIVE_VIEW(env, obj)->moveSelection(x, y);
}

static void nativeResetSelection(JNIEnv *env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->resetSelection();
}

static void nativeSelectAll(JNIEnv* env, jobject obj)
{
    GET_NATIVE_VIEW(env, obj)->selectAll();
}

static void nativeSetExtendSelection(JNIEnv *env, jobject obj)
{
    GET_NATIVE_VIEW(env, obj)->setExtendSelection();
}

static jboolean nativeStartSelection(JNIEnv *env, jobject obj, int x, int y)
{
    return GET_NATIVE_VIEW(env, obj)->startSelection(x, y);
}

static jboolean nativeWordSelection(JNIEnv *env, jobject obj, int x, int y)
{
    return GET_NATIVE_VIEW(env, obj)->wordSelection(x, y);
}

static void nativeExtendSelection(JNIEnv *env, jobject obj, int x, int y)
{
    GET_NATIVE_VIEW(env, obj)->extendSelection(x, y);
}

static jobject nativeGetSelection(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    String selection = view->getSelection();
    return env->NewString((jchar*)selection.characters(), selection.length());
}

static jboolean nativeHitSelection(JNIEnv *env, jobject obj, int x, int y)
{
    return GET_NATIVE_VIEW(env, obj)->hitSelection(x, y);
}

static jint nativeSelectionX(JNIEnv *env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->selectionX();
}

static jint nativeSelectionY(JNIEnv *env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->selectionY();
}

static void nativeSetSelectionPointer(JNIEnv *env, jobject obj, jboolean set,
    jfloat scale, jint x, jint y)
{
    GET_NATIVE_VIEW(env, obj)->setSelectionPointer(set, scale, x, y);
}

#ifdef ANDROID_DUMP_DISPLAY_TREE
static void dumpToFile(const char text[], void* file) {
    fwrite(text, 1, strlen(text), reinterpret_cast<FILE*>(file));
    fwrite("\n", 1, 1, reinterpret_cast<FILE*>(file));
}
#endif

static void nativeDumpDisplayTree(JNIEnv* env, jobject jwebview, jstring jurl)
{
#ifdef ANDROID_DUMP_DISPLAY_TREE
    WebView* view = GET_NATIVE_VIEW(env, jwebview);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);

    if (view && view->getWebViewCore()) {
        FILE* file = fopen(DISPLAY_TREE_LOG_FILE, "w");
        if (file) {
            SkFormatDumper dumper(dumpToFile, file);
            // dump the URL
            if (jurl) {
                const char* str = env->GetStringUTFChars(jurl, 0);
                SkDebugf("Dumping %s to %s\n", str, DISPLAY_TREE_LOG_FILE);
                dumpToFile(str, file);
                env->ReleaseStringUTFChars(jurl, str);
            }
            // now dump the display tree
            SkDumpCanvas canvas(&dumper);
            // this will playback the picture into the canvas, which will
            // spew its contents to the dumper
            view->getWebViewCore()->drawContent(&canvas, 0);
#if USE(ACCELERATED_COMPOSITING)
            if (true) {
                LayerAndroid* rootLayer = view->rootLayer();
                if (rootLayer) {
                    // We have to set the canvas' matrix on the root layer
                    // (to have fixed layers work as intended)
                    SkAutoCanvasRestore restore(&canvas, true);
                    rootLayer->setMatrix(canvas.getTotalMatrix());
                    canvas.resetMatrix();
                    rootLayer->draw(&canvas);
                }
            }
#endif
            // we're done with the file now
            fwrite("\n", 1, 1, file);
            fclose(file);
        }
#if USE(ACCELERATED_COMPOSITING)
        const LayerAndroid* rootLayer = view->rootLayer();
        if (rootLayer) {
          FILE* file = fopen(LAYERS_TREE_LOG_FILE,"w");
          if (file) {
              rootLayer->dumpLayers(file, 0);
              fclose(file);
          }
        }
#endif
    }
#endif
}

/*
 * JNI registration
 */
static JNINativeMethod gJavaWebViewMethods[] = {
    { "nativeCacheHitFramePointer", "()I",
        (void*) nativeCacheHitFramePointer },
    { "nativeCacheHitNodeBounds", "()Landroid/graphics/Rect;",
        (void*) nativeCacheHitNodeBounds },
    { "nativeCacheHitNodePointer", "()I",
        (void*) nativeCacheHitNodePointer },
    { "nativeClearCursor", "()V",
        (void*) nativeClearCursor },
    { "nativeCreate", "(I)V",
        (void*) nativeCreate },
    { "nativeCursorFramePointer", "()I",
        (void*) nativeCursorFramePointer },
    { "nativeCursorMatchesFocus", "()Z",
        (void*) nativeCursorMatchesFocus },
    { "nativeCursorNodeBounds", "()Landroid/graphics/Rect;",
        (void*) nativeCursorNodeBounds },
    { "nativeCursorNodePointer", "()I",
        (void*) nativeCursorNodePointer },
    { "nativeCursorIntersects", "(Landroid/graphics/Rect;)Z",
        (void*) nativeCursorIntersects },
    { "nativeCursorIsAnchor", "()Z",
        (void*) nativeCursorIsAnchor },
    { "nativeCursorIsTextInput", "()Z",
        (void*) nativeCursorIsTextInput },
    { "nativeCursorPosition", "()Landroid/graphics/Point;",
        (void*) nativeCursorPosition },
    { "nativeCursorText", "()Ljava/lang/String;",
        (void*) nativeCursorText },
    { "nativeCursorWantsKeyEvents", "()Z",
        (void*)nativeCursorWantsKeyEvents },
    { "nativeDebugDump", "()V",
        (void*) nativeDebugDump },
    { "nativeDestroy", "()V",
        (void*) nativeDestroy },
    { "nativeDrawExtras", "(Landroid/graphics/Canvas;I)V",
        (void*) nativeDrawExtras },
    { "nativeDumpDisplayTree", "(Ljava/lang/String;)V",
        (void*) nativeDumpDisplayTree },
    { "nativeEvaluateLayersAnimations", "()Z",
        (void*) nativeEvaluateLayersAnimations },
    { "nativeExtendSelection", "(II)V",
        (void*) nativeExtendSelection },
    { "nativeFindAll", "(Ljava/lang/String;Ljava/lang/String;)I",
        (void*) nativeFindAll },
    { "nativeFindNext", "(Z)V",
        (void*) nativeFindNext },
    { "nativeFindIndex", "()I",
        (void*) nativeFindIndex},
    { "nativeFocusCandidateFramePointer", "()I",
        (void*) nativeFocusCandidateFramePointer },
    { "nativeFocusCandidateHasNextTextfield", "()Z",
        (void*) focusCandidateHasNextTextfield },
    { "nativeFocusCandidateIsPassword", "()Z",
        (void*) nativeFocusCandidateIsPassword },
    { "nativeFocusCandidateIsRtlText", "()Z",
        (void*) nativeFocusCandidateIsRtlText },
    { "nativeFocusCandidateIsTextInput", "()Z",
        (void*) nativeFocusCandidateIsTextInput },
    { "nativeFocusCandidateMaxLength", "()I",
        (void*) nativeFocusCandidateMaxLength },
    { "nativeFocusCandidateName", "()Ljava/lang/String;",
        (void*) nativeFocusCandidateName },
    { "nativeFocusCandidateNodeBounds", "()Landroid/graphics/Rect;",
        (void*) nativeFocusCandidateNodeBounds },
    { "nativeFocusCandidatePaddingRect", "()Landroid/graphics/Rect;",
        (void*) nativeFocusCandidatePaddingRect },
    { "nativeFocusCandidatePointer", "()I",
        (void*) nativeFocusCandidatePointer },
    { "nativeFocusCandidateText", "()Ljava/lang/String;",
        (void*) nativeFocusCandidateText },
    { "nativeFocusCandidateTextSize", "()I",
        (void*) nativeFocusCandidateTextSize },
    { "nativeFocusCandidateType", "()I",
        (void*) nativeFocusCandidateType },
    { "nativeFocusIsPlugin", "()Z",
        (void*) nativeFocusIsPlugin },
    { "nativeFocusNodeBounds", "()Landroid/graphics/Rect;",
        (void*) nativeFocusNodeBounds },
    { "nativeFocusNodePointer", "()I",
        (void*) nativeFocusNodePointer },
    { "nativeGetCursorRingBounds", "()Landroid/graphics/Rect;",
        (void*) nativeGetCursorRingBounds },
    { "nativeGetSelection", "()Ljava/lang/String;",
        (void*) nativeGetSelection },
    { "nativeHasCursorNode", "()Z",
        (void*) nativeHasCursorNode },
    { "nativeHasFocusNode", "()Z",
        (void*) nativeHasFocusNode },
    { "nativeHideCursor", "()V",
        (void*) nativeHideCursor },
    { "nativeHitSelection", "(II)Z",
        (void*) nativeHitSelection },
    { "nativeImageURI", "(II)Ljava/lang/String;",
        (void*) nativeImageURI },
    { "nativeInstrumentReport", "()V",
        (void*) nativeInstrumentReport },
    { "nativeMotionUp", "(III)Z",
        (void*) nativeMotionUp },
    { "nativeMoveCursor", "(IIZ)Z",
        (void*) nativeMoveCursor },
    { "nativeMoveCursorToNextTextInput", "()Z",
        (void*) nativeMoveCursorToNextTextInput },
    { "nativeMoveGeneration", "()I",
        (void*) nativeMoveGeneration },
    { "nativeMoveSelection", "(II)V",
        (void*) nativeMoveSelection },
    { "nativePointInNavCache", "(III)Z",
        (void*) nativePointInNavCache },
    { "nativeRecordButtons", "(ZZZ)V",
        (void*) nativeRecordButtons },
    { "nativeResetSelection", "()V",
        (void*) nativeResetSelection },
    { "nativeSelectAll", "()V",
        (void*) nativeSelectAll },
    { "nativeSelectBestAt", "(Landroid/graphics/Rect;)V",
        (void*) nativeSelectBestAt },
    { "nativeSelectionX", "()I",
        (void*) nativeSelectionX },
    { "nativeSelectionY", "()I",
        (void*) nativeSelectionY },
    { "nativeSetExtendSelection", "()V",
        (void*) nativeSetExtendSelection },
    { "nativeSetFindIsEmpty", "()V",
        (void*) nativeSetFindIsEmpty },
    { "nativeSetFindIsUp", "(Z)V",
        (void*) nativeSetFindIsUp },
    { "nativeSetFollowedLink", "(Z)V",
        (void*) nativeSetFollowedLink },
    { "nativeSetHeightCanMeasure", "(Z)V",
        (void*) nativeSetHeightCanMeasure },
    { "nativeSetRootLayer", "(I)V",
        (void*) nativeSetRootLayer },
    { "nativeSetSelectionPointer", "(ZFII)V",
        (void*) nativeSetSelectionPointer },
    { "nativeStartSelection", "(II)Z",
        (void*) nativeStartSelection },
    { "nativeSubtractLayers", "(Landroid/graphics/Rect;)Landroid/graphics/Rect;",
        (void*) nativeSubtractLayers },
    { "nativeTextGeneration", "()I",
        (void*) nativeTextGeneration },
    { "nativeUpdateCachedTextfield", "(Ljava/lang/String;I)V",
        (void*) nativeUpdateCachedTextfield },
    {  "nativeWordSelection", "(II)Z",
        (void*) nativeWordSelection },
    { "nativeGetBlockLeftEdge", "(IIF)I",
        (void*) nativeGetBlockLeftEdge },
};

int register_webview(JNIEnv* env)
{
    jclass clazz = env->FindClass("android/webkit/WebView");
    LOG_ASSERT(clazz, "Unable to find class android/webkit/WebView");
    gWebViewField = env->GetFieldID(clazz, "mNativeClass", "I");
    LOG_ASSERT(gWebViewField, "Unable to find android/webkit/WebView.mNativeClass");

    return jniRegisterNativeMethods(env, "android/webkit/WebView", gJavaWebViewMethods, NELEM(gJavaWebViewMethods));
}

} // namespace android
