/*
 * Copyright (C) 2011, 2012, Sony Ericsson Mobile Communications AB
 * Copyright (C) 2012 Sony Mobile Communications AB
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

#if ENABLE(WEBGL)
namespace WebCore {

class FBO {
public:
    static FBO* createFBO(EGLDisplay dpy, int width, int height);
    ~FBO();

    EGLSyncKHR sync() { return m_sync; }
    void setSync(EGLSyncKHR sync) { m_sync = sync; }

    GLuint fbo() { return m_fbo; }
    EGLImageKHR image() { return m_image; }

    bool isLocked() { return m_locked; }
    void setLocked(bool locked) { m_locked = locked; }

    bool lockGraphicBuffer(void** ptr) {
        return (m_grBuffer.get() &&
                (m_grBuffer->lock(GraphicBuffer::USAGE_SW_READ_RARELY, ptr) == NO_ERROR));
    }
    void unlockGraphicBuffer() {
        if (m_grBuffer.get())
            m_grBuffer->unlock();
    }

    int bytesPerRow() { return m_grBuffer.get() ? m_grBuffer->getStride() * 4 : 0; }

private:
    FBO(EGLDisplay dpy);
    bool init(int width, int height);
    GLuint createTexture(EGLImageKHR image, int width, int height);

    EGLDisplay  m_dpy;
    GLuint      m_texture;
    GLuint      m_fbo;
    GLuint      m_depthBuffer;
    EGLImageKHR m_image;
    EGLSyncKHR  m_sync;
    sp<IGraphicBufferAlloc> m_graphicBufferAlloc;
    sp<GraphicBuffer> m_grBuffer;
    bool        m_locked;
};


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
    : m_proxy(new GraphicsContext3DProxy())
    , m_compositingLayer(new WebGLLayer(m_proxy.get()))
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
    , m_syncThread(0)
    , m_threadState(THREAD_STATE_STOPPED)
    , m_syncTimer(this, &GraphicsContext3DInternal::syncTimerFired)
    , m_syncRequested(false)
    , m_extensions(0)
    , m_contextId(0)
{
    LOGWEBGL("GraphicsContext3DInternal() = %p, m_compositingLayer = %p", this, m_compositingLayer);
    m_compositingLayer->ref();
    m_proxy->setGraphicsContext(this);

    if (!m_canvas || !m_canvas->document() || !m_canvas->document()->view())
        return;
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    WebViewCore* core = WebViewCore::getWebViewCore(m_canvas->document()->view());
    if (!core)
        return;
    jobject tmp = core->getWebViewJavaObject();
    m_webView = env->NewGlobalRef(tmp);
    if (!m_webView)
        return;
    jclass webViewClass = env->GetObjectClass(m_webView);
    m_postInvalidate = env->GetMethodID(webViewClass, "postInvalidate", "()V");
    env->DeleteLocalRef(webViewClass);
    if (!m_postInvalidate)
        return;

    if (!initEGL())
        return;

    if (!createContext(true)) {
        LOGWEBGL("Create context failed. Perform JS garbage collection and try again.");
        // Probably too many contexts. Force a JS garbage collection, and then try again.
        // This typically only happens in Khronos Conformance tests.
        m_canvas->document()->frame()->script()->lowMemoryNotification();
        if (!createContext(true)) {
            LOGWEBGL("Create context still failed: aborting.");
            return;
        }
    }

    const char *ext = (const char *)glGetString(GL_EXTENSIONS);
    LOGWEBGL("GL_EXTENSIONS = %s", ext);
    // Want to keep control of which extensions are used
    String extensions = "";
    if (strstr(ext, "GL_OES_texture_npot"))
        extensions.append("GL_OES_texture_npot");
    if (strstr(ext, "GL_OES_packed_depth_stencil"))
        extensions.append(" GL_OES_packed_depth_stencil");
    if (strstr(ext, "GL_OES_texture_float"))
        extensions.append(" GL_OES_texture_float");
    m_extensions.set(new Extensions3DAndroid(extensions));

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
    LOGWEBGL("~GraphicsContext3DInternal(), this = %p", this);

    stopSyncThread();

    m_proxy->setGraphicsContext(0);
    MutexLocker lock(m_fboMutex);
    m_compositingLayer->unref();
    m_compositingLayer = 0;
    deleteContext(true);

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    env->DeleteGlobalRef(m_webView);
}

bool GraphicsContext3DInternal::initEGL()
{
    m_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_dpy == EGL_NO_DISPLAY)
        return false;

    EGLint     majorVersion;
    EGLint     minorVersion;
    EGLBoolean returnValue = eglInitialize(m_dpy, &majorVersion, &minorVersion);
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
    config_attribs[p++] = EGL_PBUFFER_BIT;
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

bool GraphicsContext3DInternal::createContext(bool createEGLContext)
{
    LOGWEBGL("createContext()");
    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE};

    if (createEGLContext) {
        EGLint surface_attribs[] = {
            EGL_WIDTH, 1,
            EGL_HEIGHT, 1,
            EGL_NONE};
        m_surface = eglCreatePbufferSurface(m_dpy, m_config, surface_attribs);
        m_context = eglCreateContext(m_dpy, m_config, EGL_NO_CONTEXT, context_attribs);
    }
    if (m_context == EGL_NO_CONTEXT) {
        deleteContext(createEGLContext);
        return false;
    }

    makeContextCurrent();
    for (int i = 0; i < NUM_BUFFERS; i++) {
        FBO* tmp = FBO::createFBO(m_dpy, m_width > 0 ? m_width : 1, m_height > 0 ? m_height : 1);
        if (tmp == 0) {
            LOGWEBGL("Failed to create FBO");
            deleteContext(createEGLContext);
            return false;
        }
        m_fbo[i] = tmp;
        m_freeBuffers.append(tmp);
    }

    m_currentFBO = dequeueBuffer();
    m_boundFBO = m_currentFBO->fbo();
    m_frontFBO = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, m_boundFBO);

    return true;
}

void GraphicsContext3DInternal::deleteContext(bool deleteEGLContext)
{
    LOGWEBGL("deleteContext(%s)", deleteEGLContext ? "true" : "false");

    makeContextCurrent();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_freeBuffers.clear();
    m_queuedBuffers.clear();
    m_preparedBuffers.clear();
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (m_fbo[i]) {
            delete m_fbo[i];
            m_fbo[i] = 0;
        }
    }
    m_currentFBO = 0;
    m_frontFBO = 0;

    eglMakeCurrent(m_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (deleteEGLContext) {
        if (m_surface != EGL_NO_SURFACE) {
            eglDestroySurface(m_dpy, m_surface);
            m_surface = EGL_NO_SURFACE;
        }
        if (m_context != EGL_NO_CONTEXT) {
            eglDestroyContext(m_dpy, m_context);
            m_context = EGL_NO_CONTEXT;
        }
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

FBO* FBO::createFBO(EGLDisplay dpy, int width, int height)
{
    LOGWEBGL("createFBO()");
    FBO* fbo = new FBO(dpy);

    if (!fbo->init(width, height)) {
        delete fbo;
        return 0;
    }
    return fbo;
}

FBO::FBO(EGLDisplay dpy)
    : m_dpy(dpy)
    , m_texture(0)
    , m_fbo(0)
    , m_depthBuffer(0)
    , m_image(0)
    , m_sync(0)
    , m_grBuffer(0)
    , m_locked(false)
{
}

bool FBO::init(int width, int height)
{
    // 1. Allocate a graphic buffer
    sp<ISurfaceComposer> composer(ComposerService::getComposerService());
    m_graphicBufferAlloc = composer->createGraphicBufferAlloc();

    status_t error;
    m_grBuffer = m_graphicBufferAlloc->createGraphicBuffer(width, height,
                                                           HAL_PIXEL_FORMAT_RGBA_8888,
                                                           GRALLOC_USAGE_HW_TEXTURE, &error);
    if (error != NO_ERROR) {
        LOGWEBGL(" failed to allocate GraphicBuffer, error = %d", error);
        return false;
    }

    void *addr = 0;
    if (m_grBuffer->lock(GRALLOC_USAGE_SW_WRITE_RARELY, &addr) != NO_ERROR) {
        LOGWEBGL("  failed to lock the GraphicBuffer");
        return false;
    }
    // WebGL requires all buffers to be initialized to 0.
    memset(addr, 0, width * height * 4);
    m_grBuffer->unlock();

    ANativeWindowBuffer* clientBuf = m_grBuffer->getNativeBuffer();
    if (clientBuf->handle == 0) {
        LOGWEBGL(" empty handle in GraphicBuffer");
        return false;
    }

    // 2. Create an EGLImage from the graphic buffer
    const EGLint attrs[] = {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE,                EGL_NONE
    };
    m_image = eglCreateImageKHR(m_dpy,
                                EGL_NO_CONTEXT,
                                EGL_NATIVE_BUFFER_ANDROID,
                                (EGLClientBuffer)clientBuf,
                                attrs);
    if (GraphicsContext3DInternal::checkEGLError("eglCreateImageKHR") != EGL_SUCCESS) {
        LOGWEBGL("eglCreateImageKHR() failed");
        return false;
    }

    // 3. Create a texture from the EGLImage
    m_texture = createTexture(m_image, width, height);
    if (m_texture == 0) {
        LOGWEBGL("createTexture() failed");
        return false;
    }

    // 4. Create the Framebuffer Object from the texture
    glGenFramebuffers(1, &m_fbo);
    glGenRenderbuffers(1, &m_depthBuffer);

    glBindRenderbuffer(GL_RENDERBUFFER, m_depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    if (GraphicsContext3DInternal::checkGLError("glRenderbufferStorage") != GL_NO_ERROR) {
        return false;
    }
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    if (GraphicsContext3DInternal::checkGLError("glFramebufferTexture2D") != GL_NO_ERROR) {
        return false;
    }
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthBuffer);
    if (GraphicsContext3DInternal::checkGLError("glFramebufferRenderbuffer") != GL_NO_ERROR) {
        return false;
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGWEBGL("Framebuffer incomplete: %d", status);
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

FBO::~FBO()
{
    LOGWEBGL("FBO::~FBO()");
    if (m_image) {
        eglDestroyImageKHR(m_dpy, m_image);
        GraphicsContext3DInternal::checkEGLError("eglDestroyImageKHR");
    }
    if (m_texture)
        glDeleteTextures(1, &m_texture);
    if (m_depthBuffer)
        glDeleteRenderbuffers(1, &m_depthBuffer);
    if (m_fbo)
        glDeleteFramebuffers(1, &m_fbo);
}

GLuint FBO::createTexture(EGLImageKHR image, int width, int height)
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

    bool error = false;
    if (image) {
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image);
        error = (GraphicsContext3DInternal::checkGLError("glEGLImageTargetTexture2DOES")
                 != GL_NO_ERROR);
    }
    else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        error = (GraphicsContext3DInternal::checkGLError("glTexImage2D()") != GL_NO_ERROR);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }

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
    FBO* fbo = 0;

    MutexLocker lock(m_threadMutex);
    m_threadState = THREAD_STATE_RUN;
    // Signal to creator that we are up and running
    m_threadCondition.broadcast();

    while (m_threadState == THREAD_STATE_RUN) {
        while (m_threadState == THREAD_STATE_RUN) {
            {
                MutexLocker lock(m_fboMutex);
                if (!m_queuedBuffers.isEmpty()) {
                    fbo = m_queuedBuffers.takeFirst();
                    break;
                }
            }
            m_threadCondition.wait(m_threadMutex);
        }
        LOGWEBGL("SyncThread: woke after waiting for FBO, fbo = %p", fbo);
        if (m_threadState != THREAD_STATE_RUN)
            break;

        if (fbo->sync() != EGL_NO_SYNC_KHR) {
            eglClientWaitSyncKHR(m_dpy, fbo->sync(), 0, 0);
            eglDestroySyncKHR(m_dpy, fbo->sync());
            fbo->setSync(EGL_NO_SYNC_KHR);
            LOGWEBGL("SyncThread: returned after waiting for Sync");
        }

        {
            MutexLocker lock(m_fboMutex);
            m_preparedBuffers.append(fbo);
            LOGWEBGL("SyncThread: prepared buffer = %p", fbo);
            updateFrontBuffer();
        }

        // Invalidate the canvas region
        if (m_postInvalidate) {
            JNIEnv* env = JSC::Bindings::getJNIEnv();
            env->CallVoidMethod(m_webView, m_postInvalidate);
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
    bool mustRestoreFBO = (m_boundFBO != (m_currentFBO ? m_currentFBO->fbo() : 0));

    m_width = width > m_maxwidth ? m_maxwidth : width;
    m_height = height > m_maxheight ? m_maxheight : height;

    stopSyncThread();
    makeContextCurrent();
    m_proxy->setGraphicsContext(0);
    {
        MutexLocker lock(m_fboMutex);
        deleteContext(false);

        if (createContext(false)) {
            if (!mustRestoreFBO) {
                m_boundFBO = m_currentFBO->fbo();
            }
            glBindFramebuffer(GL_FRAMEBUFFER, m_boundFBO);
        }
    }
    m_proxy->setGraphicsContext(this);
    startSyncThread();
}

void GraphicsContext3DInternal::recreateSurface()
{
    LOGWEBGL("recreateSurface()");
    if (m_currentFBO != 0)
        // We already have a current surface
        return;
    reshape(m_width, m_height);
    glViewport(m_savedViewport.x, m_savedViewport.y, m_savedViewport.width, m_savedViewport.height);
}

void GraphicsContext3DInternal::releaseSurface()
{
    LOGWEBGL("releaseSurface(%d)", m_contextId);
    if (m_currentFBO == 0)
        // We don't have any current surface
        return;
    stopSyncThread();
    m_proxy->setGraphicsContext(0);
    {
        MutexLocker lock(m_fboMutex);
        deleteContext(false);
    }
    makeContextCurrent();
    m_proxy->setGraphicsContext(this);
}

void GraphicsContext3DInternal::syncTimerFired(Timer<GraphicsContext3DInternal>*)
{
    m_syncRequested = false;

    // Do not perform the composition step if it is an offscreen canvas
    if (m_canvas->renderer())
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

/*
 * Must hold m_fboMutex when calling this function.
 */
FBO* GraphicsContext3DInternal::dequeueBuffer()
{
    LOGWEBGL("GraphicsContext3DInternal::dequeueBuffer()");
    while (m_freeBuffers.isEmpty()) {
        m_fboCondition.wait(m_fboMutex);
    }
    FBO* fbo = m_freeBuffers.takeFirst();

    if (fbo->sync() != EGL_NO_SYNC_KHR) {
        eglClientWaitSyncKHR(m_dpy, fbo->sync(), 0, 0);
        eglDestroySyncKHR(m_dpy, fbo->sync());
        fbo->setSync(EGL_NO_SYNC_KHR);
    }

    return fbo;
}

void GraphicsContext3DInternal::swapBuffers()
{
    LOGWEBGL("+swapBuffers()");

    MutexLocker lock(m_fboMutex);
    FBO* fbo = m_currentFBO;
    if (fbo == 0)
        return;

    makeContextCurrent();

    bool mustRestoreFBO = (m_boundFBO != fbo->fbo());
    if (mustRestoreFBO) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo());
    }

    // Create the fence sync and notify the sync thread
    fbo->setSync(eglCreateSyncKHR(m_dpy, EGL_SYNC_FENCE_KHR, 0));
    glFlush();
    m_queuedBuffers.append(fbo);
    m_threadCondition.broadcast();

    // Dequeue a new buffer
    fbo = dequeueBuffer();
    m_currentFBO = fbo;

    if (!mustRestoreFBO) {
        m_boundFBO = m_currentFBO->fbo();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, m_boundFBO);
    m_canvasDirty = false;
    m_layerComposited = true;
    LOGWEBGL("-swapBuffers()");
}

void GraphicsContext3DInternal::updateFrontBuffer()
{
    if (!m_preparedBuffers.isEmpty()) {
        if (m_frontFBO == 0) {
            m_frontFBO = m_preparedBuffers.takeFirst();
        }
        else if (!m_frontFBO->isLocked()) {
            m_freeBuffers.append(m_frontFBO);
            m_frontFBO = m_preparedBuffers.takeFirst();
            m_fboCondition.broadcast();
        }
    }
}

bool GraphicsContext3DInternal::lockFrontBuffer(EGLImageKHR& image, SkRect& rect)
{
    LOGWEBGL("GraphicsContext3DInternal::lockFrontBuffer()");
    MutexLocker lock(m_fboMutex);
    FBO* fbo = m_frontFBO;

    if (!fbo || !fbo->image()) {
        LOGWEBGL("-GraphicsContext3DInternal::lockFrontBuffer(), fbo = %p", fbo);
        return false;
    }

    fbo->setLocked(true);
    image = fbo->image();

    RenderObject* renderer = m_canvas->renderer();
    if (renderer && renderer->isBox()) {
        RenderBox* box = (RenderBox*)renderer;
        rect.setXYWH(box->borderLeft() + box->paddingLeft(),
                     box->borderTop() + box->paddingTop(),
                     box->contentWidth(),
                     box->contentHeight());
    }

    return true;
}

void GraphicsContext3DInternal::releaseFrontBuffer()
{
    LOGWEBGL("GraphicsContext3DInternal::releaseFrontBuffer()");
    MutexLocker lock(m_fboMutex);
    FBO* fbo = m_frontFBO;

    if (fbo) {
        fbo->setLocked(false);
        if (fbo->sync() != EGL_NO_SYNC_KHR) {
            eglDestroySyncKHR(m_dpy, fbo->sync());
        }
        fbo->setSync(eglCreateSyncKHR(m_dpy, EGL_SYNC_FENCE_KHR, 0));
    }
    updateFrontBuffer();
}

void GraphicsContext3DInternal::paintRenderingResultsToCanvas(CanvasRenderingContext* context)
{
    LOGWEBGL("paintRenderingResultsToCanvas()");
    ImageBuffer* imageBuffer = context->canvas()->buffer();
    const SkBitmap& canvasBitmap =
        imageBuffer->context()->platformContext()->mCanvas->getDevice()->accessBitmap(false);
    SkCanvas canvas(canvasBitmap);

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, m_width, m_height);
    bitmap.allocPixels();
    unsigned char *pixels = static_cast<unsigned char*>(bitmap.getPixels());
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    SkRect  dstRect;
    dstRect.iset(0, 0, imageBuffer->size().width(), imageBuffer->size().height());
    canvas.drawBitmapRect(bitmap, 0, dstRect);
}

PassRefPtr<ImageData> GraphicsContext3DInternal::paintRenderingResultsToImageData()
{
    LOGWEBGL("paintRenderingResultsToImageData()");
    RefPtr<ImageData> imageData = ImageData::create(IntSize(m_width, m_height));
    unsigned char* pixels = imageData->data()->data()->data();

    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    return imageData;
}

bool GraphicsContext3DInternal::paintCompositedResultsToCanvas(CanvasRenderingContext* context)
{
    LOGWEBGL("paintCompositedResultsToCanvas()");
    ImageBuffer* imageBuffer = context->canvas()->buffer();
    const SkBitmap& canvasBitmap =
        imageBuffer->context()->platformContext()->mCanvas->getDevice()->accessBitmap(false);
    SkCanvas canvas(canvasBitmap);

    MutexLocker lock(m_fboMutex);

    FBO* fbo = m_frontFBO;
    if (!fbo)
        return false;

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, m_width, m_height, fbo->bytesPerRow());

    unsigned char* bits = NULL;
    if (fbo->lockGraphicBuffer((void**)&bits)) {
        bitmap.setPixels(bits);

        SkRect  dstRect;
        dstRect.iset(0, 0, imageBuffer->size().width(), imageBuffer->size().height());
        canvas.save();
        canvas.translate(0, SkIntToScalar(imageBuffer->size().height()));
        canvas.scale(SK_Scalar1, -SK_Scalar1);
        canvas.drawBitmapRect(bitmap, 0, dstRect);
        canvas.restore();
        bitmap.setPixels(0);
        fbo->unlockGraphicBuffer();
    }

    return true;
}

void GraphicsContext3DInternal::bindFramebuffer(GC3Denum target, Platform3DObject buffer)
{
    LOGWEBGL("glBindFrameBuffer(%d, %d)", target, buffer);
    makeContextCurrent();
    MutexLocker lock(m_fboMutex);
    if (!buffer && m_currentFBO) {
        buffer = m_currentFBO->fbo();
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

    ANGLEShaderType ast = shaderType == GL_VERTEX_SHADER ?
        SHADER_TYPE_VERTEX : SHADER_TYPE_FRAGMENT;

    String src;
    String log;
    bool isValid = m_compiler.validateShaderSource(entry.source.utf8().data(), ast, src, log);

    entry.log = log;
    entry.isValid = isValid;

    if (!isValid) {
        LOGWEBGL("  shader validation failed");
        return;
    }
    int len = entry.source.length();
    CString cstr = entry.source.utf8();
    const char* s = cstr.data();

    LOGWEBGL("glShaderSource(%s)", cstr.data());
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
#endif // ENABLE(WEBGL)
