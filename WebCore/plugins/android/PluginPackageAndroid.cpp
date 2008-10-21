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

#include "config.h"
#include "WebCoreJni.h"
#include "PluginPackageAndroid.h"
#include "PluginDatabaseAndroid.h"

#include "Timer.h"
#include "DeprecatedString.h"
#include "PlatformString.h"
#include "CString.h"
#include "npruntime_impl.h"

#include <dlfcn.h>
#include <errno.h>

#include "PluginDebug.h"

namespace WebCore {

PluginPackageAndroid::PluginPackageAndroid(const String& path,
                                           uint32 lastModified)
        : m_path(path),
          m_name(),
          m_fileName(),
          m_description(),
          m_lastModified(lastModified),
          m_pluginFuncs(),
          m_mimeToDescriptions(),
          m_mimeToExtensions(),
          m_env(0),
          m_pluginObject(0),
          m_libraryHandle(NULL),
          m_libraryInitialized(false),
          m_NP_Initialize((NP_InitializeFuncPtr) NULL),
          m_NP_Shutdown((NP_ShutdownFuncPtr) NULL),
          m_NP_GetMIMEDescription((NP_GetMIMEDescriptionFuncPtr) NULL),
          m_NP_GetValue((NP_GetValueFuncPtr) NULL)
{
    PLUGIN_LOG("Constructing PluginPackageAndroid\n");
    if(android::WebCoreJni::getJavaVM()->GetEnv((void**) &m_env,
                                                JNI_VERSION_1_4) != JNI_OK) {
        PLUGIN_LOG("GetEnv failed!");
    }
}

PluginPackageAndroid::~PluginPackageAndroid()
{
    PLUGIN_LOG("Destructing PluginPackageAndroid\n");
    unload();
}

void PluginPackageAndroid::initializeBrowserFuncs()
{
    // Initialize the NPN function pointers that we hand over to the
    // plugin.

    memset(&m_browserFuncs, 0, sizeof(m_browserFuncs));

    m_browserFuncs.size = sizeof(m_browserFuncs);
    m_browserFuncs.version = NP_VERSION_MINOR;
    m_browserFuncs.geturl = NPN_GetURL;
    m_browserFuncs.posturl = NPN_PostURL;
    m_browserFuncs.requestread = NPN_RequestRead;
    m_browserFuncs.newstream = NPN_NewStream;
    m_browserFuncs.write = NPN_Write;
    m_browserFuncs.destroystream = NPN_DestroyStream;
    m_browserFuncs.status = NPN_Status;
    m_browserFuncs.uagent = NPN_UserAgent;
    m_browserFuncs.memalloc = NPN_MemAlloc;
    m_browserFuncs.memfree = NPN_MemFree;
    m_browserFuncs.memflush = NPN_MemFlush;
    m_browserFuncs.reloadplugins = NPN_ReloadPlugins;
    m_browserFuncs.geturlnotify = NPN_GetURLNotify;
    m_browserFuncs.posturlnotify = NPN_PostURLNotify;
    m_browserFuncs.getvalue = NPN_GetValue;
    m_browserFuncs.setvalue = NPN_SetValue;
    m_browserFuncs.invalidaterect = NPN_InvalidateRect;
    m_browserFuncs.invalidateregion = NPN_InvalidateRegion;
    m_browserFuncs.forceredraw = NPN_ForceRedraw;
    m_browserFuncs.getJavaEnv = NPN_GetJavaEnv;
    m_browserFuncs.getJavaPeer = NPN_GetJavaPeer;
    m_browserFuncs.pushpopupsenabledstate = NPN_PushPopupsEnabledState;
    m_browserFuncs.poppopupsenabledstate = NPN_PopPopupsEnabledState;
    m_browserFuncs.pluginthreadasynccall = NPN_PluginThreadAsyncCall;

    m_browserFuncs.releasevariantvalue = _NPN_ReleaseVariantValue;
    m_browserFuncs.getstringidentifier = _NPN_GetStringIdentifier;
    m_browserFuncs.getstringidentifiers = _NPN_GetStringIdentifiers;
    m_browserFuncs.getintidentifier = _NPN_GetIntIdentifier;
    m_browserFuncs.identifierisstring = _NPN_IdentifierIsString;
    m_browserFuncs.utf8fromidentifier = _NPN_UTF8FromIdentifier;
    m_browserFuncs.intfromidentifier = _NPN_IntFromIdentifier;
    m_browserFuncs.createobject = _NPN_CreateObject;
    m_browserFuncs.retainobject = _NPN_RetainObject;
    m_browserFuncs.releaseobject = _NPN_ReleaseObject;
    m_browserFuncs.invoke = _NPN_Invoke;
    m_browserFuncs.invokeDefault = _NPN_InvokeDefault;
    m_browserFuncs.evaluate = _NPN_Evaluate;
    m_browserFuncs.getproperty = _NPN_GetProperty;
    m_browserFuncs.setproperty = _NPN_SetProperty;
    m_browserFuncs.removeproperty = _NPN_RemoveProperty;
    m_browserFuncs.hasproperty = _NPN_HasProperty;
    m_browserFuncs.hasmethod = _NPN_HasMethod;
    m_browserFuncs.setexception = _NPN_SetException;
    m_browserFuncs.enumerate = _NPN_Enumerate;
}

jobject PluginPackageAndroid::createPluginObject(const char *name,
                                                 const char *path,
                                                 const char *fileName,
                                                 const char *description)
{
    // Create a Java "class Plugin" object instance
    jclass pluginClass = m_env->FindClass("android/webkit/Plugin");
    if(!pluginClass) {
        PLUGIN_LOG("Couldn't find class android.webkit.Plugin\n");
        return 0;
    }
    // Get Plugin(String, String, String, String, Context)
    jmethodID pluginConstructor = m_env->GetMethodID(
            pluginClass,
            "<init>",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if(!pluginConstructor) {
        PLUGIN_LOG("Couldn't get android.webkit.Plugin constructor\n");
        return 0;
    }
    // Make Java strings of name, path, fileName, description
    jstring javaName = m_env->NewStringUTF(name);
    jstring javaPath = m_env->NewStringUTF(m_path.utf8().data());
    jstring javaFileName = m_env->NewStringUTF(m_fileName.utf8().data());
    jstring javaDescription = m_env->NewStringUTF(description);
    // Make a plugin instance
    jobject pluginObject = m_env->NewObject(pluginClass,
                                            pluginConstructor,
                                            javaName,
                                            javaPath,
                                            javaFileName,
                                            javaDescription);
    return pluginObject;
}

jobject PluginPackageAndroid::getPluginListObject()
{
    // Get WebView.getPluginList()
    jclass webViewClass = m_env->FindClass("android/webkit/WebView");
    if(!webViewClass) {
        PLUGIN_LOG("Couldn't find class android.webkit.WebView\n");
        return 0;
    }
    jmethodID getPluginList = m_env->GetStaticMethodID(
            webViewClass,
            "getPluginList",
            "()Landroid/webkit/PluginList;");
    if(!getPluginList) {
        PLUGIN_LOG("Couldn't find android.webkit.WebView.getPluginList()\n");
        return 0;
    }
    // Get the PluginList instance
    jobject pluginListObject = m_env->CallStaticObjectMethod(webViewClass,
                                                             getPluginList);
    if(!pluginListObject) {
        PLUGIN_LOG("Couldn't get PluginList object\n");
        return 0;
    }
    return pluginListObject;
}

bool PluginPackageAndroid::addPluginObjectToList(jobject pluginList,
                                                 jobject plugin)
{
    // Add the Plugin object 
    jclass pluginListClass = m_env->FindClass("android/webkit/PluginList");
    if(!pluginListClass) {
        PLUGIN_LOG("Couldn't find class android.webkit.PluginList\n");
        return false;
    }
    jmethodID addPlugin = m_env->GetMethodID(
            pluginListClass,
            "addPlugin",
            "(Landroid/webkit/Plugin;)V");
    if(!addPlugin) {
        PLUGIN_LOG("Couldn't find android.webkit.PluginList.addPlugin()\n");
        return false;
    }
    m_env->CallVoidMethod(pluginList,
                          addPlugin,
                          plugin);
    return true;
}

void PluginPackageAndroid::removePluginObjectFromList(jobject pluginList,
                                                      jobject plugin)
{
    // Remove the Plugin object
    jclass pluginListClass = m_env->FindClass("android/webkit/PluginList");
    if(!pluginListClass) {
        PLUGIN_LOG("Couldn't find class android.webkit.PluginList\n");
        return;
    }
    jmethodID removePlugin = m_env->GetMethodID(
            pluginListClass,
            "removePlugin",
            "(Landroid/webkit/Plugin;)V");
    if(!removePlugin) {
        PLUGIN_LOG("Couldn't find android.webkit.PluginList.removePlugin()\n");
        return;
    }
    m_env->CallVoidMethod(pluginList,
                          removePlugin,
                          plugin);
}

bool PluginPackageAndroid::load()
{
    if(m_libraryHandle) {
        PLUGIN_LOG("Plugin \"%s\" already loaded\n", m_path.utf8().data());
        return true;
    }

    PLUGIN_LOG("Loading \"%s\"\n", m_path.utf8().data());

    // Open the library
    void *handle = dlopen(m_path.utf8().data(), RTLD_NOW);
    if(!handle) {
        PLUGIN_LOG("Couldn't load plugin library \"%s\": %s\n",
                   m_path.utf8().data(), dlerror());
        return false;
    }
    m_libraryHandle = handle;
    // This object will call dlclose() and set m_libraryHandle to NULL
    // when going out of scope.
    DynamicLibraryCloser dlCloser(this);
    
    // Get the four entry points we need for Linux Netscape Plug-ins
    if(!getEntryPoint("NP_Initialize", (void **) &m_NP_Initialize) ||
       !getEntryPoint("NP_Shutdown", (void **) &m_NP_Shutdown) ||
       !getEntryPoint("NP_GetMIMEDescription",
                      (void **) &m_NP_GetMIMEDescription) ||
       !getEntryPoint("NP_GetValue", (void **) &m_NP_GetValue)) {
        // If any of those failed to resolve, fail the entire load
        return false;
    }
    
    // Get the plugin name and description using NP_GetValue
    const char *name;
    const char *description;
    if(m_NP_GetValue(NULL, NPPVpluginNameString,
                     &name) != NPERR_NO_ERROR ||
       m_NP_GetValue(NULL, NPPVpluginDescriptionString,
                     &description) != NPERR_NO_ERROR) {
        PLUGIN_LOG("Couldn't get name/description using NP_GetValue\n");
        return false;
    }

    PLUGIN_LOG("Plugin name: \"%s\"\n", name);
    PLUGIN_LOG("Plugin description: \"%s\"\n", description);
    m_name = name;
    m_description = description;

    // fileName is just the trailing part of the path
    int last_slash = m_path.reverseFind('/');
    if(last_slash < 0)
        m_fileName = m_path;
    else
        m_fileName = m_path.substring(last_slash + 1);

    // Grab the MIME description. This is in the format, e.g:
    // application/x-somescriptformat:ssf:Some Script Format
    const char *mime_description = m_NP_GetMIMEDescription();
    if(!initializeMIMEDescription(mime_description)) {
        PLUGIN_LOG("Bad MIME description: \"%s\"\n",
                   mime_description);
        return false;
    }

    // Create a new Java Plugin object.
    jobject pluginObject = createPluginObject(name,
                                              m_path.utf8().data(),
                                              m_fileName.utf8().data(),
                                              description);
    if(!pluginObject) {
        PLUGIN_LOG("Couldn't create Java Plugin\n");
        return false;
    }
    
    // Provide the plugin with our browser function table and grab its
    // plugin table. Provide the Java environment and the Plugin which
    // can be used to override the defaults if the plugin wants.
    initializeBrowserFuncs();
    memset(&m_pluginFuncs, 0, sizeof(m_pluginFuncs));
    m_pluginFuncs.size = sizeof(m_pluginFuncs);
    if(m_NP_Initialize(&m_browserFuncs,
                       &m_pluginFuncs,
                       m_env,
                       pluginObject) != NPERR_NO_ERROR) {
        PLUGIN_LOG("Couldn't initialize plugin\n");
        return false;
    }

    // Add the Plugin to the PluginList
    jobject pluginListObject = getPluginListObject();
    if(!pluginListObject) {
        PLUGIN_LOG("Couldn't get PluginList object\n");
        m_NP_Shutdown();
        return false;
    }
    if(!addPluginObjectToList(pluginListObject, pluginObject)) {
        PLUGIN_LOG("Couldn't add Plugin to PluginList\n");
        m_NP_Shutdown();
        return false;
    }

    // Retain the Java Plugin object
    m_pluginObject = m_env->NewGlobalRef(pluginObject);

    // Need to call NP_Shutdown at some point in the future.
    m_libraryInitialized = true;
    
    // Don't close the library - loaded OK.
    dlCloser.ok();
    // Retain the handle so we can close it in the future.
    m_libraryHandle = handle;
    
    PLUGIN_LOG("Loaded OK\n");
    return true;
}

void PluginPackageAndroid::unload()
{
    if(!m_libraryHandle) {
        PLUGIN_LOG("Plugin \"%s\" already unloaded\n", m_path.utf8().data());
        return; // No library loaded
    }
    PLUGIN_LOG("Unloading \"%s\"\n", m_path.utf8().data());

    if(m_libraryInitialized) {
        // Shutdown the plug-in
        ASSERT(m_NP_Shutdown != NULL);
        PLUGIN_LOG("Calling NP_Shutdown\n");
        NPError err = m_NP_Shutdown();
        if(err != NPERR_NO_ERROR)
            PLUGIN_LOG("Couldn't shutdown plug-in \"%s\"\n",
                       m_path.utf8().data());
        m_libraryInitialized = false;

        // Remove the plugin from the PluginList
        if(m_pluginObject) {
            jobject pluginListObject = getPluginListObject();
            if(pluginListObject) {
                removePluginObjectFromList(pluginListObject, m_pluginObject);
            }
            // Remove a reference to the Plugin object so it can
            // garbage collect.
            m_env->DeleteGlobalRef(m_pluginObject);
            m_pluginObject = 0;
        }
    }

    PLUGIN_LOG("Closing library\n");
    dlclose(m_libraryHandle);
    m_libraryHandle = 0;
    memset(&m_browserFuncs, 0, sizeof(m_browserFuncs));
    memset(&m_pluginFuncs, 0, sizeof(m_pluginFuncs));
    m_NP_Initialize = 0;
    m_NP_Shutdown = 0;
    m_NP_GetMIMEDescription = 0;
    m_NP_GetValue = 0;
}

const NPPluginFuncs* PluginPackageAndroid::pluginFuncs() const
{
    ASSERT(m_libraryHandle && m_libraryInitialized);
    return &m_pluginFuncs;
}

PluginPackageAndroid* PluginPackageAndroid::createPackage(const String& path,
                                                          uint32 lastModified)
{
    PluginPackageAndroid* package = new PluginPackageAndroid(path,
                                                             lastModified);

    if (!package->load()) {
        delete package;
        return 0;
    }
    
    return package;
}

bool PluginPackageAndroid::getEntryPoint(const char *name, void **entry_point)
{
    dlerror();
    *entry_point = dlsym(m_libraryHandle, name);
    const char *error = dlerror();
    if(error == NULL && *entry_point != NULL) {
        return true;
    } else {
        PLUGIN_LOG("Couldn't get entry point \"%s\": %s\n",
                   name, error);
        return false;
    }
}

bool PluginPackageAndroid::initializeMIMEEntry(const String& mimeEntry)
{
    // Each part is split into 3 fields separated by colons
    // Field 1 is the MIME type (e.g "application/x-shockwave-flash").
    // Field 2 is a comma separated list of file extensions.
    // Field 3 is a human readable short description.
    Vector<String> fields = mimeEntry.split(':', true);
    if(fields.size() != 3)
        return false;
    
    const String& mimeType = fields[0];
    Vector<String> extensions = fields[1].split(',', true);
    const String& description = fields[2];

    PLUGIN_LOG("mime_type: \"%s\"\n",
               mimeType.utf8().data());
    PLUGIN_LOG("extensions: \"%s\"\n",
               fields[1].utf8().data());
    PLUGIN_LOG("description: \"%s\"\n",
               description.utf8().data());

    // Map the mime type to the vector of extensions and the description
    if(!extensions.isEmpty())
        m_mimeToExtensions.set(mimeType, extensions);
    if(!description.isEmpty())
        m_mimeToDescriptions.set(mimeType, description);

    return true;
}

bool PluginPackageAndroid::initializeMIMEDescription(
        const String& mimeDescription)
{
    PLUGIN_LOG("MIME description: \"%s\"\n", mimeDescription.utf8().data());

    // Clear out the current mappings.
    m_mimeToDescriptions.clear();
    m_mimeToExtensions.clear();
    
    // Split the description into its component entries, separated by
    // semicolons.
    Vector<String> mimeEntries = mimeDescription.split(';', true);
    
    // Iterate through the entries, adding them to the MIME mappings.
    for(Vector<String>::const_iterator it = mimeEntries.begin();
            it != mimeEntries.end(); ++it) {
        if(!initializeMIMEEntry(*it)) {
            PLUGIN_LOG("Bad MIME entry: \"%s\"\n", it->utf8().data());
            m_mimeToDescriptions.clear();
            m_mimeToExtensions.clear();
            return false;
        }
    }

    return true;
}

PluginPackageAndroid::DynamicLibraryCloser::~DynamicLibraryCloser()
{
    // Close the PluginPackageAndroid's library
    if(m_package)
    {
        if(m_package->m_libraryHandle)
        {
            dlclose(m_package->m_libraryHandle);
            m_package->m_libraryHandle = NULL;
        }
        m_package = NULL;
    }
}

} // namespace WebCore

#endif
