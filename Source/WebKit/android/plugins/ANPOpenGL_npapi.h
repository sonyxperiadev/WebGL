/*
 * Copyright 2010, The Android Open Source Project
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

#ifndef ANPOpenGL_npapi_H
#define ANPOpenGL_npapi_H

#include "android_npapi.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>

/**
 * TODO should we not use EGL and GL data types for ABI safety?
 */
struct ANPTextureInfo {
    GLuint      textureId;
    uint32_t    width;
    uint32_t    height;
    GLenum      internalFormat;
};

struct ANPOpenGLInterfaceV0 : ANPInterface {
    /**
     */
    EGLContext (*acquireContext)(NPP instance);

    /**
     */
    ANPTextureInfo (*lockTexture)(NPP instance);

    /**
     */
    void (*releaseTexture)(NPP instance, const ANPTextureInfo*);

    /**
     * Invert the contents of the plugin on the y-axis.
     * default is to not be inverted (i.e. use OpenGL coordinates)
     */
    void (*invertPluginContent)(NPP instance, bool isContentInverted);
};

#endif //ANPOpenGL_npapi_H
