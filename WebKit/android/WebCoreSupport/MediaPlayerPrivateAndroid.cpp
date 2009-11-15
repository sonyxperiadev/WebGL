/*
 * Copyright 2009, The Android Open Source Project
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

#include "config.h"

#if ENABLE(VIDEO)

#include "GraphicsContext.h"
#include "MediaPlayerPrivateAndroid.h"
#include "SkiaUtils.h"
#include "WebCoreJni.h"
#include "WebViewCore.h"
#include "jni_utility.h"

#include <GraphicsJNI.h>
#include <JNIHelp.h>
#include <SkBitmap.h>

using namespace android;

namespace WebCore {

static const char* g_ProxyJavaClass = "android/webkit/HTML5VideoViewProxy";

struct MediaPlayerPrivate::JavaGlue
{
    jobject   m_javaProxy;
    jmethodID m_getInstance;
    jmethodID m_play;
    jmethodID m_teardown;
    jmethodID m_loadPoster;
    jmethodID m_seek;
    jmethodID m_pause;
};

MediaPlayerPrivate::~MediaPlayerPrivate()
{
    if (m_glue->m_javaProxy) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (env) {
            env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_teardown);
            env->DeleteGlobalRef(m_glue->m_javaProxy);
        }
    }

    delete m_glue;
}

void MediaPlayerPrivate::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(create, getSupportedTypes, supportsType);
}

void MediaPlayerPrivate::load(const String& url)
{
    // Just save the URl.
    m_url = url;
}

void MediaPlayerPrivate::cancelLoad()
{
}

void MediaPlayerPrivate::play()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue->m_javaProxy || !m_url.length())
        return;

    m_paused = false;
    jstring jUrl = env->NewString((unsigned short *)m_url.characters(), m_url.length());
    env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_play, jUrl);
    env->DeleteLocalRef(jUrl);
    checkException(env);
}

void MediaPlayerPrivate::pause()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue->m_javaProxy || !m_url.length())
        return;

    m_paused = true;
    env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_pause);
    checkException(env);
}

IntSize MediaPlayerPrivate::naturalSize() const
{
    return m_naturalSize;
}

bool MediaPlayerPrivate::hasVideo() const
{
    return false;
}

void MediaPlayerPrivate::setVisible(bool visible)
{
    m_isVisible = visible;
    if (m_isVisible)
        createJavaPlayerIfNeeded();
}

float MediaPlayerPrivate::duration() const
{
    return m_duration;
}

float MediaPlayerPrivate::currentTime() const
{
    return m_currentTime;
}

void MediaPlayerPrivate::seek(float time)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue->m_javaProxy || !m_url.length())
        return;

    m_currentTime = time;
    env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_seek, static_cast<jint>(time * 1000.0f));
    checkException(env);
}

bool MediaPlayerPrivate::seeking() const
{
    return false;
}

void MediaPlayerPrivate::setEndTime(float)
{
}

void MediaPlayerPrivate::setRate(float)
{
}

bool MediaPlayerPrivate::paused() const
{
    return m_paused;
}

void MediaPlayerPrivate::setVolume(float)
{
}

MediaPlayer::NetworkState MediaPlayerPrivate::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivate::readyState() const
{
    return m_readyState;
}

float MediaPlayerPrivate::maxTimeSeekable() const
{
    return 0;
}

float MediaPlayerPrivate::maxTimeBuffered() const
{
    return 0;
}

int MediaPlayerPrivate::dataRate() const
{
    return 0;
}

unsigned MediaPlayerPrivate::totalBytes() const
{
    return 0;
}

unsigned MediaPlayerPrivate::bytesLoaded() const
{
    return 0;
}

void MediaPlayerPrivate::setSize(const IntSize&)
{
}

void MediaPlayerPrivate::setPoster(const String& url)
{
    m_posterUrl = url;
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue->m_javaProxy || !m_posterUrl.length())
        return;
    // Send the poster
    jstring jUrl = env->NewString((unsigned short *)m_posterUrl.characters(), m_posterUrl.length());
    env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_loadPoster, jUrl);
    env->DeleteLocalRef(jUrl);
}

void MediaPlayerPrivate::prepareToPlay() {
    // We are about to start playing. Since our Java VideoView cannot
    // buffer any data, we just simply transition to the HaveEnoughData
    // state in here. This will allow the MediaPlayer to transition to
    // the "play" state, at which point our VideoView will start downloading
    // the content and start the playback.
    m_networkState = MediaPlayer::Loaded;
    m_player->networkStateChanged();
    m_readyState = MediaPlayer::HaveEnoughData;
    m_player->readyStateChanged();
}

void MediaPlayerPrivate::paint(GraphicsContext* ctxt, const IntRect& r)
{
    if (ctxt->paintingDisabled())
        return;

    if (!m_isVisible)
        return;

    if (!m_poster || (!m_poster->getPixels() && !m_poster->pixelRef()))
        return;

    SkCanvas*   canvas = ctxt->platformContext()->mCanvas;
    // We paint with the following rules in mind:
    // - only downscale the poster, never upscale
    // - maintain the natural aspect ratio of the poster
    // - the poster should be centered in the target rect
    float originalRatio = static_cast<float>(m_poster->width()) / static_cast<float>(m_poster->height());
    int posterWidth = r.width() > m_poster->width() ? m_poster->width() : r.width();
    int posterHeight = posterWidth / originalRatio;
    int posterX = ((r.width() - posterWidth) / 2) + r.x();
    int posterY = ((r.height() - posterHeight) / 2) + r.y();
    IntRect targetRect(posterX, posterY, posterWidth, posterHeight);
    canvas->drawBitmapRect(*m_poster, 0, targetRect, 0);
}

MediaPlayerPrivateInterface* MediaPlayerPrivate::create(MediaPlayer* player)
{
    return new MediaPlayerPrivate(player);
}

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>&)
{
}

MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(const String& type, const String& codecs)
{
    return MediaPlayer::IsNotSupported;
}

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_player(player),
    m_glue(0),
    m_duration(6000),
    m_currentTime(0),
    m_paused(true),
    m_readyState(MediaPlayer::HaveNothing),
    m_networkState(MediaPlayer::Empty),
    m_poster(0),
    m_naturalSize(100, 100),
    m_naturalSizeUnknown(true),
    m_isVisible(false)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env)
        return;

    jclass clazz = env->FindClass(g_ProxyJavaClass);
    if (!clazz)
        return;

    m_glue = new JavaGlue;
    m_glue->m_getInstance = env->GetStaticMethodID(clazz, "getInstance", "(Landroid/webkit/WebViewCore;I)Landroid/webkit/HTML5VideoViewProxy;");
    m_glue->m_play = env->GetMethodID(clazz, "play", "(Ljava/lang/String;)V");
    m_glue->m_teardown = env->GetMethodID(clazz, "teardown", "()V");
    m_glue->m_loadPoster = env->GetMethodID(clazz, "loadPoster", "(Ljava/lang/String;)V");
    m_glue->m_seek = env->GetMethodID(clazz, "seek", "(I)V");
    m_glue->m_pause = env->GetMethodID(clazz, "pause", "()V");
    m_glue->m_javaProxy = NULL;
    env->DeleteLocalRef(clazz);
    // An exception is raised if any of the above fails.
    checkException(env);
}

void MediaPlayerPrivate::createJavaPlayerIfNeeded()
{
    // Check if we have been already created.
    if (m_glue->m_javaProxy)
        return;

    FrameView* frameView = m_player->frameView();
    if (!frameView)
        return;

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env)
        return;

    jclass clazz = env->FindClass(g_ProxyJavaClass);
    if (!clazz)
        return;

    WebViewCore* webViewCore =  WebViewCore::getWebViewCore(frameView);
    ASSERT(webViewCore);

    // Get the HTML5VideoViewProxy instance
    jobject obj = env->CallStaticObjectMethod(clazz, m_glue->m_getInstance, webViewCore->getJavaObject().get(), this);
    m_glue->m_javaProxy = env->NewGlobalRef(obj);
    // Send the poster
    jstring jUrl = 0;
    if (m_posterUrl.length())
        jUrl = env->NewString((unsigned short *)m_posterUrl.characters(), m_posterUrl.length());
    // Sending a NULL jUrl allows the Java side to try to load the default poster.
    env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_loadPoster, jUrl);
    if (jUrl)
        env->DeleteLocalRef(jUrl);
    // Clean up.
    env->DeleteLocalRef(obj);
    env->DeleteLocalRef(clazz);
    checkException(env);
}

void MediaPlayerPrivate::onPrepared(int duration, int width, int height) {
    m_duration = duration / 1000.0f;
    m_naturalSize = IntSize(width, height);
    m_naturalSizeUnknown = false;
    m_player->durationChanged();
    m_player->sizeChanged();
}

void MediaPlayerPrivate::onEnded() {
    m_paused = true;
    m_currentTime = 0;
    m_networkState = MediaPlayer::Idle;
    m_readyState = MediaPlayer::HaveNothing;
}

void MediaPlayerPrivate::onPosterFetched(SkBitmap* poster) {
    m_poster = poster;
    if (m_naturalSizeUnknown) {
        // We had to fake the size at startup, or else our paint
        // method would not be called. If we haven't yet received
        // the onPrepared event, update the intrinsic size to the size
        // of the poster. That will be overriden when onPrepare comes.
        // In case of an error, we should report the poster size, rather
        // than our initial fake value.
        m_naturalSize = IntSize(poster->width(), poster->height());
        m_player->sizeChanged();
    }
}

}

namespace android {

static void OnPrepared(JNIEnv* env, jobject obj, int duration, int width, int height, int pointer) {
    if (pointer) {
        WebCore::MediaPlayerPrivate* player = reinterpret_cast<WebCore::MediaPlayerPrivate*>(pointer);
        player->onPrepared(duration, width, height);
    }
}

static void OnEnded(JNIEnv* env, jobject obj, int pointer) {
    if (pointer) {
        WebCore::MediaPlayerPrivate* player = reinterpret_cast<WebCore::MediaPlayerPrivate*>(pointer);
        player->onEnded();
    }
}

static void OnPosterFetched(JNIEnv* env, jobject obj, jobject poster, int pointer) {
    if (!pointer || !poster)
        return;

    WebCore::MediaPlayerPrivate* player = reinterpret_cast<WebCore::MediaPlayerPrivate*>(pointer);
    SkBitmap* posterNative = GraphicsJNI::getNativeBitmap(env, poster);
    if (!posterNative)
        return;
    player->onPosterFetched(posterNative);
}

/*
 * JNI registration
 */
static JNINativeMethod g_MediaPlayerMethods[] = {
    { "nativeOnPrepared", "(IIII)V",
        (void*) OnPrepared },
    { "nativeOnEnded", "(I)V",
        (void*) OnEnded },
    { "nativeOnPosterFetched", "(Landroid/graphics/Bitmap;I)V",
        (void*) OnPosterFetched },
};

int register_mediaplayer(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, g_ProxyJavaClass,
            g_MediaPlayerMethods, NELEM(g_MediaPlayerMethods));
}

}
#endif // VIDEO
