/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ShaderProgram_h
#define ShaderProgram_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatRect.h"
#include "IntRect.h"
#include "SkRect.h"
#include "TransformationMatrix.h"
#include <GLES2/gl2.h>

#define MAX_CONTRAST 5

namespace WebCore {

class ShaderProgram {
public:
    ShaderProgram();
    void init();
    int program() { return m_program; }

    // Drawing
    void setViewport(SkRect& viewport, float scale);
    float zValue(const TransformationMatrix& drawMatrix, float w, float h);

    // For drawQuad and drawLayerQuad, they can handle 3 cases for now:
    // 1) textureTarget == GL_TEXTURE_2D
    // Normal texture in GL_TEXTURE_2D target.
    // 2) textureTarget == GL_TEXTURE_EXTERNAL_OES
    // Surface texture in GL_TEXTURE_EXTERNAL_OES target.
    // 3) textureTarget == 0 (Will be deprecated soon)
    // Surface texture in GL_TEXTURE_2D target.
    //
    // TODO: Shrink the support modes into 2 (1 and 2) after media framework
    // support Surface texture in GL_TEXTURE_EXTERNAL_OES target on all
    // platforms.
    void drawQuad(SkRect& geometry, int textureId, float opacity,
                  GLenum textureTarget = GL_TEXTURE_2D,
                  GLint texFilter = GL_LINEAR);
    void drawLayerQuad(const TransformationMatrix& drawMatrix,
                       const SkRect& geometry, int textureId, float opacity,
                       bool forceBlending = false,
                       GLenum textureTarget = GL_TEXTURE_2D);
    void drawVideoLayerQuad(const TransformationMatrix& drawMatrix,
                     float* textureMatrix, SkRect& geometry, int textureId);
    void setViewRect(const IntRect& viewRect);
    FloatRect rectInScreenCoord(const TransformationMatrix& drawMatrix,
                                const IntSize& size);
    FloatRect rectInInvScreenCoord(const TransformationMatrix& drawMatrix,
                                const IntSize& size);

    FloatRect rectInInvScreenCoord(const FloatRect& rect);
    FloatRect rectInScreenCoord(const FloatRect& rect);
    FloatRect convertScreenCoordToDocumentCoord(const FloatRect& rect);
    FloatRect convertInvScreenCoordToScreenCoord(const FloatRect& rect);
    FloatRect convertScreenCoordToInvScreenCoord(const FloatRect& rect);

    void setTitleBarHeight(int height) { m_titleBarHeight = height; }
    void setWebViewRect(const IntRect& rect) { m_webViewRect = rect; }
    void setScreenClip(const IntRect& clip);
    void clip(const FloatRect& rect);
    IntRect clippedRectWithViewport(const IntRect& rect, int margin = 0);
    FloatRect documentViewport() { return m_documentViewport; }

    void resetBlending();
    float contrast() { return m_contrast; }
    void setContrast(float c) {
        float contrast = c;
        if (contrast < 0)
            contrast = 0;
        if (contrast > MAX_CONTRAST)
            contrast = MAX_CONTRAST;
        m_contrast = contrast;
    }
    void setWebViewMatrix(const float* matrix, bool alphaLayer);

    // This delta is the delta from the layout pos and the current animation pos.
    // Basically, in terms of layout, the webview is still in the original layout
    // pos, as without animation. Such that the viewport and visible rect etc are
    // still in that pos, too, except the clipping info.
    // Our rendering approach is after applying all the matrix, webView is
    // rendered as if it was at the original layout pos, but then offset the
    // glViewport to match the animation.
    void calculateAnimationDelta();
    int getAnimationDeltaX() { return m_animationDelta.x(); }
    int getAnimationDeltaY() { return m_animationDelta.y(); }

private:
    GLuint loadShader(GLenum shaderType, const char* pSource);
    GLuint createProgram(const char* vertexSource, const char* fragmentSource);
    void setProjectionMatrix(SkRect& geometry, GLint projectionMatrixHandle);

    void setBlendingState(bool enableBlending);

    void drawQuadInternal(SkRect& geometry, GLint textureId, float opacity,
                          GLint program, GLint projectionMatrixHandle,
                          GLint texSampler, GLenum textureTarget,
                          GLint position, GLint alpha,
                          GLint texFilter, GLint contrast = -1);

    void drawLayerQuadInternal(const GLfloat* projectionMatrix, int textureId,
                               float opacity, GLenum textureTarget, GLint program,
                               GLint matrix, GLint texSample,
                               GLint position, GLint alpha, GLint contrast = -1);

    bool m_blendingEnabled;

    int m_program;
    int m_programInverted;
    int m_videoProgram;
    int m_surfTexOESProgram;
    int m_surfTexOESProgramInverted;

    TransformationMatrix m_projectionMatrix;
    GLuint m_textureBuffer[1];

    TransformationMatrix m_documentToScreenMatrix;
    TransformationMatrix m_documentToInvScreenMatrix;
    SkRect m_viewport;
    IntRect m_viewRect;
    FloatRect m_clipRect;
    IntRect m_screenClip;
    int m_titleBarHeight;
    IntRect m_webViewRect;

    FloatRect m_documentViewport;

    // uniforms
    GLint m_hProjectionMatrix;
    GLint m_hAlpha;
    GLint m_hTexSampler;
    GLint m_hProjectionMatrixInverted;
    GLint m_hAlphaInverted;
    GLint m_hContrastInverted;
    GLint m_hTexSamplerInverted;
    GLint m_hVideoProjectionMatrix;
    GLint m_hVideoTextureMatrix;
    GLint m_hVideoTexSampler;

    GLint m_hSTOESProjectionMatrix;
    GLint m_hSTOESAlpha;
    GLint m_hSTOESTexSampler;
    GLint m_hSTOESPosition;

    GLint m_hSTOESProjectionMatrixInverted;
    GLint m_hSTOESAlphaInverted;
    GLint m_hSTOESContrastInverted;
    GLint m_hSTOESTexSamplerInverted;
    GLint m_hSTOESPositionInverted;

    float m_contrast;

    // attribs
    GLint m_hPosition;
    GLint m_hPositionInverted;
    GLint m_hVideoPosition;

    bool  m_alphaLayer;
    TransformationMatrix m_webViewMatrix;
    float m_currentScale;

    // After the webViewTranform, we need to reposition the rect to match our viewport.
    // Basically, the webViewTransformMatrix should apply on the screen resolution.
    // So we start by doing the scale and translate to get each tile into screen coordinates.
    // After applying the webViewTransformMatrix, b/c the way it currently set up
    // for scroll and titlebar, we need to offset both of them.
    // Finally, map everything back to (-1, 1) by using the m_projectionMatrix.
    // TODO: Given that m_webViewMatrix contains most of the tranformation
    // information, we should be able to get rid of some parameter we got from
    // Java side and simplify our code.
    TransformationMatrix  m_repositionMatrix;
    IntPoint m_animationDelta;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // ShaderProgram_h
