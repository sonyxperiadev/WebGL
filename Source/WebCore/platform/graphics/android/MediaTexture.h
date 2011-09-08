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

typedef void (*FramerateCallbackProc)(ANativeWindow* window, int64_t timestamp);

class MediaListener;

class MediaTexture : public android::LightRefBase<MediaTexture> {

public:
    MediaTexture(jobject webViewRef);
    ~MediaTexture();

    void initNativeWindowIfNeeded();
    void drawContent(const TransformationMatrix& matrix);
    void drawVideo(const TransformationMatrix& matrix, const SkRect& parentBounds);

    ANativeWindow* requestNewWindow();
    ANativeWindow* getNativeWindow();
    void releaseNativeWindow();
    void setDimensions(const SkRect& dimensions);
    void setFramerateCallback(FramerateCallbackProc callback);


private:
    GLuint m_textureId;
    sp<android::SurfaceTexture> m_surfaceTexture;
    sp<ANativeWindow> m_surfaceTextureClient;
    sp<MediaListener> m_mediaListener;
    SkRect m_dimensions;
    bool m_newWindowRequest;
    bool m_newWindowReady;

    jobject m_weakWebViewRef;

    android::Mutex m_mediaLock;
    android::Condition m_newMediaRequestCond;
};


} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif  // MediaTexture_h
