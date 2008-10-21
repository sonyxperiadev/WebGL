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

#ifndef PluginPackageAndroid_H
#define PluginPackageAndroid_H

#include "PlatformString.h"
#include "StringHash.h"
#include "Timer.h"
#include "npfunctions.h"
#include <wtf/HashMap.h>
#include "wtf/RefCounted.h"

namespace WebCore {
    typedef HashMap<String, String> MIMEToDescriptionsMap;
    typedef HashMap<String, Vector<String> > MIMEToExtensionsMap;

    class PluginPackageAndroid : public RefCounted<PluginPackageAndroid> {
    public:
        ~PluginPackageAndroid();
        static PluginPackageAndroid* createPackage(const String& path,
                                                   uint32 lastModified);
        
        String path() const { return m_path; }
        String name() const { return m_name; }
        String fileName() const { return m_fileName; }
        String description() const { return m_description; }
        uint32 getLastModified() const { return m_lastModified; }

        const MIMEToDescriptionsMap& mimeToDescriptions() const {
            return m_mimeToDescriptions;
        }
        const MIMEToExtensionsMap& mimeToExtensions() const {
            return m_mimeToExtensions;
        }

        bool load();
        void unload();

        const NPPluginFuncs* pluginFuncs() const;

    private:
        // Simple class which calls dlclose() on a dynamic library when going
        // out of scope. Call ok() if the handle should stay open.
        class DynamicLibraryCloser
        {
          public:
            DynamicLibraryCloser(PluginPackageAndroid *package)
                    : m_package(package) { }
            ~DynamicLibraryCloser();
            void ok() { m_package = NULL; }
          private:
            PluginPackageAndroid *m_package;
        };
        friend class DynamicLibraryCloser;

        PluginPackageAndroid(const String& path,
                             uint32 lastModified);
        
        // Set all the NPN function pointers in m_browserFuncs
        void initializeBrowserFuncs();

        // Call dlsym() and set *entry_point to the return value for
        // the symbol 'name'. Return true if the symbol exists.
        bool getEntryPoint(const char *name, void **entry_point);
        
        // Initialize m_mimeToDescriptions and m_mimeToExtensions from
        // this individual MIME entry. If badly formatted, returns
        // false.
        bool initializeMIMEEntry(const String& mime_entry);

        // Splits the MIME description into its entries and passes
        // them to initializeMIMEEntry. Returns false if any of the
        // description is badly format.
        bool initializeMIMEDescription(const String& mime_description);
        
        // Create a Java Plugin object instance
        jobject createPluginObject(const char *name,
                                   const char *path,
                                   const char *fileName,
                                   const char *description);
        
        // Get hold of the Java PluginList instance.
        jobject getPluginListObject();

        // Add the plugin to the PluginList.
        bool addPluginObjectToList(jobject pluginList,
                                   jobject plugin);

        // Remove the plugin from the Pluginlist
        void removePluginObjectFromList(jobject pluginList,
                                        jobject plugin);

        String m_path;          // Path to open this library
        String m_name;          // Human readable name e.g "Shockwave Flash"
        String m_fileName;      // Name of the file e.g "libflashplayer.so"
        String m_description;   // Verbose string e.g "Shockwave Flash 9.0 r48"
        uint32 m_lastModified;  // Last modification time, Unix seconds.

        NPNetscapeFuncs m_browserFuncs;
        NPPluginFuncs m_pluginFuncs;
        MIMEToDescriptionsMap m_mimeToDescriptions;
        MIMEToExtensionsMap m_mimeToExtensions;

        // The Java environment.
        JNIEnv *m_env;

        // The Java Plugin object.
        jobject m_pluginObject;

        // Handle to the dynamic library. Non-NULL if open.
        void *m_libraryHandle;

        // True if the library is in the initialized state
        // (NP_Initialize called)
        bool m_libraryInitialized;
        
        // Entry points into the library obtained using dlsym()
        NP_InitializeFuncPtr m_NP_Initialize;
        NP_ShutdownFuncPtr m_NP_Shutdown;
        NP_GetMIMEDescriptionFuncPtr m_NP_GetMIMEDescription;
        NP_GetValueFuncPtr m_NP_GetValue;
    };
} // namespace WebCore

#endif

#endif
