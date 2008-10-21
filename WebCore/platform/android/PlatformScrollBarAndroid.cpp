/* 
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
#include "Color.h"
#include "GraphicsContext.h"
#include "PlatformScrollBar.h"
#include "WebCoreViewBridge.h"

#define MINIMUM_SCROLL_KNOB_HEIGHT 10
#define SCROLL_BAR_ROUNDING 5

class ScrollBarView {
public: 
                
private:
    int m_height;
    int m_width;    
};

namespace WebCore {

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation o,
    ScrollbarControlSize s) : Scrollbar(client, o, s)
{

}

PlatformScrollbar::~PlatformScrollbar() 
{ 

}

int PlatformScrollbar::width() const 
{ 
    return 8; 

}

int PlatformScrollbar::height() const {
    return 8;

}

void PlatformScrollbar::setRect(const IntRect& r) 
{
}

void PlatformScrollbar::setEnabled(bool) 
{

}

void PlatformScrollbar::paint(GraphicsContext* gc, const IntRect& damageRect) 
{
}

void PlatformScrollbar::updateThumbPosition()
{
}

void PlatformScrollbar::updateThumbProportion()
{
}

}
