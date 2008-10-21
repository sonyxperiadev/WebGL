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

#ifndef PluginDatabaseAndroid_H
#define PluginDatabaseAndroid_H

#include <wtf/Vector.h>

#include "PlatformString.h"
#include "PluginPackageAndroid.h"

namespace WebCore {
    class Element;
    class Frame;
    class IntSize;
    class KURL;
    class PluginPackageAndroid;
    class PluginViewAndroid;

    class PluginDatabaseAndroid {
    public:
        ~PluginDatabaseAndroid();

        static PluginDatabaseAndroid* installedPlugins();
        PluginViewAndroid* createPluginView(Frame* parentFrame,
                                            const IntSize&,
                                            Element* element,
                                            const KURL& url,
                                            const Vector<String>& paramNames,
                                            const Vector<String>& paramValues,
                                            const String& mimeType,
                                            bool loadManually);

        bool refresh();
        Vector<PluginPackageAndroid*> plugins() const;
        bool isMIMETypeRegistered(const String& mimeType) const;
        void addExtraPluginPath(const String&);
        static bool isPluginBlacklisted(PluginPackageAndroid* plugin);
        static void setDefaultPluginsPath(const String&);

     private:
        explicit PluginDatabaseAndroid();

        bool addPluginFile(const String& path);
        bool addPluginsInDirectory(const String& path);
        PluginPackageAndroid* findPlugin(const KURL& url, String& mimeType);
        PluginPackageAndroid* pluginForMIMEType(const String& mimeType);
        String MIMETypeForExtension(const String& extension) const;
        PluginPackageAndroid* findPluginByPath(const String& path);
        void removePlugin(PluginPackageAndroid* plugin);

        // List of all paths to search for plugins
        Vector<String> m_pluginsPaths;

        // List of all PluginPackageAndroid instances
        typedef Vector<RefPtr<PluginPackageAndroid> > PackageRefVector;
        PackageRefVector m_plugins;
        
        // The default plugins path from Settings.
        static String s_defaultPluginsPath;
    };

} // namespace WebCore

#endif

#endif
