/*
 * Copyright 2009,2010 The Android Open Source Project
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

#ifndef MediaPlayerPrivateAndroid_h
#define MediaPlayerPrivateAndroid_h

#if ENABLE(VIDEO)

class SkBitmap;

#include "MediaPlayerPrivate.h"
#include "TimeRanges.h"

namespace WebCore {

class MediaPlayerPrivate : public MediaPlayerPrivateInterface {
public:
    virtual ~MediaPlayerPrivate();

    static void registerMediaEngine(MediaEngineRegistrar);

    virtual void load(const String& url) = 0;
    virtual void cancelLoad() { }

    virtual void play() = 0;
    virtual void pause();

    virtual IntSize naturalSize() const { return m_naturalSize; }

    virtual bool hasAudio() const { return false; }
    virtual bool hasVideo() const { return false; }

    virtual void setVisible(bool);

    virtual float duration() const { return m_duration; }

    virtual float currentTime() const { return m_currentTime; };
    virtual void seek(float time);
    virtual bool seeking() const { return false; }

    virtual void setEndTime(float time) { }

    virtual void setRate(float) { }
    virtual bool paused() const { return m_paused; }

    virtual void setVolume(float) { }

    virtual MediaPlayer::NetworkState networkState() const { return m_networkState; }
    virtual MediaPlayer::ReadyState readyState() const { return m_readyState; }

    virtual float maxTimeSeekable() const { return 0; }
    virtual PassRefPtr<TimeRanges> buffered() const { return TimeRanges::create(); }

    virtual int dataRate() const { return 0; }

    virtual bool totalBytesKnown() const { return totalBytes() > 0; }
    virtual unsigned totalBytes() const { return 0; }
    virtual unsigned bytesLoaded() const { return 0; }

    virtual void setSize(const IntSize&) { }

    virtual bool canLoadPoster() const { return false; }
    virtual void setPoster(const String&) { }
    virtual void prepareToPlay();

    virtual void paint(GraphicsContext*, const IntRect&) { }

    virtual void onPrepared(int duration, int width, int height) { }
    void onEnded();
    void onPaused();
    virtual void onPosterFetched(SkBitmap*) { }
    void onBuffering(int percent);
    void onTimeupdate(int position);
protected:
    // Android-specific methods and fields.
    static MediaPlayerPrivateInterface* create(MediaPlayer* player);
    static void getSupportedTypes(HashSet<String>&) { }
    static MediaPlayer::SupportsType supportsType(const String& type, const String& codecs);

    MediaPlayerPrivate(MediaPlayer *);
    virtual void createJavaPlayerIfNeeded() { }

    MediaPlayer* m_player;
    String m_url;
    struct JavaGlue;
    JavaGlue* m_glue;

    float m_duration;
    float m_currentTime;

    bool m_paused;
    bool m_hasVideo;
    MediaPlayer::ReadyState m_readyState;
    MediaPlayer::NetworkState m_networkState;

    SkBitmap* m_poster;  // not owned
    String m_posterUrl;

    IntSize m_naturalSize;
    bool m_naturalSizeUnknown;

    bool m_isVisible;
};

}  // namespace WebCore

#endif

#endif // MediaPlayerPrivateAndroid_h
