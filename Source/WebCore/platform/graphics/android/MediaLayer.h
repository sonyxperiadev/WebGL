/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef MediaLayer_h
#define MediaLayer_h

#if USE(ACCELERATED_COMPOSITING)

#include "MediaTexture.h"
#include "LayerAndroid.h"
#include <jni.h>

namespace android {
    class SurfaceTexture;
}

namespace WebCore {

class MediaLayer : public LayerAndroid {

public:
    MediaLayer(jobject weakWebViewRef);
    MediaLayer(const MediaLayer& layer);
    virtual ~MediaLayer();

    virtual bool drawGL(GLWebViewState*, SkMatrix&);
    virtual void paintBitmapGL() const { };
    virtual bool needsTexture() { return false; }

    virtual bool isMedia() const { return true; }
    virtual LayerAndroid* copy() const { return new MediaLayer(*this); }

    MediaTexture* getTexture() const { return m_bufferedTexture; }

    void setCurrentTextureInfo(TextureInfo* info) { m_currentTextureInfo = info; }
    TextureInfo* getCurrentTextureInfo() const { return m_currentTextureInfo; }

    void invertContents(bool invertContent) { m_isContentInverted = invertContent; }
    void setOutlineSize(int size) { m_outlineSize = size; }

    // functions to manipulate secondary layers for video playback
    ANativeWindow* acquireNativeWindowForVideo();
    void setWindowDimensionsForVideo(const ANativeWindow* window, const SkRect& dimensions);
    void releaseNativeWindowForVideo(ANativeWindow* window);

private:
    bool m_isCopy;

    // Primary GL texture variables
    MediaTexture* m_bufferedTexture;
    TextureInfo* m_currentTextureInfo;

    bool m_isContentInverted;
    int m_outlineSize;

    // Video texture variables
    VideoTexture* m_videoTexture;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif  // MediaLayer_h
