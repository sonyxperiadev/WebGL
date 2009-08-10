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

// must include config.h first for webkit to fiddle with new/delete
#include "config.h"

#include "PluginSurface.h"
#include "PluginView.h"
#include "PluginWidgetAndroid.h"
#include "SkANP.h"

using namespace WebCore;

static ANPSurface* anp_newRasterSurface(NPP instance, ANPBitmapFormat format,
                                        bool fixedSize) {
    if (instance && instance->ndata) {
        PluginView* view = static_cast<PluginView*>(instance->ndata);
        PluginWidgetAndroid* widget = view->platformPluginWidget();
        return widget->createRasterSurface(format, fixedSize);
    }
    return NULL;
}

static void anp_deleteSurface(ANPSurface* surface) {
    if (surface) {
        if (surface->data) {
            android::PluginSurface* s =
                    static_cast<android::PluginSurface*>(surface->data);
            s->destroy();
        }
        delete surface;
    }
}

static bool anp_lock(ANPSurface* surface, ANPBitmap* bitmap, ANPRectI* dirtyRect) {
    if (bitmap && surface && surface->data) {
        android::PluginSurface* s =
                static_cast<android::PluginSurface*>(surface->data);

        return s->lock(dirtyRect, bitmap);
    }
    return false;
}

static void anp_unlock(ANPSurface* surface) {
    if (surface && surface->data) {
        android::PluginSurface* s =
                static_cast<android::PluginSurface*>(surface->data);
        s->unlock();
    }
}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void ANPSurfaceInterfaceV0_Init(ANPInterface* value) {
    ANPSurfaceInterfaceV0* i = reinterpret_cast<ANPSurfaceInterfaceV0*>(value);

    ASSIGN(i, newRasterSurface);
    ASSIGN(i, deleteSurface);
    ASSIGN(i, lock);
    ASSIGN(i, unlock);
}

