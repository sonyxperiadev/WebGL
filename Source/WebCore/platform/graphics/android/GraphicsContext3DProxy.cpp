

#include "GraphicsContext3DProxy.h"
#include "GraphicsContext3DInternal.h"

namespace WebCore {

GraphicsContext3DProxy::GraphicsContext3DProxy()
{
    LOGWEBGL("GraphicsContext3DProxy::GraphicsContext3DProxy(), this = %p", this);
}

GraphicsContext3DProxy::~GraphicsContext3DProxy()
{
    LOGWEBGL("GraphicsContext3DProxy::~GraphicsContext3DProxy(), this = %p", this);
}

void GraphicsContext3DProxy::setGraphicsContext(GraphicsContext3DInternal* context)
{
    MutexLocker lock(m_mutex);
    m_context = context;
}

bool GraphicsContext3DProxy::lockFrontBuffer(EGLImageKHR& image, int& width, int& height,
                                             SkRect& rect, bool& requestUpdate)
{
    MutexLocker lock(m_mutex);
    if (!m_context) {
        return false;
    }
    return m_context->lockFrontBuffer(image, width, height, rect, requestUpdate);
}

void GraphicsContext3DProxy::releaseFrontBuffer()
{
    MutexLocker lock(m_mutex);
    if (!m_context) {
        return;
    }
    m_context->releaseFrontBuffer();
}
}
