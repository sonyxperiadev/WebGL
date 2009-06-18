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

namespace WebCore {

void MediaPlayerPrivate::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(create, getSupportedTypes, supportsType);
}

void MediaPlayerPrivate::load(const String& url)
{
}

void MediaPlayerPrivate::cancelLoad()
{
}

void MediaPlayerPrivate::play()
{
}

void MediaPlayerPrivate::pause()
{
}

IntSize MediaPlayerPrivate::naturalSize() const
{
    return IntSize();
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
    return 0;
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
    return false;
}

void MediaPlayerPrivate::setVolume(float)
{
}

MediaPlayer::NetworkState MediaPlayerPrivate::networkState() const
{
    return MediaPlayer::Empty;
}

MediaPlayer::ReadyState MediaPlayerPrivate::readyState() const
{
    return MediaPlayer::HaveNothing;
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

void MediaPlayerPrivate::paint(GraphicsContext*, const IntRect&)
{
}

MediaPlayerPrivateInterface* MediaPlayerPrivate::create(MediaPlayer* player)
{
    return new MediaPlayerPrivate();
}

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>&)
{
}

MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(const String& type, const String& codecs)
{
    return MediaPlayer::IsNotSupported;
}

}

#endif // VIDEO
