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

#include "BaseLayerAndroid.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "SkiaUtils.h"
#include "TilesManager.h"
#include "VideoLayerAndroid.h"
#include "WebCoreJni.h"
#include "WebViewCore.h"
#include <GraphicsJNI.h>
#include <JNIHelp.h>
#include <JNIUtility.h>
#include <SkBitmap.h>
#include <gui/SurfaceTexture.h>

using namespace android;
// Forward decl
namespace android {
sp<SurfaceTexture> SurfaceTexture_getSurfaceTexture(JNIEnv* env, jobject thiz);
};

namespace WebCore {

static const char* g_ProxyJavaClass = "android/webkit/HTML5VideoViewProxy";
static const char* g_ProxyJavaClassAudio = "android/webkit/HTML5Audio";

struct MediaPlayerPrivate::JavaGlue {
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
    TilesManager::instance()->videoLayerManager()->removeLayer(m_videoLayer->uniqueId());
    // m_videoLayer is reference counted, unref is enough here.
    m_videoLayer->unref();
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
    registrar(create, getSupportedTypes, supportsType, 0, 0, 0);
}

MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(const String& type, const String& codecs)
{
    if (WebViewCore::isSupportedMediaMimeType(type))
        return MediaPlayer::MayBeSupported;
    return MediaPlayer::IsNotSupported;
}

void MediaPlayerPrivate::pause()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue->m_javaProxy || !m_url.length())
        return;

    m_paused = true;
    m_player->playbackStateChanged();
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

void MediaPlayerPrivate::prepareToPlay()
{
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

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_player(player),
    m_glue(0),
    m_duration(1), // keep this minimal to avoid initial seek problem
    m_currentTime(0),
    m_paused(true),
    m_readyState(MediaPlayer::HaveNothing),
    m_networkState(MediaPlayer::Empty),
    m_poster(0),
    m_naturalSize(100, 100),
    m_naturalSizeUnknown(true),
    m_isVisible(false),
    m_videoLayer(new VideoLayerAndroid())
{
}

void MediaPlayerPrivate::onEnded()
{
    m_currentTime = duration();
    m_player->timeChanged();
    m_paused = true;
    m_player->playbackStateChanged();
    m_networkState = MediaPlayer::Idle;
}

void MediaPlayerPrivate::onPaused()
{
    m_paused = true;
    m_player->playbackStateChanged();
    m_networkState = MediaPlayer::Idle;
    m_player->playbackStateChanged();
}

void MediaPlayerPrivate::onTimeupdate(int position)
{
    m_currentTime = position / 1000.0f;
    m_player->timeChanged();
}

void MediaPlayerPrivate::onStopFullscreen()
{
    if (m_player && m_player->mediaPlayerClient()
        && m_player->mediaPlayerClient()->mediaPlayerOwningDocument()) {
        m_player->mediaPlayerClient()->mediaPlayerOwningDocument()->webkitCancelFullScreen();
    }
}

class MediaPlayerVideoPrivate : public MediaPlayerPrivate {
public:
    void load(const String& url)
    {
        m_url = url;
        // Cheat a bit here to make sure Window.onLoad event can be triggered
        // at the right time instead of real video play time, since only full
        // screen video play is supported in Java's VideoView.
        // See also comments in prepareToPlay function.
        m_networkState = MediaPlayer::Loading;
        m_player->networkStateChanged();
        m_readyState = MediaPlayer::HaveCurrentData;
        m_player->readyStateChanged();
    }

    void play()
    {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env || !m_url.length() || !m_glue->m_javaProxy)
            return;

        // We only play video fullscreen on Android, so stop sites playing fullscreen video in the onload handler.
        Frame* frame = m_player->frameView()->frame();
        if (frame && !frame->loader()->documentLoader()->wasOnloadHandled())
            return;

        m_paused = false;
        m_player->playbackStateChanged();

        if (m_currentTime == duration())
            m_currentTime = 0;

        jstring jUrl = wtfStringToJstring(env, m_url);
        env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_play, jUrl,
                            static_cast<jint>(m_currentTime * 1000.0f),
                            m_videoLayer->uniqueId());
        env->DeleteLocalRef(jUrl);

        checkException(env);
    }
    bool canLoadPoster() const { return true; }
    void setPoster(const String& url)
    {
        if (m_posterUrl == url)
            return;

        m_posterUrl = url;
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env || !m_glue->m_javaProxy || !m_posterUrl.length())
            return;
        // Send the poster
        jstring jUrl = wtfStringToJstring(env, m_posterUrl);
        env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_loadPoster, jUrl);
        env->DeleteLocalRef(jUrl);
    }
    void paint(GraphicsContext* ctxt, const IntRect& r)
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

    void onPosterFetched(SkBitmap* poster)
    {
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

    void onPrepared(int duration, int width, int height)
    {
        m_duration = duration / 1000.0f;
        m_naturalSize = IntSize(width, height);
        m_naturalSizeUnknown = false;
        m_player->durationChanged();
        m_player->sizeChanged();
        TilesManager::instance()->videoLayerManager()->updateVideoLayerSize(
            m_player->platformLayer()->uniqueId(), width*height);
    }

    virtual bool hasAudio() const { return false; } // do not display the audio UI
    virtual bool hasVideo() const { return true; }
    virtual bool supportsFullscreen() const { return true; }

    MediaPlayerVideoPrivate(MediaPlayer* player) : MediaPlayerPrivate(player)
    {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env)
            return;

        jclass clazz = env->FindClass(g_ProxyJavaClass);

        if (!clazz)
            return;

        m_glue = new JavaGlue;
        m_glue->m_getInstance = env->GetStaticMethodID(clazz, "getInstance", "(Landroid/webkit/WebViewCore;I)Landroid/webkit/HTML5VideoViewProxy;");
        m_glue->m_loadPoster = env->GetMethodID(clazz, "loadPoster", "(Ljava/lang/String;)V");
        m_glue->m_play = env->GetMethodID(clazz, "play", "(Ljava/lang/String;II)V");

        m_glue->m_teardown = env->GetMethodID(clazz, "teardown", "()V");
        m_glue->m_seek = env->GetMethodID(clazz, "seek", "(I)V");
        m_glue->m_pause = env->GetMethodID(clazz, "pause", "()V");
        m_glue->m_javaProxy = 0;
        env->DeleteLocalRef(clazz);
        // An exception is raised if any of the above fails.
        checkException(env);
    }

    void createJavaPlayerIfNeeded()
    {
        // Check if we have been already created.
        if (m_glue->m_javaProxy)
            return;

        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env)
            return;

        jclass clazz = env->FindClass(g_ProxyJavaClass);

        if (!clazz)
            return;

        jobject obj = 0;

        FrameView* frameView = m_player->frameView();
        if (!frameView)
            return;
        AutoJObject javaObject = WebViewCore::getWebViewCore(frameView)->getJavaObject();
        if (!javaObject.get())
            return;

        // Get the HTML5VideoViewProxy instance
        obj = env->CallStaticObjectMethod(clazz, m_glue->m_getInstance, javaObject.get(), this);
        m_glue->m_javaProxy = env->NewGlobalRef(obj);
        // Send the poster
        jstring jUrl = 0;
        if (m_posterUrl.length())
            jUrl = wtfStringToJstring(env, m_posterUrl);
        // Sending a NULL jUrl allows the Java side to try to load the default poster.
        env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_loadPoster, jUrl);
        if (jUrl)
            env->DeleteLocalRef(jUrl);

        // Clean up.
        env->DeleteLocalRef(obj);
        env->DeleteLocalRef(clazz);
        checkException(env);
    }

    float maxTimeSeekable() const
    {
        return m_duration;
    }
};

class MediaPlayerAudioPrivate : public MediaPlayerPrivate {
public:
    void load(const String& url)
    {
        m_url = url;
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env || !m_url.length())
            return;

        createJavaPlayerIfNeeded();

        if (!m_glue->m_javaProxy)
            return;

        jstring jUrl = wtfStringToJstring(env, m_url);
        // start loading the data asynchronously
        env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_setDataSource, jUrl);
        env->DeleteLocalRef(jUrl);
        checkException(env);
    }

    void play()
    {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env || !m_url.length())
            return;

        createJavaPlayerIfNeeded();

        if (!m_glue->m_javaProxy)
            return;

        m_paused = false;
        m_player->playbackStateChanged();
        env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_play);
        checkException(env);
    }

    virtual bool hasAudio() const { return true; }
    virtual bool hasVideo() const { return false; }
    virtual bool supportsFullscreen() const { return false; }

    float maxTimeSeekable() const
    {
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

    MediaPlayerAudioPrivate(MediaPlayer* player) : MediaPlayerPrivate(player)
    {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env)
            return;

        jclass clazz = env->FindClass(g_ProxyJavaClassAudio);

        if (!clazz)
            return;

        m_glue = new JavaGlue;
        m_glue->m_newInstance = env->GetMethodID(clazz, "<init>", "(Landroid/webkit/WebViewCore;I)V");
        m_glue->m_setDataSource = env->GetMethodID(clazz, "setDataSource", "(Ljava/lang/String;)V");
        m_glue->m_play = env->GetMethodID(clazz, "play", "()V");
        m_glue->m_getMaxTimeSeekable = env->GetMethodID(clazz, "getMaxTimeSeekable", "()F");
        m_glue->m_teardown = env->GetMethodID(clazz, "teardown", "()V");
        m_glue->m_seek = env->GetMethodID(clazz, "seek", "(I)V");
        m_glue->m_pause = env->GetMethodID(clazz, "pause", "()V");
        m_glue->m_javaProxy = 0;
        env->DeleteLocalRef(clazz);
        // An exception is raised if any of the above fails.
        checkException(env);
    }

    void createJavaPlayerIfNeeded()
    {
        // Check if we have been already created.
        if (m_glue->m_javaProxy)
            return;

        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env)
            return;

        jclass clazz = env->FindClass(g_ProxyJavaClassAudio);

        if (!clazz)
            return;

        FrameView* frameView = m_player->mediaPlayerClient()->mediaPlayerOwningDocument()->view();
        if (!frameView)
            return;
        AutoJObject javaObject = WebViewCore::getWebViewCore(frameView)->getJavaObject();
        if (!javaObject.get())
            return;

        jobject obj = 0;

        // Get the HTML5Audio instance
        obj = env->NewObject(clazz, m_glue->m_newInstance, javaObject.get(), this);
        m_glue->m_javaProxy = env->NewGlobalRef(obj);

        // Clean up.
        if (obj)
            env->DeleteLocalRef(obj);
        env->DeleteLocalRef(clazz);
        checkException(env);
    }

    void onPrepared(int duration, int width, int height)
    {
        // Android media player gives us a duration of 0 for a live
        // stream, so in that case set the real duration to infinity.
        // We'll still be able to handle the case that we genuinely
        // get an audio clip with a duration of 0s as we'll get the
        // ended event when it stops playing.
        if (duration > 0) {
            m_duration = duration / 1000.0f;
        } else {
            m_duration = std::numeric_limits<float>::infinity();
        }
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

static void OnPrepared(JNIEnv* env, jobject obj, int duration, int width, int height, int pointer)
{
    if (pointer) {
        WebCore::MediaPlayerPrivate* player = reinterpret_cast<WebCore::MediaPlayerPrivate*>(pointer);
        player->onPrepared(duration, width, height);
    }
}

static void OnEnded(JNIEnv* env, jobject obj, int pointer)
{
    if (pointer) {
        WebCore::MediaPlayerPrivate* player = reinterpret_cast<WebCore::MediaPlayerPrivate*>(pointer);
        player->onEnded();
    }
}

static void OnPaused(JNIEnv* env, jobject obj, int pointer)
{
    if (pointer) {
        WebCore::MediaPlayerPrivate* player = reinterpret_cast<WebCore::MediaPlayerPrivate*>(pointer);
        player->onPaused();
    }
}

static void OnPosterFetched(JNIEnv* env, jobject obj, jobject poster, int pointer)
{
    if (!pointer || !poster)
        return;

    WebCore::MediaPlayerPrivate* player = reinterpret_cast<WebCore::MediaPlayerPrivate*>(pointer);
    SkBitmap* posterNative = GraphicsJNI::getNativeBitmap(env, poster);
    if (!posterNative)
        return;
    player->onPosterFetched(posterNative);
}

static void OnBuffering(JNIEnv* env, jobject obj, int percent, int pointer)
{
    if (pointer) {
        WebCore::MediaPlayerPrivate* player = reinterpret_cast<WebCore::MediaPlayerPrivate*>(pointer);
        // TODO: player->onBuffering(percent);
    }
}

static void OnTimeupdate(JNIEnv* env, jobject obj, int position, int pointer)
{
    if (pointer) {
        WebCore::MediaPlayerPrivate* player = reinterpret_cast<WebCore::MediaPlayerPrivate*>(pointer);
        player->onTimeupdate(position);
    }
}

// This is called on the UI thread only.
// The video layers are composited on the webkit thread and then copied over
// to the UI thread with the same ID. For rendering, we are only using the
// video layers on the UI thread. Therefore, on the UI thread, we have to use
// the videoLayerId from Java side to find the exact video layer in the tree
// to set the surface texture.
// Every time a play call into Java side, the videoLayerId will be sent and
// saved in Java side. Then every time setBaseLayer call, the saved
// videoLayerId will be passed to this function to find the Video Layer.
// Return value: true when the video layer is found.
static bool SendSurfaceTexture(JNIEnv* env, jobject obj, jobject surfTex,
                               int baseLayer, int videoLayerId,
                               int textureName, int playerState) {
    if (!surfTex)
        return false;

    sp<SurfaceTexture> texture = android::SurfaceTexture_getSurfaceTexture(env, surfTex);
    if (!texture.get())
        return false;

    BaseLayerAndroid* layerImpl = reinterpret_cast<BaseLayerAndroid*>(baseLayer);
    if (!layerImpl)
        return false;
    if (!layerImpl->countChildren())
        return false;
    LayerAndroid* compositedRoot = static_cast<LayerAndroid*>(layerImpl->getChild(0));
    if (!compositedRoot)
        return false;

    VideoLayerAndroid* videoLayer =
        static_cast<VideoLayerAndroid*>(compositedRoot->findById(videoLayerId));
    if (!videoLayer)
        return false;

    // Set the SurfaceTexture to the layer we found
    videoLayer->setSurfaceTexture(texture, textureName, static_cast<PlayerState>(playerState));
    return true;
}

static void OnStopFullscreen(JNIEnv* env, jobject obj, int pointer)
{
    if (pointer) {
        WebCore::MediaPlayerPrivate* player =
            reinterpret_cast<WebCore::MediaPlayerPrivate*>(pointer);
        player->onStopFullscreen();
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
    { "nativeOnStopFullscreen", "(I)V",
        (void*) OnStopFullscreen },
    { "nativeOnPaused", "(I)V",
        (void*) OnPaused },
    { "nativeOnPosterFetched", "(Landroid/graphics/Bitmap;I)V",
        (void*) OnPosterFetched },
    { "nativeSendSurfaceTexture", "(Landroid/graphics/SurfaceTexture;IIII)Z",
        (void*) SendSurfaceTexture },
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

int registerMediaPlayerVideo(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, g_ProxyJavaClass,
            g_MediaPlayerMethods, NELEM(g_MediaPlayerMethods));
}

int registerMediaPlayerAudio(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, g_ProxyJavaClassAudio,
            g_MediaAudioPlayerMethods, NELEM(g_MediaAudioPlayerMethods));
}

}
#endif // VIDEO
