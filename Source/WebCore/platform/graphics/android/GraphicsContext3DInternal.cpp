/*
 * Copyright (C) 2011, Sony Ericsson Mobile Communications AB
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Sony Ericsson Mobile Communications AB nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SONY ERICSSON MOBILE COMMUNICATIONS AB BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"

#include "GraphicsContext3DInternal.h"

#include "Frame.h"
#include "HostWindow.h"
#include "HTMLCanvasElement.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "PlatformGraphicsContext.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderObject.h"
#include "TilesManager.h"
#include "TransformationMatrix.h"
#include "WebViewCore.h"

#include "SkBitmap.h"
#include "SkDevice.h"
#include <hardware/hardware.h>
#include <surfaceflinger/ISurfaceComposer.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#include <surfaceflinger/IGraphicBufferAlloc.h>
#include <JNIUtility.h>

namespace WebCore {

#define CANVAS_MAX_WIDTH    1280
#define CANVAS_MAX_HEIGHT   1280

EGLint GraphicsContext3DInternal::checkEGLError(const char* s)
{
    EGLint error = eglGetError();
    if (error == EGL_SUCCESS) {
        LOGWEBGL("%s() OK", s);
    }
    else {
        LOGWEBGL("after %s() eglError = 0x%x", s, error);
    }

    return error;
}

GLint GraphicsContext3DInternal::checkGLError(const char* s)
{
    GLint error = glGetError();
    if (error == GL_NO_ERROR) {
        LOGWEBGL("%s() OK", s);
    }
    else {
        LOGWEBGL("after %s() glError (0x%x)", s, error);
    }

    return error;
}

GraphicsContext3DInternal::GraphicsContext3DInternal(HTMLCanvasElement* canvas,
                                                     GraphicsContext3D::Attributes attrs,
                                                     HostWindow* hostWindow)
    : m_compositingLayer(new WebGLLayer())
    , m_canvas(canvas)
    , m_attrs(attrs)
    , m_layerComposited(false)
    , m_canvasDirty(false)
    , m_width(1)
    , m_height(1)
    , m_maxwidth(CANVAS_MAX_WIDTH)
    , m_maxheight(CANVAS_MAX_HEIGHT)
    , m_dpy(EGL_NO_DISPLAY)
    , m_config(0)
    , m_surface(EGL_NO_SURFACE)
    , m_context(EGL_NO_CONTEXT)
    , m_currentIndex(-1)
    , m_syncTimer(this, &GraphicsContext3DInternal::syncTimerFired)
    , m_syncRequested(false)
    , m_extensions(0)
    , m_contextId(0)
{
    LOGWEBGL("GraphicsContext3DInternal() = %p", this);
    m_compositingLayer->ref();
    m_compositingLayer->setGraphicsContext(this);

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    WebViewCore* core = WebViewCore::getWebViewCore(m_canvas->document()->view());
    jobject tmp = core->getWebViewJavaObject();
    m_webView = env->NewGlobalRef(tmp);
    if (m_webView) {
        jclass webViewClass = env->GetObjectClass(m_webView);
        m_postInvalidate = env->GetMethodID(webViewClass, "postInvalidate", "(IIII)V");
        env->DeleteLocalRef(webViewClass);
    }

    if (!initEGL() ||
        !createContext(true))
        return;

    const char *ext = (const char *)glGetString(GL_EXTENSIONS);
    // Only willing to support GL_OES_texture_npot at this time
    m_extensions.set(new Extensions3DAndroid(strstr(ext, "GL_OES_texture_npot") ? "GL_OES_texture_npot" : ""));
    LOGWEBGL("GL_EXTENSIONS = %s", ext);

    // ANGLE initialization.
    ShBuiltInResources resources;
    ShInitBuiltInResources(&resources);

    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &resources.MaxVertexAttribs);
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &resources.MaxVertexUniformVectors);
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &resources.MaxVaryingVectors);
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &resources.MaxVertexTextureImageUnits);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &resources.MaxCombinedTextureImageUnits);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &resources.MaxTextureImageUnits);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &resources.MaxFragmentUniformVectors);

    resources.MaxDrawBuffers = 1;
    m_compiler.setResources(resources);

    m_eglCreateSyncKHR = (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
    m_eglDestroySyncKHR = (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
    m_eglClientWaitSyncKHR = (PFNEGLCLIENTWAITSYNCKHRPROC)eglGetProcAddress("eglClientWaitSyncKHR");

    m_savedViewport.x = 0;
    m_savedViewport.y = 0;
    m_savedViewport.width = m_width;
    m_savedViewport.height = m_height;

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    startSyncThread();

    static int contextCounter = 1;
    m_contextId = contextCounter++;
}

GraphicsContext3DInternal::~GraphicsContext3DInternal()
{
    LOGWEBGL("~GraphicsContext3DInternal()");

    stopSyncThread();

    MutexLocker lock(m_fboMutex);
    m_compositingLayer->setGraphicsContext(0);
    m_compositingLayer->unref();
    m_compositingLayer = 0;
    deleteContext(true);

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->DeleteGlobalRef(m_webView);
}

bool GraphicsContext3DInternal::initEGL()
{
    EGLBoolean returnValue;
    EGLint     majorVersion;
    EGLint     minorVersion;

    m_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_dpy == EGL_NO_DISPLAY)
        return false;

    returnValue = eglInitialize(m_dpy, &majorVersion, &minorVersion);
    if (returnValue != EGL_TRUE)
        return false;

    LOGWEBGL("EGL version %d.%d", majorVersion, minorVersion);
    const char *s = eglQueryString(m_dpy, EGL_VENDOR);
    LOGWEBGL("EGL_VENDOR = %s", s);
    s = eglQueryString(m_dpy, EGL_VERSION);
    LOGWEBGL("EGL_VERSION = %s", s);
    s = eglQueryString(m_dpy, EGL_EXTENSIONS);
    LOGWEBGL("EGL_EXTENSIONS = %s", s);
    s = eglQueryString(m_dpy, EGL_CLIENT_APIS);
    LOGWEBGL("EGL_CLIENT_APIS = %s", s);

    EGLint* config_attribs = new EGLint[21];
    int p = 0;
    config_attribs[p++] = EGL_BLUE_SIZE;
    config_attribs[p++] = 8;
    config_attribs[p++] = EGL_GREEN_SIZE;
    config_attribs[p++] = 8;
    config_attribs[p++] = EGL_RED_SIZE;
    config_attribs[p++] = 8;
    config_attribs[p++] = EGL_SURFACE_TYPE;
    // If the platform does not have common configurations for both PBUFFER and WINDOW,
    // we will have to create two separate configs. For now, one seems to be enough.
    config_attribs[p++] = EGL_PBUFFER_BIT | EGL_WINDOW_BIT;
    config_attribs[p++] = EGL_RENDERABLE_TYPE;
    config_attribs[p++] = EGL_OPENGL_ES2_BIT;
    config_attribs[p++] = EGL_ALPHA_SIZE;
    config_attribs[p++] = m_attrs.alpha ? 8 : 0;
    if (m_attrs.depth) {
        config_attribs[p++] = EGL_DEPTH_SIZE;
        config_attribs[p++] = 16;
    }
    if (m_attrs.stencil) {
        config_attribs[p++] = EGL_STENCIL_SIZE;
        config_attribs[p++] = 8;
    }
    if (m_attrs.antialias) {
        config_attribs[p++] = EGL_SAMPLE_BUFFERS;
        config_attribs[p++] = 1;
        config_attribs[p++] = EGL_SAMPLES;
        config_attribs[p++] = 4;
    }
    config_attribs[p] = EGL_NONE;

    EGLint num_configs = 0;
    return (eglChooseConfig(m_dpy, config_attribs, &m_config, 1, &num_configs) == EGL_TRUE);
}

EGLSurface GraphicsContext3DInternal::createPbufferSurface(int width, int height)
{
    LOGWEBGL("createPbufferSurface(%d, %d)", width, height);
    if (width == 0 || height == 0)
        width = height = 1;

    EGLint surface_attribs[] = {
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_NONE};

    EGLSurface surface = eglCreatePbufferSurface(m_dpy, m_config, surface_attribs);
    checkEGLError("eglCreatePbufferSurface");

    return surface;
}

bool GraphicsContext3DInternal::createContext(bool createEGLContext)
{
    LOGWEBGL("createContext()");
    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE};

    m_surface = createPbufferSurface(1, 1);

    if (createEGLContext) {
        m_context = eglCreateContext(m_dpy, m_config, EGL_NO_CONTEXT, context_attribs);
        EGLint error = checkEGLError("eglCreateContext");
        if (error == EGL_BAD_ALLOC) {
            // Probably too many contexts. Force a JS garbage collection, and then try again.
            // This typically only happens in Khronos Conformance tests.
            LOGWEBGL(" error == EGL_BAD_ALLOC: try again after GC");
            m_canvas->document()->frame()->script()->lowMemoryNotification();
            m_context = eglCreateContext(m_dpy, m_config, EGL_NO_CONTEXT, context_attribs);
            checkEGLError("eglCreateContext");
        }
    }

    makeContextCurrent();
    createFBO(&m_fbo[0], m_width > 0 ? m_width : 1, m_height > 0 ? m_height : 1);
    createFBO(&m_fbo[1], m_width > 0 ? m_width : 1, m_height > 0 ? m_height : 1);

    m_currentIndex = 0;
    m_boundFBO = currentFBO();
    m_frontFBO = 0;
    m_pendingFBO = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, m_boundFBO);

    return (m_context != EGL_NO_CONTEXT);
}

void GraphicsContext3DInternal::deleteContext(bool deleteEGLContext)
{
    LOGWEBGL("deleteContext(%s)", deleteEGLContext ? "true" : "false");

    makeContextCurrent();
    deleteFBO(&m_fbo[0]);
    deleteFBO(&m_fbo[1]);
    m_currentIndex = -1;
    m_frontFBO = 0;
    m_pendingFBO = 0;

    eglMakeCurrent(m_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (m_surface != EGL_NO_SURFACE) {
        eglDestroySurface(m_dpy, m_surface);
        m_surface = EGL_NO_SURFACE;
    }
    if (deleteEGLContext && m_context != EGL_NO_CONTEXT) {
        eglDestroyContext(m_dpy, m_context);
        m_context = EGL_NO_CONTEXT;
    }
}

void GraphicsContext3DInternal::makeContextCurrent()
{
    if (eglGetCurrentContext() != m_context && m_context != EGL_NO_CONTEXT) {
        eglMakeCurrent(m_dpy, m_surface, m_surface, m_context);
    }
}

GraphicsContext3D::Attributes GraphicsContext3DInternal::getContextAttributes()
{
    return m_attrs;
}

unsigned long GraphicsContext3DInternal::getError()
{
    if (m_syntheticErrors.size() > 0) {
        ListHashSet<unsigned long>::iterator iter = m_syntheticErrors.begin();
        unsigned long err = *iter;
        m_syntheticErrors.remove(iter);
        return err;
    }
    LOGWEBGL("glGetError()");
    makeContextCurrent();
    return glGetError();
}

void GraphicsContext3DInternal::synthesizeGLError(unsigned long error)
{
    m_syntheticErrors.add(error);
}

void GraphicsContext3DInternal::createFBO(fbo_t *fbo, int width, int height)
{
    LOGWEBGL("createFBO()");

    // 1. Allocate a graphic buffer
    sp<ISurfaceComposer> composer(ComposerService::getComposerService());
    sp<IGraphicBufferAlloc> mGraphicBufferAlloc = composer->createGraphicBufferAlloc();
    status_t error;
    fbo->m_grBuffer = mGraphicBufferAlloc->createGraphicBuffer(width, height,
                                                               HAL_PIXEL_FORMAT_RGBA_8888,
                                                               GRALLOC_USAGE_HW_TEXTURE, &error);
    if (fbo->m_grBuffer->initCheck() == NO_ERROR) {
        LOGWEBGL(" allocated GraphicBuffer");
    }
    else {
        LOGWEBGL(" failed to allocate GraphicBuffer, error = %d", error);
    }

    // 2. Create an EGLImage from the graphic buffer
    const EGLint attrs[] = {
        //        EGL_IMAGE_PRESERVED_KHR,    EGL_TRUE,
        EGL_NONE,                   EGL_NONE
    };
    ANativeWindowBuffer* clientBuf = fbo->m_grBuffer->getNativeBuffer();
    fbo->m_image = eglCreateImageKHR(m_dpy,
                                     EGL_NO_CONTEXT,
                                     EGL_NATIVE_BUFFER_ANDROID,
                                     (EGLClientBuffer)clientBuf,
                                     attrs);
    checkEGLError("eglCreateImageKHR");


    // 3. Create a texture from the EGLImage
    fbo->m_texture = createTexture(fbo->m_image, width, height);

    /*
    fbo->m_image = eglCreateImageKHR(m_dpy,
                                     EGL_NO_CONTEXT,
                                     EGL_GL_TEXTURE_2D_KHR,
                                     reinterpret_cast<EGLClientBuffer>(fbo->m_texture),
                                     attrs);
    checkEGLError("eglCreateImageKHR");
    */

    // 4. Create the Framebuffer Object from the texture
    glGenFramebuffers(1, &fbo->m_fbo);
    glGenRenderbuffers(1, &fbo->m_depthBuffer);

    glBindRenderbuffer(GL_RENDERBUFFER, fbo->m_depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    checkGLError("glRenderbufferStorage");
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo->m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo->m_texture, 0);
    checkGLError("glFramebufferTexture2D");
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fbo->m_depthBuffer);
    checkGLError("glFramebufferRenderbuffer");

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    checkGLError("glCheckFramebufferStatus");
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGWEBGL("Framebuffer incomplete: %d", status);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    fbo->m_state = FBO_STATE_FREE;
}

void GraphicsContext3DInternal::deleteFBO(fbo_t* fbo)
{
    LOGWEBGL("deleteFBO()");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (fbo->m_image) {
        eglDestroyImageKHR(m_dpy, fbo->m_image);
        checkEGLError("eglDestroyImageKHR");
    }

    if (fbo->m_texture)
        glDeleteTextures(1, &fbo->m_texture);
    if (fbo->m_depthBuffer)
        glDeleteRenderbuffers(1, &fbo->m_depthBuffer);
    if (fbo->m_fbo)
        glDeleteFramebuffers(1, &fbo->m_fbo);

    // FIXME: Does the eglImage own the GraphicBuffer?
    // fbo->m_grBuffer.clear();

    memset(fbo, 0, sizeof(fbo_t));
}

GLuint GraphicsContext3DInternal::createTexture(EGLImageKHR image, int width, int height)
{
    LOGWEBGL("createTexture(image = %p)", image);
    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (image) {
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image);
        checkGLError("glEGLImageTargetTexture2DOES");
    }
    else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        checkGLError("glTexImage2D()");
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    return texture;
}

void GraphicsContext3DInternal::startSyncThread()
{
    LOGWEBGL("+startSyncThread()");
    MutexLocker lock(m_threadMutex);
    m_threadState = THREAD_STATE_STOPPED;
    m_syncThread = createThread(syncThreadStart, this, "GraphicsContext3DInternal");
    // Wait for thread to start
    while (m_threadState != THREAD_STATE_RUN) {
        m_threadCondition.wait(m_threadMutex);
    }
    LOGWEBGL("-startSyncThread()");
}

void GraphicsContext3DInternal::stopSyncThread()
{
    LOGWEBGL("+stopSyncThread()");
    MutexLocker lock(m_threadMutex);
    if (m_syncThread) {
        m_threadState = THREAD_STATE_STOP;
        // Signal thread to wake up
        m_threadCondition.broadcast();
        // Wait for thread to stop
        while (m_threadState != THREAD_STATE_STOPPED) {
            m_threadCondition.wait(m_threadMutex);
        }
        m_syncThread = 0;
    }
    LOGWEBGL("-stopSyncThread()");
}

void* GraphicsContext3DInternal::syncThreadStart(void* ctx)
{    
    GraphicsContext3DInternal* context = static_cast<GraphicsContext3DInternal*>(ctx);
    context->runSyncThread();

    return 0;
}

void GraphicsContext3DInternal::runSyncThread()
{
    LOGWEBGL("SyncThread: starting");
    fbo_t* fbo = 0;

    MutexLocker lock(m_threadMutex);
    m_threadState = THREAD_STATE_RUN;
    // Signal to creator that we are up and running
    m_threadCondition.broadcast();

    while (m_threadState == THREAD_STATE_RUN) {
        while (m_threadState == THREAD_STATE_RUN) {
            {
                MutexLocker lock(m_fboMutex);
                if (m_pendingFBO != 0 && m_pendingFBO->m_state == FBO_STATE_LOCKED) {
                    fbo = m_pendingFBO;
                    break;
                }
            }
            m_threadCondition.wait(m_threadMutex);
        }
        LOGWEBGL("SyncThread: woke after waiting for FBO, m_pendingFBO = %p", fbo);
        if (m_threadState != THREAD_STATE_RUN)
            break;

        m_eglClientWaitSyncKHR(m_dpy, fbo->m_sync, 0, 0);
        m_eglDestroySyncKHR(m_dpy, fbo->m_sync);
        LOGWEBGL("SyncThread: returned after waiting for Sync");

        {
            MutexLocker lock(m_fboMutex);
            m_pendingFBO = 0;
            fbo->m_sync = 0;
            fbo->m_state = FBO_STATE_FREE;
            m_frontFBO = fbo;
            m_layerComposited = true;
            m_fboCondition.broadcast();
        }
    
        // Invalidate the canvas region
        RenderObject* renderer = m_canvas->renderer();
        if (renderer && renderer->isBox() && m_postInvalidate) {
            IntRect rect = ((RenderBox*)renderer)->absoluteContentBox();
            LOGWEBGL("SyncThread: invalidating [%d, %d, %d, %d]", rect.x(), rect.y(), rect.width(), rect.height());
            JNIEnv* env = JSC::Bindings::getJNIEnv();
            env->CallVoidMethod(m_webView, m_postInvalidate, rect.x(), rect.y(),
                                rect.x() + rect.width(), rect.y() + rect.height());
        }
    }

    // Signal to calling thread that we have stopped
    m_threadState = THREAD_STATE_STOPPED;
    m_threadCondition.broadcast();
    LOGWEBGL("SyncThread: terminating");
}

PlatformLayer* GraphicsContext3DInternal::platformLayer() const
{
    LOGWEBGL("platformLayer()");
    return m_compositingLayer;
}

void GraphicsContext3DInternal::reshape(int width, int height)
{
    LOGWEBGL("reshape(%d, %d)", width, height);
    //bool mustRestoreFBO = (m_boundFBO != currentFBO());

    m_width = width > m_maxwidth ? m_maxwidth : width;
    m_height = height > m_maxheight ? m_maxheight : height;

    stopSyncThread();
    makeContextCurrent();
    {
        MutexLocker lock(m_fboMutex);
        deleteContext(false);
        createContext(false);

        m_currentIndex = 0;
        //if (!mustRestoreFBO) {
        m_boundFBO = currentFBO();
        //}
        glBindFramebuffer(GL_FRAMEBUFFER, m_boundFBO);
    }
    startSyncThread();
}

void GraphicsContext3DInternal::recreateSurface()
{
    LOGWEBGL("recreateSurface()");
    if (m_currentIndex >= 0)
        // We already have a current surface
        return;
    reshape(m_width, m_height);
    glViewport(m_savedViewport.x, m_savedViewport.y, m_savedViewport.width, m_savedViewport.height);
}

void GraphicsContext3DInternal::releaseSurface()
{
    LOGWEBGL("releaseSurface(%d)", m_contextId);
    MutexLocker lock(m_fboMutex);
    if (m_currentIndex < 0)
        // We don't have any current surface
        return;
    stopSyncThread();
    deleteContext(false);
}

void GraphicsContext3DInternal::syncTimerFired(Timer<GraphicsContext3DInternal>*)
{
    m_syncRequested = false;
    swapBuffers();
}

void GraphicsContext3DInternal::markContextChanged()
{
    if (!m_syncRequested) {
        m_syncTimer.startOneShot(0);
        m_syncRequested = true;
    }
    m_canvasDirty = true;
    m_layerComposited = false;
}

void GraphicsContext3DInternal::swapBuffers()
{
    LOGWEBGL("+swapBuffers()");

    if (m_currentIndex < 0)
        return;

    makeContextCurrent();
    MutexLocker lock(m_fboMutex);
    fbo_t* fbo = &m_fbo[m_currentIndex];
    LOGWEBGL("swap: currentIndex = %d, currentFBO = %p", m_currentIndex, fbo);
    while (m_pendingFBO != 0) {
        m_fboCondition.wait(m_fboMutex);
    }
    bool mustRestoreFBO = (m_boundFBO != fbo->m_fbo);
    if (mustRestoreFBO) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo->m_fbo);
    }

    // Create the fence sync and notify the sync thread
    fbo->m_sync = m_eglCreateSyncKHR(m_dpy, EGL_SYNC_FENCE_KHR, 0);
    glFlush();
    m_pendingFBO = fbo;
    m_pendingFBO->m_state = FBO_STATE_LOCKED;
    m_threadCondition.broadcast();

    // Dequeue a new buffer
    int index = m_currentIndex ? 0 : 1;
    fbo = &m_fbo[index];
    while (fbo->m_state != FBO_STATE_FREE) {
        m_fboCondition.wait(m_fboMutex);
    }
    m_currentIndex = index;
    LOGWEBGL("swap: currentIndex = %d, currentFBO = %p", m_currentIndex, fbo);
    fbo->m_state = FBO_STATE_DEQUEUED;

    if (!mustRestoreFBO) {
        m_boundFBO = fbo->m_fbo;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, m_boundFBO);
    m_canvasDirty = false;
    LOGWEBGL("-swapBuffers()");
}

GLuint GraphicsContext3DInternal::lockFrontTexture(SkRect& rect)
{
    LOGWEBGL("+GraphicsContext3DInternal::lockFrontTexture()");
    MutexLocker lock(m_fboMutex);

    fbo_t *fbo = m_frontFBO;
    if (!fbo || !fbo->m_image) {
        LOGWEBGL("-GraphicsContext3DInternal::lockFrontTexture(), fbo = %p", fbo);
        return false;
    }

    // If necessary, create the texture
    if (fbo->m_drawingTexture == 0)
        fbo->m_drawingTexture = createTexture(fbo->m_image, m_width, m_height);

    RenderObject* renderer = m_canvas->renderer();
    if (renderer && renderer->isBox()) {
        RenderBox* box = (RenderBox*)renderer; 
        rect.set(box->borderLeft() + box->paddingLeft(),
                 box->borderTop() + box->paddingTop(),
                 box->borderLeft() + box->paddingLeft() + box->contentWidth(),
                 box->borderTop() + box->paddingTop() + box->contentHeight());
    }

    LOGWEBGL("-GraphicsContext3DInternal::lockFrontTexture()");
    return fbo->m_drawingTexture;
}

void GraphicsContext3DInternal::releaseFrontTexture(GLuint texture)
{
    LOGWEBGL("+GraphicsContext3DInternal::releaseFrontTexture()");
    MutexLocker lock(m_fboMutex);

    LOGWEBGL("-GraphicsContext3DInternal::releaseFrontTexture()");
}

void GraphicsContext3DInternal::paintRenderingResultsToCanvas(CanvasRenderingContext* context)
{
    LOGWEBGL("+paintRenderingResultsToCanvas()");
    ImageBuffer* imageBuffer = context->canvas()->buffer();
    const SkBitmap& canvasBitmap = imageBuffer->context()->platformContext()->mCanvas->getDevice()->accessBitmap(false);
    SkCanvas canvas(canvasBitmap);

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, m_width, m_height);
    bitmap.allocPixels();
    unsigned char *pixels = static_cast<unsigned char*>(bitmap.getPixels());
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    SkRect  dstRect;
    dstRect.iset(0, 0, imageBuffer->size().width(), imageBuffer->size().height());
    canvas.drawBitmapRect(bitmap, 0, dstRect);
    LOGWEBGL("-paintRenderingResultsToCanvas()");
}

PassRefPtr<ImageData> GraphicsContext3DInternal::paintRenderingResultsToImageData()
{
    LOGWEBGL("+paintRenderingResultsToImageData()");
    RefPtr<ImageData> imageData = ImageData::create(IntSize(m_width, m_height));
    unsigned char* pixels = imageData->data()->data()->data();

    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    LOGWEBGL("-paintRenderingResultsToImageData()");

    return imageData;
}

bool GraphicsContext3DInternal::paintCompositedResultsToCanvas(CanvasRenderingContext* context)
{
    LOGWEBGL("paintCompositedResultsToCanvas()");
    ImageBuffer* imageBuffer = context->canvas()->buffer();
    const SkBitmap& canvasBitmap = imageBuffer->context()->platformContext()->mCanvas->getDevice()->accessBitmap(false);
    SkCanvas canvas(canvasBitmap);

    MutexLocker lock(m_fboMutex);

    fbo_t *fbo = m_frontFBO;
    if (!fbo || !fbo->m_grBuffer.get())
        return false;

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, m_width, m_height, fbo->m_grBuffer->getStride() * 4);

    unsigned char* bits = NULL;
    if (fbo->m_grBuffer->lock(GraphicBuffer::USAGE_SW_READ_RARELY, (void**)&bits) == NO_ERROR) {
        bitmap.setPixels(bits);

        SkRect  dstRect;
        dstRect.iset(0, 0, imageBuffer->size().width(), imageBuffer->size().height());
        canvas.save();
        canvas.translate(0, SkIntToScalar(imageBuffer->size().height()));
        canvas.scale(SK_Scalar1, -SK_Scalar1);
        canvas.drawBitmapRect(bitmap, 0, dstRect);
        canvas.restore();
        bitmap.setPixels(0);
        fbo->m_grBuffer->unlock();
    }

    return true;
}

void GraphicsContext3DInternal::bindFramebuffer(GC3Denum target, Platform3DObject buffer)
{
    LOGWEBGL("bindFrameBuffer()");
    makeContextCurrent();
    if (!buffer) {
        buffer = currentFBO();
    }
    glBindFramebuffer(target, buffer);
    m_boundFBO = buffer;
}

void GraphicsContext3DInternal::compileShader(Platform3DObject shader)
{
    LOGWEBGL("compileShader()");
    HashMap<Platform3DObject, ShaderSourceEntry>::iterator result = m_shaderSourceMap.find(shader);
    if (result == m_shaderSourceMap.end()) {
        LOGWEBGL("  shader not found");
        return;
    }
    ShaderSourceEntry& entry = result->second;

    int shaderType;
    glGetShaderiv(shader, GL_SHADER_TYPE, &shaderType);

    ANGLEShaderType ast = shaderType == GL_VERTEX_SHADER ? SHADER_TYPE_VERTEX : SHADER_TYPE_FRAGMENT;

    String src;
    String log;
    bool isValid = m_compiler.validateShaderSource(entry.source.utf8().data(), ast, src, log);

    entry.log = log;
    entry.isValid = isValid;

    if (!isValid) {
        LOGWEBGL("  shader validation failed");
        return;
    }
    int len = src.length();
    CString cstr = src.utf8();
    const char* s = cstr.data();

    LOGWEBGL("glShaderSource(%s)", s);
    glShaderSource(shader, 1, &s, &len);
 
    LOGWEBGL("glCompileShader()");
    glCompileShader(shader);
}

String GraphicsContext3DInternal::getShaderInfoLog(Platform3DObject shader)
{
    LOGWEBGL("getShaderInfoLog()");
    HashMap<Platform3DObject, ShaderSourceEntry>::iterator result = m_shaderSourceMap.find(shader);

    if (result == m_shaderSourceMap.end()) {
        LOGWEBGL("  shader not found");
        return "";
    }

    ShaderSourceEntry entry = result->second;

    if (entry.isValid) {
        LOGWEBGL("  validated shader, retrieve OpenGL log");
        GLuint shaderID = shader;
        GLint logLength;
        glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLength);
        if (!logLength)
            return "";

        char* log = 0;
        if ((log = (char *)fastMalloc(logLength * sizeof(char))) == 0)
            return "";
        GLsizei returnedLogLength;
        glGetShaderInfoLog(shaderID, logLength, &returnedLogLength, log);
        String res = String(log, returnedLogLength);
        fastFree(log);

        return res;
    }
    else {
        LOGWEBGL("  non-validated shader, use ANGLE log");
        return entry.log;
    }
}

String GraphicsContext3DInternal::getShaderSource(Platform3DObject shader)
{
    LOGWEBGL("getShaderSource()");
    HashMap<Platform3DObject, ShaderSourceEntry>::iterator result = m_shaderSourceMap.find(shader);

    if (result == m_shaderSourceMap.end())
        return "";

    return result->second.source;
}

void GraphicsContext3DInternal::shaderSource(Platform3DObject shader, const String& string)
{
    LOGWEBGL("shaderSource()");
    ShaderSourceEntry entry;

    entry.source = string;

    m_shaderSourceMap.set(shader, entry);
    LOGWEBGL("entry.source = %s", entry.source.utf8().data());
}

void GraphicsContext3DInternal::viewport(long x, long y, unsigned long width, unsigned long height)
{
    LOGWEBGL("glViewport(%d, %d, %d, %d)", x, y, width, height);
    glViewport(x, y, width, height);
    m_savedViewport.x = x;
    m_savedViewport.y = y;
    m_savedViewport.width = width;
    m_savedViewport.height = height;
}
}
