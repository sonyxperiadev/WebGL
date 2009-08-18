/*
 * Copyright 2009, The Android Open Source Project
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

#ifdef ANDROID_PLUGINS

#define LOG_TAG "WebKit"

#include "config.h"
#include "PluginDatabase.h"
#include "PluginPackage.h"

#include "Timer.h"
#include "PlatformString.h"
#include "PluginMainThreadScheduler.h"
#include "CString.h"
#include "jni_utility.h"
#include "npruntime_impl.h"
#include "npfunctions.h"
#include <dlfcn.h>
#include <errno.h>

#include "PluginDebug.h"
#include "PluginDebugAndroid.h"

namespace WebCore {

// Simple class which calls dlclose() on a dynamic library when going
// out of scope. Call ok() if the handle should stay open.
class DynamicLibraryCloser
{
  public:
    DynamicLibraryCloser(PlatformModule *module) : m_module(module) { }
    ~DynamicLibraryCloser()
    {
        // Close the library if non-NULL reference and open.
        if (m_module && *m_module)
        {
            dlclose(*m_module);
            *m_module = 0;
        }
    }
    void ok() { m_module = NULL; }

  private:
    PlatformModule *m_module;
};

// A container for a dummy npp instance. This is used to allow
// NPN_PluginThreadAsyncCall() to be used with NULL passed as the npp
// instance. This is substituted instead, and is shared between all
// plugins which behave in this way. This will be lazily created in
// the first call to NPN_PluginThreadAsyncCall().
class DummyNpp {
  public:
    DummyNpp() {
        m_npp = new NPP_t();
        m_npp->pdata = NULL;
        m_npp->ndata = NULL;
        PluginMainThreadScheduler::scheduler().registerPlugin(m_npp);
    }
    ~DummyNpp() {
        PluginMainThreadScheduler::scheduler().unregisterPlugin(m_npp);
        delete m_npp;
    }
    NPP_t *getInstance() { return m_npp; }
    
  private:
    NPP_t *m_npp;
};

static bool getEntryPoint(PlatformModule module,
                          const char *name,
                          void **entry_point)
{
    dlerror();
    *entry_point = dlsym(module, name);
    const char *error = dlerror();
    if(error == NULL && *entry_point != NULL) {
        return true;
    } else {
        PLUGIN_LOG("Couldn't get entry point \"%s\": %s\n",
                   name, error);
        return false;
    }
}

int PluginPackage::compareFileVersion(
        const PlatformModuleVersion& compareVersion) const
{
    // return -1, 0, or 1 if plug-in version is less than, equal to,
    // or greater than the passed version
    if (m_moduleVersion != compareVersion)
        return m_moduleVersion > compareVersion ? 1 : -1;
    else
        return 0;
}

bool PluginPackage::isPluginBlacklisted()
{
    // No blacklisted Android plugins... yet!
    return false;
}

void PluginPackage::determineQuirks(const String& mimeType)
{
    // The Gears implementation relies on it being loaded *all the time*, 
    // so check to see if this package represents the Gears plugin and
    // load it.
    if (mimeType == "application/x-googlegears") {
        m_quirks.add(PluginQuirkDontUnloadPlugin);
    }
}

static void Android_NPN_PluginThreadAsyncCall(NPP instance,
                                              void (*func) (void *),
                                              void *userData)
{
    // Translate all instance == NULL to a dummy actual npp.
    static DummyNpp dummyNpp;
    if (instance == NULL) {
        instance = dummyNpp.getInstance();
    }
    // Call through to the wrapped function.
    NPN_PluginThreadAsyncCall(instance, func, userData);
}

static void initializeExtraBrowserFuncs(NPNetscapeFuncs *funcs)
{
    funcs->version = NP_VERSION_MINOR;
    funcs->geturl = NPN_GetURL;
    funcs->posturl = NPN_PostURL;
    funcs->requestread = NPN_RequestRead;
    funcs->newstream = NPN_NewStream;
    funcs->write = NPN_Write;
    funcs->destroystream = NPN_DestroyStream;
    funcs->status = NPN_Status;
    funcs->uagent = NPN_UserAgent;
    funcs->memalloc = NPN_MemAlloc;
    funcs->memfree = NPN_MemFree;
    funcs->memflush = NPN_MemFlush;
    funcs->reloadplugins = NPN_ReloadPlugins;
    funcs->geturlnotify = NPN_GetURLNotify;
    funcs->posturlnotify = NPN_PostURLNotify;
    funcs->getvalue = NPN_GetValue;
    funcs->setvalue = NPN_SetValue;
    funcs->invalidaterect = NPN_InvalidateRect;
    funcs->invalidateregion = NPN_InvalidateRegion;
    funcs->forceredraw = NPN_ForceRedraw;
    funcs->getJavaEnv = NPN_GetJavaEnv;
    funcs->getJavaPeer = NPN_GetJavaPeer;
    funcs->pushpopupsenabledstate = NPN_PushPopupsEnabledState;
    funcs->poppopupsenabledstate = NPN_PopPopupsEnabledState;
    funcs->pluginthreadasynccall = Android_NPN_PluginThreadAsyncCall;
    funcs->scheduletimer = NPN_ScheduleTimer;
    funcs->unscheduletimer = NPN_UnscheduleTimer;

    funcs->releasevariantvalue = _NPN_ReleaseVariantValue;
    funcs->getstringidentifier = _NPN_GetStringIdentifier;
    funcs->getstringidentifiers = _NPN_GetStringIdentifiers;
    funcs->getintidentifier = _NPN_GetIntIdentifier;
    funcs->identifierisstring = _NPN_IdentifierIsString;
    funcs->utf8fromidentifier = _NPN_UTF8FromIdentifier;
    funcs->intfromidentifier = _NPN_IntFromIdentifier;
    funcs->createobject = _NPN_CreateObject;
    funcs->retainobject = _NPN_RetainObject;
    funcs->releaseobject = _NPN_ReleaseObject;
    funcs->invoke = _NPN_Invoke;
    funcs->invokeDefault = _NPN_InvokeDefault;
    funcs->evaluate = _NPN_Evaluate;
    funcs->getproperty = _NPN_GetProperty;
    funcs->setproperty = _NPN_SetProperty;
    funcs->removeproperty = _NPN_RemoveProperty;
    funcs->hasproperty = _NPN_HasProperty;
    funcs->hasmethod = _NPN_HasMethod;
    funcs->setexception = _NPN_SetException;
    funcs->enumerate = _NPN_Enumerate;
}

static jobject createPluginObject(const char *name,
                                  const char *path,
                                  const char *fileName,
                                  const char *description)
{
    JNIEnv *env = JSC::Bindings::getJNIEnv();
    // Create a Java "class Plugin" object instance
    jclass pluginClass = env->FindClass("android/webkit/Plugin");
    if(!pluginClass) {
        PLUGIN_LOG("Couldn't find class android.webkit.Plugin\n");
        return 0;
    }
    // Get Plugin(String, String, String, String, Context)
    jmethodID pluginConstructor = env->GetMethodID(
            pluginClass,
            "<init>",
            "(Ljava/lang/String;"
            "Ljava/lang/String;"
            "Ljava/lang/String;"
            "Ljava/lang/String;)V");
    if(!pluginConstructor) {
        PLUGIN_LOG("Couldn't get android.webkit.Plugin constructor\n");
        return 0;
    }
    // Make Java strings of name, path, fileName, description
    jstring javaName = env->NewStringUTF(name);
    jstring javaPath = env->NewStringUTF(path);
    jstring javaFileName = env->NewStringUTF(fileName);
    jstring javaDescription = env->NewStringUTF(description);
    // Make a plugin instance
    jobject pluginObject = env->NewObject(pluginClass,
                                          pluginConstructor,
                                          javaName,
                                          javaPath,
                                          javaFileName,
                                          javaDescription);
    return pluginObject;
}

bool PluginPackage::load()
{
    PLUGIN_LOG("tid:%d isActive:%d isLoaded:%d loadCount:%d\n",
               gettid(),
               m_freeLibraryTimer.isActive(),
               m_isLoaded,
               m_loadCount);
    if (m_freeLibraryTimer.isActive()) {
        ASSERT(m_module);
        m_freeLibraryTimer.stop();
    } else if (m_isLoaded) {
        if (m_quirks.contains(PluginQuirkDontAllowMultipleInstances))
            return false;
        m_loadCount++;
        PLUGIN_LOG("Already loaded, count now %d\n", m_loadCount);
        return true;
    }
    ASSERT(m_loadCount == 0);
    ASSERT(m_module == NULL);

    PLUGIN_LOG("Loading \"%s\"\n", m_path.utf8().data());

    // Open the library
    void *handle = dlopen(m_path.utf8().data(), RTLD_NOW);
    if(!handle) {
        PLUGIN_LOG("Couldn't load plugin library \"%s\": %s\n",
                   m_path.utf8().data(), dlerror());
        return false;
    }
    m_module = handle;
    PLUGIN_LOG("Fetch Info Loaded %p\n", m_module);
    // This object will call dlclose() and set m_module to NULL
    // when going out of scope.
    DynamicLibraryCloser dlCloser(&m_module);
    
    
    NP_InitializeFuncPtr NP_Initialize;
    if(!getEntryPoint(m_module, "NP_Initialize", (void **) &NP_Initialize) || 
            !getEntryPoint(handle, "NP_Shutdown", (void **) &m_NPP_Shutdown)) {
        PLUGIN_LOG("Couldn't find Initialize function\n");
        return false;
    }

    // Provide the plugin with our browser function table and grab its
    // plugin table. Provide the Java environment and the Plugin which
    // can be used to override the defaults if the plugin wants.
    initializeBrowserFuncs();
    // call this afterwards, which may re-initialize some methods, but ensures
    // that any additional (or changed) procs are set. There is no real attempt
    // to have this step be minimal (i.e. only what we add/override), since the
    // core version (initializeBrowserFuncs) can change in the future.
    initializeExtraBrowserFuncs(&m_browserFuncs);

    memset(&m_pluginFuncs, 0, sizeof(m_pluginFuncs));
    m_pluginFuncs.size = sizeof(m_pluginFuncs);
    if(NP_Initialize(&m_browserFuncs,
                     &m_pluginFuncs,
                     JSC::Bindings::getJNIEnv(),
                     m_pluginObject) != NPERR_NO_ERROR) {
        PLUGIN_LOG("Couldn't initialize plugin\n");
        return false;
    }

    // Don't close the library - loaded OK.
    dlCloser.ok();
    // Retain the handle so we can close it in the future.
    m_module = handle;
    m_isLoaded = true;
    ++m_loadCount;
    PLUGIN_LOG("Initial load ok, count now %d\n", m_loadCount);
    return true;
}

bool PluginPackage::fetchInfo()
{
    PLUGIN_LOG("Fetch Info Loading \"%s\"\n", m_path.utf8().data());

    // Open the library
    void *handle = dlopen(m_path.utf8().data(), RTLD_NOW);
    if(!handle) {
        PLUGIN_LOG("Couldn't load plugin library \"%s\": %s\n",
                   m_path.utf8().data(), dlerror());
        return false;
    }
    PLUGIN_LOG("Fetch Info Loaded %p\n", handle);
    
    // This object will call dlclose() and set m_module to NULL
    // when going out of scope.
    DynamicLibraryCloser dlCloser(&handle);
    
    // Get the three entry points we need for Linux Netscape Plug-ins
    NP_GetMIMEDescriptionFuncPtr NP_GetMIMEDescription;
    NPP_GetValueProcPtr NP_GetValue;
    if(!getEntryPoint(handle, "NP_GetMIMEDescription",
            (void **) &NP_GetMIMEDescription) ||
            !getEntryPoint(handle, "NP_GetValue", (void **) &NP_GetValue)) {
        // If any of those failed to resolve, fail the entire load
        return false;
    }
     
    // Get the plugin name and description using NP_GetValue
    const char *name;
    const char *description;
    if(NP_GetValue(NULL, NPPVpluginNameString, &name) != NPERR_NO_ERROR ||
            NP_GetValue(NULL, NPPVpluginDescriptionString, &description) !=
                NPERR_NO_ERROR) {
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
    String mimeDescription(NP_GetMIMEDescription());
    PLUGIN_LOG("MIME description: \"%s\"\n", mimeDescription.utf8().data());
    // Clear out the current mappings.
    m_mimeToDescriptions.clear();
    m_mimeToExtensions.clear();    
    // Split the description into its component entries, separated by
    // semicolons.
    Vector<String> mimeEntries;
    mimeDescription.split(';', true, mimeEntries);
    // Iterate through the entries, adding them to the MIME mappings.
    for(Vector<String>::const_iterator it = mimeEntries.begin();
            it != mimeEntries.end(); ++it) {
        // Each part is split into 3 fields separated by colons
        // Field 1 is the MIME type (e.g "application/x-shockwave-flash").
        // Field 2 is a comma separated list of file extensions.
        // Field 3 is a human readable short description.
        const String &mimeEntry = *it;
        Vector<String> fields;
        mimeEntry.split(':', true, fields);
        if(fields.size() != 3) {
            PLUGIN_LOG("Bad MIME entry \"%s\"\n", mimeEntry.utf8().data());
            return false;
        }

        const String& mimeType = fields[0];
        Vector<String> extensions;
        fields[1].split(',', true, extensions);
        const String& description = fields[2];

        determineQuirks(mimeType);

        PLUGIN_LOG("mime_type: \"%s\"\n", mimeType.utf8().data());
        PLUGIN_LOG("extensions: \"%s\"\n", fields[1].utf8().data());
        PLUGIN_LOG("description: \"%s\"\n", description.utf8().data());
         
        // Map the mime type to the vector of extensions and the description
        if(!extensions.isEmpty())
            m_mimeToExtensions.set(mimeType, extensions);
        if(!description.isEmpty())
            m_mimeToDescriptions.set(mimeType, description);
    }

    // Create a new Java Plugin object, this object is an instance of
    // android.os.WebView.Plugin
    CString path = m_path.utf8();
    CString filename = m_fileName.utf8();
    jobject pluginObject = createPluginObject(name,
                                              path.data(),
                                              filename.data(),
                                              description);
    if(!pluginObject) {
        PLUGIN_LOG("Couldn't create Java Plugin\n");
        return false;
    }

    // Retain the Java Plugin object
    m_pluginObject = JSC::Bindings::getJNIEnv()->NewGlobalRef(pluginObject);

    PLUGIN_LOG("Fetch Info Loaded plugin details ok \"%s\"\n",
            m_path.utf8().data());

    // If this plugin needs to be kept in memory, unload the module now
    // and load it permanently.
    if (m_quirks.contains(PluginQuirkDontUnloadPlugin)) {
        dlCloser.ok();
        dlclose(handle);
        load();
    }
    
    // dlCloser will unload the plugin if required.
    return true;
}

unsigned PluginPackage::hash() const
{ 
    const unsigned hashCodes[] = {
        m_name.impl()->hash(),
        m_description.impl()->hash(),
        m_mimeToExtensions.size(),
    };

    return StringImpl::computeHash(reinterpret_cast<const UChar*>(hashCodes),
                                   sizeof(hashCodes) / sizeof(UChar));
}

bool PluginPackage::equal(const PluginPackage& a, const PluginPackage& b)
{
    if (a.m_name != b.m_name)
        return false;

    if (a.m_description != b.m_description)
        return false;

    if (a.m_mimeToExtensions.size() != b.m_mimeToExtensions.size())
        return false;

    MIMEToExtensionsMap::const_iterator::Keys end =
            a.m_mimeToExtensions.end().keys();
    for (MIMEToExtensionsMap::const_iterator::Keys it =
                 a.m_mimeToExtensions.begin().keys();
         it != end;
         ++it) {
        if (!b.m_mimeToExtensions.contains(*it)) {
            return false;
        }
    }

    return true;
}

} // namespace WebCore

#endif
