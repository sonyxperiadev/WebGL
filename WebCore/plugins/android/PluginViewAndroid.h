#ifdef ANDROID_PLUGINS

/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PluginViewAndroid_H
#define PluginViewAndroid_H

#include "CString.h"
#include "IntRect.h"
#include "KURL.h"
#include "PlatformString.h"
#include "ResourceRequest.h"
#include "Widget.h"
#include "npapi.h"
#include "WebCoreViewBridge.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace KJS {
    namespace Bindings {
        class Instance;
    }
}

namespace WebCore {
    class Element;
    class Frame;
    class Image;
    class KURL;
    class PluginPackageAndroid;
    
    enum PluginStatus {
        PluginStatusCanNotFindPlugin,
        PluginStatusCanNotLoadPlugin,
        PluginStatusLoadedSuccessfully
    };

    class PluginViewBridgeAndroid : public WebCoreViewBridge {
      public:
        PluginViewBridgeAndroid();
        ~PluginViewBridgeAndroid();
        virtual void draw(GraphicsContext* gc, const IntRect& rect, bool);
      private:
        Image* m_image;
    };

    class PluginViewAndroid : public Widget {
    public:
        PluginViewAndroid(Frame* parentFrame,
                          const IntSize&,
                          PluginPackageAndroid* plugin,
                          Element*,
                          const KURL&,
                          const Vector<String>& paramNames,
                          const Vector<String>& paramValues,
                          const String& mimeType,
                          bool loadManually);
        virtual ~PluginViewAndroid();

        PluginPackageAndroid* plugin() const { return m_plugin.get(); }
        NPP instance() const { return m_instance; }

        static PluginViewAndroid* currentPluginView();

        KJS::Bindings::Instance* bindingInstance();

        PluginStatus status() const { return m_status; }

        // NPN stream functions
        NPError getURLNotify(const char* url,
                             const char* target,
                             void* notifyData);
        NPError getURL(const char* url,
                       const char* target);
        NPError postURLNotify(const char* url,
                              const char* target,
                              uint32 len,
                              const char* but,
                              NPBool file,
                              void* notifyData);
        NPError postURL(const char* url,
                        const char* target,
                        uint32 len,
                        const char* but,
                        NPBool file);
        NPError newStream(NPMIMEType type,
                          const char* target,
                          NPStream** stream);
        int32 write(NPStream* stream, int32 len, void* buffer);
        NPError destroyStream(NPStream* stream, NPReason reason);

        // NPN misc functions
        const char* userAgent();
        void status(const char* message);
        NPError getValue(NPNVariable variable, void* value);
        NPError setValue(NPPVariable variable, void* value);

        // NPN UI functions
        void invalidateRect(NPRect*);
        void invalidateRegion(NPRegion);
        void forceRedraw();

        // Widget functions
        virtual void setParent(ScrollView*);

    private:
        void setParameters(const Vector<String>& paramNames,
                           const Vector<String>& paramValues);
        void init();
        bool start();
        void stop();
        static void setCurrentPluginView(PluginViewAndroid*);

        // Maintain a refcount on the plugin. It should be deleted
        // once all views no longer reference it.
        RefPtr<PluginPackageAndroid> m_plugin;
        Element* m_element;
        Frame* m_parentFrame;
        CString m_userAgent;
        bool m_isStarted;
        KURL m_url;
        PluginStatus m_status;

        int m_mode;
        int m_paramCount;
        char** m_paramNames;
        char** m_paramValues;

        CString m_mimeType;
        NPP m_instance;
        NPP_t m_instanceStruct;

        bool m_isWindowed;
        bool m_isTransparent;
        bool m_haveInitialized;

        unsigned m_lastMessage;

        bool m_loadManually;

        // Sorry, can't use RefPtr<> due to name collision with SkRefCnt.
        PluginViewBridgeAndroid *m_viewBridge;

        static PluginViewAndroid* s_currentPluginView;
    };

} // namespace WebCore

#endif 

#endif // defined(ANDROID_PLUGINS)
