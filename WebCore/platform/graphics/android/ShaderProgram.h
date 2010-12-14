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

#ifndef SHADERPROGRAM_H_
#define SHADERPROGRAM_H_

#if USE(ACCELERATED_COMPOSITING)

#include <GLES2/gl2.h>
#include "SkRect.h"
#include "TransformationMatrix.h"

namespace WebCore {

class ShaderProgram {
 public:
  ShaderProgram();
  int projectionMatrix() { return m_hProjectionMatrix; }
  int alpha() { return m_hAlpha; }
  int textureSampler() { return m_hTexSampler; }
  int program() { return m_program; }

  // Drawing
  void setViewport(SkRect& viewport);
  void drawQuad(SkRect& geometry, int textureId, float opacity);
  void drawLayerQuad(const TransformationMatrix& drawMatrix,
                     SkRect& geometry, int textureId, float opacity);

 private:
  GLuint loadShader(GLenum shaderType, const char* pSource);
  GLuint createProgram(const char* vertexSource, const char* fragmentSource);
  void setProjectionMatrix(SkRect& geometry);

  int m_program;

  TransformationMatrix m_projectionMatrix;
  GLuint m_textureBuffer[1];

  // uniforms
  int m_hProjectionMatrix;
  int m_hAlpha;
  int m_hTexSampler;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // SHADERPROGRAM_H_
