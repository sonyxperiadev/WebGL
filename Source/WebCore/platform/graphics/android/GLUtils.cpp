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

#include "config.h"
#include "GLUtils.h"

#if USE(ACCELERATED_COMPOSITING)

#include "ShaderProgram.h"
#include "TilesManager.h"

#include <cutils/log.h>
#include <gui/SurfaceTexture.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>


#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "GLUtils", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

struct ANativeWindowBuffer;

namespace WebCore {

using namespace android;

/////////////////////////////////////////////////////////////////////////////////////////
// Matrix utilities
/////////////////////////////////////////////////////////////////////////////////////////

void GLUtils::toGLMatrix(GLfloat* flattened, const TransformationMatrix& m)
{
    flattened[0] = m.m11(); // scaleX
    flattened[1] = m.m12(); // skewY
    flattened[2] = m.m13();
    flattened[3] = m.m14(); // persp0
    flattened[4] = m.m21(); // skewX
    flattened[5] = m.m22(); // scaleY
    flattened[6] = m.m23();
    flattened[7] = m.m24(); // persp1
    flattened[8] = m.m31();
    flattened[9] = m.m32();
    flattened[10] = m.m33();
    flattened[11] = m.m34();
    flattened[12] = m.m41(); // transX
    flattened[13] = m.m42(); // transY
    flattened[14] = m.m43();
    flattened[15] = m.m44(); // persp2
}

void GLUtils::toSkMatrix(SkMatrix& matrix, const TransformationMatrix& m)
{
    matrix[0] = m.m11(); // scaleX
    matrix[1] = m.m21(); // skewX
    matrix[2] = m.m41(); // transX
    matrix[3] = m.m12(); // skewY
    matrix[4] = m.m22(); // scaleY
    matrix[5] = m.m42(); // transY
    matrix[6] = m.m14(); // persp0
    matrix[7] = m.m24(); // persp1
    matrix[8] = m.m44(); // persp2
}

void GLUtils::setOrthographicMatrix(TransformationMatrix& ortho, float left, float top,
                                    float right, float bottom, float nearZ, float farZ)
{
    float deltaX = right - left;
    float deltaY = top - bottom;
    float deltaZ = farZ - nearZ;
    if (!deltaX || !deltaY || !deltaZ)
        return;

    ortho.setM11(2.0f / deltaX);
    ortho.setM41(-(right + left) / deltaX);
    ortho.setM22(2.0f / deltaY);
    ortho.setM42(-(top + bottom) / deltaY);
    ortho.setM33(-2.0f / deltaZ);
    ortho.setM43(-(nearZ + farZ) / deltaZ);
}

/////////////////////////////////////////////////////////////////////////////////////////
// GL & EGL error checks
/////////////////////////////////////////////////////////////////////////////////////////

static void crashIfOOM(GLint errorCode) {
    const GLint OOM_ERROR_CODE = 0x505;
    if (errorCode == OOM_ERROR_CODE) {
        XLOG("Fatal OOM detected.");
        CRASH();
    }
}

void GLUtils::checkEglError(const char* op, EGLBoolean returnVal)
{
    if (returnVal != EGL_TRUE) {
        XLOG("EGL ERROR - %s() returned %d\n", op, returnVal);
    }

    for (EGLint error = eglGetError(); error != EGL_SUCCESS; error = eglGetError()) {
        XLOG("after %s() eglError (0x%x)\n", op, error);
        crashIfOOM(error);
    }
}

bool GLUtils::checkGlError(const char* op)
{
    bool ret = false;
    for (GLint error = glGetError(); error; error = glGetError()) {
        XLOG("GL ERROR - after %s() glError (0x%x)\n", op, error);
        crashIfOOM(error);
        ret = true;
    }
    return ret;
}

bool GLUtils::checkGlErrorOn(void* p, const char* op)
{
    bool ret = false;
    for (GLint error = glGetError(); error; error = glGetError()) {
        XLOG("GL ERROR on %x - after %s() glError (0x%x)\n", p, op, error);
        crashIfOOM(error);
        ret = true;
    }
    return ret;
}

void GLUtils::checkSurfaceTextureError(const char* functionName, int status)
{
    if (status !=  NO_ERROR) {
        XLOG("ERROR at calling %s status is (%d)", functionName, status);
    }
}
/////////////////////////////////////////////////////////////////////////////////////////
// GL & EGL extension checks
/////////////////////////////////////////////////////////////////////////////////////////

bool GLUtils::isEGLImageSupported()
{
    const char* eglExtensions = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
    const char* glExtensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));

    return eglExtensions && glExtensions
        && strstr(eglExtensions, "EGL_KHR_image_base")
        && strstr(eglExtensions, "EGL_KHR_gl_texture_2D_image")
        && strstr(glExtensions, "GL_OES_EGL_image");
}

bool GLUtils::isEGLFenceSyncSupported()
{
    const char* eglExtensions = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
    return eglExtensions && strstr(eglExtensions, "EGL_KHR_fence_sync");
}

/////////////////////////////////////////////////////////////////////////////////////////
// Textures utilities
/////////////////////////////////////////////////////////////////////////////////////////

static GLenum getInternalFormat(SkBitmap::Config config)
{
    switch (config) {
    case SkBitmap::kA8_Config:
        return GL_ALPHA;
    case SkBitmap::kARGB_4444_Config:
        return GL_RGBA;
    case SkBitmap::kARGB_8888_Config:
        return GL_RGBA;
    case SkBitmap::kRGB_565_Config:
        return GL_RGB;
    default:
        return -1;
    }
}

static GLenum getType(SkBitmap::Config config)
{
    switch (config) {
    case SkBitmap::kA8_Config:
        return GL_UNSIGNED_BYTE;
    case SkBitmap::kARGB_4444_Config:
        return GL_UNSIGNED_SHORT_4_4_4_4;
    case SkBitmap::kARGB_8888_Config:
        return GL_UNSIGNED_BYTE;
    case SkBitmap::kIndex8_Config:
        return -1; // No type for compressed data.
    case SkBitmap::kRGB_565_Config:
        return GL_UNSIGNED_SHORT_5_6_5;
    default:
        return -1;
    }
}

static EGLConfig defaultPbufferConfig(EGLDisplay display)
{
    EGLConfig config;
    EGLint numConfigs;

    static const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    eglChooseConfig(display, configAttribs, &config, 1, &numConfigs);
    GLUtils::checkEglError("eglPbufferConfig");
    if (numConfigs != 1)
        LOGI("eglPbufferConfig failed (%d)\n", numConfigs);

    return config;
}

static EGLSurface createPbufferSurface(EGLDisplay display, const EGLConfig& config,
                                       EGLint* errorCode)
{
    const EGLint attribList[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    EGLSurface surface = eglCreatePbufferSurface(display, config, attribList);

    if (errorCode)
        *errorCode = eglGetError();
    else
        GLUtils::checkEglError("eglCreatePbufferSurface");

    if (surface == EGL_NO_SURFACE)
        return EGL_NO_SURFACE;

    return surface;
}

EGLContext GLUtils::createBackgroundContext(EGLContext sharedContext)
{
    checkEglError("<init>");
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    checkEglError("eglGetDisplay");
    if (display == EGL_NO_DISPLAY) {
        XLOG("eglGetDisplay returned EGL_NO_DISPLAY");
        return EGL_NO_CONTEXT;
    }

    EGLint majorVersion;
    EGLint minorVersion;
    EGLBoolean returnValue = eglInitialize(display, &majorVersion, &minorVersion);
    checkEglError("eglInitialize", returnValue);
    if (returnValue != EGL_TRUE) {
        XLOG("eglInitialize failed\n");
        return EGL_NO_CONTEXT;
    }

    EGLConfig config = defaultPbufferConfig(display);
    EGLSurface surface = createPbufferSurface(display, config, 0);

    EGLint surfaceConfigId;
    EGLBoolean success = eglGetConfigAttrib(display, config, EGL_CONFIG_ID, &surfaceConfigId);

    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext context = eglCreateContext(display, config, sharedContext, contextAttribs);
    checkEglError("eglCreateContext");
    if (context == EGL_NO_CONTEXT) {
        XLOG("eglCreateContext failed\n");
        return EGL_NO_CONTEXT;
    }

    returnValue = eglMakeCurrent(display, surface, surface, context);
    checkEglError("eglMakeCurrent", returnValue);
    if (returnValue != EGL_TRUE) {
        XLOG("eglMakeCurrent failed\n");
        return EGL_NO_CONTEXT;
    }

    return context;
}

void GLUtils::deleteTexture(GLuint* texture)
{
    glDeleteTextures(1, texture);
    GLUtils::checkGlError("glDeleteTexture");
    *texture = 0;
}

GLuint GLUtils::createSampleColorTexture(int r, int g, int b) {
    GLuint texture;
    glGenTextures(1, &texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLubyte pixels[4 *3] = {
        r, g, b,
        r, g, b,
        r, g, b,
        r, g, b
    };
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    GLUtils::checkGlError("glTexImage2D");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

GLuint GLUtils::createSampleTexture()
{
    GLuint texture;
    glGenTextures(1, &texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLubyte pixels[4 *3] = {
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 0
    };
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    GLUtils::checkGlError("glTexImage2D");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

GLuint GLUtils::createBaseTileGLTexture(int width, int height)
{
    GLuint texture;
    glGenTextures(1, &texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLubyte* pixels = 0;
#ifdef DEBUG
    int length = width * height * 4;
    pixels = new GLubyte[length];
    for (int i = 0; i < length; i++)
        pixels[i] = i % 256;
#endif
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    GLUtils::checkGlError("glTexImage2D");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

void GLUtils::paintTextureWithBitmap(const TileRenderInfo* renderInfo,
                                     const SkBitmap& bitmap)
{
    if (!renderInfo)
        return;
    const int x = renderInfo->invalRect->fLeft;
    const int y = renderInfo->invalRect->fTop;
    const SkSize& requiredSize = renderInfo->tileSize;
    TextureInfo* textureInfo = renderInfo->textureInfo;
    SharedTextureMode mode = textureInfo->getSharedTextureMode();
    if (requiredSize.equals(textureInfo->m_width, textureInfo->m_height)) {
        if (mode == EglImageMode)
            GLUtils::updateTextureWithBitmap(textureInfo->m_textureId, x, y, bitmap);
        else if (mode == SurfaceTextureMode)
#if DEPRECATED_SURFACE_TEXTURE_MODE
            GLUtils::updateSurfaceTextureWithBitmap(renderInfo, x, y, bitmap);
#else
            GLUtils::updateSharedSurfaceTextureWithBitmap(renderInfo, x, y, bitmap);
#endif
    } else {

        if (!requiredSize.equals(bitmap.width(), bitmap.height())) {
            XLOG("The bitmap size (%d,%d) does not equal the texture size (%d,%d)",
                    bitmap.width(), bitmap.height(),
                    requiredSize.width(), requiredSize.height());
        }

        if (mode == EglImageMode)
            GLUtils::createTextureWithBitmap(textureInfo->m_textureId, bitmap);
        else if (mode == SurfaceTextureMode)
#if DEPRECATED_SURFACE_TEXTURE_MODE
            GLUtils::createSurfaceTextureWithBitmap(renderInfo, bitmap);
#else
            GLUtils::updateSharedSurfaceTextureWithBitmap(renderInfo, 0, 0, bitmap);
#endif
        textureInfo->m_width = bitmap.width();
        textureInfo->m_height = bitmap.height();
        textureInfo->m_internalFormat = GL_RGBA;
    }
}

#if DEPRECATED_SURFACE_TEXTURE_MODE
void GLUtils::createSurfaceTextureWithBitmap(const TileRenderInfo* renderInfo, const SkBitmap& bitmap, GLint filter)
{

    TextureInfo* texture = renderInfo->textureInfo;

    texture->m_width = bitmap.width();
    texture->m_height = bitmap.height();
    texture->m_internalFormat = GL_RGBA;

    sp<android::SurfaceTexture> surfaceTexture = texture->m_surfaceTexture;
    sp<ANativeWindow> ANW = texture->m_ANW;

    int result;
    result = native_window_set_buffers_geometry(ANW.get(),
            texture->m_width, texture->m_height, HAL_PIXEL_FORMAT_RGBA_8888);
    checkSurfaceTextureError("native_window_set_buffers_geometry", result);
    result = native_window_set_usage(ANW.get(),
            GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
    checkSurfaceTextureError("native_window_set_usage", result);

    updateSurfaceTextureWithBitmap(renderInfo, 0, 0, bitmap, filter);
}

void GLUtils::updateSurfaceTextureWithBitmap(const TileRenderInfo* renderInfo, int x, int y, const SkBitmap& bitmap, GLint filter)
{
    TextureInfo* texture = renderInfo->textureInfo;
    sp<android::SurfaceTexture> surfaceTexture = texture->m_surfaceTexture;
    sp<ANativeWindow> ANW = texture->m_ANW;

    ANativeWindowBuffer* anb;
    int status = ANW->dequeueBuffer(ANW.get(), &anb);
    checkSurfaceTextureError("dequeueBuffer", status);

    if (status != NO_ERROR) { // FIXME: add proper error handling!
        native_window_set_buffer_count(ANW.get(), 3);
        return;
    }

    sp<android::GraphicBuffer> buf(new android::GraphicBuffer(anb, false));
    status |= ANW->lockBuffer(ANW.get(), buf->getNativeBuffer());
    checkSurfaceTextureError("lockBuffer", status);

    // Fill the buffer with the content of the bitmap
    uint8_t* img = 0;
    status |= buf->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, (void**)(&img));
    checkSurfaceTextureError("lock", status);

    if (status == NO_ERROR) {
        int row, col;
        int bpp = 4; // Now only deal with RGBA8888 format.

        bitmap.lockPixels();
        uint8_t* bitmapOrigin = static_cast<uint8_t*>(bitmap.getPixels());
        // Copied line by line since we need to handle the offsets and stride.
        for (row = 0 ; row < bitmap.height(); row ++) {
            uint8_t* dst = &(img[(buf->getStride() * (row + x) + y) * bpp]);
            uint8_t* src = &(bitmapOrigin[bitmap.width() * row * bpp]);
            memcpy(dst, src, bpp * bitmap.width());
        }
        bitmap.unlockPixels();
    }
    buf->unlock();
    status = ANW->queueBuffer(ANW.get(), buf->getNativeBuffer());
    checkSurfaceTextureError("queueBuffer", status);
}
#endif

void GLUtils::updateSharedSurfaceTextureWithBitmap(const TileRenderInfo* renderInfo, int x, int y, const SkBitmap& bitmap)
{
    if (!renderInfo
        || !renderInfo->textureInfo
        || !renderInfo->baseTile)
        return;

    TilesManager::instance()->transferQueue()->updateQueueWithBitmap(renderInfo, x, y, bitmap);
}

void GLUtils::createTextureWithBitmap(GLuint texture, const SkBitmap& bitmap, GLint filter)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    SkBitmap::Config config = bitmap.getConfig();
    int internalformat = getInternalFormat(config);
    int type = getType(config);
    bitmap.lockPixels();
    glTexImage2D(GL_TEXTURE_2D, 0, internalformat, bitmap.width(), bitmap.height(),
                 0, internalformat, type, bitmap.getPixels());
    bitmap.unlockPixels();
    if (GLUtils::checkGlError("glTexImage2D")) {
        XLOG("GL ERROR: glTexImage2D parameters are : bitmap.width() %d, bitmap.height() %d,"
             " internalformat 0x%x, type 0x%x, bitmap.getPixels() %p",
             bitmap.width(), bitmap.height(), internalformat, type, bitmap.getPixels());
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    // The following is a workaround -- remove when EGLImage texture upload is fixed.
    GLuint fboID;
    glGenFramebuffers(1, &fboID);
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glCheckFramebufferStatus(GL_FRAMEBUFFER); // should return GL_FRAMEBUFFER_COMPLETE

    glBindFramebuffer(GL_FRAMEBUFFER, 0); // rebind the standard FBO
    glDeleteFramebuffers(1, &fboID);
}

void GLUtils::updateTextureWithBitmap(GLuint texture, int x, int y, const SkBitmap& bitmap, GLint filter)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    SkBitmap::Config config = bitmap.getConfig();
    int internalformat = getInternalFormat(config);
    int type = getType(config);
    bitmap.lockPixels();
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, bitmap.width(), bitmap.height(),
                    internalformat, type, bitmap.getPixels());
    bitmap.unlockPixels();
    if (GLUtils::checkGlError("glTexSubImage2D")) {
        XLOG("GL ERROR: glTexSubImage2D parameters are : bitmap.width() %d, bitmap.height() %d,"
             " x %d, y %d, internalformat 0x%x, type 0x%x, bitmap.getPixels() %p",
             bitmap.width(), bitmap.height(), x, y, internalformat, type, bitmap.getPixels());
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
}

void GLUtils::createEGLImageFromTexture(GLuint texture, EGLImageKHR* image)
{
    EGLClientBuffer buffer = reinterpret_cast<EGLClientBuffer>(texture);
    static const EGLint attr[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE };
    *image = eglCreateImageKHR(eglGetCurrentDisplay(), eglGetCurrentContext(),
                               EGL_GL_TEXTURE_2D_KHR, buffer, attr);
    GLUtils::checkEglError("eglCreateImage", (*image != EGL_NO_IMAGE_KHR));
}

void GLUtils::createTextureFromEGLImage(GLuint texture, EGLImageKHR image, GLint filter)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
}

void GLUtils::convertToTransformationMatrix(const float* matrix, TransformationMatrix& transformMatrix)
{
    transformMatrix.setMatrix(
        matrix[0], matrix[1], matrix[2], matrix[3],
        matrix[4], matrix[5], matrix[6], matrix[7],
        matrix[8], matrix[9], matrix[10], matrix[11],
        matrix[12], matrix[13], matrix[14], matrix[15]);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
