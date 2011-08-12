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


#include "config.h"
#include "GaneshContext.h"
#include "GLUtils.h"

#include "android/native_window.h"

#if USE(ACCELERATED_COMPOSITING)

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/CurrentTime.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "GaneshContext", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

GaneshContext::GaneshContext()
    : m_grContext(0)
    , m_baseTileDeviceFBO(0)
    , m_baseTileFBO(0)
    , m_baseTileStencil(0)
    , m_baseTileDeviceSurface(0)
    , m_surfaceConfig(0)
    , m_surfaceContext(EGL_NO_CONTEXT)
{
}

GaneshContext* GaneshContext::gInstance = 0;

GaneshContext* GaneshContext::instance()
{
    if (!gInstance)
        gInstance = new GaneshContext();
    return gInstance;
}

GrContext* GaneshContext::getGrContext()
{
    if (!m_grContext) {
        m_grContext = GrContext::Create(kOpenGL_Shaders_GrEngine, NULL);
    }
    return m_grContext;
}

void GaneshContext::flush()
{
    if (m_grContext)
        m_grContext->flush();
}

SkDevice* GaneshContext::getDeviceForBaseTile(const TileRenderInfo& renderInfo)
{
    // Ganesh should be the only code in the rendering thread that is using GL
    // and setting the EGLContext.  If this is not the case then we need to
    // reset the Ganesh context to prevent rendering issues.
    bool contextNeedsReset = false;
    if (eglGetCurrentContext() != m_surfaceContext) {
        XLOG("Warning: EGLContext has Changed! %p, %p", m_surfaceContext,
                                                        eglGetCurrentContext());
        contextNeedsReset = true;
    }

    SkDevice* device = 0;
    if (renderInfo.textureInfo->getSharedTextureMode() == SurfaceTextureMode)
        device = getDeviceForBaseTileSurface(renderInfo);
    else if (renderInfo.textureInfo->getSharedTextureMode() == EglImageMode)
        device = getDeviceForBaseTileFBO(renderInfo);

    // We must reset the Ganesh context only after we are sure we have
    // re-established our EGLContext as the current context.
    if (device && contextNeedsReset)
        getGrContext()->resetContext();

    return device;
}

SkDevice* GaneshContext::getDeviceForBaseTileSurface(const TileRenderInfo& renderInfo)
{
    EGLDisplay display;

    if (!m_surfaceContext) {

        if(eglGetCurrentContext() != EGL_NO_CONTEXT) {
            XLOG("ERROR: should not have a context yet");
        }

        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        GLUtils::checkEglError("eglGetDisplay");

        EGLint majorVersion;
        EGLint minorVersion;
        EGLBoolean returnValue = eglInitialize(display, &majorVersion, &minorVersion);
        GLUtils::checkEglError("eglInitialize", returnValue);

        EGLint numConfigs;
        static const EGLint configAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_STENCIL_SIZE, 8,
            EGL_NONE
        };

        eglChooseConfig(display, configAttribs, &m_surfaceConfig, 1, &numConfigs);
        GLUtils::checkEglError("eglChooseConfig");

        static const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };

        m_surfaceContext = eglCreateContext(display, m_surfaceConfig, NULL, contextAttribs);
        GLUtils::checkEglError("eglCreateContext");
    } else {
        display = eglGetCurrentDisplay();
        GLUtils::checkEglError("eglGetCurrentDisplay");
    }

    TransferQueue* tileQueue = TilesManager::instance()->transferQueue();

    if (tileQueue->m_eglSurface == EGL_NO_SURFACE) {

        const float tileWidth = renderInfo.tileSize.width();
        const float tileHeight = renderInfo.tileSize.height();

        ANativeWindow* anw = tileQueue->m_ANW.get();

        int result = ANativeWindow_setBuffersGeometry(anw, (int)tileWidth,
                (int)tileHeight, WINDOW_FORMAT_RGBA_8888);

        renderInfo.textureInfo->m_width = tileWidth;
        renderInfo.textureInfo->m_height = tileHeight;
        tileQueue->m_eglSurface = eglCreateWindowSurface(display, m_surfaceConfig, anw, NULL);

        GLUtils::checkEglError("eglCreateWindowSurface");
        XLOG("eglCreateWindowSurface");
    }

    EGLBoolean returnValue = eglMakeCurrent(display, tileQueue->m_eglSurface, tileQueue->m_eglSurface, m_surfaceContext);
    GLUtils::checkEglError("eglMakeCurrent", returnValue);
    XLOG("eglMakeCurrent");

    if (!m_baseTileDeviceSurface) {

        GrPlatformSurfaceDesc surfaceDesc;
        surfaceDesc.fSurfaceType = kRenderTarget_GrPlatformSurfaceType;
        surfaceDesc.fRenderTargetFlags = kNone_GrPlatformRenderTargetFlagBit;
        surfaceDesc.fWidth = TilesManager::tileWidth();
        surfaceDesc.fHeight = TilesManager::tileHeight();
        surfaceDesc.fConfig = kRGBA_8888_GrPixelConfig;
        surfaceDesc.fStencilBits = 8;
        surfaceDesc.fPlatformRenderTarget = 0;

        GrContext* grContext = getGrContext();
        GrRenderTarget* renderTarget = (GrRenderTarget*) grContext->createPlatformSurface(surfaceDesc);

        SkBitmap bitmap;
        bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                         renderInfo.tileSize.width(),
                         renderInfo.tileSize.height());

        m_baseTileDeviceSurface = new SkGpuDevice(grContext, bitmap, renderTarget);
        renderTarget->unref();
        XLOG("generated device %p", m_baseTileDeviceSurface);
    }

    GLUtils::checkGlError("getDeviceForBaseTile");
    return m_baseTileDeviceSurface;
}

SkDevice* GaneshContext::getDeviceForBaseTileFBO(const TileRenderInfo& renderInfo)
{
    const GLuint textureId = renderInfo.textureInfo->m_textureId;
    const float tileWidth = renderInfo.tileSize.width();
    const float tileHeight = renderInfo.tileSize.height();

    // bind to the current texture
    glBindTexture(GL_TEXTURE_2D, textureId);

    // setup the texture if needed
    if (renderInfo.textureInfo->m_width != tileWidth
            || renderInfo.textureInfo->m_height != tileHeight) {

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tileWidth, tileHeight,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        renderInfo.textureInfo->m_width = tileWidth;
        renderInfo.textureInfo->m_height = tileHeight;

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    if (!m_baseTileFBO) {
        glGenFramebuffers(1, &m_baseTileFBO);
        XLOG("generated FBO");
    }

    if (!m_baseTileStencil) {
        glGenRenderbuffers(1, &m_baseTileStencil);
        glBindRenderbuffer(GL_RENDERBUFFER, m_baseTileStencil);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8,
                              TilesManager::tileWidth(),
                              TilesManager::tileHeight());
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        XLOG("generated stencil");
    }

    // bind the FBO and attach the texture and stencil
    glBindFramebuffer(GL_FRAMEBUFFER, m_baseTileFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_baseTileStencil);

    if (!m_baseTileDeviceFBO) {

        GrPlatformSurfaceDesc surfaceDesc;
        surfaceDesc.fSurfaceType = kRenderTarget_GrPlatformSurfaceType;
        surfaceDesc.fRenderTargetFlags = kNone_GrPlatformRenderTargetFlagBit;
        surfaceDesc.fWidth = TilesManager::tileWidth();
        surfaceDesc.fHeight = TilesManager::tileHeight();
        surfaceDesc.fConfig = kRGBA_8888_GrPixelConfig;
        surfaceDesc.fStencilBits = 8;
        surfaceDesc.fPlatformRenderTarget = m_baseTileFBO;

        GrContext* grContext = getGrContext();
        GrRenderTarget* renderTarget = (GrRenderTarget*) grContext->createPlatformSurface(surfaceDesc);

        SkBitmap bitmap;
        bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                         TilesManager::tileWidth(), TilesManager::tileWidth());

        m_baseTileDeviceFBO = new SkGpuDevice(grContext, bitmap, renderTarget);
        renderTarget->unref();
        XLOG("generated device %p", m_baseTileDeviceFBO);
    }

    GLUtils::checkGlError("getDeviceForBaseTile");
    return m_baseTileDeviceFBO;
}


} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
