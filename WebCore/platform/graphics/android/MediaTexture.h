/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef MediaTexture_h
#define MediaTexture_h

#if USE(ACCELERATED_COMPOSITING)

#include "RefPtr.h"
#include "DoubleBufferedTexture.h"
#include "LayerAndroid.h"
#include <utils/RefBase.h>
#include <jni.h>

namespace android {
    class SurfaceTexture;
}

namespace WebCore {

class VideoListener;

class MediaTexture : public DoubleBufferedTexture {

public:
    MediaTexture(EGLContext sharedContext);

    void producerInc();
    void producerDec();
    void consumerInc();
    void consumerDec();

    int getProducerCount() { android::Mutex::Autolock lock(m_mediaLock); return m_producerRefCount; }
    int getConsumerCount() { android::Mutex::Autolock lock(m_mediaLock); return m_consumerRefCount; }

private:
    android::Mutex m_mediaLock;
    int m_producerRefCount;
    int m_consumerRefCount;
};

class VideoTexture : public android::LightRefBase<VideoTexture> {

public:
    VideoTexture(jobject weakWebViewRef);
    ~VideoTexture();

    void initNativeWindowIfNeeded();
    void drawVideo(const TransformationMatrix& matrix, const SkRect& parentBounds);

    ANativeWindow* requestNewWindow();
    ANativeWindow* getNativeWindow();
    void releaseNativeWindow();
    void setDimensions(const SkRect& dimensions);


private:
    GLuint m_textureId;
    sp<android::SurfaceTexture> m_surfaceTexture;
    sp<ANativeWindow> m_surfaceTextureClient;
    sp<VideoListener> m_videoListener;
    SkRect m_dimensions;
    bool m_newWindowRequest;
    bool m_newWindowReady;

    jobject m_weakWebViewRef;

    android::Mutex m_videoLock;
    android::Condition m_newVideoRequestCond;
};


} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif  // MediaTexture_h
