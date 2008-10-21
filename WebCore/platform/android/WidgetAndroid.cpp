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
#include "WebCoreViewBridge.h"
#include "WidgetClient.h"
#include "Font.h"
#include "IntRect.h"
#include "GraphicsContext.h"
#include "FrameAndroid.h"
#include "WebCoreFrameBridge.h"
#include "Document.h"
#include "Element.h"

#define LOG_TAG "WebCore"
#undef LOG
#include "utils/Log.h"

namespace WebCore {

#define notImplemented() LOGV("WidgetAndroid: NotYetImplemented")

    class WidgetPrivate
    {
      public:
        Font                m_font;
        WebCoreViewBridge*  m_viewbridge;     // we point to this, but don't delete it (unless we had refcounting...)
        WidgetClient*       m_client;
        bool                m_visible;
    };

    Widget::Widget() : data(new WidgetPrivate)
    {
        data->m_viewbridge = NULL;
        data->m_client = NULL;
        data->m_visible = false;
    }

    Widget::~Widget()
    {
        Release(data->m_viewbridge);
        delete data;
    }

    void Widget::setEnabled(bool enabled)
    {
        WebCoreViewBridge* view = data->m_viewbridge;
        ASSERT(data->m_viewbridge);
        if (view)
            view->setEnabled(enabled);
    }

    bool Widget::isEnabled() const
    {
        ASSERT(data->m_viewbridge);
        return data->m_viewbridge->isEnabled();
    }

    IntRect Widget::frameGeometry() const
    {
        ASSERT(data->m_viewbridge);
        return data->m_viewbridge->getBounds();
    }

    void Widget::setFocus()
    {
        ASSERT(data->m_viewbridge);
        data->m_viewbridge->setFocus(true);
    }

    void Widget::paint(GraphicsContext* ctx, const IntRect& r)
    {
        WebCoreViewBridge* viewBridge = data->m_viewbridge;
        ASSERT(viewBridge);
        viewBridge->layout();
        viewBridge->draw(ctx, r, true);
    }

    void Widget::setCursor(const Cursor& cursor)
    {
        notImplemented();
    }

    void Widget::show()
    {
        if (!data || data->m_visible)
            return;

        data->m_visible = true;
    }

    void Widget::hide()
    {
        notImplemented();
    }

    void Widget::setFrameGeometry(const IntRect& rect)
    {
        ASSERT(data->m_viewbridge);
        data->m_viewbridge->setBounds(rect.x(), rect.y(), rect.x() + rect.width(), rect.y() + rect.height());
    }

    void Widget::setIsSelected(bool isSelected)
    {
        notImplemented();
    }

    void Widget::invalidate() 
    {
        notImplemented();
    }
    
    void Widget::invalidateRect(const IntRect&) 
    {
        notImplemented(); 
    }

    void Widget::removeFromParent() 
    { 
        notImplemented(); 
    }

    void Widget::setClient(WidgetClient* c)
    {
        data->m_client = c;
    }

    WidgetClient* Widget::client() const
    {
        return data->m_client;
    }

    WebCoreViewBridge* Widget::getWebCoreViewBridge() const
    {
        return data->m_viewbridge;
    }

    void Widget::setWebCoreViewBridge(WebCoreViewBridge* view)
    {
        Release(data->m_viewbridge);
        data->m_viewbridge = view;
        view->setWidget(this);
        Retain(data->m_viewbridge);
    }

} // WebCore namepsace
