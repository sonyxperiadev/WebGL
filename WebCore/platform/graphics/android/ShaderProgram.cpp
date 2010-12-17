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
    "  gl_FragColor.a *= alpha; "
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
    if (!vertexShader)
        return 0;

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader)
        return 0;

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
            program = 0;
        }
    }
    return program;
}

ShaderProgram::ShaderProgram()
{
    m_program = createProgram(gVertexShader, gFragmentShader);

    m_hProjectionMatrix = glGetUniformLocation(m_program, "projectionMatrix");
    m_hAlpha = glGetUniformLocation(m_program, "alpha");
    m_hTexSampler = glGetUniformLocation(m_program, "s_texture");

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

/////////////////////////////////////////////////////////////////////////////////////////
// Drawing
/////////////////////////////////////////////////////////////////////////////////////////

void ShaderProgram::setViewport(SkRect& viewport)
{
    TransformationMatrix ortho;
    GLUtils::setOrthographicMatrix(ortho, viewport.fLeft, viewport.fTop,
                                   viewport.fRight, viewport.fBottom, -1000, 1000);
    m_projectionMatrix = ortho;
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
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBindAttribLocation(program(), 1, "vPosition");
    glUniform1f(alpha(), opacity);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    GLUtils::checkGlError("drawQuad");
}

void ShaderProgram::drawLayerQuad(const TransformationMatrix& drawMatrix,
                                  SkRect& geometry, int textureId, float opacity)
{

    TransformationMatrix renderMatrix = drawMatrix;
    renderMatrix.scale3d(geometry.width(), geometry.height(), 1);
    renderMatrix.multiply(m_projectionMatrix);

    GLfloat projectionMatrix[16];
    GLUtils::toGLMatrix(projectionMatrix, renderMatrix);
    glUniformMatrix4fv(m_hProjectionMatrix, 1, GL_FALSE, projectionMatrix);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glBindBuffer(GL_ARRAY_BUFFER, m_textureBuffer[0]);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBindAttribLocation(program(), 1, "vPosition");
    glUniform1f(alpha(), opacity);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
