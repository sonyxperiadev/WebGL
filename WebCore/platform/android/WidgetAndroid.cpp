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
#include "Widget.h"

#include "Document.h"
#include "Element.h"
#include "Font.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "NotImplemented.h"

#include "WebCoreFrameBridge.h"
#include "WebCoreViewBridge.h"
#include "WebViewCore.h"

namespace WebCore {

    class WidgetPrivate
    {
      public:
        Font                m_font;
    };

    Widget::Widget(PlatformWidget widget) : m_data(new WidgetPrivate)
    {
        init(widget);
    }

    Widget::~Widget()
    {
        ASSERT(!parent());
        releasePlatformWidget();
        delete m_data;
    }

    IntRect Widget::frameRect() const
    {
        // FIXME: use m_frame instead?
        if (!platformWidget())
            return IntRect(0, 0, 0, 0);
        return platformWidget()->getBounds();
    }

    void Widget::setFocus()
    {
        notImplemented();
    }

    void Widget::paint(GraphicsContext* ctx, const IntRect& r)
    {
        // FIXME: in what case, will this be called for the top frame?
        if (!platformWidget())
            return;
        platformWidget()->draw(ctx, r);
    }

    void Widget::releasePlatformWidget()
    {
        Release(platformWidget());
    }

    void Widget::retainPlatformWidget()
    {
        Retain(platformWidget());
    }

    void Widget::setCursor(const Cursor& cursor)
    {
        notImplemented();
    }

    void Widget::show()
    {
        notImplemented(); 
    }

    void Widget::hide()
    {
        notImplemented(); 
    }

    void Widget::setFrameRect(const IntRect& rect)
    {
        // FIXME: set m_frame instead?
        // platformWidget() is NULL when called from Scrollbar
        if (!platformWidget())
            return;
        platformWidget()->setLocation(rect.x(), rect.y());
        platformWidget()->setSize(rect.width(), rect.height());
    }

    void Widget::setIsSelected(bool isSelected)
    {
        notImplemented();
    }

    int Widget::screenWidth() const
    {
        const Widget* widget = this;
        while (!widget->isFrameView()) {
            widget = widget->parent();
            if (!widget)
                break;
        }
        if (!widget)
            return 0;

        return android::WebViewCore::getWebViewCore(
                static_cast<const ScrollView*>(widget))->screenWidth();
    }

} // WebCore namepsace
