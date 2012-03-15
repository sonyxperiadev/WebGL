/*
 * Copyright (C) 2011, 2012, Sony Ericsson Mobile Communications AB
 * Copyright (C) 2012, Sony Mobile Communications AB
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

#ifndef GraphicsContext3DInternal_h
#define GraphicsContext3D3DInternal_h

#include "ANGLEWebKitBridge.h"
#include "CanvasRenderingContext.h"
#include "Extensions3DAndroid.h"
#include "GraphicsContext3D.h"
#include "HTMLCanvasElement.h"
#include "SkRect.h"
#include "Threading.h"
#include "Timer.h"
#include "TransformationMatrix.h"
#include "WebGLLayer.h"

#include <wtf/Deque.h>
#include <wtf/RefPtr.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <ui/android_native_buffer.h>
#include <ui/GraphicBuffer.h>

#include <utils/Log.h>

#undef WEBGL_LOGGING
//#define WEBGL_LOGGING 1
#ifdef WEBGL_LOGGING
#define LOGWEBGL(...) ((void)android_printLog(ANDROID_LOG_DEBUG, "WebGL", __VA_ARGS__))
#else
#define LOGWEBGL(...)
#endif

// This can be increased to 3, for example, if that has a positive impact on performance.
#define NUM_BUFFERS 2

using namespace android;

namespace WebCore {

#define CLAMP(x) GraphicsContext3DInternal::clampValue(x)

typedef enum {
    THREAD_STATE_STOPPED,
    THREAD_STATE_RUN,
    THREAD_STATE_RESTART,
    THREAD_STATE_STOP
} thread_state_t;

class GraphicsContext3DProxy;
class FBO;

class GraphicsContext3DInternal : public RefCounted<GraphicsContext3DInternal> {
public:
    GraphicsContext3DInternal(HTMLCanvasElement* canvas, GraphicsContext3D::Attributes attrs,
                              HostWindow* hostWindow);
    ~GraphicsContext3DInternal();

    bool isValid() { return m_contextId > 0; }

    PlatformLayer* platformLayer() const;

    void makeContextCurrent();
    GraphicsContext3D::Attributes getContextAttributes();
    void reshape(int width, int height);

    unsigned long getError();
    void synthesizeGLError(unsigned long error);

    void recreateSurface();
    void releaseSurface();

    void markContextChanged();
    void markLayerComposited() { m_layerComposited = true; }
    bool layerComposited() const { return m_layerComposited; }

    void updateFrontBuffer();
    bool lockFrontBuffer(EGLImageKHR& image, SkRect& rect);
    void releaseFrontBuffer();

    void paintRenderingResultsToCanvas(CanvasRenderingContext* context);
    PassRefPtr<ImageData> paintRenderingResultsToImageData();
    bool paintCompositedResultsToCanvas(CanvasRenderingContext* context);

    // Shader handling, required since we validate shader source with ANGLE
    void compileShader(Platform3DObject shader);
    String getShaderInfoLog(Platform3DObject shader);
    String getShaderSource(Platform3DObject shader);
    void shaderSource(Platform3DObject shader, const String& string);

    void viewport(long x, long y, unsigned long width, unsigned long height);

    int width() { return m_width; }
    int height() { return m_height; }

    Extensions3D* getExtensions() { return m_extensions.get(); }

    void bindFramebuffer(GC3Denum target, Platform3DObject buffer);

    static GLclampf clampValue(GLclampf x)  {
        GLclampf tmp = x;
        if (tmp < 0.0f)
            tmp = 0.0f;
        else if (tmp > 1.0f)
            tmp = 1.0f;
        return tmp;
    }

    static GLclampf clampValue(double d)  {
        GLclampf tmp = (GLclampf)d;
        if (tmp < 0.0f)
            tmp = 0.0f;
        else if (tmp > 1.0f)
            tmp = 1.0f;
        return tmp;
    }

    static EGLint checkEGLError(const char* s);
    static GLint checkGLError(const char* s);

private:
    bool initEGL();
    bool createContext(bool createEGLContext);
    void deleteContext(bool deleteEGLContext);

    RefPtr<GraphicsContext3DProxy> m_proxy;
    WebGLLayer *m_compositingLayer;
    HTMLCanvasElement* m_canvas;
    GraphicsContext3D::Attributes m_attrs;
    bool m_layerComposited;
    bool m_canvasDirty;

    int m_width;
    int m_height;
    int m_maxwidth;
    int m_maxheight;

    EGLDisplay m_dpy;
    EGLConfig  m_config;
    EGLSurface m_surface;
    EGLContext m_context;

    // Routines for FBOs
    FBO*                 m_fbo[NUM_BUFFERS];
    GLuint               m_boundFBO;
    FBO*                 m_currentFBO;
    FBO*                 m_frontFBO;
    Deque<FBO*>          m_freeBuffers;
    Deque<FBO*>          m_queuedBuffers;
    Deque<FBO*>          m_preparedBuffers;
    WTF::Mutex           m_fboMutex;
    WTF::ThreadCondition m_fboCondition;

    void startSyncThread();
    void stopSyncThread();
    static void* syncThreadStart(void* ctx);
    void runSyncThread();
    ThreadIdentifier m_syncThread;
    thread_state_t   m_threadState;
    WTF::Mutex           m_threadMutex;
    WTF::ThreadCondition m_threadCondition;

    void syncTimerFired(Timer<GraphicsContext3DInternal>*);
    FBO* dequeueBuffer();
    void swapBuffers();
    Timer<GraphicsContext3DInternal> m_syncTimer;
    bool m_syncRequested;

    typedef struct {
        String source;
        String log;
        bool isValid;
    } ShaderSourceEntry;
    HashMap<Platform3DObject, ShaderSourceEntry> m_shaderSourceMap;
    ANGLEWebKitBridge m_compiler;

    // Errors raised by synthesizeGLError().
    ListHashSet<unsigned long> m_syntheticErrors;

    struct {
        GLint x, y;
        GLsizei width, height;
    } m_savedViewport;
    OwnPtr<Extensions3DAndroid> m_extensions;
    int m_contextId;

    // Java method used for invalidating the canvas area
    jobject m_webView;
    jmethodID m_postInvalidate;
};

} // namespace WebCore

#endif // GraphicsContext3DInternal_h
