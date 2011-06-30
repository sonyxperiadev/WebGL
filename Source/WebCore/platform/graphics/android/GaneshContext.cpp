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
    , m_baseTileDevice(0)
    , m_baseTileFbo(0)
    , m_baseTileStencil(0)
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

SkDevice* GaneshContext::getDeviceForBaseTile(GLuint textureId)
{
    if (!m_baseTileFbo) {
        glGenFramebuffers(1, &m_baseTileFbo);
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

    glBindFramebuffer(GL_FRAMEBUFFER, m_baseTileFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_baseTileStencil);

    if (!m_baseTileDevice) {

        GrPlatformSurfaceDesc surfaceDesc;
        surfaceDesc.fSurfaceType = kRenderTarget_GrPlatformSurfaceType;
        surfaceDesc.fRenderTargetFlags = kNone_GrPlatformRenderTargetFlagBit;
        surfaceDesc.fWidth = TilesManager::tileWidth();
        surfaceDesc.fHeight = TilesManager::tileWidth();
        surfaceDesc.fConfig = kRGBA_8888_GrPixelConfig;
        surfaceDesc.fStencilBits = 8;
        surfaceDesc.fPlatformRenderTarget = m_baseTileFbo;

        GrContext* grContext = getGrContext();
        GrRenderTarget* renderTarget = (GrRenderTarget*) grContext->createPlatformSurface(surfaceDesc);

        SkBitmap bitmap;
        bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                         TilesManager::tileWidth(), TilesManager::tileWidth());

        m_baseTileDevice = new SkGpuDevice(grContext, bitmap, renderTarget);
        renderTarget->unref();
        XLOG("generated device %p", m_baseTileDevice);
    }

    GLUtils::checkGlError("getDeviceForBaseTile");
    return m_baseTileDevice;
}



} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
