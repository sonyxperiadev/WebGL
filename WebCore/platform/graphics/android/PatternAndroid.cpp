/* 
 *
 * Copyright 2006, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *
 *     http://www.apache.org/licenses/LICENSE-2.0 
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License.
 */

#include "config.h"
#include "Pattern.h"

#include "android_graphics.h"
#include "GraphicsContext.h"
#include "SkBitmapRef.h"
#include "SkCanvas.h"
#include "SkColorShader.h"
#include "SkShader.h"
#include "SkPaint.h"

namespace WebCore {

static SkShader::TileMode toTileMode(bool doRepeat) {
    return doRepeat ? SkShader::kRepeat_TileMode : SkShader::kClamp_TileMode;
}

SkShader* Pattern::createPlatformPattern(const AffineTransform& transform) const
{
    SkBitmapRef* ref = tileImage()->nativeImageForCurrentFrame();
    SkShader* s = SkShader::CreateBitmapShader(ref->bitmap(),
                                               toTileMode(m_repeatX),
                                               toTileMode(m_repeatY));
    
    // TODO: do I treat transform as a local matrix???
    return s;
}

} //namespace
