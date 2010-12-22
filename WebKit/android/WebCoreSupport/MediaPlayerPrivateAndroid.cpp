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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaPlayerPrivateAndroid.h"

#if ENABLE(VIDEO)

#include "GraphicsContext.h"
#include "SkiaUtils.h"
#include "WebCoreJni.h"
#include "WebViewCore.h"

#include <GraphicsJNI.h>
#include <JNIHelp.h>
#include <JNIUtility.h>
#include <SkBitmap.h>

using namespace android;

namespace WebCore {

static const char* g_ProxyJavaClass = "android/webkit/HTML5VideoViewProxy";
static const char* g_ProxyJavaClassAudio = "android/webkit/HTML5Audio";

struct MediaPlayerPrivate::JavaGlue
{
    jobject   m_javaProxy;
    jmethodID m_play;
    jmethodID m_teardown;
    jmethodID m_seek;
    jmethodID m_pause;
    // Audio
    jmethodID m_newInstance;
    jmethodID m_setDataSource;
    jmethodID m_getMaxTimeSeekable;
    // Video
    jmethodID m_getInstance;
    jmethodID m_loadPoster;
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

void MediaPlayerPrivate::pause()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue->m_javaProxy || !m_url.length())
        return;

    m_paused = true;
    env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_pause);
    checkException(env);
}

void MediaPlayerPrivate::setVisible(bool visible)
{
    m_isVisible = visible;
    if (m_isVisible)
        createJavaPlayerIfNeeded();
}

void MediaPlayerPrivate::seek(float time)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_url.length())
        return;

    if (m_glue->m_javaProxy) {
        env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_seek, static_cast<jint>(time * 1000.0f));
        m_currentTime = time;
    }
    checkException(env);
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

MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(const String& type, const String& codecs)
{
    if (WebViewCore::supportsMimeType(type))
        return MediaPlayer::MayBeSupported;
    return MediaPlayer::IsNotSupported;
}

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_player(player),
    m_glue(0),
    m_duration(6000),
    m_currentTime(0),
    m_paused(true),
    m_hasVideo(false),
    m_readyState(MediaPlayer::HaveNothing),
    m_networkState(MediaPlayer::Empty),
    m_poster(0),
    m_naturalSize(100, 100),
    m_naturalSizeUnknown(true),
    m_isVisible(false)
{
}

void MediaPlayerPrivate::onEnded() {
    m_currentTime = duration();
    m_player->timeChanged();
    m_paused = true;
    m_currentTime = 0;
    m_hasVideo = false;
    m_networkState = MediaPlayer::Idle;
    m_readyState = MediaPlayer::HaveNothing;
}

void MediaPlayerPrivate::onPaused() {
    m_paused = true;
    m_currentTime = 0;
    m_hasVideo = false;
    m_networkState = MediaPlayer::Idle;
    m_readyState = MediaPlayer::HaveNothing;
}

void MediaPlayerPrivate::onTimeupdate(int position) {
    m_currentTime = position / 1000.0f;
    m_player->timeChanged();
}

class MediaPlayerVideoPrivate : public MediaPlayerPrivate {
public:
    void load(const String& url) { m_url = url; }
    void play() {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env || !m_url.length() || !m_glue->m_javaProxy)
            return;

        m_paused = false;
        jstring jUrl = env->NewString((unsigned short *)m_url.characters(), m_url.length());
        env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_play, jUrl);
        env->DeleteLocalRef(jUrl);

        checkException(env);
    }
    bool canLoadPoster() const { return true; }
    void setPoster(const String& url) {
        m_posterUrl = url;
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env || !m_glue->m_javaProxy || !m_posterUrl.length())
            return;
        // Send the poster
        jstring jUrl = env->NewString((unsigned short *)m_posterUrl.characters(), m_posterUrl.length());
        env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_loadPoster, jUrl);
        env->DeleteLocalRef(jUrl);
    }
    void paint(GraphicsContext* ctxt, const IntRect& r) {
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

    void onPosterFetched(SkBitmap* poster) {
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

    void onPrepared(int duration, int width, int height) {
        m_duration = duration / 1000.0f;
        m_naturalSize = IntSize(width, height);
        m_naturalSizeUnknown = false;
        m_hasVideo = true;
        m_player->durationChanged();
        m_player->sizeChanged();
    }

    bool hasAudio() { return false; } // do not display the audio UI
    bool hasVideo() { return m_hasVideo; }

    MediaPlayerVideoPrivate(MediaPlayer* player) : MediaPlayerPrivate(player) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env)
            return;

        jclass clazz = env->FindClass(g_ProxyJavaClass);

        if (!clazz)
            return;

        m_glue = new JavaGlue;
        m_glue->m_getInstance = env->GetStaticMethodID(clazz, "getInstance", "(Landroid/webkit/WebViewCore;I)Landroid/webkit/HTML5VideoViewProxy;");
        m_glue->m_loadPoster = env->GetMethodID(clazz, "loadPoster", "(Ljava/lang/String;)V");
        m_glue->m_play = env->GetMethodID(clazz, "play", "(Ljava/lang/String;)V");

        m_glue->m_teardown = env->GetMethodID(clazz, "teardown", "()V");
        m_glue->m_seek = env->GetMethodID(clazz, "seek", "(I)V");
        m_glue->m_pause = env->GetMethodID(clazz, "pause", "()V");
        m_glue->m_javaProxy = NULL;
        env->DeleteLocalRef(clazz);
        // An exception is raised if any of the above fails.
        checkException(env);
    }

    void createJavaPlayerIfNeeded() {
        // Check if we have been already created.
        if (m_glue->m_javaProxy)
            return;

        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env)
            return;

        jclass clazz = env->FindClass(g_ProxyJavaClass);

        if (!clazz)
            return;

        jobject obj = NULL;

        FrameView* frameView = m_player->frameView();
        if (!frameView)
            return;
        WebViewCore* webViewCore =  WebViewCore::getWebViewCore(frameView);
        ASSERT(webViewCore);

        // Get the HTML5VideoViewProxy instance
        obj = env->CallStaticObjectMethod(clazz, m_glue->m_getInstance, webViewCore->getJavaObject().get(), this);
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
        if (obj)
            env->DeleteLocalRef(obj);
        env->DeleteLocalRef(clazz);
        checkException(env);
    }

    float maxTimeSeekable() const {
        return m_duration;
    }
};

class MediaPlayerAudioPrivate : public MediaPlayerPrivate {
public:
    void load(const String& url) {
        m_url = url;
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env || !m_url.length())
            return;

        createJavaPlayerIfNeeded();

        if (!m_glue->m_javaProxy)
            return;

        jstring jUrl = env->NewString((unsigned short *)m_url.characters(), m_url.length());
        // start loading the data asynchronously
        env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_setDataSource, jUrl);
        env->DeleteLocalRef(jUrl);
        checkException(env);
    }

    void play() {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env || !m_url.length())
            return;

        createJavaPlayerIfNeeded();

        if (!m_glue->m_javaProxy)
            return;

        m_paused = false;
        env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_play);
        checkException(env);
    }

    bool hasAudio() { return true; }

    float maxTimeSeekable() const {
        if (m_glue->m_javaProxy) {
            JNIEnv* env = JSC::Bindings::getJNIEnv();
            if (env) {
                float maxTime = env->CallFloatMethod(m_glue->m_javaProxy,
                                                     m_glue->m_getMaxTimeSeekable);
                checkException(env);
                return maxTime;
            }
        }
        return 0;
    }

    MediaPlayerAudioPrivate(MediaPlayer* player) : MediaPlayerPrivate(player) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env)
            return;

        jclass clazz = env->FindClass(g_ProxyJavaClassAudio);

        if (!clazz)
            return;

        m_glue = new JavaGlue;
        m_glue->m_newInstance = env->GetMethodID(clazz, "<init>", "(I)V");
        m_glue->m_setDataSource = env->GetMethodID(clazz, "setDataSource", "(Ljava/lang/String;)V");
        m_glue->m_play = env->GetMethodID(clazz, "play", "()V");
        m_glue->m_getMaxTimeSeekable = env->GetMethodID(clazz, "getMaxTimeSeekable", "()F");
        m_glue->m_teardown = env->GetMethodID(clazz, "teardown", "()V");
        m_glue->m_seek = env->GetMethodID(clazz, "seek", "(I)V");
        m_glue->m_pause = env->GetMethodID(clazz, "pause", "()V");
        m_glue->m_javaProxy = NULL;
        env->DeleteLocalRef(clazz);
        // An exception is raised if any of the above fails.
        checkException(env);
    }

    void createJavaPlayerIfNeeded() {
        // Check if we have been already created.
        if (m_glue->m_javaProxy)
            return;

        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env)
            return;

        jclass clazz = env->FindClass(g_ProxyJavaClassAudio);

        if (!clazz)
            return;

        jobject obj = NULL;

        // Get the HTML5Audio instance
        obj = env->NewObject(clazz, m_glue->m_newInstance, this);
        m_glue->m_javaProxy = env->NewGlobalRef(obj);

        // Clean up.
        if (obj)
            env->DeleteLocalRef(obj);
        env->DeleteLocalRef(clazz);
        checkException(env);
    }

    void onPrepared(int duration, int width, int height) {
        m_duration = duration / 1000.0f;
        m_player->durationChanged();
        m_player->sizeChanged();
        m_player->prepareToPlay();
    }
};

MediaPlayerPrivateInterface* MediaPlayerPrivate::create(MediaPlayer* player)
{
    if (player->mediaElementType() == MediaPlayer::Video)
       return new MediaPlayerVideoPrivate(player);
    return new MediaPlayerAudioPrivate(player);
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

static void OnPaused(JNIEnv* env, jobject obj, int pointer) {
    if (pointer) {
        WebCore::MediaPlayerPrivate* player = reinterpret_cast<WebCore::MediaPlayerPrivate*>(pointer);
        player->onPaused();
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

static void OnBuffering(JNIEnv* env, jobject obj, int percent, int pointer) {
    if (pointer) {
        WebCore::MediaPlayerPrivate* player = reinterpret_cast<WebCore::MediaPlayerPrivate*>(pointer);
        //TODO: player->onBuffering(percent);
    }
}

static void OnTimeupdate(JNIEnv* env, jobject obj, int position, int pointer) {
    if (pointer) {
        WebCore::MediaPlayerPrivate* player = reinterpret_cast<WebCore::MediaPlayerPrivate*>(pointer);
        player->onTimeupdate(position);
    }
}

/*
 * JNI registration
 */
static JNINativeMethod g_MediaPlayerMethods[] = {
    { "nativeOnPrepared", "(IIII)V",
        (void*) OnPrepared },
    { "nativeOnEnded", "(I)V",
        (void*) OnEnded },
    { "nativeOnPaused", "(I)V",
        (void*) OnPaused },
    { "nativeOnPosterFetched", "(Landroid/graphics/Bitmap;I)V",
        (void*) OnPosterFetched },
    { "nativeOnTimeupdate", "(II)V",
        (void*) OnTimeupdate },
};

static JNINativeMethod g_MediaAudioPlayerMethods[] = {
    { "nativeOnBuffering", "(II)V",
        (void*) OnBuffering },
    { "nativeOnEnded", "(I)V",
        (void*) OnEnded },
    { "nativeOnPrepared", "(IIII)V",
        (void*) OnPrepared },
    { "nativeOnTimeupdate", "(II)V",
        (void*) OnTimeupdate },
};

int register_mediaplayer_video(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, g_ProxyJavaClass,
            g_MediaPlayerMethods, NELEM(g_MediaPlayerMethods));
}

int register_mediaplayer_audio(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, g_ProxyJavaClassAudio,
            g_MediaAudioPlayerMethods, NELEM(g_MediaAudioPlayerMethods));
}

}
#endif // VIDEO
