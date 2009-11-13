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

#ifndef ANPSurface_npapi_H
#define ANPSurface_npapi_H

#include "android_npapi.h"
#include <jni.h>

struct ANPSurfaceInterfaceV0 : ANPInterface {
    /** Locks the surface from manipulation by other threads and provides a bitmap
        to be written to.  The dirtyRect param specifies which portion of the
        bitmap will be written to.  If the dirtyRect is NULL then the entire
        surface will be considered dirty.  If the lock was successful the function
        will return true and the bitmap will be set to point to a valid bitmap.
        If not the function will return false and the bitmap will be set to NULL.
     */
    bool (*lock)(JNIEnv* env, jobject surface, ANPBitmap* bitmap, ANPRectI* dirtyRect);
    /** Given a locked surface handle (i.e. result of a successful call to lock)
        the surface is unlocked and the contents of the bitmap, specifically
        those inside the dirtyRect are written to the screen.
     */
    void (*unlock)(JNIEnv* env, jobject surface);
};

#endif //ANPSurface_npapi_H
