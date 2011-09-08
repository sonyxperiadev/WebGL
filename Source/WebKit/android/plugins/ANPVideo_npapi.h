/*
 * Copyright 2011, The Android Open Source Project
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

#ifndef ANPVideo_npapi_h
#define ANPVideo_npapi_h

#include "android_npapi.h"
#include <android/native_window.h>

struct ANPVideoInterfaceV0 : ANPInterface {

    /**
     * Constructs a new native window to be used for rendering video content.
     *
     * Subsequent calls will produce new windows, but may also return NULL after
     * n attempts if the browser has reached it's limit. Further, if the browser
     * is unable to acquire the window quickly it may also return NULL in order
     * to not prevent the plugin from executing. A subsequent call will then
     * return the window if it is avaiable.
     *
     * NOTE: The hardware may fail if you try to decode more than the allowable
     * number of videos supported on that device.
     */
    ANativeWindow* (*acquireNativeWindow)(NPP instance);

    /**
     * Sets the rectangle that specifies where the video content is to be drawn.
     * The dimensions are in document space. Further, if the rect is NULL the
     * browser will not attempt to draw the window, therefore do not set the
     * dimensions until you queue the first buffer in the window.
     */
    void (*setWindowDimensions)(NPP instance, const ANativeWindow* window, const ANPRectF* dimensions);

    /**
     */
    void (*releaseNativeWindow)(NPP instance, ANativeWindow* window);
};

/** Called to notify the plugin that a video frame has been composited by the
 *  browser for display.  This will be called in a separate thread and as such
 *  you cannot call releaseNativeWindow from the callback.
 *
 *  The timestamp is in nanoseconds, and is monotonically increasing.
 */
typedef void (*ANPVideoFrameCallbackProc)(ANativeWindow* window, int64_t timestamp);

struct ANPVideoInterfaceV1 : ANPVideoInterfaceV0 {
    /** Set a callback to be notified when an ANativeWindow is composited by
     *  the browser.
     */
    void (*setFramerateCallback)(NPP instance, const ANativeWindow* window, ANPVideoFrameCallbackProc);
};

#endif // ANPVideo_npapi_h
