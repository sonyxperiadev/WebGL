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

#include "GLUtils.h"

#include <GLES2/gl2.h>
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
{
    init();
}

void ShaderProgram::init()
{
    m_program = createProgram(gVertexShader, gFragmentShader);
    m_videoProgram = createProgram(gVideoVertexShader, gVideoFragmentShader);
    if (m_program == -1 || m_videoProgram == -1)
        return;

    m_hProjectionMatrix = glGetUniformLocation(m_program, "projectionMatrix");
    m_hAlpha = glGetUniformLocation(m_program, "alpha");
    m_hTexSampler = glGetUniformLocation(m_program, "s_texture");

    m_hPosition = glGetAttribLocation(m_program, "vPosition");

    m_hVideoProjectionMatrix = glGetUniformLocation(m_videoProgram, "projectionMatrix");
    m_hVideoTextureMatrix = glGetUniformLocation(m_videoProgram, "textureMatrix");
    m_hVideoTexSampler = glGetUniformLocation(m_videoProgram, "s_yuvTexture");

    m_hVideoPosition = glGetAttribLocation(m_program, "vPosition");

    const GLfloat coord[] = {
        0.0f, 0.0f, // C
        1.0f, 0.0f, // D
        0.0f, 1.0f, // A
        1.0f, 1.0f // B
    };

    glGenBuffers(1, m_textureBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_textureBuffer[0]);
    glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(GLfloat), coord, GL_STATIC_DRAW);
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

void ShaderProgram::setViewport(SkRect& viewport)
{
    TransformationMatrix ortho;
    GLUtils::setOrthographicMatrix(ortho, viewport.fLeft, viewport.fTop,
                                   viewport.fRight, viewport.fBottom, -1000, 1000);
    m_projectionMatrix = ortho;
    m_viewport = viewport;
}

void ShaderProgram::setProjectionMatrix(SkRect& geometry)
{
    TransformationMatrix translate;
    translate.translate3d(geometry.fLeft, geometry.fTop, 0.0);
    TransformationMatrix scale;
    scale.scale3d(geometry.width(), geometry.height(), 1.0);

    TransformationMatrix total = m_projectionMatrix;
    total.multLeft(translate);
    total.multLeft(scale);

    GLfloat projectionMatrix[16];
    GLUtils::toGLMatrix(projectionMatrix, total);
    glUniformMatrix4fv(m_hProjectionMatrix, 1, GL_FALSE, projectionMatrix);
}

void ShaderProgram::drawQuad(SkRect& geometry, int textureId, float opacity)
{
    setProjectionMatrix(geometry);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glBindBuffer(GL_ARRAY_BUFFER, m_textureBuffer[0]);
    glEnableVertexAttribArray(m_hPosition);
    glVertexAttribPointer(m_hPosition, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glUniform1f(alpha(), opacity);

    setBlendingState(opacity < 1.0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

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

    m_clippingMatrix = m_projectionMatrix;
    m_clippingMatrix.multiply(translate);
    m_clippingMatrix.multiply(scale);
}

// This function transform a clip rect extracted from the current layer
// into a clip rect in screen coordinates
FloatRect ShaderProgram::clipRectInScreenCoord(const TransformationMatrix& drawMatrix,
                             const IntSize& size)
{
    FloatRect srect(0, 0, size.width(), size.height());
    TransformationMatrix renderMatrix = drawMatrix;
    renderMatrix.multiply(m_clippingMatrix);
    return renderMatrix.mapRect(srect);
}

// clip is in screen coordinates
void ShaderProgram::clip(const FloatRect& clip)
{
    if (clip == m_clipRect)
        return;

    // we should only call glScissor in this function, so that we can easily
    // track the current clipping rect.
    glScissor(m_viewRect.x() + clip.x(), m_viewRect.y() + clip.y(),
              clip.width(), clip.height());

    m_clipRect = clip;
}

IntRect ShaderProgram::clippedRectWithViewport(const IntRect& rect, int margin)
{
    IntRect viewport(m_viewport.fLeft - margin, m_viewport.fTop - margin,
                     m_viewport.width() + margin, m_viewport.height() + margin);
    viewport.intersect(rect);
    return viewport;
}

FloatRect ShaderProgram::projectedRect(const TransformationMatrix& drawMatrix,
                                     IntSize& size)
{
    FloatRect srect(0, 0, size.width(), size.height());

    TransformationMatrix translate;
    translate.translate(1.0, 1.0);
    TransformationMatrix scale;
    scale.scale3d(m_viewport.width() * 0.5f, m_viewport.height() * 0.5f, 1);
    TransformationMatrix translateViewport;
    translateViewport.translate(-m_viewport.fLeft, -m_viewport.fTop);

    TransformationMatrix projectionMatrix = m_projectionMatrix;
    projectionMatrix.scale3d(1, -1, 1);
    projectionMatrix.multiply(translate);
    projectionMatrix.multiply(scale);
    projectionMatrix.multiply(translateViewport);

    TransformationMatrix renderMatrix = drawMatrix;
    renderMatrix.multiply(projectionMatrix);

    FloatRect bounds = renderMatrix.mapRect(srect);
    FloatRect ret(bounds.x(), bounds.y() - m_viewport.height(),
                  bounds.width(), bounds.height());
    return ret;
}

void ShaderProgram::drawLayerQuad(const TransformationMatrix& drawMatrix,
                                  SkRect& geometry, int textureId, float opacity,
                                  bool forceBlending)
{

    TransformationMatrix renderMatrix = drawMatrix;
    // move the drawing depending on where the texture is on the layer
    renderMatrix.translate(geometry.fLeft, geometry.fTop);
    renderMatrix.scale3d(geometry.width(), geometry.height(), 1);
    renderMatrix.multiply(m_projectionMatrix);

    GLfloat projectionMatrix[16];
    GLUtils::toGLMatrix(projectionMatrix, renderMatrix);
    glUniformMatrix4fv(m_hProjectionMatrix, 1, GL_FALSE, projectionMatrix);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glBindBuffer(GL_ARRAY_BUFFER, m_textureBuffer[0]);
    glEnableVertexAttribArray(m_hPosition);
    glVertexAttribPointer(m_hPosition, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glUniform1f(alpha(), opacity);

    setBlendingState(forceBlending || opacity < 1.0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void ShaderProgram::drawVideoLayerQuad(const TransformationMatrix& drawMatrix,
                                       float* textureMatrix, SkRect& geometry,
                                       int textureId)
{
    // switch to our custom yuv video rendering program
    glUseProgram(m_videoProgram);

    TransformationMatrix renderMatrix = drawMatrix;
    renderMatrix.translate(geometry.fLeft, geometry.fTop);
    renderMatrix.scale3d(geometry.width(), geometry.height(), 1);
    renderMatrix.multiply(m_projectionMatrix);

    GLfloat projectionMatrix[16];
    GLUtils::toGLMatrix(projectionMatrix, renderMatrix);
    glUniformMatrix4fv(m_hVideoProjectionMatrix, 1, GL_FALSE, projectionMatrix);
    glUniformMatrix4fv(m_hVideoTextureMatrix, 1, GL_FALSE, textureMatrix);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);

    glBindBuffer(GL_ARRAY_BUFFER, m_textureBuffer[0]);
    glEnableVertexAttribArray(m_hVideoPosition);
    glVertexAttribPointer(m_hVideoPosition, 2, GL_FLOAT, GL_FALSE, 0, 0);

    setBlendingState(false);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // switch back to our normal rendering program
    glUseProgram(m_program);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
