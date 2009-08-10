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

#include "PluginSurface.h"

#include "android_graphics.h"
#include "PluginWidgetAndroid.h"
#include "WebViewCore.h"
#include "jni_utility.h"

#include <ui/Rect.h>
#include <ui/Region.h>
#include <ui/Surface.h>

namespace android {

// jni field offset for the native surface pointer.
static jfieldID  gSurfaceField;
static jmethodID gGetHolder;
static jmethodID gGetSurface;

static void initFields(JNIEnv* env) {
    if (gSurfaceField)
        return;

    jclass clazz = env->FindClass("android/view/Surface");
    gSurfaceField = env->GetFieldID(clazz, "mSurface", "I");

    clazz = env->FindClass("android/view/SurfaceView");
    gGetHolder = env->GetMethodID(clazz, "getHolder", "()Landroid/view/SurfaceHolder;");

    clazz = env->FindClass("android/view/SurfaceHolder");
    gGetSurface = env->GetMethodID(clazz, "getSurface", "()Landroid/view/Surface;");
}

static inline sp<Surface> getSurface(jobject view) {
    if (!view) {
        return NULL;
    }
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    initFields(env);
    jobject holder = env->CallObjectMethod(view, gGetHolder);
    jobject surface = env->CallObjectMethod(holder, gGetSurface);
    return sp<Surface>((Surface*) env->GetIntField(surface, gSurfaceField));
}

static inline ANPBitmapFormat convertPixelFormat(PixelFormat format) {
    switch (format) {
        case PIXEL_FORMAT_RGBA_8888:    return kRGBA_8888_ANPBitmapFormat;
        case PIXEL_FORMAT_RGB_565:      return kRGB_565_ANPBitmapFormat;
        default:                        return kUnknown_ANPBitmapFormat;
    }
}

static inline PixelFormat convertANPBitmapFormat(ANPBitmapFormat format) {
    switch (format) {
        case kRGBA_8888_ANPBitmapFormat:  return PIXEL_FORMAT_RGBA_8888;
        case kRGB_565_ANPBitmapFormat:    return PIXEL_FORMAT_RGB_565;
        default:                          return PIXEL_FORMAT_UNKNOWN;
    }
}

PluginSurface::PluginSurface(PluginWidgetAndroid* widget, ANPBitmapFormat format,
                             bool isFixedSize)
        : m_jSurfaceView(0)
        , m_widget(widget) {
    // Create our java SurfaceView.
    jobject obj = widget->webViewCore()->createSurface(this, convertANPBitmapFormat(format), isFixedSize);
    if (obj) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        m_jSurfaceView = env->NewGlobalRef(obj);
        env->DeleteLocalRef(obj);
    }
}

void PluginSurface::attach(int x, int y, int width, int height) {
    if (m_jSurfaceView) {
        m_widget->webViewCore()->attachSurface(m_jSurfaceView, x, y, width,
                height);
    }
}

void PluginSurface::destroy() {
    m_surface.clear();
    if (m_jSurfaceView) {
        m_widget->webViewCore()->destroySurface(m_jSurfaceView);
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        env->DeleteGlobalRef(m_jSurfaceView);
        m_jSurfaceView = 0;
    }
}

bool PluginSurface::lock(ANPRectI* dirty, ANPBitmap* bitmap) {
    if (!bitmap || !Surface::isValid(m_surface)) {
        return false;
    }

    Region dirtyRegion;
    if (dirty) {
        Rect rect(dirty->left, dirty->top, dirty->right, dirty->bottom);
        if (!rect.isEmpty()) {
            dirtyRegion.set(rect);
        }
    } else {
        dirtyRegion.set(Rect(0x3FFF, 0x3FFF));
    }

    Surface::SurfaceInfo info;
    status_t err = m_surface->lock(&info, &dirtyRegion);
    if (err < 0) {
        return false;
    }

    ssize_t bpr = info.s * bytesPerPixel(info.format);

    bitmap->format = convertPixelFormat(info.format);
    bitmap->width = info.w;
    bitmap->height = info.h;
    bitmap->rowBytes = bpr;

    if (info.w > 0 && info.h > 0) {
        bitmap->baseAddr = info.bits;
    } else {
        bitmap->baseAddr = NULL;
        return false;
    }

    return true;
}

void PluginSurface::unlock() {
    if (!Surface::isValid(m_surface)) {
        return;
    }

    m_surface->unlockAndPost();
}

static void sendSurfaceEvent(PluginWidgetAndroid* widget,
        ANPSurfaceAction action, int format = 0, int width = 0,
        int height = 0) {
    // format is currently not reported to the plugin. The plumbing from Java
    // to C is still provided in case we add the format back to the event.
    ANPEvent event;
    SkANP::InitEvent(&event, kSurface_ANPEventType);

    event.data.surface.action = action;
    if (action == kChanged_ANPSurfaceAction) {
        event.data.surface.data.changed.width = width;
        event.data.surface.data.changed.height = height;
    }

    widget->sendEvent(event);
}

// SurfaceCallback methods
void PluginSurface::surfaceCreated() {
    m_surface = getSurface(m_jSurfaceView);
    // Not sure what values for format, width, and height should be here.
    sendSurfaceEvent(m_widget, kCreated_ANPSurfaceAction);
}

void PluginSurface::surfaceChanged(int format, int width, int height) {
    m_surface = getSurface(m_jSurfaceView);
    sendSurfaceEvent(m_widget, kChanged_ANPSurfaceAction, format, width,
            height);
}

void PluginSurface::surfaceDestroyed() {
    m_surface = getSurface(m_jSurfaceView);
    // Not sure what values for format, width, and height should be here.
    sendSurfaceEvent(m_widget, kDestroyed_ANPSurfaceAction);
}

} // namespace android
