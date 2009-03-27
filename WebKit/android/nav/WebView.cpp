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

#define LOG_TAG "webviewglue"

#include <config.h>

#include "android_graphics.h"
#include "AndroidLog.h"
#include "AtomicString.h"
#include "CachedFrame.h"
#include "CachedNode.h"
#include "CachedRoot.h"
#include "FindCanvas.h"
#include "Frame.h"
#include "GraphicsJNI.h"
#include "IntPoint.h"
#include "IntRect.h"
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
#include "WebCoreJni.h"
#include "WebViewCore.h"
#include "jni_utility.h"

#ifdef ANDROID_INSTRUMENT
#include "TimeCounter.h"
#endif

#ifdef GET_NATIVE_VIEW
#undef GET_NATIVE_VIEW
#endif

#define GET_NATIVE_VIEW(env, obj) ((WebView*)env->GetIntField(obj, gWebViewField))

#include <ui/KeycodeLabels.h>
#include <JNIHelp.h>
#include <jni.h>

#define REPLAY_BUFFER_SIZE 4096

namespace android {

struct CommonParams {
    enum Trigger {
        NoData,
        ClearFocusParams,
        FirstMoveFocusParams,
        MoveFocusParams,
        MotionUpParams
    }  m_trigger;
    int m_generation;
};

struct CacheParams {
    void setFocus(const CachedNode* node,
        const CachedFrame* frame, const CachedRoot* root,
            const WebCore::IntPoint& focusLocation)
    {
        m_node = (WebCore::Node*) (node ? node->nodePointer() : 0);
        m_frame = (WebCore::Frame*) (node ? frame->framePointer() : 0);
        m_x = focusLocation.x();
        m_y = focusLocation.y();
    }

    WebCore::Node* m_node;
    WebCore::Frame* m_frame;
    int m_x;
    int m_y;
};

struct ClearFocusParams {
    CommonParams d;
    CacheParams c;
    int m_x;
    int m_y;
};

struct MotionUpParams {
    CommonParams d;
    int m_x;
    int m_y;
    int m_slop;
    bool m_isClick;
};

struct FirstMoveFocusParams {
    CommonParams d;
    int m_keyCode;
    int m_count;
    bool m_ignoreScroll;
};

struct MoveFocusParams {
    FirstMoveFocusParams d;
    CacheParams c;
    void* m_sentFocus;
    WebCore::IntRect m_sentBounds;
    WebCore::IntRect m_visibleRect;
    CachedHistory m_history; // FIXME: make this a subset
    int m_xMax;
    int m_yMax;
};

typedef MoveFocusParams LargestParams;

#if DEBUG_NAV_UI
static const char* TriggerNames[] = {
    "*** no data ! ***",
    "clearFocus",
    "firstMoveFocus",
    "moveFocus",
    "motionUp"
};
#endif

class FocusReplay {
public:
FocusReplay() : m_start(m_buffer), m_end(m_buffer), m_lastGeneration(0)
{
}

// find the most recent common data
void add(const CommonParams& data, size_t len)
{
    DBG_NAV_LOGD("m_start=%d m_end=%d trigger=%s moveGeneration=%d", m_start - m_buffer,
        m_end - m_buffer, TriggerNames[data.m_trigger], data.m_generation);
    m_lastGeneration = data.m_generation;
    char* limit = m_buffer + sizeof(m_buffer);
    int used = m_end - m_start;
    if (used < 0)
        used += sizeof(m_buffer);
    int needed = (int) len - ((int) sizeof(m_buffer) - used);
    if (needed >= 0)
        reclaim(++needed);
    if (m_end + len <= limit) {
        memcpy(m_end, (void*) &data, len);
        m_end += len;
        DBG_NAV_LOGD("m_start=%d m_end=%d", m_start - m_buffer, m_end - m_buffer);
        return;
    }
    size_t partial = limit - m_end;
    memcpy(m_end, (void*) &data, partial);
    const void* remainder = (const void*) ((const char*) &data + partial);
    partial = len - partial;
    memcpy(m_buffer, remainder, partial);
    m_end = m_buffer + partial;
    DBG_NAV_LOGD("wrap m_start=%d m_end=%d",
        m_start - m_buffer, m_end - m_buffer);
}

int count()
{
    DBG_NAV_LOGD("m_start=%d m_end=%d",
        m_start - m_buffer, m_end - m_buffer);
    if (m_start == m_end)
        return 0;
    char* limit = m_buffer + sizeof(m_buffer);
    char* saveStart = m_start;
    int result = 0;
    while (true) {
        ++result;
        m_start += triggerSize();
        if (m_start == m_end)
            break;
        if (m_start < limit)
            continue;
        m_start -= sizeof(m_buffer);
        if (m_start == m_end)
            break;
    }
    m_start = saveStart;
    DBG_NAV_LOGD("count=%d", result);
    return result;
}

void discard(int generation)
{
    DBG_NAV_LOGD("generation=%d", generation);
    LargestParams storage;
    const CommonParams& params = storage.d.d;
    char* pos = position();
    retrieve(&storage.d.d);
    if (params.m_generation > generation) {
        DBG_NAV_LOGD("params.m_generation=%d > generation=%d",
            params.m_generation, generation);
        rewind(pos);
        DBG_NAV_LOGD("m_start=%d m_end=%d", m_start - m_buffer, m_end - m_buffer);
        return;
    }
    LOG_ASSERT(params.m_generation == generation, "params.m_generation != generation");
    DBG_NAV_LOGD("m_start=%d m_end=%d", m_start - m_buffer, m_end - m_buffer);
}

int lastAdd()
{
    return m_lastGeneration;
}

char* position()
{
    return m_start;
}

int retrieve(CommonParams* data)
{
    if (m_end == m_start) {
        // changed from LOGD to LOGV, as it always fires when I click to center
        // text (mrr)
        LOGV("%s *** no data to retrieve (error condition) ***", __FUNCTION__);
        data->m_trigger = CommonParams::NoData;
        return data->m_generation = INT_MAX;
    }
    DBG_NAV_LOGD("m_start=%d m_end=%d",
        m_start - m_buffer, m_end - m_buffer);
    char* limit = m_buffer + sizeof(m_buffer);
    size_t size = triggerSize();
    if (m_start < m_end) {
        LOG_ASSERT((size_t) (m_end - m_start) >= size, "m_end - m_start < size");
        memcpy(data, m_start, size);
        m_start += size;
    } else {
        int partial = limit - m_start;
        if (partial > (int) size)
            partial = size;
        memcpy(data, m_start, partial);
        m_start += partial;
        void* remainder = (void*) ((char*) data + partial);
        partial = size - partial;
        if (partial > 0) {
            memcpy(remainder, m_buffer, partial);
            m_start = m_buffer + partial;
            LOG_ASSERT(m_start <= m_end, "m_start > m_end");
        }
    }
    if (m_start == limit) {
        m_start = m_buffer;
        if (m_end == limit)
            m_end = m_buffer;
    }
    DBG_NAV_LOGD("m_start=%d m_end=%d trigger=%s moveGeneration=%d",
        m_start - m_buffer, m_end - m_buffer, TriggerNames[data->m_trigger],
        data->m_generation);
    return data->m_generation;
}

void rewind(char* pos)
{
    m_start = pos;
}

private:
void reclaim(int needed)
{
    DBG_NAV_LOGD("needed=%d", needed);
    char* limit = m_buffer + sizeof(m_buffer);
    do {
        size_t size = triggerSize();
        m_start += size;
        needed -= size;
        if (m_start >= limit) {
            m_start = m_buffer + (m_start - limit);
            if (m_end == limit)
                m_end = m_buffer;
        }
    } while (needed > 0 && m_start != m_end);
    DBG_NAV_LOGD("m_start=%d m_end=%d",
        m_start - m_buffer, m_end - m_buffer);
}

size_t triggerSize()
{
    LOG_ASSERT(m_start != m_end, "m_start == m_end");
    char* limit = m_buffer + sizeof(m_buffer);
    LOG_ASSERT(m_start + sizeof(CommonParams::Trigger) <= limit, "trigger not in limit");
    CommonParams::Trigger trigger;
    memcpy(&trigger, m_start, sizeof(trigger));
    switch (trigger) {
        case CommonParams::ClearFocusParams:
            return sizeof(ClearFocusParams);
        case CommonParams::FirstMoveFocusParams:
            return sizeof(FirstMoveFocusParams);
        case CommonParams::MoveFocusParams:
            return sizeof(MoveFocusParams);
        case CommonParams::MotionUpParams:
            return sizeof(MotionUpParams);
        default:
            LOG_ASSERT(0, "trigger undefined");
    }
    return 0;
}

char m_buffer[REPLAY_BUFFER_SIZE];
char* m_start;
char* m_end;
int m_lastGeneration;
}; // end of helper class ReplayFocus

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

enum OutOfFocusFix {
    DoNothing,
    ClearTextEntry,
    UpdateTextEntry
};

struct JavaGlue {
    jobject     m_obj;
    jmethodID   m_clearTextEntry;
    jmethodID   m_overrideLoading;
    jmethodID   m_scrollBy;
    jmethodID   m_sendFinalFocus;
    jmethodID   m_sendKitFocus;
    jmethodID   m_sendMotionUp;
    jmethodID   m_setFocusData;
    jmethodID   m_getScaledMaxXScroll;
    jmethodID   m_getScaledMaxYScroll;
    jmethodID   m_getVisibleRect;
    jmethodID   m_updateTextEntry;
    jmethodID   m_displaySoftKeyboard;
    jmethodID   m_viewInvalidate;
    jmethodID   m_viewInvalidateRect;
    jmethodID   m_postInvalidateDelayed;
    jfieldID    m_rectLeft;
    jfieldID    m_rectTop;
    jmethodID   m_rectWidth;
    jmethodID   m_rectHeight;
    jfieldID    m_focusNode;
    jmethodID   m_setAll;
    AutoJObject object(JNIEnv* env) {
        return getRealObject(env, m_obj);
    }
} m_javaGlue;

WebView(JNIEnv* env, jobject javaWebView, int viewImpl)
{
    jclass clazz = env->FindClass("android/webkit/WebView");
 //   m_javaGlue = new JavaGlue;
    m_javaGlue.m_obj = adoptGlobalRef(env, javaWebView);
    m_javaGlue.m_scrollBy = GetJMethod(env, clazz, "setContentScrollBy", "(IIZ)V");
    m_javaGlue.m_clearTextEntry = GetJMethod(env, clazz, "clearTextEntry", "()V");
    m_javaGlue.m_overrideLoading = GetJMethod(env, clazz, "overrideLoading", "(Ljava/lang/String;)V");
    m_javaGlue.m_sendFinalFocus = GetJMethod(env, clazz, "sendFinalFocus", "(IIII)V");
    m_javaGlue.m_sendKitFocus = GetJMethod(env, clazz, "sendKitFocus", "()V");
    m_javaGlue.m_sendMotionUp = GetJMethod(env, clazz, "sendMotionUp", "(IIIIIIIZZ)V");
    m_javaGlue.m_setFocusData = GetJMethod(env, clazz, "setFocusData", "(IIIIIIZ)V");
    m_javaGlue.m_getScaledMaxXScroll = GetJMethod(env, clazz, "getScaledMaxXScroll", "()I");
    m_javaGlue.m_getScaledMaxYScroll = GetJMethod(env, clazz, "getScaledMaxYScroll", "()I");
    m_javaGlue.m_getVisibleRect = GetJMethod(env, clazz, "sendOurVisibleRect", "()Landroid/graphics/Rect;");
    m_javaGlue.m_updateTextEntry = GetJMethod(env, clazz, "updateTextEntry", "()V");
    m_javaGlue.m_displaySoftKeyboard = GetJMethod(env, clazz, "displaySoftKeyboard", "()V");
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

    // Set up class for updateFocusNode
    jclass focusnodeClass = env->FindClass("android/webkit/WebView$FocusNode");
    LOG_ASSERT(focusnodeClass, "Could not find FocusNode class!");
    m_javaGlue.m_focusNode = env->GetFieldID(clazz, "mFocusNode", "Landroid/webkit/WebView$FocusNode;");
    m_javaGlue.m_setAll = GetJMethod(env, focusnodeClass, "setAll", "(ZZZZZIIIIIIIILjava/lang/String;Ljava/lang/String;I)V");
    env->DeleteLocalRef(focusnodeClass);

    env->SetIntField(javaWebView, gWebViewField, (jint)this);
    m_viewImpl = (WebViewCore*) viewImpl;
    m_frameCacheUI = 0;
    m_navPictureUI = 0;
    m_invalidNode = 0;
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
        env->DeleteGlobalRef(m_javaGlue.m_obj);
        m_javaGlue.m_obj = 0;
    }
    delete m_frameCacheUI;
    delete m_navPictureUI;
    if (m_matches)
        delete m_matches;
}

void clearFocus(int x, int y, bool inval)
{
    DBG_NAV_LOGD("x=%d y=%d inval=%s", x, y,
        inval ? "true" : "false");
    clearTextEntry();
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root)
        return;
    const CachedFrame* oldFrame = 0;
    const CachedNode* oldFocusNode = root->currentFocus(&oldFrame);
    WebCore::IntPoint focusLocation = WebCore::IntPoint(0, 0);
    setFocusData(root->generation(), 0, 0, x, y, !oldFocusNode);
    sendKitFocus();
    if (oldFocusNode) {
        DBG_NAV_LOG("oldFocusNode");
        focusLocation = root->focusLocation();
        root->setCachedFocus(0, 0);
        if (inval)
            viewInvalidate();
    }
    ClearFocusParams params;
    params.d.m_trigger = CommonParams::ClearFocusParams;
    params.d.m_generation = m_generation;
    params.c.setFocus(oldFocusNode, oldFrame, root, focusLocation);
    params.m_x = x;
    params.m_y = y;
    m_replay.add(params.d, sizeof(params));
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
// their subpictures according to their current focus state.
// Called from the UI thread.  This is the one place in the UI thread where we
// access the buttons stored in the WebCore thread.
// hasFocus keeps track of whether the WebView has focus && windowFocus.
// If not, we do not want to draw the button in a focused or pressed state
void nativeRecordButtons(bool hasFocus, bool pressed, bool invalidate)
{
    bool focusIsButton = false;
    const CachedNode* cachedFocus = 0;
    // Lock the mutex, since we now share with the WebCore thread.
    m_viewImpl->gButtonMutex.lock();
    if (m_viewImpl->m_buttons.size()) {
        // Find the focused node so we can determine which node has focus, and
        // therefore which state to paint them in.
        // FIXME: In a future change, we should keep track of whether the focus
        // has changed to short circuit (note that we would still need to update
        // if we received new buttons from the WebCore thread).
        WebCore::Node* focus = 0;
        CachedRoot* root = getFrameCache(DontAllowNewer);
        if (root) {
            cachedFocus = root->currentFocus();
            if (cachedFocus)
                focus = (WebCore::Node*) cachedFocus->nodePointer();
        }

        // Traverse the array, and update each button, depending on whether it
        // is focused.
        Container* end = m_viewImpl->m_buttons.end();
        for (Container* ptr = m_viewImpl->m_buttons.begin(); ptr != end; ptr++) {
            WebCore::RenderSkinAndroid::State state;
            if (ptr->matches(focus)) {
                focusIsButton = true;
                // If the WebView is out of focus/window focus, set the state to
                // normal, but still keep track of the fact that the focus is a
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
    if (invalidate && cachedFocus && focusIsButton) {
        const WebCore::IntRect& b = cachedFocus->getBounds();
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

// Put a cap on the number of matches to draw.  If the current page has more
// matches than this, only draw the focused match.
#define MAX_NUMBER_OF_MATCHES_TO_DRAW 101

void drawMatches(SkCanvas* canvas)
{
    if (!m_matches || !m_matches->size()) {
        return;
    }
    if (m_findIndex >= m_matches->size()) {
        m_findIndex = 0;
    }
    const MatchInfo& matchInfo = (*m_matches)[m_findIndex];
    const SkRegion& currentMatchRegion = matchInfo.getLocation();
    const SkIRect& currentMatchBounds = currentMatchRegion.getBounds();
    int left = currentMatchBounds.fLeft;
    int top = currentMatchBounds.fTop;
    int right = currentMatchBounds.fRight;
    int bottom = currentMatchBounds.fBottom;
    WebCore::IntRect visible;
    getVisibleRect(&visible);
    // Check to make sure that the highlighted match is on screen.  If not,
    // scroll it onscreen and return.
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
    if ((dx|dy)) {
        scrollBy(dx, dy);
        viewInvalidate();
        return;
    }
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
        SkIRect visibleIRect;
        android_setrect(&visibleIRect, visible);
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

void drawFocusRing(SkCanvas* canvas)
{
    const CachedRoot* root = getFrameCache(AllowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        m_followedLink = false;
        return;
    }
    const CachedNode* node = root->currentFocus();
    if (!node) {
        DBG_NAV_LOG("!node");
        m_followedLink = false;
        return;
    }
    if (!node->hasFocusRing()) {
        DBG_NAV_LOG("!node->hasFocusRing()");
        return;
    }
    const WTF::Vector<WebCore::IntRect>& rings = node->focusRings();
    if (!rings.size()) {
        DBG_NAV_LOG("!rings.size()");
        return;
    }

    bool isButton = false;
    m_viewImpl->gButtonMutex.lock();
    // If this is a button drawn by us (rather than webkit) do not draw the
    // focus ring, since its focus will be shown by a change in what we draw.
    // Should be in sync with recordButtons, since that will be called
    // before this.
    if (m_viewImpl->m_buttons.size() > 0) {
        WebCore::Node* focusPointer = (WebCore::Node*) node->nodePointer();
        Container* end = m_viewImpl->m_buttons.end();
        for (Container* ptr = m_viewImpl->m_buttons.begin(); ptr != end; ptr++) {
            if (ptr->matches(focusPointer)) {
                isButton = true;
                break;
            }
        }
    }
    m_viewImpl->gButtonMutex.unlock();
    WebCore::IntRect bounds = node->bounds();
    bounds.inflate(SkScalarCeil(FOCUS_RING_OUTER_DIAMETER));
    SkRect sbounds;
    android_setrect(&sbounds, bounds);
    if (canvas->quickReject(sbounds, SkCanvas::kAA_EdgeType)) {
        DBG_NAV_LOG("canvas->quickReject");
        m_followedLink = false;
        return;
    }
    FocusRing::Flavor flavor = FocusRing::NORMAL_FLAVOR;
    if (!isButton) {
        flavor = node->type() != NORMAL_CACHEDNODETYPE ?
            FocusRing::FAKE_FLAVOR : node->nodePointer() == m_invalidNode ?
            FocusRing::INVALID_FLAVOR : FocusRing::NORMAL_FLAVOR;
        if (flavor != FocusRing::INVALID_FLAVOR && m_followedLink) {
            flavor = (FocusRing::Flavor) (flavor + FocusRing::NORMAL_ANIMATING);
        }
#if DEBUG_NAV_UI
        const WebCore::IntRect& ring = rings[0];
        DBG_NAV_LOGD("cachedFocusNode=%d (nodePointer=%p) flavor=%s rings=%d"
            " (%d, %d, %d, %d)", node->index(), node->nodePointer(),
            flavor == FocusRing::FAKE_FLAVOR ? "FAKE_FLAVOR" :
            flavor == FocusRing::INVALID_FLAVOR ? "INVALID_FLAVOR" :
            flavor == FocusRing::NORMAL_ANIMATING ? "NORMAL_ANIMATING" :
            flavor == FocusRing::FAKE_ANIMATING ? "FAKE_ANIMATING" : "NORMAL_FLAVOR",
            rings.size(), ring.x(), ring.y(), ring.width(), ring.height());
#endif
    }
    if (isButton || flavor >= FocusRing::NORMAL_ANIMATING) {
        SkMSec time = SkTime::GetMSecs();
        if (time < m_ringAnimationEnd) {
            // views assume that inval bounds coordinates are non-negative
            bounds.intersect(WebCore::IntRect(0, 0, INT_MAX, INT_MAX));
            postInvalidateDelayed(m_ringAnimationEnd - time, bounds);
        } else {
            m_followedLink = false;
            flavor = (FocusRing::Flavor) (flavor - FocusRing::NORMAL_ANIMATING);
        }
    }
    if (!isButton)
        FocusRing::DrawRing(canvas, rings, flavor);
}

OutOfFocusFix fixOutOfDateFocus(bool useReplay)
{
    if (!m_frameCacheUI) {
        DBG_NAV_LOG("!m_frameCacheUI");
        return DoNothing;
    }
    const CachedFrame* cachedFrame = 0;
    const CachedNode* cachedFocusNode = m_frameCacheUI->currentFocus(&cachedFrame);
    if (!cachedFocusNode) {
        DBG_NAV_LOG("!cachedFocusNode");
        return DoNothing;
    }
    CachedRoot* webRoot = m_viewImpl->m_frameCacheKit;
    if (!webRoot) {
        DBG_NAV_LOG("!webRoot");
        return DoNothing;
    }
    int uiWidth = m_frameCacheUI->width();
    int webWidth = webRoot->width();
    if (uiWidth != webWidth) {
        DBG_NAV_LOGD("uiWidth=%d webWidth=%d", uiWidth, webWidth);
        return DoNothing; // allow text inputs to preserve their state
    } else {
        const WebCore::IntRect& cachedBounds = m_frameCacheUI->focusBounds();
        const CachedFrame* webFrame = 0;
        const CachedNode* webFocusNode = webRoot->currentFocus(&webFrame);
        DBG_NAV_LOGD("cachedBounds=(%d,%d,w=%d,h=%d) cachedFrame=%p (%d)"
            " webFocusNode=%p (%d) webFrame=%p (%d)",
            cachedBounds.x(), cachedBounds.y(),
            cachedBounds.width(), cachedBounds.height(),
            cachedFrame, cachedFrame ? cachedFrame->indexInParent() : -1,
            webFocusNode, webFocusNode ? webFocusNode->index() : -1,
            webFrame, webFrame ? webFrame->indexInParent() : -1);
        if (webFocusNode && webFrame && webFrame->sameFrame(cachedFrame)) {
            if (useReplay && !m_replay.count()) {
                DBG_NAV_LOG("!m_replay.count()");
                return DoNothing;
            }
            if (webFocusNode->index() == cachedFocusNode->index()) {
                DBG_NAV_LOG("index ==");
                return DoNothing;
            }
            const WebCore::IntRect& webBounds = webRoot->focusBounds();
            DBG_NAV_LOGD("webBounds=(%d,%d,w=%d,h=%d)",
                webBounds.x(), webBounds.y(),
                webBounds.width(), webBounds.height());
            if (cachedBounds.contains(webBounds)) {
                DBG_NAV_LOG("contains");
                return DoNothing;
            }
            if (webBounds.contains(cachedBounds)) {
                DBG_NAV_LOG("webBounds contains");
                return DoNothing;
            }
        }
        const CachedFrame* foundFrame = 0;
        int x, y;
        const CachedNode* found = findAt(webRoot, cachedBounds, &foundFrame, &x, &y);
#if DEBUG_NAV_UI
        DBG_NAV_LOGD("found=%p (%d) frame=%p (%d)",
            found, found ? found->index() : -1,
            foundFrame, foundFrame ? foundFrame->indexInParent() : -1);
        if (found) {
            WebCore::IntRect newBounds = found->bounds();
            DBG_NAV_LOGD("found=(%d,%d,w=%d,h=%d) x=%d y=%d",
                newBounds.x(), newBounds.y(), newBounds.width(),
                newBounds.height(), x, y);
        }
#endif
        webRoot->setCachedFocus(const_cast<CachedFrame*>(foundFrame),
            const_cast<CachedNode*>(found));
        if (found)
            webRoot->rootHistory()->setNavBounds(found->bounds());
        WebCore::Frame* framePointer = foundFrame ? (WebCore::Frame*) foundFrame->framePointer() : 0;
        WebCore::Node* nodePointer = found ? (WebCore::Node*) found->nodePointer() : 0;
        setFocusData(webRoot->generation(), framePointer, nodePointer, x, y, !found);
        sendFinalFocus(framePointer, nodePointer, x, y);
        if (found && (found->isTextArea() || found->isTextField()))
            return UpdateTextEntry;
    }
checkOldFocus:
    return cachedFocusNode->isTextArea() || cachedFocusNode->isTextField() ? ClearTextEntry : DoNothing;
}

bool focusIsTextArea(FrameCachePermission allowNewer)
{
    CachedRoot* root = getFrameCache(allowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        return false;
    }
    const CachedNode* focus = root->currentFocus();
    if (!focus)
        return false;
    return focus->isTextArea() || focus->isTextField();
}

void focusRingBounds(WebCore::IntRect* bounds)
{
    DBG_NAV_LOGD("%s", "");
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (root) {
        const CachedNode* cachedNode = root->currentFocus();
        if (cachedNode) {
            cachedNode->focusRingBounds(bounds);
            DBG_NAV_LOGD("bounds={%d,%d,%d,%d}", bounds->x(), bounds->y(),
                bounds->width(), bounds->height());
            return;
        }
    }
    *bounds = WebCore::IntRect(0, 0, 0, 0);
}

CachedRoot* getFrameCache(FrameCachePermission allowNewer)
{
    if (!m_viewImpl->m_updatedFrameCache)
        return m_frameCacheUI;
    m_viewImpl->gRecomputeFocusMutex.lock();
    bool recomputeInProgress = m_viewImpl->m_recomputeEvents.size() > 0;
    m_viewImpl->gRecomputeFocusMutex.unlock();
    if (allowNewer != AllowNewest && recomputeInProgress)
        return m_frameCacheUI;
    if (allowNewer == DontAllowNewer && m_viewImpl->m_lastGeneration < m_generation)
        return m_frameCacheUI;
    DBG_NAV_LOGD("%s", "m_viewImpl->m_updatedFrameCache == true");
    bool hadFocus = m_frameCacheUI && m_frameCacheUI->currentFocus();
    m_viewImpl->gFrameCacheMutex.lock();
    OutOfFocusFix fix = DoNothing;
    if (allowNewer != DontAllowNewer)
        fix = fixOutOfDateFocus(m_viewImpl->m_useReplay);
    delete m_frameCacheUI;
    delete m_navPictureUI;
    m_viewImpl->m_updatedFrameCache = false;
    m_frameCacheUI = m_viewImpl->m_frameCacheKit;
    m_navPictureUI = m_viewImpl->m_navPictureKit;
    m_viewImpl->m_frameCacheKit = 0;
    m_viewImpl->m_navPictureKit = 0;
    m_viewImpl->gFrameCacheMutex.unlock();
    if (hadFocus && (!m_frameCacheUI || !m_frameCacheUI->currentFocus()))
        viewInvalidate(); // redraw in case focus ring is still visible
    if (fix == UpdateTextEntry)
        updateTextEntry();
    else if (fix == ClearTextEntry)
        clearTextEntry();
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
            LOGD("------- bad key sent to WebView::moveFocus");
            return CachedFrame::UNINITIALIZED;
    }
}

bool invalidFrame(WebCore::Frame* frame, const CachedRoot* root)
{
    if (!frame)
        return false;
    int frameBuild = m_viewImpl->retrieveFrameGeneration(frame);
    int rootBuild = root->generation();
    return frameBuild > rootBuild;
}

WebCore::String imageURI(int x, int y)
{
    const CachedRoot* root = getFrameCache(DontAllowNewer);
    return root ? root->imageURI(x, y) : WebCore::String();
}

bool focusNodeWantsKeyEvents()
{
    const CachedRoot* root = getFrameCache(DontAllowNewer);
    if (root) {
        const CachedNode* focus = root->currentFocus();
        if (focus) {
            return focus->isWantsKeyEvents();
        }
    }
    return false;
}

/* returns true if the key had no effect (neither scrolled nor changed focus) */
bool moveFocus(int keyCode, int count, bool ignoreScroll, bool inval,
    void* lastSentFocus, const WebCore::IntRect* lastSentBounds)
{
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        setFocusData(0, 0, 0, 0, 0, true);
        sendKitFocus(); // will build cache and retry
        FirstMoveFocusParams params;
        params.d.m_trigger = CommonParams::FirstMoveFocusParams;
        params.d.m_generation = m_generation;
        params.m_keyCode = keyCode;
        params.m_count = count;
        params.m_ignoreScroll = ignoreScroll;
        m_replay.add(params.d, sizeof(params));
        return true;
    }

    CachedFrame::Direction direction = KeyToDirection((KeyCode) keyCode);
    const CachedFrame* cachedFrame, * oldFrame = 0;
    const CachedNode* focus = root->currentFocus(&oldFrame);
    WebCore::IntPoint focusLocation = root->focusLocation();
    DBG_NAV_LOGD("old focus %d (nativeNode=%p) focusLocation={%d, %d}",
        focus ? focus->index() : 0,
        focus ? focus->nodePointer() : 0, focusLocation.x(), focusLocation.y());
    WebCore::IntRect visibleRect;
    getVisibleRect(&visibleRect);
    DBG_NAV_LOGD("getVisibleRect %d,%d,%d,%d",
        visibleRect.x(), visibleRect.y(), visibleRect.width(), visibleRect.height());
    root->setVisibleRect(visibleRect);
    int xMax = getScaledMaxXScroll();
    int yMax = getScaledMaxYScroll();
    root->setMaxScroll(xMax, yMax);
    CachedHistory savedHistory = *root->rootHistory();
    bool oldNodeIsTextArea = focusIsTextArea(DontAllowNewer);
    const CachedNode* cachedNode = 0;
    int dx = 0;
    int dy = 0;
    int counter = count;
    if (!focus || !focus->isInput() || !m_followedLink)
        root->setScrollOnly(m_followedLink);
    while (--counter >= 0) {
        WebCore::IntPoint scroll = WebCore::IntPoint(0, 0);
        cachedNode = root->moveFocus(direction, &cachedFrame, &scroll);
        dx += scroll.x();
        dy += scroll.y();
    }
    DBG_NAV_LOGD("new focus %d (nativeNode=%p) focusLocation={%d, %d}",
        cachedNode ? cachedNode->index() : 0,
        cachedNode ? cachedNode->nodePointer() : 0, root->focusLocation().x(),
            root->focusLocation().y());
    // If !m_heightCanMeasure (such as in the browser), we want to scroll no
    // matter what
    if (!ignoreScroll && (!m_heightCanMeasure ||
            !cachedNode ||
            (focus && focus->nodePointer() == cachedNode->nodePointer())))
    {
        if (count == 1 && dx != 0 && dy == 0 && -m_lastDx == dx &&
                SkTime::GetMSecs() - m_lastDxTime < 1000)
            root->checkForJiggle(&dx);
        DBG_NAV_LOGD("scrollBy %d,%d", dx, dy);
        if ((dx | dy))
            this->scrollBy(dx, dy);
        m_lastDx = dx;
        m_lastDxTime = SkTime::GetMSecs();
        ignoreScroll = true; // if move re-executes, don't scroll the second time
    }
    bool result = false;
    if (cachedNode) {
        WebCore::IntPoint pos;
        root->setCachedFocus((CachedFrame*) cachedFrame, (CachedNode*) cachedNode);
        root->getSimulatedMousePosition(&pos);
        if (lastSentFocus == cachedNode->nodePointer() && lastSentBounds &&
                *lastSentBounds == cachedNode->bounds())
        {
            sendFinalFocus((WebCore::Frame*) cachedFrame->framePointer(),
                (WebCore::Node*) cachedNode->nodePointer(), pos.x(), pos.y());
        } else {
            setFocusData(root->generation(),
                (WebCore::Frame*) cachedFrame->framePointer(),
                (WebCore::Node*) cachedNode->nodePointer(), pos.x(), pos.y(),
                true);
            sendKitFocus();
            if (inval)
                viewInvalidate();
            MoveFocusParams params;
            params.d.d.m_trigger = CommonParams::MoveFocusParams;
            params.d.d.m_generation = m_generation;
            params.c.setFocus(focus, oldFrame, root, focusLocation);
            params.m_sentFocus = cachedNode->nodePointer();
            params.m_sentBounds = cachedNode->bounds();
            params.m_visibleRect = visibleRect;
            params.m_history = savedHistory;
            DBG_NAV_LOGD("history.mDidFirstLayout=%s",
                    params.m_history.didFirstLayout() ? "true" : "false");
            params.m_xMax = xMax;
            params.m_yMax = yMax;
            params.d.m_keyCode = keyCode;
            params.d.m_count = count;
            params.d.m_ignoreScroll = ignoreScroll;
            m_replay.add(params.d.d, sizeof(params));
        }
    } else {
        if (visibleRect.intersects(root->focusBounds()) == false) {
            setFocusData(root->generation(), 0, 0, 0, 0, true);
            sendKitFocus(); // will build cache and retry
        }
        FirstMoveFocusParams params;
        params.d.m_trigger = CommonParams::FirstMoveFocusParams;
        params.d.m_generation = m_generation;
        params.m_keyCode = keyCode;
        params.m_count = count;
        params.m_ignoreScroll = ignoreScroll;
        m_replay.add(params.d, sizeof(params));
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
    if (focusIsTextArea(DontAllowNewer))
        updateTextEntry();
    else if (oldNodeIsTextArea)
        clearTextEntry();
    return result;
}

void notifyFocusSet(FrameCachePermission inEditingMode)
{
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (root) {
        // make sure the mFocusData in WebView.java is in sync with WebView.cpp
        const CachedFrame* frame = 0;
        const CachedNode* node = root->currentFocus(&frame);
        const WebCore::IntPoint& focusLocation = root->focusLocation();
        setFocusData(root->generation(),
                frame ? (WebCore::Frame*) frame->framePointer() : 0,
                node ? (WebCore::Node*) node->nodePointer() : 0, 
                focusLocation.x(), focusLocation.y(), false);
    }

    if (focusIsTextArea(inEditingMode))
        updateTextEntry();
    else if (inEditingMode)
        clearTextEntry();
#if DEBUG_NAV_UI
    if (m_frameCacheUI) {
        const CachedNode* focus = m_frameCacheUI->currentFocus();
        DBG_NAV_LOGD("focus %d (nativeNode=%p)",
            focus ? focus->index() : 0,
            focus ? focus->nodePointer() : 0);
    }
#endif
}

void notifyProgressFinished()
{
    DBG_NAV_LOGD("focusIsTextArea=%d", focusIsTextArea(DontAllowNewer));
    updateTextEntry();
#if DEBUG_NAV_UI
    if (m_frameCacheUI) {
        const CachedNode* focus = m_frameCacheUI->currentFocus();
        DBG_NAV_LOGD("focus %d (nativeNode=%p)",
            focus ? focus->index() : 0,
            focus ? focus->nodePointer() : 0);
    }
#endif
}

void recomputeFocus()
{
    int generation;
    do {
        m_viewImpl->gRecomputeFocusMutex.lock();
        if (!m_viewImpl->m_recomputeEvents.size()) {
            m_viewImpl->gRecomputeFocusMutex.unlock();
            return;
        }
        generation = m_viewImpl->m_recomputeEvents.first();
        m_viewImpl->m_recomputeEvents.remove(0);
        m_viewImpl->gRecomputeFocusMutex.unlock();
        DBG_NAV_LOGD("generation=%d", generation);
        CachedRoot* root = getFrameCache(AllowNewest);
        if (!root) {
            DBG_NAV_LOG("!root");
            return;
        }
        LargestParams storage;
        const CommonParams& params = storage.d.d;
        char* pos = m_replay.position();
        while (m_replay.retrieve(&storage.d.d) < generation)
            DBG_NAV_LOGD("dropped ", params.m_generation);
        if (params.m_generation > generation) {
            DBG_NAV_LOGD("params.m_generation=%d > generation=%d",
                params.m_generation, generation);
            m_replay.rewind(pos);
            return;
        }
        int lastAdd = m_replay.lastAdd();
        do {
            LOG_ASSERT(params.m_trigger != CommonParams::NoData, "expected data");
            bool inval = generation == m_generation;
            switch (params.m_trigger) {
                case CommonParams::ClearFocusParams: {
                    const ClearFocusParams& sParams = *(ClearFocusParams*) &storage;
                    const CacheParams& cParams = sParams.c;
                    if (invalidFrame(cParams.m_frame, root)) {
                        DBG_NAV_LOGD("dropped %s generation=%d",
                            TriggerNames[params.m_trigger], generation);
                        return;
                    }
                    root->setFocus(cParams.m_frame, cParams.m_node, cParams.m_x, cParams.m_y);
                    clearFocus(sParams.m_x, sParams.m_y, inval);
                    DBG_NAV_LOGD("clearFocus(x,y)={%d,%d}", sParams.m_x, sParams.m_y);
                    } break;
                case CommonParams::MotionUpParams: {
                    const MotionUpParams& mParams = *(MotionUpParams*) &storage;
                //    const CacheParams& cParams = mParams.c;
                //    if (invalidFrame(cParams.m_frame, root) == false)
                //        root->setFocus(cParams.m_frame, cParams.m_node,
                //            cParams.m_x, cParams.m_y);
                    motionUp(mParams.m_x, mParams.m_y, mParams.m_slop, mParams.m_isClick, inval, true);
                    DBG_NAV_LOGD("motionUp m_x=%d m_y=%d", mParams.m_x, mParams.m_y);
                    } break;
                case CommonParams::FirstMoveFocusParams: {
                    if (invalidFrame((WebCore::Frame*) root->framePointer(), root)) {
                        DBG_NAV_LOGD("dropped %s generation=%d",
                            TriggerNames[params.m_trigger], generation);
                        return;
                    }
                    const FirstMoveFocusParams& fParams = *(FirstMoveFocusParams*) &storage;
                    DBG_NAV_LOGD("first moveFocus keyCode=%d count=%d"
                        " ignoreScroll=%s", fParams.m_keyCode, fParams.m_count,
                        fParams.m_ignoreScroll ? "true" : "false");
                    moveFocus(fParams.m_keyCode, fParams.m_count,
                        fParams.m_ignoreScroll, inval, 0, 0);
                    } break;
                case CommonParams::MoveFocusParams: {
                    const MoveFocusParams& mParams = *(MoveFocusParams*) &storage;
                    const CacheParams& cParams = mParams.c;
                    if (invalidFrame(cParams.m_frame, root)) {
                        DBG_NAV_LOGD("dropped %s generation=%d",
                            TriggerNames[params.m_trigger], generation);
                        return;
                    }
                    DBG_NAV_LOGD("moveFocus keyCode=%d count=%d ignoreScroll=%s "
                        "history.mDidFirstLayout=%s", mParams.d.m_keyCode,
                        mParams.d.m_count, mParams.d.m_ignoreScroll ? "true" : "false",
                        mParams.m_history.didFirstLayout() ? "true" : "false");
                    if (!root->setFocus(cParams.m_frame, cParams.m_node,
                            cParams.m_x, cParams.m_y)) {
                        DBG_NAV_LOGD("can't restore focus frame=%p node=%p",
                            "x=%d y=%d %s", cParams.m_frame, cParams.m_node,
                            cParams.m_x, cParams.m_y, TriggerNames[params.m_trigger]);
                        return;
                    }
                    root->setVisibleRect(mParams.m_visibleRect);
                    root->setMaxScroll(mParams.m_xMax, mParams.m_yMax);
                    *root->rootHistory() = mParams.m_history;
                    moveFocus(mParams.d.m_keyCode, mParams.d.m_count,
                        mParams.d.m_ignoreScroll, inval,
                        mParams.m_sentFocus, &mParams.m_sentBounds);
                    } break;
                default:
                    LOG_ASSERT(0, "unknown trigger");
            }
            if (params.m_generation >= lastAdd)
                break;
            root = getFrameCache(DontAllowNewer); // re-execution may have retrieved newer cache
            m_replay.retrieve(&storage.d.d);
            DBG_NAV_LOGD("continuation m_generation %d", params.m_generation);
        } while (true);
    } while (true);
}

void resetFocus()
{
    DEBUG_NAV_UI_LOGD("%s", __FUNCTION__);
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root)
        return;
    root->setCachedFocus(0, 0);
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
    return root->findAt(rect, framePtr, rxPtr, ryPtr);
}

void selectBestAt(const WebCore::IntRect& rect)
{
    const CachedFrame* frame;
    int rx, ry;
    CachedRoot* root = getFrameCache(DontAllowNewer);
    const CachedNode* node = findAt(root, rect, &frame, &rx, &ry);
    int rootGeneration = root ? root->generation() : 0;
    setFocusData(rootGeneration,
        frame ? (WebCore::Frame*) frame->framePointer() : 0,
        node ? (WebCore::Node*) node->nodePointer() : 0, rx, ry, false);
    if (!node) {
        DBG_NAV_LOGD("no nodes found root=%p", root);
        if (root) {
            root->clearFocus();
            root->setCachedFocus(0, 0);
        }
        sendKitFocus();
        viewInvalidate();
        clearTextEntry();
        return;
    }
    DBG_NAV_LOGD("CachedNode:%p (%d)", node, node->index());
    const CachedFrame* oldFrame = 0;
    const CachedNode* oldFocusNode = root->currentFocus(&oldFrame);
    bool oldNodeIsTextArea = focusIsTextArea(DontAllowNewer);
    root->setCachedFocus(const_cast<CachedFrame*>(frame),
        const_cast<CachedNode*>(node));
    viewInvalidate();
    if (focusIsTextArea(DontAllowNewer))
        updateTextEntry();
    else if (oldNodeIsTextArea)
        clearTextEntry();
}

WebCore::IntRect getNavBounds()
{
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root)
        return WebCore::IntRect(0, 0, 0, 0);
    return root->rootHistory()->navBounds();
}

void setNavBounds(const WebCore::IntRect& rect)
{
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root)
        return;
    root->rootHistory()->setNavBounds(rect);
}

void markNodeInvalid(WebCore::Node* node)
{
    DBG_NAV_LOGD("node=%p", node);
    m_invalidNode = node;
    viewInvalidate();
}

bool motionUp(int x, int y, int slop, bool isClick, bool inval, bool retry)
{
    bool pageScrolled = false;
    m_followedLink = false;
    const CachedFrame* frame;
    WebCore::IntRect rect = WebCore::IntRect(x - slop, y - slop, slop * 2, slop * 2);
    int rx, ry;
    CachedRoot* root = getFrameCache(AllowNewer);
    const CachedNode* result = findAt(root, rect, &frame, &rx, &ry);
    if (!result) {
        DBG_NAV_LOGD("no nodes found root=%p", root);
        int rootGeneration = 0;
        if (root) {
            root->clearFocus();
            rootGeneration = root->generation();
            if (!retry) { // scroll first time only
                int dx = root->checkForCenter(x, y);
                if (dx) {
                    scrollBy(dx, 0);
                    retry = true; // don't recompute later since we scrolled
                    pageScrolled = true;
                }
            }
        }
        sendMotionUp(rootGeneration, frame ?
            (WebCore::Frame*) frame->framePointer() : 0,
            0, x, y, slop, isClick, retry);
        if (inval)
            viewInvalidate();
        if (!retry) {
            MotionUpParams params;
            params.d.m_trigger = CommonParams::MotionUpParams;
            params.d.m_generation = m_generation;
            params.m_x = x;
            params.m_y = y;
            params.m_slop = slop;
	    params.m_isClick = isClick;
            m_replay.add(params.d, sizeof(params));
        }
        clearTextEntry();
        return pageScrolled;
    }
    DBG_NAV_LOGD("CachedNode:%p (%d) x=%d y=%d rx=%d ry=%d", result,
        result->index(), x, y, rx, ry);
 //   const CachedFrame* oldFrame = 0;
 //   const CachedNode* oldFocusNode = root->currentFocus(&oldFrame);
 //   WebCore::IntPoint focusLocation = root->focusLocation();
    bool oldNodeIsTextArea = !retry && focusIsTextArea(DontAllowNewer);
    root->setCachedFocus(const_cast<CachedFrame*>(frame),
        const_cast<CachedNode*>(result));
    bool newNodeIsTextArea = focusIsTextArea(DontAllowNewer);
    CachedNodeType type = result->type();
    if (type == NORMAL_CACHEDNODETYPE || newNodeIsTextArea) {
        sendMotionUp(root->generation(),
            frame ? (WebCore::Frame*) frame->framePointer() : 0,
            result ? (WebCore::Node*) result->nodePointer() : 0, rx, ry,
            slop, isClick, retry);
        if (inval)
            viewInvalidate();
        if (!retry) {
            MotionUpParams params;
            params.d.m_trigger = CommonParams::MotionUpParams;
            params.d.m_generation = m_generation;
            params.m_x = x;
            params.m_y = y;
            params.m_slop = slop;
            params.m_isClick = isClick;
        //    params.c.setFocus(oldFocusNode, oldFrame, root, focusLocation);
            m_replay.add(params.d, sizeof(params));
        }
    } else if (inval)
        viewInvalidate();
    if (newNodeIsTextArea) {
        updateTextEntry();
        displaySoftKeyboard();
    } else {
        if (isClick) {
            setFollowedLink(true);
            if (type != NORMAL_CACHEDNODETYPE) {
                overrideUrlLoading(result->getExport());
            }
        }
        if (oldNodeIsTextArea)
            clearTextEntry();
    }
    return pageScrolled;
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
    if (!extendSelection)
        m_selStart = m_selEnd = CopyPaste::findClosest(picture, area, x, y);
    else
        m_selEnd = CopyPaste::findClosest(picture, area, x, y);
    DBG_NAV_LOGD("x=%d y=%d extendSelection=%s m_selStart=(%d, %d, %d, %d)"
        " m_selEnd=(%d, %d, %d, %d)", x, y, extendSelection ? "true" : "false",
        m_selStart.fLeft, m_selStart.fTop, m_selStart.fRight, m_selStart.fBottom,
        m_selEnd.fLeft, m_selEnd.fTop, m_selEnd.fRight, m_selEnd.fBottom);
}

const SkRegion& getSelection()
{
    return m_selRegion;
}

void drawSelection(SkCanvas* canvas, int x, int y, bool extendSelection)
{
    if (!extendSelection) {
        int dx = x - m_selStart.fLeft;
        dx *= dx;
        int otherX = x - m_selStart.fRight;
        if (dx > (otherX *= otherX))
            dx = otherX;
        int dy = y - m_selStart.fTop;
        int dist = dx * dx + dy * dy;
        if (dist > MIN_ARROW_DISTANCE)
            drawSelectionArrow(canvas, x, y);
        else
            drawSelectionPointer(canvas, x, y, true);
    } else {
        drawSelectionRegion(canvas);
        drawSelectionPointer(canvas, x, y, false);
    }
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

void drawSelectionPointer(SkCanvas* canvas, int x, int y, bool gridded)
{
    SkPath path;
    getSelectionCaret(&path);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(SK_ColorBLACK);
    SkPixelXorXfermode xorMode(SK_ColorWHITE);
    paint.setXfermode(&xorMode);
    int sc = canvas->save();
    if (gridded) {
        bool useLeft = x <= (m_selStart.fLeft + m_selStart.fRight) >> 1;
        canvas->translate(SkIntToScalar(useLeft ? m_selStart.fLeft :
            m_selStart.fRight), SkIntToScalar(m_selStart.fTop));
    } else
        canvas->translate(SkIntToScalar(x), SkIntToScalar(y));
    canvas->drawPath(path, paint);
    canvas->restoreToCount(sc);
}

void drawSelectionArrow(SkCanvas* canvas, int x, int y)
{
    SkPath path;
    getSelectionArrow(&path);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(SK_ColorBLACK);
    paint.setStrokeWidth(SK_Scalar1 * 2);
    int sc = canvas->save();
    canvas->translate(SkIntToScalar(x), SkIntToScalar(y));
    canvas->drawPath(path, paint);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SK_ColorWHITE);
    canvas->drawPath(path, paint);
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
    path->lineTo(0, height);
    SkScalar bottom = height + dist;
    path->lineTo(-dist, bottom);
    SkScalar edge = bottom - SK_Scalar1/2;
    path->moveTo(-dist, edge);
    path->lineTo(dist, edge);
    path->moveTo(dist, bottom);
    path->lineTo(0, height);
}

void sendFinalFocus(WebCore::Frame* framePtr, WebCore::Node* nodePtr, int x, int y)
{
    DBG_NAV_LOGD("framePtr=%p nodePtr=%p x=%d y=%d", framePtr, nodePtr, x, y);
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(), m_javaGlue.m_sendFinalFocus,
        (jint) framePtr, (jint) nodePtr, x, y);
    checkException(env);
}

void sendKitFocus()
{
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(), m_javaGlue.m_sendKitFocus);
    checkException(env);
}

void sendMotionUp(int buildGeneration,
    WebCore::Frame* framePtr, WebCore::Node* nodePtr, int x, int y, int slop,
    bool isClick, bool retry)
{
    m_viewImpl->m_touchGeneration = m_viewImpl->m_generation = ++m_generation;
    DBG_NAV_LOGD("buildGeneration=%d m_generation=%d framePtr=%p nodePtr=%p"
        " x=%d y=%d slop=%d", buildGeneration,
        m_generation, framePtr, nodePtr, x, y, slop);
    LOG_ASSERT(m_javaGlue.m_obj, "A WebView was not associated with this WebViewNative!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(), m_javaGlue.m_sendMotionUp, m_generation,
        buildGeneration, (jint) framePtr, (jint) nodePtr, x, y, slop, isClick, retry);
    checkException(env);
}

void setFocusData(int buildGeneration, WebCore::Frame* framePtr,
    WebCore::Node* nodePtr, int x, int y, bool ignoreNullFocus)
{
    m_viewImpl->m_moveGeneration = m_viewImpl->m_generation = ++m_generation;
    DBG_NAV_LOGD("moveGeneration=%d buildGeneration=%d framePtr=%p nodePtr=%p"
        " x=%d y=%d", m_generation, buildGeneration, framePtr, nodePtr, x, y);
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(), m_javaGlue.m_setFocusData, m_generation,
        buildGeneration, (jint) framePtr, (jint) nodePtr, x, y, ignoreNullFocus);
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

void scrollBy(int dx, int dy)
{
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(), m_javaGlue.m_scrollBy, 
        dx, dy, true);
    checkException(env);
}

bool updateFocusNode(JNIEnv* env)
{
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        return false;
    }
    const CachedFrame* cachedFrame = 0;
    const CachedNode* cachedFocusNode = root->currentFocus(&cachedFrame);
    if (!cachedFocusNode) {
        DBG_NAV_LOG("!cachedFocusNode");
        return false;
    }
    DBG_NAV_LOGD("cachedFocusNode=%d (nodePointer=%p)",
        cachedFocusNode->index(),
        cachedFocusNode->nodePointer());
    jobject focusnode = env->GetObjectField(m_javaGlue.object(env).get(), m_javaGlue.m_focusNode);
    LOG_ASSERT(focusnode, "Could not find WebView's FocusNode");

    bool isTextArea = cachedFocusNode->isTextArea();
    bool isTextField = cachedFocusNode->isTextField();
    int maxLength;
    jstring jName;
    if (isTextField) {
        maxLength = cachedFocusNode->maxLength();
        const WebCore::String& name = cachedFocusNode->name();
        jName = env->NewString((jchar*)name.characters(), name.length());
    } else {
        maxLength = -1;
        jName = 0;
    }
    WebCore::IntRect bounds = cachedFocusNode->bounds();
    WebCore::String value = cachedFocusNode->getExport();
    jstring val = !value.isEmpty() ? env->NewString((jchar *)value.characters(), value.length()) : 0;
    env->CallVoidMethod(focusnode, m_javaGlue.m_setAll, isTextField, isTextArea, cachedFocusNode->isPassword(),
            cachedFocusNode->isAnchor(), cachedFocusNode->isRtlText(), maxLength, cachedFocusNode->textSize(),
            bounds.x(), bounds.y(), bounds.right(), bounds.bottom(), (int)(cachedFocusNode->nodePointer()),
            (int)(cachedFrame->framePointer()), val, jName, root->textGeneration());
    env->DeleteLocalRef(val);
    env->DeleteLocalRef(focusnode);
    if (isTextField)
        env->DeleteLocalRef(jName);
    return true;
}

void updateTextEntry()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(), m_javaGlue.m_updateTextEntry);
    checkException(env);
}

void displaySoftKeyboard()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->CallVoidMethod(m_javaGlue.object(env).get(),
            m_javaGlue.m_displaySoftKeyboard);
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

private: // local state for WebView
    // private to getFrameCache(); other functions operate in a different thread
    CachedRoot* m_frameCacheUI; // navigation data ready for use
    FocusReplay m_replay;
    WebViewCore* m_viewImpl;
    WebCore::Node* m_invalidNode;
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

static void nativeClearFocus(JNIEnv *env, jobject obj, int x, int y)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->clearFocus(x, y, true);
}

static void nativeCreate(JNIEnv *env, jobject obj, int viewImpl)
{
    WebView* webview = new WebView(env, obj, viewImpl);
    // NEED THIS OR SOMETHING LIKE IT!
    //Release(obj);
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

static void nativeDrawFocusRing(JNIEnv *env, jobject obj,
                                       jobject canv)
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
    view->drawFocusRing(canvas);
}

static void nativeDrawSelection(JNIEnv *env, jobject obj,
                                       jobject canv, jint x, jint y, bool ex)
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
    view->drawSelection(canvas, x, y, ex);
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

static bool nativeFocusNodeWantsKeyEvents(JNIEnv* env, jobject jwebview) {
    WebView* view = GET_NATIVE_VIEW(env, jwebview);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return view->focusNodeWantsKeyEvents();
}

static void nativeInstrumentReport(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounter::reportNow();
#endif
}

static WebCore::IntRect jrect_to_webrect(JNIEnv* env, jobject obj)
{
    int L, T, R, B;
    GraphicsJNI::get_jrect(env, obj, &L, &T, &R, &B);
    return WebCore::IntRect(L, T, R - L, B - T);
}

static void nativeSelectBestAt(JNIEnv *env, jobject obj, jobject jrect)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    WebCore::IntRect rect = jrect_to_webrect(env, jrect);
    view->selectBestAt(rect);
}

static void nativeMarkNodeInvalid(JNIEnv *env, jobject obj, int node)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->markNodeInvalid((WebCore::Node*) node);
}

static bool nativeMotionUp(JNIEnv *env, jobject obj,
    int x, int y, int slop, bool isClick)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return view->motionUp(x, y, slop, isClick, true, false);
}

static bool nativeUpdateFocusNode(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return view->updateFocusNode(env);
}

static bool nativeMoveFocus(JNIEnv *env, jobject obj,
    int key, int count, bool ignoreScroll)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    DBG_NAV_LOGD("env=%p obj=%p view=%p", env, obj, view);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return view->moveFocus(key, count, ignoreScroll, true, 0, 0);
}

static void nativeNotifyFocusSet(JNIEnv *env, jobject obj, bool inEditingMode)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->notifyFocusSet((WebView::FrameCachePermission) inEditingMode);
}

static void nativeRecomputeFocus(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->recomputeFocus();
}

static void nativeRecordButtons(JNIEnv* env, jobject obj, bool hasFocus,
        bool pressed, bool invalidate)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->nativeRecordButtons(hasFocus, pressed, invalidate);
}

static void nativeResetFocus(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->resetFocus();
}

static void nativeSetFindIsDown(JNIEnv *env, jobject obj)
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

static jobject nativeGetFocusRingBounds(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    jclass rectClass = env->FindClass("android/graphics/Rect");
    LOG_ASSERT(rectClass, "Could not find Rect class!");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    LOG_ASSERT(init, "Could not find constructor for Rect");
    WebCore::IntRect webRect;
    view->focusRingBounds(&webRect);
    jobject rect = env->NewObject(rectClass, init, webRect.x(),
        webRect.y(), webRect.right(), webRect.bottom());
    return rect;
}

static jobject nativeGetNavBounds(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    jclass rectClass = env->FindClass("android/graphics/Rect");
    LOG_ASSERT(rectClass, "Could not find Rect class!");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    LOG_ASSERT(init, "Could not find constructor for Rect");
    WebCore::IntRect webRect = view->getNavBounds();
    jobject rect = env->NewObject(rectClass, init, webRect.x(),
        webRect.y(), webRect.right(), webRect.bottom());
    return rect;
}

static void nativeSetNavBounds(JNIEnv *env, jobject obj, jobject jrect)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    WebCore::IntRect rect = jrect_to_webrect(env, jrect);
    view->setNavBounds(rect);
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
    static const int MAX_16_BIT_INT = 65535;
    int width = root->documentWidth();
    if (width > MAX_16_BIT_INT) width = MAX_16_BIT_INT;
    int height = root->documentHeight();
    if (height > MAX_16_BIT_INT) height = MAX_16_BIT_INT;
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
    if (!cachedFocusNode || (!cachedFocusNode->isTextField() && !cachedFocusNode->isTextArea()))
        return;
    WebCore::String webcoreString = to_string(env, updatedText);
    (const_cast<CachedNode*>(cachedFocusNode))->setExport(webcoreString);
    root->setTextGeneration(generation);
    checkException(env);
}

static void nativeDestroy(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOGD("nativeDestroy view: %p", view);
    LOG_ASSERT(view, "view not set in nativeDestroy");
    delete view;
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
    return GraphicsJNI::createRegion(env, new SkRegion(view->getSelection()));
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

    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (root) {
        SkPicture* picture = root->getPicture();
        if (picture) {
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
                picture->draw(&canvas);
                // we're done with the file now
                fwrite("\n", 1, 1, file);
                fclose(file);
            }
        }
    }
#endif
}

/*
 * JNI registration
 */
static JNINativeMethod gJavaWebViewMethods[] = {
    { "nativeFindAll", "(Ljava/lang/String;Ljava/lang/String;)I",
        (void*) nativeFindAll },
    { "nativeFindNext", "(Z)V",
        (void*) nativeFindNext },
    { "nativeClearFocus", "(II)V",
        (void*) nativeClearFocus },
    { "nativeCreate", "(I)V",
        (void*) nativeCreate },
    { "nativeDebugDump", "()V",
        (void*) nativeDebugDump },
    { "nativeDestroy", "()V",
        (void*) nativeDestroy },
    { "nativeDrawMatches", "(Landroid/graphics/Canvas;)V",
        (void*) nativeDrawMatches },
    { "nativeDrawFocusRing", "(Landroid/graphics/Canvas;)V",
        (void*) nativeDrawFocusRing },
    { "nativeDrawSelection", "(Landroid/graphics/Canvas;IIZ)V",
        (void*) nativeDrawSelection },
    { "nativeDrawSelectionRegion", "(Landroid/graphics/Canvas;)V",
        (void*) nativeDrawSelectionRegion },
    { "nativeUpdateFocusNode", "()Z",
        (void*) nativeUpdateFocusNode },
    { "nativeGetFocusRingBounds", "()Landroid/graphics/Rect;",
        (void*) nativeGetFocusRingBounds },
    { "nativeGetNavBounds", "()Landroid/graphics/Rect;",
        (void*) nativeGetNavBounds },
    { "nativeInstrumentReport", "()V",
        (void*) nativeInstrumentReport },
    { "nativeMarkNodeInvalid", "(I)V",
        (void*) nativeMarkNodeInvalid },
    { "nativeMotionUp", "(IIIZ)Z",
        (void*) nativeMotionUp },
    { "nativeMoveFocus", "(IIZ)Z",
        (void*) nativeMoveFocus },
    { "nativeNotifyFocusSet", "(Z)V",
        (void*) nativeNotifyFocusSet },
    { "nativeRecomputeFocus", "()V",
        (void*) nativeRecomputeFocus },
    { "nativeRecordButtons", "(ZZZ)V",
        (void*) nativeRecordButtons },
    { "nativeResetFocus", "()V",
        (void*) nativeResetFocus },
    { "nativeSelectBestAt", "(Landroid/graphics/Rect;)V",
        (void*) nativeSelectBestAt },
    { "nativeSetFindIsDown", "()V",
        (void*) nativeSetFindIsDown },
    { "nativeSetFollowedLink", "(Z)V",
        (void*) nativeSetFollowedLink },
    { "nativeSetHeightCanMeasure", "(Z)V",
        (void*) nativeSetHeightCanMeasure },
    { "nativeSetNavBounds", "(Landroid/graphics/Rect;)V",
        (void*) nativeSetNavBounds },
    { "nativeImageURI", "(II)Ljava/lang/String;",
        (void*) nativeImageURI },
    { "nativeFocusNodeWantsKeyEvents", "()Z",
        (void*)nativeFocusNodeWantsKeyEvents },
    { "nativeUpdateCachedTextfield", "(Ljava/lang/String;I)V",
        (void*) nativeUpdateCachedTextfield },
    { "nativeMoveSelection", "(IIZ)V",
        (void*) nativeMoveSelection },
    { "nativeGetSelection", "()Landroid/graphics/Region;",
        (void*) nativeGetSelection },
    { "nativeDumpDisplayTree", "(Ljava/lang/String;)V",
        (void*) nativeDumpDisplayTree }
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
