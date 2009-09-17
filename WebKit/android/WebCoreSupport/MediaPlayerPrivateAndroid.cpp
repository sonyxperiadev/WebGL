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

#include "MediaPlayerPrivateAndroid.h"
#include "WebCoreJni.h"
#include "WebViewCore.h"
#include "jni_utility.h"

#include <JNIHelp.h>
using namespace android;

namespace WebCore {

static const char* g_ProxyJavaClass = "android/webkit/HTML5VideoViewProxy";

struct MediaPlayerPrivate::JavaGlue
{
    jobject   m_javaProxy;
    jmethodID m_getInstance;
    jmethodID m_play;
    jmethodID m_createView;
    jmethodID m_attachView;
    jmethodID m_removeView;
    jmethodID m_setPoster;
};

MediaPlayerPrivate::~MediaPlayerPrivate()
{
    if (m_glue->m_javaProxy) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (env) {
            env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_removeView);
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
    // To be able to create our java player, we need a Context object. To get
    // the Context object, we need a WebViewCore java object. To get a java
    // WebViewCore object, we need a WebCore::FrameView pointer. To get
    // the FrameView pointer, the MediaPlayer::setFrameView() must have been
    // called. However, that method is called only after the MediaPlayerClient
    // is called back and informed that enough data has been loaded.
    // We therefore need to fake a readyStateChanged callback before creating
    // the java player.
    m_player->readyStateChanged();
    // We now have a RenderVideo created and the MediaPlayer must have
    // been updated with a FrameView. Create our JavaPlayer.
    createJavaPlayerIfNeeded();
    // Save the URl.
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

    FrameView* frameView = m_player->frameView();
    if (!frameView)
        return;

    jstring jUrl = env->NewString((unsigned short *)m_url.characters(), m_url.length());
    env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_play, jUrl);
    env->DeleteLocalRef(jUrl);
    checkException(env);
}

void MediaPlayerPrivate::pause()
{
}

IntSize MediaPlayerPrivate::naturalSize() const
{
    return IntSize(300, 150);
}

bool MediaPlayerPrivate::hasVideo() const
{
    return false;
}

void MediaPlayerPrivate::setVisible(bool)
{
}

float MediaPlayerPrivate::duration() const
{
    return 100;
}

float MediaPlayerPrivate::currentTime() const
{
    return 0;
}

void MediaPlayerPrivate::seek(float time)
{
}

bool MediaPlayerPrivate::seeking() const
{
    return false;
}

void MediaPlayerPrivate::setEndTime(float time)
{
}

void MediaPlayerPrivate::setRate(float)
{
}

bool MediaPlayerPrivate::paused() const
{
    return true;
}

void MediaPlayerPrivate::setVolume(float)
{
}

MediaPlayer::NetworkState MediaPlayerPrivate::networkState() const
{
    return MediaPlayer::Loaded;
}

MediaPlayer::ReadyState MediaPlayerPrivate::readyState() const
{
    return MediaPlayer::HaveEnoughData;
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
    JNIEnv* env = JSC::Bindings::getJNIEnv();
     if (!env)
         return;
     jstring jUrl = env->NewString((unsigned short *)url.characters(), url.length());
     env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_setPoster, jUrl);
     env->DeleteLocalRef(jUrl);
     checkException(env);
}

void MediaPlayerPrivate::paint(GraphicsContext*, const IntRect& r)
{
    createJavaPlayerIfNeeded();
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env)
        return;

    IntSize size = m_player->size();
    env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_attachView,
            r.x(), r.y(), size.width(), size.height());
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

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player) : m_player(player), m_glue(NULL)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env)
        return;

    jclass clazz = env->FindClass(g_ProxyJavaClass);
    if (!clazz)
        return;

    m_glue = new JavaGlue;
    m_glue->m_getInstance = env->GetStaticMethodID(clazz, "getInstance", "(Landroid/webkit/WebViewCore;)Landroid/webkit/HTML5VideoViewProxy;");
    m_glue->m_play = env->GetMethodID(clazz, "play", "(Ljava/lang/String;)V");
    m_glue->m_createView = env->GetMethodID(clazz, "createView", "()V");
    m_glue->m_attachView = env->GetMethodID(clazz, "attachView", "(IIII)V");
    m_glue->m_removeView = env->GetMethodID(clazz, "removeView", "()V");
    m_glue->m_setPoster = env->GetMethodID(clazz, "loadPoster", "(Ljava/lang/String;)V");
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
    jobject obj = env->CallStaticObjectMethod(clazz, m_glue->m_getInstance, webViewCore->getJavaObject().get());
    m_glue->m_javaProxy = env->NewGlobalRef(obj);
    // Create our VideoView object.
    env->CallVoidMethod(obj, m_glue->m_createView);
    // Clean up.
    env->DeleteLocalRef(obj);
    env->DeleteLocalRef(clazz);
    checkException(env);
}

}

#endif // VIDEO
