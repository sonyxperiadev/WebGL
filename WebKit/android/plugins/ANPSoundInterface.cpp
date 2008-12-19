/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// must include config.h first for webkit to fiddle with new/delete
#include "config.h"
#include "android_npapi.h"

#include "media/AudioTrack.h"

struct ANPAudioTrack {
    void*                mUser;
    ANPAudioCallbackProc mProc;
    android::AudioTrack* mTrack;
};

static ANPSampleFormat toANPFormat(int fm) {
    switch (fm) {
        case android::AudioSystem::PCM_16_BIT:
            return kPCM16Bit_ANPSampleFormat;
        case android::AudioSystem::PCM_8_BIT:
            return kPCM8Bit_ANPSampleFormat;
        default:
            return kUnknown_ANPSamleFormat;
    }
}

static android::AudioSystem::audio_format fromANPFormat(ANPSampleFormat fm) {
    switch (fm) {
        case kPCM16Bit_ANPSampleFormat:
            return android::AudioSystem::PCM_16_BIT;
        case kPCM8Bit_ANPSampleFormat:
            return android::AudioSystem::PCM_8_BIT;
        default:
            return android::AudioSystem::INVALID_FORMAT;
    }
}

static void callbackProc(int event, void* user, void* info) {
    ANPAudioTrack* track = reinterpret_cast<ANPAudioTrack*>(user);
    
    switch (event) {
        case android::AudioTrack::EVENT_MORE_DATA: {
            ANPAudioBuffer dst;
            android::AudioTrack::Buffer* src;
            
            src = reinterpret_cast<android::AudioTrack::Buffer*>(info);
            dst.bufferData      = src->raw;
            dst.channelCount    = src->channelCount;
            dst.format          = toANPFormat(src->format);
            dst.size            = src->size;
            track->mProc(kMoreData_ANPAudioEvent, track->mUser, &dst);
            // return the updated size field
            src->size = dst.size;
            break;
        }
        case android::AudioTrack::EVENT_UNDERRUN:
            track->mProc(kUnderRun_ANPAudioEvent, track->mUser, NULL);
            break;
        default:
            SkDebugf("------ unknown audio event for plugin %d\n", event);
            break;
    }
}

static ANPAudioTrack* ANPCreateTrack(uint32_t sampleRate,
                                     ANPSampleFormat format,
                                     int channelCount,
                                     ANPAudioCallbackProc proc,
                                     void* user) {
    
    ANPAudioTrack* track = new ANPAudioTrack;
    
    track->mUser = user;
    track->mProc = proc;
    track->mTrack = new android::AudioTrack(android::AudioTrack::MUSIC,
                                            sampleRate,
                                            fromANPFormat(format),
                                            channelCount,
                                            0,  // frameCount
                                            0,  // flags
                                            callbackProc,
                                            track,
                                            0);
    
    if (track->mTrack->initCheck() != 0) {  // failure
        delete track->mTrack;
        delete track;
        track = NULL;
    }
    return track;
}

static void ANPDeleteTrack(ANPAudioTrack* track) {
    if (track) {
        delete track->mTrack;
        delete track;
    }
}

static void ANPTrackStart(ANPAudioTrack* track) {
    track->mTrack->start();
}

static void ANPTrackPause(ANPAudioTrack* track) {
    track->mTrack->pause();
}

static void ANPTrackStop(ANPAudioTrack* track) {
    track->mTrack->stop();
}

static bool ANPTrackIsStopped(ANPAudioTrack* track) {
    return track->mTrack->stopped();
}

///////////////////////////////////////////////////////////////////////////////

void ANPAudioTrackInterfaceV0_Init(ANPInterface* value) {
    ANPAudioTrackInterfaceV0* si = reinterpret_cast<ANPAudioTrackInterfaceV0*>(value);
    si->newTrack    = ANPCreateTrack;
    si->deleteTrack = ANPDeleteTrack;
    si->start       = ANPTrackStart;
    si->pause       = ANPTrackPause;
    si->stop        = ANPTrackStop;
    si->isStopped   = ANPTrackIsStopped;
}

