/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2008 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#ifndef PluginSurface_H
#define PluginSurface_H

#include "android_npapi.h"
#include "SkANP.h"
#include "SurfaceCallback.h"

#include <jni.h>
#include <ui/Surface.h>
#include <utils/RefBase.h>

struct PluginWidgetAndroid;
class  SkBitmap;
struct SkIRect;

struct ANPSurface {
    void*           data;
    ANPSurfaceType  type;
};

namespace android {

class Surface;

class PluginSurface : public SurfaceCallback {
public:
    PluginSurface(PluginWidgetAndroid* widget, bool isFixedSize);
    virtual ~PluginSurface() {
        destroy();
    }

    void attach(int x, int y, int width, int height);
    void destroy();
    bool lock(SkIRect* dirty, SkBitmap* bitmap);
    void unlock();

    virtual void surfaceCreated();
    virtual void surfaceChanged(int format, int width, int height);
    virtual void surfaceDestroyed();

private:
    jobject              m_jSurfaceView;
    sp<Surface>          m_surface;
    PluginWidgetAndroid* m_widget;
};

} // namespace android

#endif
