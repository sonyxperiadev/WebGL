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
#include "LayerAndroid.h"
#include "Vector.h"
#include <GLES2/gl2.h>
#include <ui/GraphicBuffer.h>
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

    bool isContentInverted();
    void invertContents(bool invertContent);

    void initNativeWindowIfNeeded();
    void draw(const TransformationMatrix& contentMatrix,
              const TransformationMatrix& videoMatrix,
              const SkRect& mediaBounds);

    ANativeWindow* getNativeWindowForContent();
    ANativeWindow* requestNativeWindowForVideo();

    void releaseNativeWindow(const ANativeWindow* window);
    void setDimensions(const ANativeWindow* window, const SkRect& dimensions);
    void setFramerateCallback(const ANativeWindow* window,
                              FramerateCallbackProc callback);

private:
    struct TextureWrapper {
        GLuint textureId;
        sp<android::SurfaceTexture> surfaceTexture;
        sp<ANativeWindow> nativeWindow;
        sp<MediaListener> mediaListener;
        SkRect dimensions; // only used by the video layer
    };

    TextureWrapper* createTexture();
    void deleteTexture(TextureWrapper* item, bool force = false);

    TextureWrapper* m_contentTexture;
    Vector<TextureWrapper*> m_videoTextures;
    Vector<GLuint> m_unusedTextures;

    // used to track if the content is to be drawn inverted
    bool m_isContentInverted;

    // used to generate new video textures
    bool m_newWindowRequest;
    sp<ANativeWindow> m_newWindow;

    jobject m_weakWebViewRef;

    android::Mutex m_mediaLock;
    android::Condition m_newMediaRequestCond;
};


} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif  // MediaTexture_h
