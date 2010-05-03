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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// must include config.h first for webkit to fiddle with new/delete
#include "config.h"
#include "ANPSurface_npapi.h"

#include "PluginView.h"
#include "PluginWidgetAndroid.h"
#include "SkANP.h"
#include "android_graphics.h"
#include <JNIUtility.h>
#include <surfaceflinger/Surface.h>
#include <ui/Rect.h>
#include <ui/Region.h>
#include <utils/RefBase.h>

using namespace android;

// used to cache JNI method and field IDs for Surface Objects
static struct ANPSurfaceInterfaceJavaGlue {
    bool        initialized;
    jmethodID   getSurfaceHolder;
    jmethodID   getSurface;
    jfieldID    surfacePointer;
} gSurfaceJavaGlue;

static inline sp<Surface> getSurface(JNIEnv* env, jobject view) {
    if (!env || !view) {
        return NULL;
    }

    if (!gSurfaceJavaGlue.initialized) {

        jclass surfaceViewClass = env->FindClass("android/view/SurfaceView");
        gSurfaceJavaGlue.getSurfaceHolder = env->GetMethodID(surfaceViewClass, "getHolder",
                                                             "()Landroid/view/SurfaceHolder;");

        jclass surfaceHolderClass = env->FindClass("android/view/SurfaceHolder");
        gSurfaceJavaGlue.getSurface = env->GetMethodID(surfaceHolderClass, "getSurface",
                                                       "()Landroid/view/Surface;");

        jclass surfaceClass = env->FindClass("android/view/Surface");
        gSurfaceJavaGlue.surfacePointer = env->GetFieldID(surfaceClass,
                ANDROID_VIEW_SURFACE_JNI_ID, "I");

        env->DeleteLocalRef(surfaceClass);
        env->DeleteLocalRef(surfaceViewClass);
        env->DeleteLocalRef(surfaceHolderClass);

        gSurfaceJavaGlue.initialized = true;
    }

    jobject holder = env->CallObjectMethod(view, gSurfaceJavaGlue.getSurfaceHolder);
    jobject surface = env->CallObjectMethod(holder, gSurfaceJavaGlue.getSurface);
    jint surfacePointer = env->GetIntField(surface, gSurfaceJavaGlue.surfacePointer);

    env->DeleteLocalRef(holder);
    env->DeleteLocalRef(surface);

    return sp<Surface>((Surface*) surfacePointer);
}

static inline ANPBitmapFormat convertPixelFormat(PixelFormat format) {
    switch (format) {
        case PIXEL_FORMAT_RGBA_8888:    return kRGBA_8888_ANPBitmapFormat;
        case PIXEL_FORMAT_RGB_565:      return kRGB_565_ANPBitmapFormat;
        default:                        return kUnknown_ANPBitmapFormat;
    }
}

static bool anp_lock(JNIEnv* env, jobject surfaceView, ANPBitmap* bitmap, ANPRectI* dirtyRect) {
    if (!bitmap || !surfaceView) {
        return false;
    }

    sp<Surface> surface = getSurface(env, surfaceView);

    if (!bitmap || !Surface::isValid(surface)) {
            return false;
    }

    Region dirtyRegion;
    if (dirtyRect) {
        Rect rect(dirtyRect->left, dirtyRect->top, dirtyRect->right, dirtyRect->bottom);
        if (!rect.isEmpty()) {
            dirtyRegion.set(rect);
        }
    } else {
        dirtyRegion.set(Rect(0x3FFF, 0x3FFF));
    }

    Surface::SurfaceInfo info;
    status_t err = surface->lock(&info, &dirtyRegion);
    if (err < 0) {
        return false;
    }

    // the surface may have expanded the dirty region so we must to pass that
    // information back to the plugin.
    if (dirtyRect) {
        Rect dirtyBounds = dirtyRegion.getBounds();
        dirtyRect->left = dirtyBounds.left;
        dirtyRect->right = dirtyBounds.right;
        dirtyRect->top = dirtyBounds.top;
        dirtyRect->bottom = dirtyBounds.bottom;
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

static void anp_unlock(JNIEnv* env, jobject surfaceView) {
    if (!surfaceView) {
        return;
    }

    sp<Surface> surface = getSurface(env, surfaceView);

    if (!Surface::isValid(surface)) {
        return;
    }

    surface->unlockAndPost();
}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void ANPSurfaceInterfaceV0_Init(ANPInterface* value) {
    ANPSurfaceInterfaceV0* i = reinterpret_cast<ANPSurfaceInterfaceV0*>(value);

    ASSIGN(i, lock);
    ASSIGN(i, unlock);

    // setup the java glue struct
    gSurfaceJavaGlue.initialized = false;
}
