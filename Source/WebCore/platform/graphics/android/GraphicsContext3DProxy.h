

#ifndef GraphicsContext3DProxy_h
#define GraphicsContext3DProxy_h

#include "config.h"

#include "SkRect.h"
#include "Threading.h"
#include <wtf/RefCounted.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

namespace WebCore {

class GraphicsContext3DInternal;

class GraphicsContext3DProxy: public RefCounted<GraphicsContext3DProxy> {
public:
    GraphicsContext3DProxy();
    ~GraphicsContext3DProxy();

    void setGraphicsContext(GraphicsContext3DInternal* context);

    bool lockFrontBuffer(EGLImageKHR& image, int& width, int& height, SkRect& rect, bool& requestUpdate);
    void releaseFrontBuffer();

private:
    WTF::Mutex                 m_mutex;
    GraphicsContext3DInternal* m_context;
};

}
#endif
