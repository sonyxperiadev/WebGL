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
#include "SkBlurMaskFilter.h"
#include "SkCanvas.h"
#include "SkCornerPathEffect.h"
#include "SkDumpCanvas.h"
#include "SkPath.h"
#include "SkPicture.h"
#include "SkPixelXorXfermode.h"
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

struct JavaGlue {
    jweak       m_obj;
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
    AutoJObject object(JNIEnv* env) {
        return getRealObject(env, m_obj);
    }
} m_javaGlue;

WebView(JNIEnv* env, jobject javaWebView, int viewImpl)
{
    jclass clazz = env->FindClass("android/webkit/WebView");
 //   m_javaGlue = new JavaGlue;
    m_javaGlue.m_obj = env->NewWeakGlobalRef(javaWebView);
    m_javaGlue.m_scrollBy = GetJMethod(env, clazz, "setContentScrollBy", "(IIZ)Z");
    m_javaGlue.m_clearTextEntry = GetJMethod(env, clazz, "clearTextEntry", "()V");
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

    env->SetIntField(javaWebView, gWebViewField, (jint)this);
    m_viewImpl = (WebViewCore*) viewImpl;
    m_frameCacheUI = 0;
    m_navPictureUI = 0;
    m_generation = 0;
    m_heightCanMeasure = false;
    m_followedLink = false;
    m_lastDx = 0;
    m_lastDxTime = 0;
    m_ringAnimationEnd = 0;
    m_selStart.setEmpty();
    m_selEnd.setEmpty();
    m_matches = 0;
    m_hasCurrentLocation = false;
    m_isFindPaintSetUp = false;
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
    if (m_matches)
        delete m_matches;
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
    env->CallVoidMethod(m_javaGlue.object(env).get(), m_javaGlue.m_clearTextEntry);
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
            cachedCursor = root->currentCursor();
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
                } else if (m_followedLink || pressed) {
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
        const WebCore::IntRect& b = cachedCursor->getBounds();
        viewInvalidateRect(b.x(), b.y(), b.right(), b.bottom());
    }
}

// These two functions separate out the particular look of the drawn find
// matches from the code that draws them.  This function sets up the paints that
// are used to draw the matches.
void setUpFindPaint()
{
    // Set up the foreground paint
    m_findPaint.setAntiAlias(true);
    const SkScalar roundiness = SkIntToScalar(2);
    SkCornerPathEffect* cornerEffect = new SkCornerPathEffect(roundiness);
    m_findPaint.setPathEffect(cornerEffect);
    m_findPaint.setARGB(255, 132, 190, 0);

    // Set up the background blur paint.
    m_findBlurPaint.setAntiAlias(true);
    m_findBlurPaint.setARGB(204, 0, 0, 0);
    m_findBlurPaint.setPathEffect(cornerEffect);
    cornerEffect->unref();
    SkMaskFilter* blurFilter = SkBlurMaskFilter::Create(SK_Scalar1,
            SkBlurMaskFilter::kNormal_BlurStyle);
    m_findBlurPaint.setMaskFilter(blurFilter)->unref();
    m_isFindPaintSetUp = true;
}

// Draw the match specified by region to the canvas.
void drawMatch(const SkRegion& region, SkCanvas* canvas, bool focused)
{
    // For the match which has focus, use a filled paint.  For the others, use
    // a stroked paint.
    if (focused) {
        m_findPaint.setStyle(SkPaint::kFill_Style);
        m_findBlurPaint.setStyle(SkPaint::kFill_Style);
    } else {
        m_findPaint.setStyle(SkPaint::kStroke_Style);
        m_findPaint.setStrokeWidth(SK_Scalar1);
        m_findBlurPaint.setStyle(SkPaint::kStroke_Style);
        m_findBlurPaint.setStrokeWidth(SkIntToScalar(2));
    }
    // Find the path for the current match
    SkPath matchPath;
    region.getBoundaryPath(&matchPath);
    // Offset the path for a blurred shadow
    SkPath blurPath;
    matchPath.offset(SK_Scalar1, SkIntToScalar(2), &blurPath);
    int saveCount = 0;
    if (!focused) {
        saveCount = canvas->save();
        canvas->clipPath(matchPath, SkRegion::kDifference_Op);
    }
    // Draw the blurred background
    canvas->drawPath(blurPath, m_findBlurPaint);
    if (!focused) {
        canvas->restoreToCount(saveCount);
    }
    // Draw the foreground
    canvas->drawPath(matchPath, m_findPaint);
}

bool scrollRectOnScreen(int left, int top, int right, int bottom)
{
    WebCore::IntRect visible;
    getVisibleRect(&visible);
    int dx = 0;
    if (left < visible.x()) {
        dx = left - visible.x();
    // Only scroll right if the entire width can fit on screen.
    } else if (right > visible.right() && right - left < visible.width()) {
        dx = right - visible.right();
    }
    int dy = 0;
    if (top < visible.y()) {
        dy = top - visible.y();
    // Only scroll down if the entire height can fit on screen
    } else if (bottom > visible.bottom() && bottom - top < visible.height()) {
        dy = bottom - visible.bottom();
    }
    if ((dx|dy) == 0 || !scrollBy(dx, dy))
        return false;
    viewInvalidate();
    return true;
}

// Put a cap on the number of matches to draw.  If the current page has more
// matches than this, only draw the focused match.
#define MAX_NUMBER_OF_MATCHES_TO_DRAW 101

void drawMatches(SkCanvas* canvas)
{
    if (!m_matches || !m_matches->size())
        return;
    if (m_findIndex >= m_matches->size())
        m_findIndex = 0;
    const MatchInfo& matchInfo = (*m_matches)[m_findIndex];
    const SkRegion& currentMatchRegion = matchInfo.getLocation();
    const SkIRect& currentMatchBounds = currentMatchRegion.getBounds();
    if (scrollRectOnScreen(currentMatchBounds.fLeft, currentMatchBounds.fTop,
            currentMatchBounds.fRight, currentMatchBounds.fBottom))
        return;

    // Set up the paints used for drawing the matches
    if (!m_isFindPaintSetUp)
        setUpFindPaint();

    // Draw the current match
    drawMatch(currentMatchRegion, canvas, true);
    // Now draw the picture, so that it shows up on top of the rectangle
    canvas->drawPicture(*matchInfo.getPicture());

    // Draw the rest
    unsigned numberOfMatches = m_matches->size();
    if (numberOfMatches > 1
            && numberOfMatches < MAX_NUMBER_OF_MATCHES_TO_DRAW) {
        WebCore::IntRect visible;
        getVisibleRect(&visible);
        SkIRect visibleIRect(visible);
        for(unsigned i = 0; i < numberOfMatches; i++) {
            // The current match has already been drawn
            if (i == m_findIndex)
                continue;
            const SkRegion& region = (*m_matches)[i].getLocation();
            // Do not draw matches which intersect the current one, or if it is
            // offscreen
            if (currentMatchRegion.intersects(region)
                    || !region.intersects(visibleIRect))
                continue;
            drawMatch(region, canvas, false);
        }
    }
}

void resetCursorRing()
{
    m_followedLink = false;
    m_viewImpl->m_hasCursorBounds = false;
}

void drawCursorRing(SkCanvas* canvas)
{
    const CachedRoot* root = getFrameCache(AllowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        resetCursorRing();
        return;
    }
    const CachedFrame* frame;
    const CachedNode* node = root->currentCursor(&frame);
    if (!node) {
        DBG_NAV_LOGV("%s", "!node");
        resetCursorRing();
        return;
    }
    if (node->isHidden()) {
        DBG_NAV_LOG("node->isHidden()");
        m_viewImpl->m_hasCursorBounds = false;
        return;
    }
    const WTF::Vector<WebCore::IntRect>* rings = &node->cursorRings();
    if (!rings->size()) {
        DBG_NAV_LOG("!rings->size()");
        m_viewImpl->m_hasCursorBounds = false;
        return;
    }
    bool isButton = false;
    m_viewImpl->gButtonMutex.lock();
    // If this is a button drawn by us (rather than webkit) do not draw the
    // cursor ring, since its cursor will be shown by a change in what we draw.
    // Should be in sync with recordButtons, since that will be called
    // before this.
    if (m_viewImpl->m_buttons.size() > 0) {
        WebCore::Node* cursorPointer = (WebCore::Node*) node->nodePointer();
        Container* end = m_viewImpl->m_buttons.end();
        for (Container* ptr = m_viewImpl->m_buttons.begin(); ptr != end; ptr++) {
            if (ptr->matches(cursorPointer)) {
                isButton = true;
                break;
            }
        }
    }
    m_viewImpl->gButtonMutex.unlock();
    WebCore::IntRect bounds = node->bounds();
    updateCursorBounds(root, frame, node);

    WTF::Vector<WebCore::IntRect> oneRing;
    bool useHitBounds = node->useHitBounds();
    if (useHitBounds) {
        bounds = node->hitBounds();
    }
    if (useHitBounds || node->useBounds()) {
        oneRing.append(bounds);
        rings = &oneRing;
    }
    bounds.inflate(SkScalarCeil(CURSOR_RING_OUTER_DIAMETER));
    if (canvas->quickReject(bounds, SkCanvas::kAA_EdgeType)) {
        DBG_NAV_LOGD("canvas->quickReject cursorNode=%d (nodePointer=%p)"
            " bounds=(%d,%d,w=%d,h=%d)", node->index(), node->nodePointer(),
            bounds.x(), bounds.y(), bounds.width(), bounds.height());
        m_followedLink = false;
        return;
    }
    if (!node->hasCursorRing() || (node->isPlugin() && node->isFocus()))
        return;
    CursorRing::Flavor flavor = CursorRing::NORMAL_FLAVOR;
    if (!isButton) {
        flavor = node->isSyntheticLink()
            ? CursorRing::FAKE_FLAVOR : CursorRing::NORMAL_FLAVOR;
        if (m_followedLink) {
            flavor = static_cast<CursorRing::Flavor>
                    (flavor + CursorRing::NORMAL_ANIMATING);
        }
#if DEBUG_NAV_UI
        const WebCore::IntRect& ring = (*rings)[0];
        DBG_NAV_LOGD("cursorNode=%d (nodePointer=%p) flavor=%s rings=%d"
            " (%d, %d, %d, %d) isPlugin=%s",
            node->index(), node->nodePointer(),
            flavor == CursorRing::FAKE_FLAVOR ? "FAKE_FLAVOR" :
            flavor == CursorRing::NORMAL_ANIMATING ? "NORMAL_ANIMATING" :
            flavor == CursorRing::FAKE_ANIMATING ? "FAKE_ANIMATING" : "NORMAL_FLAVOR",
            rings->size(), ring.x(), ring.y(), ring.width(), ring.height(),
            node->isPlugin() ? "true" : "false");
#endif
    }
    if (isButton || flavor >= CursorRing::NORMAL_ANIMATING) {
        SkMSec time = SkTime::GetMSecs();
        if (time < m_ringAnimationEnd) {
            // views assume that inval bounds coordinates are non-negative
            bounds.intersect(WebCore::IntRect(0, 0, INT_MAX, INT_MAX));
            postInvalidateDelayed(m_ringAnimationEnd - time, bounds);
        } else {
            if (m_followedLink)
                hideCursor();
            m_followedLink = false;
            flavor = static_cast<CursorRing::Flavor>
                    (flavor - CursorRing::NORMAL_ANIMATING);
        }
    }
    if (!isButton)
        CursorRing::DrawRing(canvas, *rings, flavor);
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
        const CachedNode* cachedNode = root->currentCursor();
        if (cachedNode) {
            cachedNode->cursorRingBounds(bounds);
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
    IntRect newBounds = node->bounds();
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
    bool hadCursor = m_frameCacheUI && m_frameCacheUI->currentCursor();
    const CachedNode* oldFocus = m_frameCacheUI ? m_frameCacheUI->currentFocus() : 0;
    m_viewImpl->gFrameCacheMutex.lock();
    delete m_frameCacheUI;
    delete m_navPictureUI;
    m_viewImpl->m_updatedFrameCache = false;
    m_frameCacheUI = m_viewImpl->m_frameCacheKit;
    m_navPictureUI = m_viewImpl->m_navPictureKit;
    m_viewImpl->m_frameCacheKit = 0;
    m_viewImpl->m_navPictureKit = 0;
    m_viewImpl->gFrameCacheMutex.unlock();
    fixCursor();
    if (oldFocus && m_frameCacheUI) {
        const CachedNode* newFocus = m_frameCacheUI->currentFocus();
        if (newFocus && oldFocus->nodePointer() != newFocus->nodePointer()
                && oldFocus->isTextInput() && newFocus->isTextInput()
                && newFocus != m_frameCacheUI->currentCursor()) {
            // The focus has changed.  We may need to update things.
            LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
            JNIEnv* env = JSC::Bindings::getJNIEnv();
            env->CallVoidMethod(m_javaGlue.object(env).get(),
                    m_javaGlue.m_domChangedFocus);
            checkException(env);
        }
    }
    if (hadCursor && (!m_frameCacheUI || !m_frameCacheUI->currentCursor()))
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

static CachedFrame::Direction KeyToDirection(KeyCode keyCode)
{
    switch (keyCode) {
        case kKeyCodeDpadRight:
            DBG_NAV_LOGD("keyCode=%s", "right");
            return CachedFrame::RIGHT;
        case kKeyCodeDpadLeft:
            DBG_NAV_LOGD("keyCode=%s", "left");
            return CachedFrame::LEFT;
        case kKeyCodeDpadDown:
            DBG_NAV_LOGD("keyCode=%s", "down");
            return CachedFrame::DOWN;
        case kKeyCodeDpadUp:
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

// This needs to be called each time we call CachedRoot::setCursor() with
// non-null CachedNode/CachedFrame, since otherwise the WebViewCore's data
// about the cursor is incorrect.  When we call setCursor(0,0), we need
// to set m_viewImpl->hasCursorBounds to false.
void updateCursorBounds(const CachedRoot* root, const CachedFrame* cachedFrame,
        const CachedNode* cachedNode)
{
    LOG_ASSERT(root, "updateCursorBounds: root cannot be null");
    LOG_ASSERT(cachedNode, "updateCursorBounds: cachedNode cannot be null");
    LOG_ASSERT(cachedFrame, "updateCursorBounds: cachedFrame cannot be null");
    m_viewImpl->gCursorBoundsMutex.lock();
    m_viewImpl->m_hasCursorBounds = !cachedNode->isHidden();
    // If m_viewImpl->m_hasCursorBounds is false, we never look at the other
    // values, so do not bother setting them.
    if (m_viewImpl->m_hasCursorBounds) {
        WebCore::IntRect bounds = cachedNode->bounds();
        if (m_viewImpl->m_cursorBounds != bounds)
            DBG_NAV_LOGD("new cursor bounds=(%d,%d,w=%d,h=%d)",
                bounds.x(), bounds.y(), bounds.width(), bounds.height());
        m_viewImpl->m_cursorBounds = cachedNode->bounds();
        m_viewImpl->m_cursorHitBounds = cachedNode->hitBounds();
        m_viewImpl->m_cursorFrame = cachedFrame->framePointer();
        root->getSimulatedMousePosition(&m_viewImpl->m_cursorLocation);
        m_viewImpl->m_cursorNode = cachedNode->nodePointer();
    }
    m_viewImpl->gCursorBoundsMutex.unlock();
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
    CachedFrame::Direction direction = KeyToDirection((KeyCode) keyCode);
    const CachedFrame* cachedFrame, * oldFrame = 0;
    const CachedNode* cursor = root->currentCursor(&oldFrame);
    WebCore::IntPoint cursorLocation = root->cursorLocation();
    DBG_NAV_LOGD("old cursor %d (nativeNode=%p) cursorLocation={%d, %d}",
        cursor ? cursor->index() : 0,
        cursor ? cursor->nodePointer() : 0, cursorLocation.x(), cursorLocation.y());
    WebCore::IntRect visibleRect;
    getVisibleRect(&visibleRect);
    DBG_NAV_LOGD("getVisibleRect %d,%d,%d,%d",
        visibleRect.x(), visibleRect.y(), visibleRect.width(), visibleRect.height());
    root->setVisibleRect(visibleRect);
    int xMax = getScaledMaxXScroll();
    int yMax = getScaledMaxYScroll();
    root->setMaxScroll(xMax, yMax);
    const CachedNode* cachedNode = 0;
    int dx = 0;
    int dy = 0;
    int counter = count;
    if (!cursor || !m_followedLink)
        root->setScrollOnly(m_followedLink);
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
            cachedNode ? cachedNode->bounds().x() : 0,
            cachedNode ? cachedNode->bounds().y() : 0,
            cachedNode ? cachedNode->bounds().width() : 0,
            cachedNode ? cachedNode->bounds().height() : 0);
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
        updateCursorBounds(root, cachedFrame, cachedNode);
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
    WebCore::IntRect visibleRect;
    getVisibleRect(&visibleRect);
    root->setVisibleRect(visibleRect);
    return root->findAt(rect, framePtr, rxPtr, ryPtr, true);
}

void selectBestAt(const WebCore::IntRect& rect)
{
    const CachedFrame* frame;
    int rx, ry;
    bool disableFocusController = false;
    CachedRoot* root = getFrameCache(DontAllowNewer);
    const CachedNode* node = findAt(root, rect, &frame, &rx, &ry);

    if (!node) {
        DBG_NAV_LOGD("no nodes found root=%p", root);
        disableFocusController = true;
        m_viewImpl->m_hasCursorBounds = false;
        if (root)
            root->setCursor(0, 0);
    } else {
        DBG_NAV_LOGD("CachedNode:%p (%d)", node, node->index());
        root->rootHistory()->setMouseBounds(node->bounds());
        updateCursorBounds(root, frame, node);
        root->setCursor(const_cast<CachedFrame*>(frame),
                const_cast<CachedNode*>(node));
        if (!node->wantsKeyEvents()) {
            disableFocusController = true;
        }
    }
    sendMoveMouseIfLatest(disableFocusController);
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
    m_followedLink = false;
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
    updateCursorBounds(root, frame, result);
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
    m_viewImpl->m_findIsUp = up;
    if (!up)
        m_hasCurrentLocation = false;
}

void setFollowedLink(bool followed)
{
    if ((m_followedLink = followed) != false) {
        m_ringAnimationEnd = SkTime::GetMSecs() + 500;
        viewInvalidate();
    }
}

void setHeightCanMeasure(bool measure)
{
    m_heightCanMeasure = measure;
}

SkIRect m_selStart, m_selEnd;
SkRegion m_selRegion;
#define MIN_ARROW_DISTANCE (20 * 20)

void moveSelection(int x, int y, bool extendSelection)
{
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root)
        return;
    const SkPicture& picture = *m_navPictureUI;
    WebCore::IntRect r;
    getVisibleRect(&r);
    SkIRect area;
    area.set(r.x(), r.y(), r.right(), r.bottom());
    m_selEnd = CopyPaste::findClosest(picture, area, x, y);
    if (!extendSelection)
        m_selStart = m_selEnd;
    DBG_NAV_LOGD("x=%d y=%d extendSelection=%s m_selStart=(%d, %d, %d, %d)"
        " m_selEnd=(%d, %d, %d, %d)", x, y, extendSelection ? "true" : "false",
        m_selStart.fLeft, m_selStart.fTop, m_selStart.fRight, m_selStart.fBottom,
        m_selEnd.fLeft, m_selEnd.fTop, m_selEnd.fRight, m_selEnd.fBottom);
}

const String getSelection()
{
    WebCore::IntRect r;
    getVisibleRect(&r);
    SkIRect area;
    area.set(r.x(), r.y(), r.right(), r.bottom());
    String result = CopyPaste::text(*m_navPictureUI, area, m_selRegion);
    DBG_NAV_LOGD("text=%s", result.latin1().data());
    return result;
}

void drawSelectionRegion(SkCanvas* canvas)
{
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root)
        return;
    WebCore::IntRect r;
    getVisibleRect(&r);
    SkIRect area;
    area.set(r.x(), r.y(), r.right(), r.bottom());
    m_selRegion.setEmpty();
    CopyPaste::buildSelection(*m_navPictureUI, area, m_selStart, m_selEnd, &m_selRegion);
    SkPath path;
    m_selRegion.getBoundaryPath(&path);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SkColorSetARGB(0x40, 255, 51, 204));
    canvas->drawPath(path, paint);
}

void drawSelectionPointer(SkCanvas* canvas, float scale, int x, int y, bool ex)
{
    SkPath path;
    if (ex)
        getSelectionCaret(&path);
    else
        getSelectionArrow(&path);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(SK_ColorBLACK);
    SkPixelXorXfermode xorMode(SK_ColorWHITE);
    if (ex)
        paint.setXfermode(&xorMode);
    else
        paint.setStrokeWidth(SK_Scalar1 * 2);
    int sc = canvas->save();
    canvas->scale(scale, scale);
    canvas->translate(SkIntToScalar(x), SkIntToScalar(y));
    canvas->drawPath(path, paint);
    if (!ex) {
        paint.setStyle(SkPaint::kFill_Style);
        paint.setColor(SK_ColorWHITE);
        canvas->drawPath(path, paint);
    }
    canvas->restoreToCount(sc);
}

void getSelectionArrow(SkPath* path)
{
    const int arrow[] = {
        0, 14, 3, 11, 5, 15, 9, 15, 7, 11, 11, 11
    };
    for (unsigned index = 0; index < sizeof(arrow)/sizeof(arrow[0]); index += 2)
        path->lineTo(SkIntToScalar(arrow[index]), SkIntToScalar(arrow[index + 1]));
    path->close();
}

void getSelectionCaret(SkPath* path)
{
    SkScalar height = SkIntToScalar(m_selStart.fBottom - m_selStart.fTop);
    SkScalar dist = height / 4;
    path->moveTo(0, -height / 2);
    path->rLineTo(0, height);
    path->rLineTo(-dist, dist);
    path->rMoveTo(0, -SK_Scalar1/2);
    path->rLineTo(dist * 2, 0);
    path->rMoveTo(0, SK_Scalar1/2);
    path->rLineTo(-dist, -dist);
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

// This function is only used by findNext and setMatches.  In it, we store
// upper left corner of the match specified by m_findIndex in
// m_currentMatchLocation.
void inline storeCurrentMatchLocation()
{
    SkASSERT(m_findIndex < m_matches->size());
    const SkIRect& bounds = (*m_matches)[m_findIndex].getLocation().getBounds();
    m_currentMatchLocation.set(bounds.fLeft, bounds.fTop);
    m_hasCurrentLocation = true;
}

void findNext(bool forward)
{
    if (!m_matches || !m_matches->size())
        return;
    if (forward) {
        m_findIndex++;
        if (m_findIndex == m_matches->size())
            m_findIndex = 0;
    } else {
        if (m_findIndex == 0) {
            m_findIndex = m_matches->size() - 1;
        } else {
            m_findIndex--;
        }
    }
    storeCurrentMatchLocation();
    viewInvalidate();
}

// With this call, WebView takes ownership of matches, and is responsible for
// deleting it.
void setMatches(WTF::Vector<MatchInfo>* matches)
{
    if (m_matches)
        delete m_matches;
    m_matches = matches;
    if (m_matches->size()) {
        if (m_hasCurrentLocation) {
            for (unsigned i = 0; i < m_matches->size(); i++) {
                const SkIRect& rect = (*m_matches)[i].getLocation().getBounds();
                if (rect.fLeft == m_currentMatchLocation.fX
                        && rect.fTop == m_currentMatchLocation.fY) {
                    m_findIndex = i;
                    viewInvalidate();
                    return;
                }
            }
        }
        // If we did not have a stored location, or if we were unable to restore
        // it, store the new one.
        m_findIndex = 0;
        storeCurrentMatchLocation();
    } else {
        m_hasCurrentLocation = false;
    }
    viewInvalidate();
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

private: // local state for WebView
    // private to getFrameCache(); other functions operate in a different thread
    CachedRoot* m_frameCacheUI; // navigation data ready for use
    WebViewCore* m_viewImpl;
    int m_generation; // associate unique ID with sent kit focus to match with ui
    SkPicture* m_navPictureUI;
    bool m_followedLink;
    SkMSec m_ringAnimationEnd;
    // Corresponds to the same-named boolean on the java side.
    bool m_heightCanMeasure;
    int m_lastDx;
    SkMSec m_lastDxTime;
    WTF::Vector<MatchInfo>* m_matches;
    // Stores the location of the current match.
    SkIPoint m_currentMatchLocation;
    // Tells whether the value in m_currentMatchLocation is valid.
    bool m_hasCurrentLocation;
    // Tells whether we have done the setup to draw the Find matches.
    bool m_isFindPaintSetUp;
    // Paint used to draw our Find matches.
    SkPaint m_findPaint;
    // Paint used for the background of our Find matches.
    SkPaint m_findBlurPaint;
    unsigned m_findIndex;
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

static const CachedNode* getFocusCandidate(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return 0;
    const CachedNode* cursor = root->currentCursor();
    if (cursor && cursor->wantsKeyEvents())
        return cursor;
    return root->currentFocus();
}

static const CachedNode* getFocusNode(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    return root ? root->currentFocus() : 0;
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
    const CachedNode* node = getCursorNode(env, obj);
    WebCore::IntRect bounds = node ? node->getBounds()
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
    const CachedNode* node = getCursorNode(env, obj);
    return node ? node->getBounds().intersects(jrect_to_webrect(env, visRect))
        : false;
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

static void nativeDrawMatches(JNIEnv *env, jobject obj, jobject canv)
{
    SkCanvas* canvas = GraphicsJNI::getNativeCanvas(env, canv);
    if (!canv) {
        DBG_NAV_LOG("!canv");
        return;
    }
    WebView* view = GET_NATIVE_VIEW(env, obj);
    if (!view) {
        DBG_NAV_LOG("!view");
        return;
    }
    view->drawMatches(canvas);
}

static void nativeDrawLayers(JNIEnv *env, jobject obj,
    jint layer, jint scrollX, jint scrollY,
    jint width, jint height,
    jfloat scale, jobject canv)
{
    if (!env)
        return;
    if (!layer)
        return;
    if (!canv)
        return;

#if USE(ACCELERATED_COMPOSITING)
    LayerAndroid* layerImpl = reinterpret_cast<LayerAndroid*>(layer);
    SkCanvas* canvas = GraphicsJNI::getNativeCanvas(env, canv);
    if (canvas)
        layerImpl->paintOn(scrollX, scrollY, width, height, scale, canvas);
#endif
}

static void nativeUpdateLayers(JNIEnv *env, jobject obj,
                               jint layer, jint updates)
{
    if (!env)
        return;
    if (!layer)
        return;
    if (!updates)
        return;

#if USE(ACCELERATED_COMPOSITING)
    Vector<RefPtr<AndroidAnimationValue> >* updatesImpl =
        reinterpret_cast<Vector<RefPtr<AndroidAnimationValue> >* >(updates);
    if (updatesImpl) {
        for (unsigned int i = 0; i < updatesImpl->size(); i++)
            (updatesImpl->at(i))->apply();
        delete updatesImpl;
    }
#endif
}

static bool nativeLayersHaveAnimations(JNIEnv *env, jobject obj, jint layer)
{
    if (!env)
        return false;
    if (!layer)
        return false;
#if USE(ACCELERATED_COMPOSITING)
    LayerAndroid* layerImpl = reinterpret_cast<LayerAndroid*>(layer);
    return layerImpl->hasAnimations();
#else
    return false;
#endif
}

static int nativeEvaluateLayersAnimations(JNIEnv *env, jobject obj, jint layer)
{
    if (!env)
        return 0;
    if (!layer)
        return 0;
#if USE(ACCELERATED_COMPOSITING)
    LayerAndroid* layerImpl = reinterpret_cast<LayerAndroid*>(layer);
    return reinterpret_cast<int>(layerImpl->evaluateAnimations());
#else
    return 0;
#endif
}

static void nativeDestroyLayer(JNIEnv *env, jobject obj, jint layer)
{
    if (!env)
        return;
    if (!layer)
        return;
#if USE(ACCELERATED_COMPOSITING)
    LayerAndroid* layerImpl = reinterpret_cast<LayerAndroid*>(layer);
    delete layerImpl;
#endif
}

static void nativeDrawCursorRing(JNIEnv *env, jobject obj, jobject canv)
{
    SkCanvas* canvas = GraphicsJNI::getNativeCanvas(env, canv);
    if (!canv) {
        DBG_NAV_LOG("!canv");
        return;
    }
    WebView* view = GET_NATIVE_VIEW(env, obj);
    if (!view) {
        DBG_NAV_LOG("!view");
        return;
    }
    view->drawCursorRing(canvas);
}

static void nativeDrawSelectionPointer(JNIEnv *env, jobject obj,
    jobject canv, jfloat scale, jint x, jint y, bool ex)
{
    SkCanvas* canvas = GraphicsJNI::getNativeCanvas(env, canv);
    if (!canv) {
        DBG_NAV_LOG("!canv");
        return;
    }
    WebView* view = GET_NATIVE_VIEW(env, obj);
    if (!view) {
        DBG_NAV_LOG("!view");
        return;
    }
    view->drawSelectionPointer(canvas, scale, x, y, ex);
}

static void nativeDrawSelectionRegion(JNIEnv *env, jobject obj, jobject canv)
{
    SkCanvas* canvas = GraphicsJNI::getNativeCanvas(env, canv);
    if (!canv) {
        DBG_NAV_LOG("!canv");
        return;
    }
    WebView* view = GET_NATIVE_VIEW(env, obj);
    if (!view) {
        DBG_NAV_LOG("!view");
        return;
    }
    view->drawSelectionRegion(canvas);
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
    const CachedNode* node = getFocusCandidate(env, obj);
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

static jobject nativeFocusCandidateNodeBounds(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getFocusCandidate(env, obj);
    WebCore::IntRect bounds = node ? node->getBounds()
        : WebCore::IntRect(0, 0, 0, 0);
    jclass rectClass = env->FindClass("android/graphics/Rect");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    jobject rect = env->NewObject(rectClass, init, bounds.x(),
        bounds.y(), bounds.right(), bounds.bottom());
    return rect;
}

static jint nativeFocusCandidatePointer(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getFocusCandidate(env, obj);
    return reinterpret_cast<int>(node ? node->nodePointer() : 0);
}

static jobject nativeFocusCandidateText(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getFocusCandidate(env, obj);
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

static void nativeSetFindIsUp(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->setFindIsUp(false);
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
    view->setFindIsUp(true);
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
    canvas.drawPicture(*(root->getPicture()));
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

static void nativeMoveCursorToNextTextInput(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return;
    const CachedFrame* containingFrame;
    const CachedNode* current = root->currentCursor(&containingFrame);
    if (!current)
        current = root->currentFocus(&containingFrame);
    if (!current)
        return;
    const CachedFrame* frame;
    const CachedNode* next = containingFrame->nextTextField(current, &frame,
            true);
    if (!next)
        return;
    const WebCore::IntRect& bounds = next->bounds();
    root->rootHistory()->setMouseBounds(bounds);
    view->updateCursorBounds(root, frame, next);
    root->setCursor(const_cast<CachedFrame*>(frame),
            const_cast<CachedNode*>(next));
    view->sendMoveFocus(static_cast<WebCore::Frame*>(frame->framePointer()),
            static_cast<WebCore::Node*>(next->nodePointer()));
    view->scrollRectOnScreen(bounds.x(), bounds.y(), bounds.right(),
            bounds.bottom());
    view->getWebViewCore()->m_moveGeneration++;
}

static jint nativeTextFieldAction(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return static_cast<jint>(CachedRoot::FAILURE);
    return static_cast<jint>(root->currentTextFieldAction());
}

static int nativeMoveGeneration(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    if (!view)
        return 0;
    return view->moveGeneration();
}

static void nativeMoveSelection(JNIEnv *env, jobject obj, int x, int y, bool ex)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->moveSelection(x, y, ex);
}

static jobject nativeGetSelection(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    String selection = view->getSelection();
    return env->NewString((jchar*)selection.characters(), selection.length());
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
            // we're done with the file now
            fwrite("\n", 1, 1, file);
            fclose(file);
        }
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
    { "nativeDrawCursorRing", "(Landroid/graphics/Canvas;)V",
        (void*) nativeDrawCursorRing },
    { "nativeDestroyLayer", "(I)V",
        (void*) nativeDestroyLayer },
    { "nativeLayersHaveAnimations", "(I)Z",
        (void*) nativeLayersHaveAnimations },
    { "nativeEvaluateLayersAnimations", "(I)I",
        (void*) nativeEvaluateLayersAnimations },
    { "nativeDrawLayers", "(IIIIIFLandroid/graphics/Canvas;)V",
        (void*) nativeDrawLayers },
    { "nativeUpdateLayers", "(II)V",
        (void*) nativeUpdateLayers },
    { "nativeDrawMatches", "(Landroid/graphics/Canvas;)V",
        (void*) nativeDrawMatches },
    { "nativeDrawSelectionPointer", "(Landroid/graphics/Canvas;FIIZ)V",
        (void*) nativeDrawSelectionPointer },
    { "nativeDrawSelectionRegion", "(Landroid/graphics/Canvas;)V",
        (void*) nativeDrawSelectionRegion },
    { "nativeDumpDisplayTree", "(Ljava/lang/String;)V",
        (void*) nativeDumpDisplayTree },
    { "nativeFindAll", "(Ljava/lang/String;Ljava/lang/String;)I",
        (void*) nativeFindAll },
    { "nativeFindNext", "(Z)V",
        (void*) nativeFindNext },
    { "nativeFocusCandidateFramePointer", "()I",
        (void*) nativeFocusCandidateFramePointer },
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
    { "nativeImageURI", "(II)Ljava/lang/String;",
        (void*) nativeImageURI },
    { "nativeInstrumentReport", "()V",
        (void*) nativeInstrumentReport },
    { "nativeMotionUp", "(III)Z",
        (void*) nativeMotionUp },
    { "nativeMoveCursor", "(IIZ)Z",
        (void*) nativeMoveCursor },
    { "nativeMoveCursorToNextTextInput", "()V",
        (void*) nativeMoveCursorToNextTextInput },
    { "nativeMoveGeneration", "()I",
        (void*) nativeMoveGeneration },
    { "nativeMoveSelection", "(IIZ)V",
        (void*) nativeMoveSelection },
    { "nativePointInNavCache", "(III)Z",
        (void*) nativePointInNavCache },
    { "nativeRecordButtons", "(ZZZ)V",
        (void*) nativeRecordButtons },
    { "nativeSelectBestAt", "(Landroid/graphics/Rect;)V",
        (void*) nativeSelectBestAt },
    { "nativeSetFindIsUp", "()V",
        (void*) nativeSetFindIsUp },
    { "nativeSetFollowedLink", "(Z)V",
        (void*) nativeSetFollowedLink },
    { "nativeSetHeightCanMeasure", "(Z)V",
        (void*) nativeSetHeightCanMeasure },
    { "nativeTextFieldAction", "()I",
        (void*) nativeTextFieldAction },
    { "nativeTextGeneration", "()I",
        (void*) nativeTextGeneration },
    { "nativeUpdateCachedTextfield", "(Ljava/lang/String;I)V",
        (void*) nativeUpdateCachedTextfield },
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
