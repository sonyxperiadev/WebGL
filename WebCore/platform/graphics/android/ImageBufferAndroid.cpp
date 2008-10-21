/* ImageBufferAndroid.cpp
**
** Copyright 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#include "config.h"
#include "ImageBuffer.h"

#include "GraphicsContext.h"

using namespace std;

namespace WebCore {

auto_ptr<ImageBuffer> ImageBuffer::create(const IntSize& size, bool grayScale)
{
    // Ignore grayScale for now, since SkBitmap doesn't support it... yet

    GraphicsContext* ctx = GraphicsContext::createOffscreenContext(size.width(), size.height());

    auto_ptr<GraphicsContext> context(ctx);
    
    return auto_ptr<ImageBuffer>(new ImageBuffer(size, context));
}


ImageBuffer::ImageBuffer(const IntSize& size, auto_ptr<GraphicsContext> context)
    : m_data(NULL), m_size(size), m_context(context.release())
{
}

ImageBuffer::~ImageBuffer()
{
}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

}
