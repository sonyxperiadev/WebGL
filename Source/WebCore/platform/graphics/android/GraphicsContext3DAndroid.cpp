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

#if ENABLE(WEBGL)
#include "BitmapImage.h"
#include "text/CString.h"
#include "GraphicsContext3D.h"
#include "GraphicsContext3DInternal.h"
#include "Image.h"
#include "ImageData.h"
#include "ImageDecoder.h"
#include "SkBitmap.h"
#include "SkBitmapRef.h"

namespace WebCore {


PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(HTMLCanvasElement* canvas, Attributes attrs,
                                                        HostWindow *win, RenderStyle style)
{
    GraphicsContext3D *context = new GraphicsContext3D(canvas, attrs, win, false);
    if (!context->m_internal->isValid()) {
        // Something failed during initialization
        delete context;
        return 0;
    }
    return adoptRef(context);
}

GraphicsContext3D::GraphicsContext3D(HTMLCanvasElement* canvas, Attributes attrs,
                                     HostWindow* hostWindow, bool renderDirectlyToHostWindow)
    : m_internal(new GraphicsContext3DInternal(canvas, attrs, hostWindow))
{
    LOGWEBGL("GraphicsContext3D() = %p", this);
    m_currentWidth = m_internal->width();
    m_currentHeight = m_internal->height();
}

GraphicsContext3D::~GraphicsContext3D()
{
    LOGWEBGL("~GraphicsContext3D()");
}

PlatformLayer* GraphicsContext3D::platformLayer() const
{
    return m_internal->platformLayer();
}

void GraphicsContext3D::makeContextCurrent() {
    m_internal->makeContextCurrent();
}

bool GraphicsContext3D::isGLES2Compliant() const
{
    return true;
}

void GraphicsContext3D::synthesizeGLError(GC3Denum error)
{
    m_internal->synthesizeGLError(error);
}

Extensions3D* GraphicsContext3D::getExtensions()
{
    return m_internal->getExtensions();
}

void GraphicsContext3D::paintRenderingResultsToCanvas(CanvasRenderingContext* context)
{
    makeContextCurrent();
    m_internal->paintRenderingResultsToCanvas(context);
}

PassRefPtr<ImageData> GraphicsContext3D::paintRenderingResultsToImageData()
{
    makeContextCurrent();
    return m_internal->paintRenderingResultsToImageData();
}

bool GraphicsContext3D::paintCompositedResultsToCanvas(CanvasRenderingContext* context)
{
    makeContextCurrent();
    return m_internal->paintCompositedResultsToCanvas(context);
}

bool GraphicsContext3D::getImageData(Image* image,
                                     unsigned int format,
                                     unsigned int type,
                                     bool premultiplyAlpha,
                                     bool ignoreGammaAndColorProfile,
                                     Vector<uint8_t>& outputVector)
{
    LOGWEBGL("getImageData(%p, %u, %u, %s, %s)", image, format, type,
             premultiplyAlpha ? "true" : "false", ignoreGammaAndColorProfile ? "true" : "false");
    if (!image)
        return false;

    AlphaOp neededAlphaOp = AlphaDoNothing;
    bool hasAlpha = (image->data() && image->isBitmapImage()) ?
        static_cast<BitmapImage*>(image)->frameHasAlphaAtIndex(0) : true;
    ImageDecoder* decoder = 0;
    ImageFrame* buf = 0;

    if ((ignoreGammaAndColorProfile || (hasAlpha && !premultiplyAlpha)) && image->data()) {
        // Attempt to get raw unpremultiplied image data
        decoder = ImageDecoder::create(*(image->data()),
                                       premultiplyAlpha ?
                                         ImageSource::AlphaPremultiplied :
                                         ImageSource::AlphaNotPremultiplied,
                                       ignoreGammaAndColorProfile ?
                                         ImageSource::GammaAndColorProfileIgnored :
                                         ImageSource::GammaAndColorProfileApplied);
        if (decoder) {
            decoder->setData(image->data(), true);
            buf = decoder->frameBufferAtIndex(0);
            if (buf && buf->hasAlpha() && premultiplyAlpha)
                neededAlphaOp = AlphaDoPremultiply;
        }
    }

    SkBitmapRef* bitmapRef = 0;
    if (!buf) {
        bitmapRef = image->nativeImageForCurrentFrame();
        if (!bitmapRef)
            return false;
        if (!premultiplyAlpha && hasAlpha)
            neededAlphaOp = AlphaDoUnmultiply;
    }

    SkBitmap& bitmap = buf ? buf->bitmap() : bitmapRef->bitmap();
    unsigned char* pixels = 0;
    int rowBytes = 0;
    uint32_t* tmpPixels = 0;

    int width = bitmap.width();
    int height = bitmap.height();
    int iwidth = image->width();
    int iheight = image->height();
    LOGWEBGL("  bitmap.width() = %d, image->width() = %d, bitmap.height = %d, image->height() = %d",
             width, iwidth, height, iheight);
    if (width != iwidth || height != iheight) {
        // This image has probably been subsampled because it was too big.
        // Currently, we cannot handle this in WebGL: give up.
        return false;
    }
    SkBitmap::Config skiaConfig = bitmap.getConfig();

    bitmap.lockPixels();
    if (skiaConfig == SkBitmap::kARGB_8888_Config) {
        LOGWEBGL("  skiaConfig = kARGB_8888_Config");
        pixels = reinterpret_cast<unsigned char*>(bitmap.getPixels());
        rowBytes = bitmap.rowBytes();
        if (!pixels) {
            bitmap.unlockPixels();
            return false;
        }
    }
    else if (skiaConfig == SkBitmap::kIndex8_Config) {
        LOGWEBGL("  skiaConfig = kIndex8_Config");
        rowBytes = width * 4;
        tmpPixels = (uint32_t*)fastMalloc(width * height * 4);
        if (!tmpPixels) {
            bitmap.unlockPixels();
            return false;
        }
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                SkPMColor c = bitmap.getIndex8Color(j, i);
                tmpPixels[i * width + j] = c;//SkExpand_8888(c);
            }
        }
        pixels = (unsigned char*)tmpPixels;
    }

    outputVector.resize(rowBytes * height);
    LOGWEBGL("rowBytes() = %d, width() = %d, height() = %d", rowBytes, width, height);

    bool res = packPixels(pixels,
                          SourceFormatRGBA8, width, height, 0,
                          format, type, neededAlphaOp, outputVector.data());
    bitmap.unlockPixels();

    if (decoder)
        delete decoder;

    if (tmpPixels)
        fastFree(tmpPixels);

    return res;
}

unsigned GraphicsContext3D::createBuffer()
{
    LOGWEBGL("glCreateBuffer()");
    makeContextCurrent();
    GLuint b = 0;
    glGenBuffers(1, &b);
    return b;
}

unsigned GraphicsContext3D::createFramebuffer()
{
    LOGWEBGL("glCreateFramebuffer()");
    makeContextCurrent();
    GLuint fb = 0;
    glGenFramebuffers(1, &fb);
    return fb;
}

unsigned GraphicsContext3D::createProgram()
{
    LOGWEBGL("glCreateProgram()");
    makeContextCurrent();
    return glCreateProgram();
}

unsigned GraphicsContext3D::createRenderbuffer()
{
    LOGWEBGL("glCreateRenderbuffer()");
    makeContextCurrent();
    GLuint rb = 0;
    glGenRenderbuffers(1, &rb);
    return rb;
}

unsigned GraphicsContext3D::createShader(GC3Denum type)
{
    LOGWEBGL("glCreateShader()");
    makeContextCurrent();
    return glCreateShader((type == FRAGMENT_SHADER) ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
}

unsigned GraphicsContext3D::createTexture()
{
    LOGWEBGL("glCreateTexture()");
    makeContextCurrent();
    GLuint t = 0;
    glGenTextures(1, &t);
    return t;
}

void GraphicsContext3D::deleteBuffer(unsigned buffer)
{
    LOGWEBGL("glDeleteBuffers()");
    makeContextCurrent();
    glDeleteBuffers(1, &buffer);
}

void GraphicsContext3D::deleteFramebuffer(unsigned framebuffer)
{
    LOGWEBGL("glDeleteFramebuffers()");
    makeContextCurrent();
    glDeleteFramebuffers(1, &framebuffer);
}

void GraphicsContext3D::deleteProgram(unsigned program)
{
    LOGWEBGL("glDeleteProgram()");
    makeContextCurrent();
    glDeleteProgram(program);
}

void GraphicsContext3D::deleteRenderbuffer(unsigned renderbuffer)
{
    LOGWEBGL("glDeleteRenderbuffers()");
    makeContextCurrent();
    glDeleteRenderbuffers(1, &renderbuffer);
}

void GraphicsContext3D::deleteShader(unsigned shader)
{
    LOGWEBGL("glDeleteShader()");
    makeContextCurrent();
    glDeleteShader(shader);
}

void GraphicsContext3D::deleteTexture(unsigned texture)
{
    LOGWEBGL("glDeleteTextures()");
    makeContextCurrent();
    glDeleteTextures(1, &texture);
}


void GraphicsContext3D::activeTexture(GC3Denum texture)
{
    LOGWEBGL("glActiveTexture(%ld)", texture);
    makeContextCurrent();
    glActiveTexture(texture);
}

void GraphicsContext3D::attachShader(Platform3DObject program, Platform3DObject shader)
{
    LOGWEBGL("glAttachShader(%d, %d)", program, shader);
    makeContextCurrent();
    glAttachShader(program, shader);
}

void GraphicsContext3D::bindAttribLocation(Platform3DObject program, GC3Duint index,
                                           const String& name)
{
    CString cs = name.utf8();
    LOGWEBGL("glBindAttribLocation(%d, %d, %s)", program, index, cs.data());
    if (!program)
        return;
    makeContextCurrent();
    glBindAttribLocation(program, index, cs.data());
}

void GraphicsContext3D::bindBuffer(GC3Denum target, Platform3DObject buffer)
{
    LOGWEBGL("glBindBuffer(%d, %d)", target, buffer);
    makeContextCurrent();
    glBindBuffer(target, buffer);
}

void GraphicsContext3D::bindFramebuffer(GC3Denum target, Platform3DObject framebuffer)
{
    LOGWEBGL("bindFrameBuffer(%d, %d)", target, framebuffer);
    m_internal->bindFramebuffer(target, framebuffer);
}

void GraphicsContext3D::bindRenderbuffer(GC3Denum target, Platform3DObject renderbuffer)
{
    LOGWEBGL("glBindRenderBuffer(%d, %d)", target, renderbuffer);
    makeContextCurrent();
    glBindRenderbuffer(target, renderbuffer);
}

void GraphicsContext3D::bindTexture(GC3Denum target, Platform3DObject texture)
{
    LOGWEBGL("glBindTexture(%d, %d)", target, texture);
    makeContextCurrent();
    glBindTexture(target, texture);
}

void GraphicsContext3D::blendColor(GC3Dclampf red, GC3Dclampf green,
                                   GC3Dclampf blue, GC3Dclampf alpha)
{
    LOGWEBGL("glBlendColor(%lf, %lf, %lf, %lf)", red, green, blue, alpha);
    makeContextCurrent();
    glBlendColor(CLAMP(red), CLAMP(green), CLAMP(blue), CLAMP(alpha));
}

void GraphicsContext3D::blendEquation(GC3Denum mode)
{
    LOGWEBGL("glBlendEquation(%d)", mode);
    makeContextCurrent();
    glBlendEquation(mode);
}

void GraphicsContext3D::blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha)
{
    LOGWEBGL("glBlendEquationSeparate(%d, %d)", modeRGB, modeAlpha);
    makeContextCurrent();
    glBlendEquationSeparate(modeRGB, modeAlpha);
}

void GraphicsContext3D::blendFunc(GC3Denum sfactor, GC3Denum dfactor)
{
    LOGWEBGL("glBlendFunc(%d, %d)", sfactor, dfactor);
    makeContextCurrent();
    glBlendFunc(sfactor, dfactor);
}

void GraphicsContext3D::blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB,
                                          GC3Denum srcAlpha, GC3Denum dstAlpha)
{
    LOGWEBGL("glBlendFuncSeparate(%lu, %lu, %lu, %lu)", srcRGB, dstRGB, srcAlpha, dstAlpha);
    makeContextCurrent();
    glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, GC3Denum usage)
{
    LOGWEBGL("glBufferData(%lu, %d, %lu)", target, size, usage);
    makeContextCurrent();
    glBufferData(target, size, 0, usage);
}

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size,
                                   const void* data, GC3Denum usage)
{
    LOGWEBGL("glBufferData(%lu, %d, %p, %lu)", target, size, data, usage);
    makeContextCurrent();
    glBufferData(target, size, data, usage);
}

void GraphicsContext3D::bufferSubData(GC3Denum target, GC3Dintptr offset,
                                      GC3Dsizeiptr size, const void* data)
{
    LOGWEBGL("glBufferSubData(%lu, %ld, %d, %p)", target, offset, size, data);
    makeContextCurrent();
    glBufferSubData(target, offset, size, data);
}

GC3Denum GraphicsContext3D::checkFramebufferStatus(GC3Denum target)
{
    LOGWEBGL("glCheckFramebufferStatus(%lu)", target);
    makeContextCurrent();
    return glCheckFramebufferStatus(target);
}

void GraphicsContext3D::clear(GC3Dbitfield mask)
{
    LOGWEBGL("glClear(%lu)", mask);
    makeContextCurrent();
    glClear(mask);
}

void GraphicsContext3D::clearColor(GC3Dclampf red, GC3Dclampf green,
                                   GC3Dclampf blue, GC3Dclampf alpha)
{
    LOGWEBGL("glClearColor(%.2lf, %.2lf, %.2lf, %.2lf)", red, green, blue, alpha);
    makeContextCurrent();
    glClearColor(CLAMP(red), CLAMP(green), CLAMP(blue), CLAMP(alpha));
}

void GraphicsContext3D::clearDepth(GC3Dclampf depth)
{
    LOGWEBGL("glClearDepthf(%.2lf)", depth);
    makeContextCurrent();
    glClearDepthf(CLAMP(depth));
}

void GraphicsContext3D::clearStencil(GC3Dint s)
{
    LOGWEBGL("glClearStencil(%ld)", s);
    makeContextCurrent();
    glClearStencil(s);
}

void GraphicsContext3D::colorMask(GC3Dboolean red, GC3Dboolean green,
                                  GC3Dboolean blue, GC3Dboolean alpha)
{
    LOGWEBGL("glColorMask(%s, %s, %s, %s)", red ? "true" : "false", green ? "true" : "false",
             blue ? "true" : "false", alpha ? "true" : "false");
    makeContextCurrent();
    glColorMask(red, green, blue, alpha);
}

void GraphicsContext3D::compileShader(Platform3DObject shader)
{
    LOGWEBGL("compileShader(%lu)", shader);
    makeContextCurrent();
    m_internal->compileShader(shader);
}

void GraphicsContext3D::copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat,
                                       GC3Dint x, GC3Dint y, GC3Dsizei width,
                                       GC3Dsizei height, GC3Dint border)
{
    LOGWEBGL("glCopyTexImage2D(%lu, %ld, %lu, %ld, %ld, %lu, %lu, %ld",
             target, level, internalformat, x, y, width, height, border);
    makeContextCurrent();
    glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void GraphicsContext3D::copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset,
                                          GC3Dint yoffset, GC3Dint x, GC3Dint y, GC3Dsizei width,
                                          GC3Dsizei height)
{
    LOGWEBGL("glCopyTexSubImage2D(%lu, %ld, %ld, %ld, %ld, %ld, %lu, %lu)",
             target, level, xoffset, yoffset, x, y, width, height);
    makeContextCurrent();
    glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void GraphicsContext3D::cullFace(GC3Denum mode)
{
    LOGWEBGL("glCullFace(%lu)", mode);
    makeContextCurrent();
    glCullFace(mode);
}

void GraphicsContext3D::depthFunc(GC3Denum func)
{
    LOGWEBGL("glDepthFunc(%lu)", func);
    makeContextCurrent();
    glDepthFunc(func);
}

void GraphicsContext3D::depthMask(GC3Dboolean flag)
{
    LOGWEBGL("glDepthMask(%s)", flag ? "true" : "false");
    makeContextCurrent();
    glDepthMask(flag);
}

void GraphicsContext3D::depthRange(GC3Dclampf zNear, GC3Dclampf zFar)
{
    LOGWEBGL("glDepthRangef(%.2lf, %.2lf)", zNear, zFar);
    makeContextCurrent();
    glDepthRangef(CLAMP(zNear), CLAMP(zFar));
}

void GraphicsContext3D::detachShader(Platform3DObject program, Platform3DObject shader)
{
    LOGWEBGL("glDetachShader(%lu, %lu)", program, shader);
    makeContextCurrent();
    glDetachShader(program, shader);
}

void GraphicsContext3D::disable(GC3Denum cap)
{
    LOGWEBGL("glDisable(%lu)", cap);
    makeContextCurrent();
    glDisable(cap);
}

void GraphicsContext3D::disableVertexAttribArray(GC3Duint index)
{
    LOGWEBGL("glDisableVertexAttribArray(%lu)", index);
    makeContextCurrent();
    glDisableVertexAttribArray(index);
}

void GraphicsContext3D::drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count)
{
    LOGWEBGL("glDrawArrays(%lu, %ld, %ld)", mode, first, count);
    makeContextCurrent();
    glDrawArrays(mode, first, count);
}

void GraphicsContext3D::drawElements(GC3Denum mode, GC3Dsizei count,
                                     GC3Denum type, GC3Dintptr offset)
{
    LOGWEBGL("glDrawElements(%lu, %lu, %lu, %ld)", mode, count, type, offset);
    makeContextCurrent();
    glDrawElements(mode, count, type, reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3D::enable(GC3Denum cap)
{
    LOGWEBGL("glEnable(0x%04x)", cap);
    makeContextCurrent();
    glEnable(cap);
}

void GraphicsContext3D::enableVertexAttribArray(GC3Duint index)
{
    LOGWEBGL("glEnableVertexAttribArray(%lu)", index);
    makeContextCurrent();
    glEnableVertexAttribArray(index);
}

void GraphicsContext3D::finish()
{
    LOGWEBGL("glFinish()");
    makeContextCurrent();
    glFinish();
}

void GraphicsContext3D::flush()
{
    LOGWEBGL("glFlush()");
    makeContextCurrent();
    glFlush();
}

void GraphicsContext3D::framebufferRenderbuffer(GC3Denum target, GC3Denum attachment,
                                                GC3Denum renderbuffertarget,
                                                Platform3DObject renderbuffer)
{
    LOGWEBGL("glFramebufferRenderbuffer(%lu, %lu, %lu, %lu)", target, attachment,
             renderbuffertarget, renderbuffer);
    makeContextCurrent();
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void GraphicsContext3D::framebufferTexture2D(GC3Denum target, GC3Denum attachment,
                                             GC3Denum textarget, Platform3DObject texture,
                                             GC3Dint level)
{
    LOGWEBGL("glFramebufferTexture2D(%lu, %lu, %lu, %lu, %ld)",
             target, attachment, textarget, texture, level);
    makeContextCurrent();
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void GraphicsContext3D::frontFace(GC3Denum mode)
{
    LOGWEBGL("glFrontFace(%lu)", mode);
    makeContextCurrent();
    glFrontFace(mode);
}

void GraphicsContext3D::generateMipmap(GC3Denum target)
{
    LOGWEBGL("glGenerateMipmap(%lu)", target);
    makeContextCurrent();
    glGenerateMipmap(target);
}

bool GraphicsContext3D::getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    LOGWEBGL("glGetActiveAttrib(%lu, %lu)", program, index);
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    makeContextCurrent();
    GLint maxAttributeSize = 0;
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxAttributeSize);
    GLchar name[maxAttributeSize];
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    glGetActiveAttrib(program, index, maxAttributeSize, &nameLength, &size, &type, name);
    if (!nameLength)
        return false;
    info.name = String(name, nameLength);
    info.type = type;
    info.size = size;
    return true;
}

bool GraphicsContext3D::getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    LOGWEBGL("glGetActiveUniform(%lu, %lu)", program, index);
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    makeContextCurrent();
    GLint maxUniformSize = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformSize);
    GLchar name[maxUniformSize];
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    glGetActiveUniform(program, index, maxUniformSize, &nameLength, &size, &type, name);
    if (!nameLength)
        return false;
    info.name = String(name, nameLength);
    info.type = type;
    info.size = size;
    return true;
}

void GraphicsContext3D::getAttachedShaders(Platform3DObject program, GC3Dsizei maxCount,
                                           GC3Dsizei* count, Platform3DObject* shaders)
{
    LOGWEBGL("glGetAttachedShaders(%lu, %d, %p, %p)", program, maxCount, count, shaders);
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return;
    }
    makeContextCurrent();
    glGetAttachedShaders(program, maxCount, count, shaders);
}

GC3Dint GraphicsContext3D::getAttribLocation(Platform3DObject program, const String& name)
{
    CString cs = name.utf8();
    LOGWEBGL("glGetAttribLocation(%lu, %s)", program, cs.data());
    if (!program) {
        return -1;
    }
    makeContextCurrent();

    return glGetAttribLocation(program, cs.data());
}

void GraphicsContext3D::getBooleanv(GC3Denum pname, GC3Dboolean* value)
{
    LOGWEBGL("glGetBooleanv(%lu, %p)", pname, value);
    makeContextCurrent();
    glGetBooleanv(pname, value);
}

void GraphicsContext3D::getBufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    LOGWEBGL("glGetBufferParameteriv(%lu, %lu, %p)", target, pname, value);
    makeContextCurrent();
    glGetBufferParameteriv(target, pname, value);
}

GraphicsContext3D::Attributes GraphicsContext3D::getContextAttributes()
{
    LOGWEBGL("getContextAttributes()");
    return m_internal->getContextAttributes();
}

GC3Denum GraphicsContext3D::getError()
{
    LOGWEBGL("getError()");
    return m_internal->getError();
}

void GraphicsContext3D::getFloatv(GC3Denum pname, GC3Dfloat* value)
{
    LOGWEBGL("glGetFloatv(%lu, %p)", pname, value);
    makeContextCurrent();
    glGetFloatv(pname, value);
}

void GraphicsContext3D::getFramebufferAttachmentParameteriv(GC3Denum target, GC3Denum attachment,
                                                            GC3Denum pname, GC3Dint* value)
{
    LOGWEBGL("glGetFramebufferAttachmentParameteriv(%lu, %lu, %lu, %p)",
             target, attachment, pname, value);
    makeContextCurrent();
    if (attachment == DEPTH_STENCIL_ATTACHMENT)
        attachment = DEPTH_ATTACHMENT;
    glGetFramebufferAttachmentParameteriv(target, attachment, pname, value);
}

void GraphicsContext3D::getIntegerv(GC3Denum pname, GC3Dint* value)
{
    LOGWEBGL("glGetIntegerv(%lu, %p)", pname, value);
    makeContextCurrent();
    glGetIntegerv(pname, value);
}

void GraphicsContext3D::getProgramiv(Platform3DObject program, GC3Denum pname, GC3Dint* value)
{
    LOGWEBGL("glGetProgramiv(%lu, %lu, %p)", program, pname, value);
    makeContextCurrent();
    glGetProgramiv(program, pname, value);
}

String GraphicsContext3D::getProgramInfoLog(Platform3DObject program)
{
    LOGWEBGL("glGetProgramInfoLog(%lu)", program);
    makeContextCurrent();
    GLint length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (!length)
        return "";

    GLsizei size;
    GLchar* info = (GLchar*)fastMalloc(length);
    glGetProgramInfoLog(program, length, &size, info);
    String s(info);
    fastFree(info);

    return s;
}

void GraphicsContext3D::getRenderbufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    LOGWEBGL("glGetRenderbufferParameteriv(%lu, %lu, %p)", target, pname, value);
    makeContextCurrent();
    glGetRenderbufferParameteriv(target, pname, value);
}

void GraphicsContext3D::getShaderiv(Platform3DObject shader, GC3Denum pname, GC3Dint* value)
{
    LOGWEBGL("glGetShaderiv(%lu, %lu, %p)", shader, pname, value);
    makeContextCurrent();
    glGetShaderiv(shader, pname, value);
}

String GraphicsContext3D::getShaderInfoLog(Platform3DObject shader)
{
    LOGWEBGL("getShaderInfoLog(%lu)", shader);
    makeContextCurrent();
    return m_internal->getShaderInfoLog(shader);
}

String GraphicsContext3D::getShaderSource(Platform3DObject shader)
{
    LOGWEBGL("getShaderSource(%lu)", shader);
    makeContextCurrent();
    return m_internal->getShaderSource(shader);
}

String GraphicsContext3D::getString(GC3Denum name)
{
    LOGWEBGL("glGetString(%lu)", name);
    makeContextCurrent();
    return String(reinterpret_cast<const char*>(glGetString(name)));
}

void GraphicsContext3D::getTexParameterfv(GC3Denum target, GC3Denum pname, GC3Dfloat* value)
{
    LOGWEBGL("glGetTexParameterfv(%lu, %lu, %p)", target, pname, value);
    makeContextCurrent();
    glGetTexParameterfv(target, pname, value);
}

void GraphicsContext3D::getTexParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    LOGWEBGL("glGetTexParameteriv(%lu, %lu, %p)", target, pname, value);
    makeContextCurrent();
    glGetTexParameteriv(target, pname, value);
}

void GraphicsContext3D::getUniformfv(Platform3DObject program, GC3Dint location, GC3Dfloat* value)
{
    LOGWEBGL("glGetUniformfv(%lu, %ld, %p)", program, location, value);
    makeContextCurrent();
    glGetUniformfv(program, location, value);
}

void GraphicsContext3D::getUniformiv(Platform3DObject program, GC3Dint location, GC3Dint* value)
{
    LOGWEBGL("glGetUniformiv(%lu, %ld, %p)", program, location, value);
    makeContextCurrent();
    glGetUniformiv(program, location, value);
}

GC3Dint GraphicsContext3D::getUniformLocation(Platform3DObject program, const String& name)
{
    CString cs = name.utf8();
    LOGWEBGL("glGetUniformLocation(%lu, %s)", program, cs.data());
    makeContextCurrent();
    return glGetUniformLocation(program, cs.data());
}

void GraphicsContext3D::getVertexAttribfv(GC3Duint index, GC3Denum pname, GC3Dfloat* value)
{
    LOGWEBGL("glGetVertexAttribfv(%lu, %lu, %p)", index, pname, value);
    makeContextCurrent();
    glGetVertexAttribfv(index, pname, value);
}

void GraphicsContext3D::getVertexAttribiv(GC3Duint index, GC3Denum pname, GC3Dint* value)
{
    LOGWEBGL("glGetVertexAttribiv(%lu, %lu, %p)", index, pname, value);
    makeContextCurrent();
    glGetVertexAttribiv(index, pname, value);
}

GC3Dsizeiptr GraphicsContext3D::getVertexAttribOffset(GC3Duint index, GC3Denum pname)
{
    LOGWEBGL("glGetVertexAttribOffset(%lu, %lu)", index, pname);
    GLvoid* pointer = 0;
    glGetVertexAttribPointerv(index, pname, &pointer);
    return static_cast<GC3Dsizeiptr>(reinterpret_cast<intptr_t>(pointer));
}

void GraphicsContext3D::hint(GC3Denum target, GC3Denum mode)
{
    LOGWEBGL("glHint(%lu, %lu)", target, mode);
    makeContextCurrent();
    glHint(target, mode);
}

GC3Dboolean GraphicsContext3D::isBuffer(Platform3DObject buffer)
{
    LOGWEBGL("glIsBuffer(%lu)", buffer);
    if (!buffer)
        return GL_FALSE;
    makeContextCurrent();
    return glIsBuffer(buffer);
}

GC3Dboolean GraphicsContext3D::isEnabled(GC3Denum cap)
{
    LOGWEBGL("glIsEnabled(%lu)", cap);
    makeContextCurrent();
    return glIsEnabled(cap);
}

GC3Dboolean GraphicsContext3D::isFramebuffer(Platform3DObject framebuffer)
{
    LOGWEBGL("glIsFramebuffer(%lu)", framebuffer);
    if (!framebuffer)
        return GL_FALSE;
    makeContextCurrent();
    return glIsFramebuffer(framebuffer);
}

GC3Dboolean GraphicsContext3D::isProgram(Platform3DObject program)
{
    LOGWEBGL("glIsProgram(%lu)", program);
    if (!program)
        return GL_FALSE;
    makeContextCurrent();
    return glIsProgram(program);
}

GC3Dboolean GraphicsContext3D::isRenderbuffer(Platform3DObject renderbuffer)
{
    LOGWEBGL("glIsRenderbuffer(%lu)", renderbuffer);
    if (!renderbuffer)
        return GL_FALSE;
    makeContextCurrent();
    return glIsRenderbuffer(renderbuffer);
}

GC3Dboolean GraphicsContext3D::isShader(Platform3DObject shader)
{
    LOGWEBGL("glIsShader(%lu)", shader);
    if (!shader)
        return GL_FALSE;
    makeContextCurrent();
    return glIsShader(shader);
}

GC3Dboolean GraphicsContext3D::isTexture(Platform3DObject texture)
{
    LOGWEBGL("glIsTexture(%lu)", texture);
    if (!texture)
        return GL_FALSE;
    makeContextCurrent();
    return glIsTexture(texture);
}

void GraphicsContext3D::lineWidth(GC3Dfloat width)
{
    LOGWEBGL("glLineWidth(%.2lf)", width);
    makeContextCurrent();
    glLineWidth((GLfloat)width);
}

void GraphicsContext3D::linkProgram(Platform3DObject program)
{
    LOGWEBGL("glLinkProgram(%lu)", program);
    makeContextCurrent();
    glLinkProgram(program);
}

void GraphicsContext3D::pixelStorei(GC3Denum pname, GC3Dint param)
{
    LOGWEBGL("glPixelStorei(%lu, %ld)", pname, param);
    makeContextCurrent();
    glPixelStorei(pname, param);
}

void GraphicsContext3D::polygonOffset(GC3Dfloat factor, GC3Dfloat units)
{
    LOGWEBGL("glPolygonOffset(%.2lf, %.2lf)", factor, units);
    makeContextCurrent();
    glPolygonOffset((GLfloat)factor, (GLfloat)units);
}

void GraphicsContext3D::readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height,
                                   GC3Denum format, GC3Denum type, void* data)
{
    LOGWEBGL("glReadPixels(%ld, %ld, %lu, %lu, %lu, %lu, %p)",
             x, y, width, height, format, type, data);
    makeContextCurrent();
    glReadPixels(x, y, width, height, format, type, data);
}

void GraphicsContext3D::releaseShaderCompiler()
{
    LOGWEBGL("glReleaseShaderCompiler()");
    makeContextCurrent();
    glReleaseShaderCompiler();
}

void GraphicsContext3D::renderbufferStorage(GC3Denum target, GC3Denum internalformat,
                                            GC3Dsizei width, GC3Dsizei height)
{
    LOGWEBGL("glRenderbufferStorage(%lu, %lu, %lu, %lu)",
             target, internalformat, width, height);
    makeContextCurrent();
    glRenderbufferStorage(target, internalformat, width, height);
}

void GraphicsContext3D::sampleCoverage(GC3Dclampf value, GC3Dboolean invert)
{
    LOGWEBGL("glSampleCoverage(%.2lf, %s)", value, invert ? "true" : "false");
    makeContextCurrent();
    glSampleCoverage(CLAMP(value), invert);
}

void GraphicsContext3D::scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    LOGWEBGL("glScissor(%ld, %ld, %lu, %lu)", x, y, width, height);
    makeContextCurrent();
    glScissor(x, y, width, height);
}

void GraphicsContext3D::shaderSource(Platform3DObject shader, const String& source)
{
    LOGWEBGL("shaderSource(%lu, %s)", shader, source.utf8().data());
    makeContextCurrent();
    m_internal->shaderSource(shader, source);
}

void GraphicsContext3D::stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    LOGWEBGL("glStencilFunc(%lu, %ld, %lu)", func, ref, mask);
    makeContextCurrent();
    glStencilFunc(func, ref, mask);
}

void GraphicsContext3D::stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    LOGWEBGL("glStencilFuncSeparate(%lu, %lu, %ld, %lu)", face, func, ref, mask);
    makeContextCurrent();
    glStencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContext3D::stencilMask(GC3Duint mask)
{
    LOGWEBGL("glStencilMask(%lu)", mask);
    makeContextCurrent();
    glStencilMask(mask);
}

void GraphicsContext3D::stencilMaskSeparate(GC3Denum face, GC3Duint mask)
{
    LOGWEBGL("glStencilMaskSeparate(%lu, %lu)", face, mask);
    makeContextCurrent();
    glStencilMaskSeparate(face, mask);
}

void GraphicsContext3D::stencilOp(GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    LOGWEBGL("glStencilOp(%lu, %lu, %lu)", fail, zfail, zpass);
    makeContextCurrent();
    glStencilOp(fail, zfail, zpass);
}

void GraphicsContext3D::stencilOpSeparate(GC3Denum face, GC3Denum fail,
                                          GC3Denum zfail, GC3Denum zpass)
{
    LOGWEBGL("glStencilOpSeparate(%lu, %lu, %lu, %lu)", face, fail, zfail, zpass);
    makeContextCurrent();
    glStencilOpSeparate(face, fail, zfail, zpass);
}

bool GraphicsContext3D::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat,
                                   GC3Dsizei width, GC3Dsizei height, GC3Dint border,
                                   GC3Denum format, GC3Denum type, const void* pixels)
{
    LOGWEBGL("glTexImage2D(%u, %u, %u, %u, %u, %u, %u, %u, %p)",
             target, level, internalformat, width, height, border, format, type, pixels);
    if (width && height && !pixels) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    makeContextCurrent();
    glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    return true;
}

void GraphicsContext3D::texParameterf(GC3Denum target, GC3Denum pname, GC3Dfloat param)
{
    LOGWEBGL("glTexParameterf(%u, %u, %f)", target, pname, param);
    makeContextCurrent();
    glTexParameterf(target, pname, param);
}

void GraphicsContext3D::texParameteri(GC3Denum target, GC3Denum pname, GC3Dint param)
{
    LOGWEBGL("glTexParameteri(%u, %u, %d)", target, pname, param);
    makeContextCurrent();
    glTexParameteri(target, pname, param);
}

void GraphicsContext3D::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset,
                                      GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height,
                                      GC3Denum format, GC3Denum type, const void* pixels)
{
    LOGWEBGL("glTexSubImage2D(%u, %u, %u, %u, %u, %u, %u, %u, %p)", target, level, xoffset,
             yoffset, width, height, format, type, pixels);
    if (width && height && !pixels) {
        synthesizeGLError(INVALID_VALUE);
        return;
    }
    makeContextCurrent();
    glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void GraphicsContext3D::uniform1f(GC3Dint location, GC3Dfloat x)
{
    LOGWEBGL("glUniform1f(%ld, %f)", location, x);
    makeContextCurrent();
    glUniform1f(location, x);
}

void GraphicsContext3D::uniform1fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    LOGWEBGL("glUniform1fv(%ld, %p, %d)", location, v, size);
    makeContextCurrent();
    glUniform1fv(location, size, v);
}

void GraphicsContext3D::uniform1i(GC3Dint location, GC3Dint x)
{
    LOGWEBGL("glUniform1i(%ld, %d)", location, x);
    makeContextCurrent();
    glUniform1i(location, x);
}

void GraphicsContext3D::uniform1iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    LOGWEBGL("glUniform1iv(%ld, %p, %d)", location, v, size);
    makeContextCurrent();
    glUniform1iv(location, size, v);
}

void GraphicsContext3D::uniform2f(GC3Dint location, GC3Dfloat x, float y)
{
    LOGWEBGL("glUniform2f(%ld, %f, %f)", location, x, y);
    makeContextCurrent();
    glUniform2f(location, x, y);
}

void GraphicsContext3D::uniform2fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    LOGWEBGL("glUniform2fv(%ld, %p, %d)", location, v, size);
    makeContextCurrent();
    glUniform2fv(location, size, v);
}

void GraphicsContext3D::uniform2i(GC3Dint location, GC3Dint x, GC3Dint y)
{
    LOGWEBGL("glUniform2i(%ld, %d, %d)", location, x, y);
    makeContextCurrent();
    glUniform2i(location, x, y);
}

void GraphicsContext3D::uniform2iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    LOGWEBGL("glUniform2iv(%ld, %p, %d)", location, v, size);
    makeContextCurrent();
    glUniform2iv(location, size, v);
}

void GraphicsContext3D::uniform3f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z)
{
    LOGWEBGL("glUniform3f(%ld, %f, %f, %f)", location, x, y, z);
    makeContextCurrent();
    glUniform3f(location, x, y, z);
}

void GraphicsContext3D::uniform3fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    LOGWEBGL("glUniform3fv(%ld, %p, %d)", location, v, size);
    makeContextCurrent();
    glUniform3fv(location, size, v);
}

void GraphicsContext3D::uniform3i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z)
{
    LOGWEBGL("glUniform3i(%ld, %d, %d, %d)", location, x, y, z);
    makeContextCurrent();
    glUniform3i(location, x, y, z);
}

void GraphicsContext3D::uniform3iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    LOGWEBGL("glUniform3iv(%ld, %p, %d)", location, v, size);
    makeContextCurrent();
    glUniform3iv(location, size, v);
}

void GraphicsContext3D::uniform4f(GC3Dint location, GC3Dfloat x, GC3Dfloat y,
                                  GC3Dfloat z, GC3Dfloat w)
{
    LOGWEBGL("glUniform4f(%ld, %f, %f, %f, %f)", location, x, y, z, w);
    makeContextCurrent();
    glUniform4f(location, x, y, z, w);
}

void GraphicsContext3D::uniform4fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    LOGWEBGL("glUniform4fv(%ld, %p, %d)", location, v, size);
    makeContextCurrent();
    glUniform4fv(location, size, v);
}

void GraphicsContext3D::uniform4i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w)
{
    LOGWEBGL("glUniform4i(%ld, %d, %d, %d, %d)", location, x, y, z, w);
    makeContextCurrent();
    glUniform4i(location, x, y, z, w);
}

void GraphicsContext3D::uniform4iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    LOGWEBGL("glUniform4iv(%ld, %p, %d)", location, v, size);
    makeContextCurrent();
    glUniform4iv(location, size, v);
}

void GraphicsContext3D::uniformMatrix2fv(GC3Dint location, GC3Dboolean transpose,
                                         GC3Dfloat* value, GC3Dsizei size)
{
    LOGWEBGL("glUniformMatrix2fv(%ld, %s, %p, %d)",
             location, transpose ? "true" : "false", value, size);
    makeContextCurrent();
    glUniformMatrix2fv(location, size, transpose, value);
}

void GraphicsContext3D::uniformMatrix3fv(GC3Dint location, GC3Dboolean transpose,
                                         GC3Dfloat* value, GC3Dsizei size)
{
    LOGWEBGL("glUniformMatrix3fv(%ld, %s, %p, %d)",
             location, transpose ? "true" : "false", value, size);
    makeContextCurrent();
    glUniformMatrix3fv(location, size, transpose, value);
}

void GraphicsContext3D::uniformMatrix4fv(GC3Dint location, GC3Dboolean transpose,
                                         GC3Dfloat* value, GC3Dsizei size)
{
    LOGWEBGL("glUniformMatrix4fv(%ld, %s, %p, %d)",
             location, transpose ? "true" : "false", value, size);
    makeContextCurrent();
    glUniformMatrix4fv(location, size, transpose, value);
}

void GraphicsContext3D::useProgram(Platform3DObject program)
{
    LOGWEBGL("glUseProgram(%lu)", program);
    makeContextCurrent();
    glUseProgram(program);
}

void GraphicsContext3D::validateProgram(Platform3DObject program)
{
    LOGWEBGL("glValidateProgram(%lu)", program);
    makeContextCurrent();
    glValidateProgram(program);
}

void GraphicsContext3D::vertexAttrib1f(GC3Duint index, GC3Dfloat x)
{
    LOGWEBGL("glVertexAttrib1f(%lu, %f)", index, x);
    makeContextCurrent();
    glVertexAttrib1f(index, x);
}

void GraphicsContext3D::vertexAttrib1fv(GC3Duint index, GC3Dfloat* values)
{
    LOGWEBGL("glVertexAttrib1fv(%lu, %p)", index, values);
    makeContextCurrent();
    glVertexAttrib1fv(index, values);
}

void GraphicsContext3D::vertexAttrib2f(GC3Duint index, GC3Dfloat x, GC3Dfloat y)
{
    LOGWEBGL("glVertexAttrib2f(%lu, %f, %f)", index, x, y);
    makeContextCurrent();
    glVertexAttrib2f(index, x, y);
}

void GraphicsContext3D::vertexAttrib2fv(GC3Duint index, GC3Dfloat* values)
{
    LOGWEBGL("glVertexAttrib2fv(%lu, %p)", index, values);
    makeContextCurrent();
    glVertexAttrib2fv(index, values);
}

void GraphicsContext3D::vertexAttrib3f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z)
{
    LOGWEBGL("glVertexAttrib3f(%lu, %f, %f, %f)", index, x, y, z);
    makeContextCurrent();
    glVertexAttrib3f(index, x, y, z);
}

void GraphicsContext3D::vertexAttrib3fv(GC3Duint index, GC3Dfloat* values)
{
    LOGWEBGL("glVertexAttrib3fv(%lu, %p)", index, values);
    makeContextCurrent();
    glVertexAttrib3fv(index, values);
}

void GraphicsContext3D::vertexAttrib4f(GC3Duint index, GC3Dfloat x, GC3Dfloat y,
                                       GC3Dfloat z, GC3Dfloat w)
{
    LOGWEBGL("glVertexAttrib4f(%lu, %f, %f, %f, %f)", index, x, y, z, w);
    makeContextCurrent();
    glVertexAttrib4f(index, x, y, z, w);
}

void GraphicsContext3D::vertexAttrib4fv(GC3Duint index, GC3Dfloat* values)
{
    LOGWEBGL("glVertexAttrib4fv(%lu, %p)", index, values);
    makeContextCurrent();
    glVertexAttrib4fv(index, values);
}

void GraphicsContext3D::vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type,
                                            GC3Dboolean normalized, GC3Dsizei stride,
                                            GC3Dintptr offset)
{
    LOGWEBGL("glVertexAttribPointer(%lu, %d, %d, %s, %lu, %lu)", index, size, type,
             normalized ? "true" : "false", stride, offset);
    makeContextCurrent();
    glVertexAttribPointer(index, size, type, normalized, stride,
                          reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3D::viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    LOGWEBGL("viewport(%ld, %ld, %lu, %lu)", x, y, width, height);
    makeContextCurrent();
    m_internal->viewport(x, y, width, height);
}

void GraphicsContext3D::reshape(int width, int height)
{
    LOGWEBGL("reshape(%d, %d)", width, height);
    if ((width == m_currentWidth) && (height == m_currentHeight)) {
        return;
    }
    m_internal->reshape(width, height);
    m_currentWidth = m_internal->width();
    m_currentHeight = m_internal->height();
}

void GraphicsContext3D::recreateSurface()
{
    LOGWEBGL("recreateSurface()");
    m_internal->recreateSurface();
}

void GraphicsContext3D::releaseSurface()
{
    LOGWEBGL("releaseSurface()");
    m_internal->releaseSurface();
}

IntSize GraphicsContext3D::getInternalFramebufferSize()
{
    return IntSize(m_currentWidth, m_currentHeight);
}

void GraphicsContext3D::markContextChanged()
{
    LOGWEBGL("markContextChanged()");
    m_internal->markContextChanged();
}

void GraphicsContext3D::markLayerComposited()
{
    LOGWEBGL("markLayerComposited()");
    m_internal->markLayerComposited();
}

bool GraphicsContext3D::layerComposited() const
{
    LOGWEBGL("layerComposited()");
    return m_internal->layerComposited();
}

void GraphicsContext3D::setContextLostCallback(PassOwnPtr<ContextLostCallback>)
{
}
}
#endif
