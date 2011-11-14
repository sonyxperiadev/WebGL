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
#include "ShaderProgram.h"

#if USE(ACCELERATED_COMPOSITING)

#include "FloatPoint3D.h"
#include "GLUtils.h"
#include "TilesManager.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "ShaderProgram", __VA_ARGS__)

namespace WebCore {

static const char gVertexShader[] =
    "attribute vec4 vPosition;\n"
    "uniform mat4 projectionMatrix;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_Position = projectionMatrix * vPosition;\n"
    "  v_texCoord = vec2(vPosition);\n"
    "}\n";

static const char gFragmentShader[] =
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform sampler2D s_texture; \n"
    "void main() {\n"
    "  gl_FragColor = texture2D(s_texture, v_texCoord); \n"
    "  gl_FragColor *= alpha; "
    "}\n";

static const char gFragmentShaderInverted[] =
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform float contrast; \n"
    "uniform sampler2D s_texture; \n"
    "void main() {\n"
    "  vec4 pixel = texture2D(s_texture, v_texCoord); \n"
    "  float a = pixel.a; \n"
    "  float color = a - (0.2989 * pixel.r + 0.5866 * pixel.g + 0.1145 * pixel.b);\n"
    "  color = ((color - a/2.0) * contrast) + a/2.0; \n"
    "  pixel.rgb = vec3(color, color, color); \n "
    "  gl_FragColor = pixel; \n"
    "  gl_FragColor *= alpha; \n"
    "}\n";

static const char gVideoVertexShader[] =
    "attribute vec4 vPosition;\n"
    "uniform mat4 textureMatrix;\n"
    "uniform mat4 projectionMatrix;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_Position = projectionMatrix * vPosition;\n"
    "  v_texCoord = vec2(textureMatrix * vec4(vPosition.x, 1.0 - vPosition.y, 0.0, 1.0));\n"
    "}\n";

static const char gVideoFragmentShader[] =
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "uniform samplerExternalOES s_yuvTexture;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(s_yuvTexture, v_texCoord);\n"
    "}\n";

static const char gSurfaceTextureOESFragmentShader[] =
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform samplerExternalOES s_texture; \n"
    "void main() {\n"
    "  gl_FragColor = texture2D(s_texture, v_texCoord); \n"
    "  gl_FragColor *= alpha; "
    "}\n";

static const char gSurfaceTextureOESFragmentShaderInverted[] =
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform float contrast; \n"
    "uniform samplerExternalOES s_texture; \n"
    "void main() {\n"
    "  vec4 pixel = texture2D(s_texture, v_texCoord); \n"
    "  float a = pixel.a; \n"
    "  float color = a - (0.2989 * pixel.r + 0.5866 * pixel.g + 0.1145 * pixel.b);\n"
    "  color = ((color - a/2.0) * contrast) + a/2.0; \n"
    "  pixel.rgb = vec3(color, color, color); \n "
    "  gl_FragColor = pixel; \n"
    "  gl_FragColor *= alpha; \n"
    "}\n";

GLuint ShaderProgram::loadShader(GLenum shaderType, const char* pSource)
{
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, 0);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                glGetShaderInfoLog(shader, infoLen, 0, buf);
                XLOG("could not compile shader %d:\n%s\n", shaderType, buf);
                free(buf);
            }
            glDeleteShader(shader);
            shader = 0;
            }
        }
    }
    return shader;
}

GLuint ShaderProgram::createProgram(const char* pVertexSource, const char* pFragmentSource)
{
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        XLOG("couldn't load the vertex shader!");
        return -1;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        XLOG("couldn't load the pixel shader!");
        return -1;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        GLUtils::checkGlError("glAttachShader vertex");
        glAttachShader(program, pixelShader);
        GLUtils::checkGlError("glAttachShader pixel");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, 0, buf);
                    XLOG("could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = -1;
        }
    }
    return program;
}

ShaderProgram::ShaderProgram()
    : m_blendingEnabled(false)
    , m_contrast(1)
    , m_alphaLayer(false)
    , m_currentScale(1.0f)
{
    init();
}

void ShaderProgram::init()
{
    m_program = createProgram(gVertexShader, gFragmentShader);
    m_programInverted = createProgram(gVertexShader, gFragmentShaderInverted);
    m_videoProgram = createProgram(gVideoVertexShader, gVideoFragmentShader);
    m_surfTexOESProgram =
        createProgram(gVertexShader, gSurfaceTextureOESFragmentShader);
    m_surfTexOESProgramInverted =
        createProgram(gVertexShader, gSurfaceTextureOESFragmentShaderInverted);

    if (m_program == -1
        || m_programInverted == -1
        || m_videoProgram == -1
        || m_surfTexOESProgram == -1
        || m_surfTexOESProgramInverted == -1)
        return;

    m_hProjectionMatrix = glGetUniformLocation(m_program, "projectionMatrix");
    m_hAlpha = glGetUniformLocation(m_program, "alpha");
    m_hTexSampler = glGetUniformLocation(m_program, "s_texture");
    m_hPosition = glGetAttribLocation(m_program, "vPosition");

    m_hProjectionMatrixInverted = glGetUniformLocation(m_programInverted, "projectionMatrix");
    m_hAlphaInverted = glGetUniformLocation(m_programInverted, "alpha");
    m_hContrastInverted = glGetUniformLocation(m_surfTexOESProgramInverted, "contrast");
    m_hTexSamplerInverted = glGetUniformLocation(m_programInverted, "s_texture");
    m_hPositionInverted = glGetAttribLocation(m_programInverted, "vPosition");

    m_hVideoProjectionMatrix =
        glGetUniformLocation(m_videoProgram, "projectionMatrix");
    m_hVideoTextureMatrix = glGetUniformLocation(m_videoProgram, "textureMatrix");
    m_hVideoTexSampler = glGetUniformLocation(m_videoProgram, "s_yuvTexture");
    m_hVideoPosition = glGetAttribLocation(m_program, "vPosition");

    m_hSTOESProjectionMatrix =
        glGetUniformLocation(m_surfTexOESProgram, "projectionMatrix");
    m_hSTOESAlpha = glGetUniformLocation(m_surfTexOESProgram, "alpha");
    m_hSTOESTexSampler = glGetUniformLocation(m_surfTexOESProgram, "s_texture");
    m_hSTOESPosition = glGetAttribLocation(m_surfTexOESProgram, "vPosition");

    m_hSTOESProjectionMatrixInverted =
        glGetUniformLocation(m_surfTexOESProgramInverted, "projectionMatrix");
    m_hSTOESAlphaInverted = glGetUniformLocation(m_surfTexOESProgramInverted, "alpha");
    m_hSTOESContrastInverted = glGetUniformLocation(m_surfTexOESProgramInverted, "contrast");
    m_hSTOESTexSamplerInverted = glGetUniformLocation(m_surfTexOESProgramInverted, "s_texture");
    m_hSTOESPositionInverted = glGetAttribLocation(m_surfTexOESProgramInverted, "vPosition");


    const GLfloat coord[] = {
        0.0f, 0.0f, // C
        1.0f, 0.0f, // D
        0.0f, 1.0f, // A
        1.0f, 1.0f // B
    };

    glGenBuffers(1, m_textureBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_textureBuffer[0]);
    glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(GLfloat), coord, GL_STATIC_DRAW);

    GLUtils::checkGlError("init");
}

void ShaderProgram::resetBlending()
{
    glDisable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    m_blendingEnabled = false;
}

void ShaderProgram::setBlendingState(bool enableBlending)
{
    if (enableBlending == m_blendingEnabled)
        return;

    if (enableBlending)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    m_blendingEnabled = enableBlending;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Drawing
/////////////////////////////////////////////////////////////////////////////////////////

void ShaderProgram::setViewport(SkRect& viewport, float scale)
{
    TransformationMatrix ortho;
    GLUtils::setOrthographicMatrix(ortho, viewport.fLeft, viewport.fTop,
                                   viewport.fRight, viewport.fBottom, -1000, 1000);
    m_projectionMatrix = ortho;
    m_viewport = viewport;
    m_currentScale = scale;
}

void ShaderProgram::setProjectionMatrix(SkRect& geometry, GLint projectionMatrixHandle)
{
    TransformationMatrix translate;
    translate.translate3d(geometry.fLeft, geometry.fTop, 0.0);
    TransformationMatrix scale;
    scale.scale3d(geometry.width(), geometry.height(), 1.0);

    TransformationMatrix total;
    if (!m_alphaLayer)
        total = m_projectionMatrix * m_repositionMatrix * m_webViewMatrix
                * translate * scale;
    else
        total = m_projectionMatrix * translate * scale;

    GLfloat projectionMatrix[16];
    GLUtils::toGLMatrix(projectionMatrix, total);
    glUniformMatrix4fv(projectionMatrixHandle, 1, GL_FALSE, projectionMatrix);
}

void ShaderProgram::drawQuadInternal(SkRect& geometry,
                                     GLint textureId,
                                     float opacity,
                                     GLint program,
                                     GLint projectionMatrixHandle,
                                     GLint texSampler,
                                     GLenum textureTarget,
                                     GLint position,
                                     GLint alpha,
                                     GLint texFilter,
                                     GLint contrast)
{
    glUseProgram(program);

    if (!geometry.isEmpty())
         setProjectionMatrix(geometry, projectionMatrixHandle);
    else {
        TransformationMatrix matrix;
        // Map x,y from (0,1) to (-1, 1)
        matrix.scale3d(2, 2, 1);
        matrix.translate3d(-0.5, -0.5, 0);
        GLfloat projectionMatrix[16];
        GLUtils::toGLMatrix(projectionMatrix, matrix);
        glUniformMatrix4fv(projectionMatrixHandle, 1, GL_FALSE, projectionMatrix);
    }

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(texSampler, 0);
    glBindTexture(textureTarget, textureId);
    glTexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, texFilter);
    glTexParameteri(textureTarget, GL_TEXTURE_MAG_FILTER, texFilter);
    glTexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindBuffer(GL_ARRAY_BUFFER, m_textureBuffer[0]);
    glEnableVertexAttribArray(position);
    glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glUniform1f(alpha, opacity);
    if (contrast != -1)
        glUniform1f(contrast, m_contrast);

    setBlendingState(opacity < 1.0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void ShaderProgram::drawQuad(SkRect& geometry, int textureId, float opacity,
                             GLenum textureTarget, GLint texFilter)
{
    if (textureTarget == GL_TEXTURE_2D) {
        if (!TilesManager::instance()->invertedScreen()) {
            drawQuadInternal(geometry, textureId, opacity, m_program,
                             m_hProjectionMatrix,
                             m_hTexSampler, GL_TEXTURE_2D,
                             m_hPosition, m_hAlpha, texFilter);
        } else {
            // With the new GPU texture upload path, we do not use an FBO
            // to blit the texture we receive from the TexturesGenerator thread.
            // To implement inverted rendering, we thus have to do the rendering
            // live, by using a different shader.
            drawQuadInternal(geometry, textureId, opacity, m_programInverted,
                             m_hProjectionMatrixInverted,
                             m_hTexSamplerInverted, GL_TEXTURE_2D,
                             m_hPositionInverted, m_hAlphaInverted, texFilter,
                             m_hContrastInverted);
        }
    } else if (textureTarget == GL_TEXTURE_EXTERNAL_OES
               && !TilesManager::instance()->invertedScreen()) {
        drawQuadInternal(geometry, textureId, opacity, m_surfTexOESProgram,
                         m_hSTOESProjectionMatrix,
                         m_hSTOESTexSampler, GL_TEXTURE_EXTERNAL_OES,
                         m_hSTOESPosition, m_hSTOESAlpha, texFilter);
    } else if (textureTarget == GL_TEXTURE_EXTERNAL_OES
               && TilesManager::instance()->invertedScreen()) {
        drawQuadInternal(geometry, textureId, opacity, m_surfTexOESProgramInverted,
                         m_hSTOESProjectionMatrixInverted,
                         m_hSTOESTexSamplerInverted, GL_TEXTURE_EXTERNAL_OES,
                         m_hSTOESPositionInverted, m_hSTOESAlphaInverted,
                         texFilter, m_hSTOESContrastInverted);
    }
    GLUtils::checkGlError("drawQuad");
}

void ShaderProgram::setViewRect(const IntRect& viewRect)
{
    m_viewRect = viewRect;

    // We do clipping using glScissor, which needs to take
    // coordinates in screen space. The following matrix transform
    // content coordinates in screen coordinates.
    TransformationMatrix translate;
    translate.translate(1.0, 1.0);

    TransformationMatrix scale;
    scale.scale3d(m_viewRect.width() * 0.5f, m_viewRect.height() * 0.5f, 1);

    m_documentToScreenMatrix = scale * translate * m_projectionMatrix;

    translate.scale3d(1, -1, 1);
    m_documentToInvScreenMatrix = scale * translate * m_projectionMatrix;

    IntRect rect(0, 0, m_webViewRect.width(), m_webViewRect.height());
    m_documentViewport = m_documentToScreenMatrix.inverse().mapRect(rect);
}

// This function transform a clip rect extracted from the current layer
// into a clip rect in screen coordinates -- used by the clipping rects
FloatRect ShaderProgram::rectInScreenCoord(const TransformationMatrix& drawMatrix, const IntSize& size)
{
    FloatRect srect(0, 0, size.width(), size.height());
    TransformationMatrix renderMatrix = m_documentToScreenMatrix * drawMatrix;
    return renderMatrix.mapRect(srect);
}

// used by the partial screen invals
FloatRect ShaderProgram::rectInInvScreenCoord(const TransformationMatrix& drawMatrix, const IntSize& size)
{
    FloatRect srect(0, 0, size.width(), size.height());
    TransformationMatrix renderMatrix = m_documentToInvScreenMatrix * drawMatrix;
    return renderMatrix.mapRect(srect);
}

FloatRect ShaderProgram::rectInInvScreenCoord(const FloatRect& rect)
{
    return m_documentToInvScreenMatrix.mapRect(rect);
}

FloatRect ShaderProgram::rectInScreenCoord(const FloatRect& rect)
{
    return m_documentToScreenMatrix.mapRect(rect);
}

FloatRect ShaderProgram::convertScreenCoordToDocumentCoord(const FloatRect& rect)
{
    return m_documentToScreenMatrix.inverse().mapRect(rect);
}

FloatRect ShaderProgram::convertInvScreenCoordToScreenCoord(const FloatRect& rect)
{
    FloatRect documentRect = m_documentToInvScreenMatrix.inverse().mapRect(rect);
    return rectInScreenCoord(documentRect);
}

FloatRect ShaderProgram::convertScreenCoordToInvScreenCoord(const FloatRect& rect)
{
    FloatRect documentRect = m_documentToScreenMatrix.inverse().mapRect(rect);
    return rectInInvScreenCoord(documentRect);
}

void ShaderProgram::setScreenClip(const IntRect& clip)
{
    m_screenClip = clip;
    IntRect mclip = clip;

    // the clip from frameworks is in full screen coordinates
    mclip.setY(clip.y() - m_webViewRect.y() - m_titleBarHeight);
    FloatRect tclip = convertInvScreenCoordToScreenCoord(mclip);
    IntRect screenClip(tclip.x(), tclip.y(), tclip.width(), tclip.height());
    m_screenClip = screenClip;
}

// clip is in screen coordinates
void ShaderProgram::clip(const FloatRect& clip)
{
    if (clip == m_clipRect)
        return;

    // we should only call glScissor in this function, so that we can easily
    // track the current clipping rect.

    IntRect screenClip(clip.x(),
                       clip.y(),
                       clip.width(), clip.height());

    if (!m_screenClip.isEmpty())
        screenClip.intersect(m_screenClip);

    screenClip.setY(screenClip.y() + m_viewRect.y());
    if (screenClip.x() < 0) {
        int w = screenClip.width();
        w += screenClip.x();
        screenClip.setX(0);
        screenClip.setWidth(w);
    }
    if (screenClip.y() < 0) {
        int h = screenClip.height();
        h += screenClip.y();
        screenClip.setY(0);
        screenClip.setHeight(h);
    }

    glScissor(screenClip.x(), screenClip.y(), screenClip.width(), screenClip.height());

    m_clipRect = clip;
}

IntRect ShaderProgram::clippedRectWithViewport(const IntRect& rect, int margin)
{
    IntRect viewport(m_viewport.fLeft - margin, m_viewport.fTop - margin,
                     m_viewport.width() + margin, m_viewport.height() + margin);
    viewport.intersect(rect);
    return viewport;
}

float ShaderProgram::zValue(const TransformationMatrix& drawMatrix, float w, float h)
{
    TransformationMatrix modifiedDrawMatrix = drawMatrix;
    modifiedDrawMatrix.scale3d(w, h, 1);
    TransformationMatrix renderMatrix = m_projectionMatrix * modifiedDrawMatrix;
    FloatPoint3D point(0.5, 0.5, 0.0);
    FloatPoint3D result = renderMatrix.mapPoint(point);
    return result.z();
}

void ShaderProgram::drawLayerQuadInternal(const GLfloat* projectionMatrix,
                                          int textureId, float opacity,
                                          GLenum textureTarget, GLint program,
                                          GLint matrix, GLint texSample,
                                          GLint position, GLint alpha,
                                          GLint contrast)
{
    glUseProgram(program);
    glUniformMatrix4fv(matrix, 1, GL_FALSE, projectionMatrix);

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(texSample, 0);
    glBindTexture(textureTarget, textureId);
    glTexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    glBindBuffer(GL_ARRAY_BUFFER, m_textureBuffer[0]);
    glEnableVertexAttribArray(position);
    glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glUniform1f(alpha, opacity);
    if (contrast != -1)
        glUniform1f(contrast, m_contrast);
}


void ShaderProgram::drawLayerQuad(const TransformationMatrix& drawMatrix,
                                  const SkRect& geometry, int textureId,
                                  float opacity, bool forceBlending,
                                  GLenum textureTarget)
{

    TransformationMatrix modifiedDrawMatrix = drawMatrix;
    // move the drawing depending on where the texture is on the layer
    modifiedDrawMatrix.translate(geometry.fLeft, geometry.fTop);
    modifiedDrawMatrix.scale3d(geometry.width(), geometry.height(), 1);

    TransformationMatrix renderMatrix;
    if (!m_alphaLayer)
        renderMatrix = m_projectionMatrix * m_repositionMatrix
                       * m_webViewMatrix * modifiedDrawMatrix;
    else
        renderMatrix = m_projectionMatrix * modifiedDrawMatrix;

    GLfloat projectionMatrix[16];
    GLUtils::toGLMatrix(projectionMatrix, renderMatrix);
    if (textureTarget == GL_TEXTURE_2D) {
        if (!TilesManager::instance()->invertedScreen()) {
            drawLayerQuadInternal(projectionMatrix, textureId, opacity,
                                  GL_TEXTURE_2D, m_program,
                                  m_hProjectionMatrix, m_hTexSampler,
                                  m_hPosition, m_hAlpha);
        } else {
            drawLayerQuadInternal(projectionMatrix, textureId, opacity,
                                  GL_TEXTURE_2D, m_programInverted,
                                  m_hProjectionMatrixInverted, m_hTexSamplerInverted,
                                  m_hPositionInverted, m_hAlphaInverted,
                                  m_hContrastInverted);
        }
    } else if (textureTarget == GL_TEXTURE_EXTERNAL_OES
               && !TilesManager::instance()->invertedScreen()) {
        drawLayerQuadInternal(projectionMatrix, textureId, opacity,
                              GL_TEXTURE_EXTERNAL_OES, m_surfTexOESProgram,
                              m_hSTOESProjectionMatrix, m_hSTOESTexSampler,
                              m_hSTOESPosition, m_hSTOESAlpha);
    } else if (textureTarget == GL_TEXTURE_EXTERNAL_OES
               && TilesManager::instance()->invertedScreen()) {
        drawLayerQuadInternal(projectionMatrix, textureId, opacity,
                              GL_TEXTURE_EXTERNAL_OES, m_surfTexOESProgramInverted,
                              m_hSTOESProjectionMatrixInverted, m_hSTOESTexSamplerInverted,
                              m_hSTOESPositionInverted, m_hSTOESAlphaInverted,
                              m_hSTOESContrastInverted);
    }

    setBlendingState(forceBlending || opacity < 1.0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    GLUtils::checkGlError("drawLayerQuad");
}

void ShaderProgram::drawVideoLayerQuad(const TransformationMatrix& drawMatrix,
                                       float* textureMatrix, SkRect& geometry,
                                       int textureId)
{
    // switch to our custom yuv video rendering program
    glUseProgram(m_videoProgram);

    TransformationMatrix modifiedDrawMatrix = drawMatrix;
    modifiedDrawMatrix.translate(geometry.fLeft, geometry.fTop);
    modifiedDrawMatrix.scale3d(geometry.width(), geometry.height(), 1);
    TransformationMatrix renderMatrix = m_projectionMatrix * modifiedDrawMatrix;

    GLfloat projectionMatrix[16];
    GLUtils::toGLMatrix(projectionMatrix, renderMatrix);
    glUniformMatrix4fv(m_hVideoProjectionMatrix, 1, GL_FALSE, projectionMatrix);
    glUniformMatrix4fv(m_hVideoTextureMatrix, 1, GL_FALSE, textureMatrix);

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(m_hVideoTexSampler, 0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);

    glBindBuffer(GL_ARRAY_BUFFER, m_textureBuffer[0]);
    glEnableVertexAttribArray(m_hVideoPosition);
    glVertexAttribPointer(m_hVideoPosition, 2, GL_FLOAT, GL_FALSE, 0, 0);

    setBlendingState(false);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void ShaderProgram::setWebViewMatrix(const float* matrix, bool alphaLayer)
{
    GLUtils::convertToTransformationMatrix(matrix, m_webViewMatrix);
    m_alphaLayer = alphaLayer;
}

void ShaderProgram::calculateAnimationDelta()
{
    // The matrix contains the scrolling info, so this rect is starting from
    // the m_viewport.
    // So we just need to map the webview's visible rect using the matrix,
    // calculate the difference b/t transformed rect and the webViewRect,
    // then we can get the delta x , y caused by the animation.
    // Note that the Y is for reporting back to GL viewport, so it is inverted.
    // When it is alpha animation, then we rely on the framework implementation
    // such that there is no matrix applied in native webkit.
    if (!m_alphaLayer) {
        FloatRect rect(m_viewport.fLeft * m_currentScale,
                       m_viewport.fTop * m_currentScale,
                       m_webViewRect.width(),
                       m_webViewRect.height());
        rect = m_webViewMatrix.mapRect(rect);
        m_animationDelta.setX(rect.x() - m_webViewRect.x() );
        m_animationDelta.setY(rect.y() + rect.height() - m_webViewRect.y()
                              - m_webViewRect.height() - m_titleBarHeight);

        m_repositionMatrix.makeIdentity();
        m_repositionMatrix.translate3d(-m_webViewRect.x(), -m_webViewRect.y() - m_titleBarHeight, 0);
        m_repositionMatrix.translate3d(m_viewport.fLeft * m_currentScale, m_viewport.fTop * m_currentScale, 0);
        m_repositionMatrix.translate3d(-m_animationDelta.x(), -m_animationDelta.y(), 0);
    } else {
        m_animationDelta.setX(0);
        m_animationDelta.setY(0);
        m_repositionMatrix.makeIdentity();
    }

}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
