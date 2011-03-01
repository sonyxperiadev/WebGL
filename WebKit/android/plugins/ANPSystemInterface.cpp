/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// must include config.h first for webkit to fiddle with new/delete
#include "config.h"

#include "ANPSystem_npapi.h"
#include "Frame.h"
#include "JavaSharedClient.h"
#include "PluginClient.h"
#include "PluginPackage.h"
#include "PluginView.h"
#include "PluginWidgetAndroid.h"
#include "Settings.h"
#include "SkString.h"
#include "WebViewCore.h"
#include <wtf/text/CString.h>

#include <dirent.h>

//#define PLUGIN_DEBUG_LOCAL // controls the printing of log messages
#include "PluginDebugAndroid.h"

static const char* gApplicationDataDir = NULL;
static const char* gApplicationDataDirIncognito = NULL;

using namespace android;

static WebCore::PluginView* pluginViewForInstance(NPP instance) {
    if (instance && instance->ndata)
        return static_cast<WebCore::PluginView*>(instance->ndata);
    return WebCore::PluginView::currentPluginView();
}

static const char* anp_getApplicationDataDirectory() {
    if (NULL == gApplicationDataDir) {
        PluginClient* client = JavaSharedClient::GetPluginClient();
        if (!client)
            return NULL;

        WTF::String path = client->getPluginSharedDataDirectory();
        int length = path.length();
        if (length == 0)
            return NULL;

        char* storage = (char*) malloc(length + 1);
        if (NULL == storage)
            return NULL;

        memcpy(storage, path.utf8().data(), length);
        storage[length] = '\0';

        static const char incognitoPath[] = "/incognito_plugins";
        char* incognitoStorage = (char*) malloc(length + strlen(incognitoPath) + 1);

        strcpy(incognitoStorage, storage);
        strcat(incognitoStorage, incognitoPath);

        // save this assignment for last, so that if multiple threads call us
        // (which should never happen), we never return an incomplete global.
        // At worst, we would allocate storage for the path twice.
        gApplicationDataDir = storage;
        gApplicationDataDirIncognito = incognitoStorage;
    }

    return gApplicationDataDir;
}

static const char* anp_getApplicationDataDirectoryV2(NPP instance) {
    WebCore::PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();

    if (NULL == gApplicationDataDir) {
        anp_getApplicationDataDirectory();
    }

    WebCore::Settings* settings = pluginWidget->webViewCore()->mainFrame()->settings();
    if (settings && settings->privateBrowsingEnabled()) {
        // if this is an incognito view then check the path to see if it exists
        // and if it is a directory, otherwise if it does not exist create it.
        struct stat st;
        if (stat(gApplicationDataDirIncognito, &st) == 0) {
            if (!S_ISDIR(st.st_mode)) {
                return NULL;
            }
        } else {
            if (mkdir(gApplicationDataDirIncognito, S_IRWXU) != 0) {
                return NULL;
            }
        }

        return gApplicationDataDirIncognito;
    }

    return gApplicationDataDir;
}

static jclass anp_loadJavaClass(NPP instance, const char* className) {
    WebCore::PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();

    jclass result;
    result = pluginWidget->webViewCore()->getPluginClass(pluginView->plugin()->path(),
                                                         className);
    return result;
}

static void anp_setPowerState(NPP instance, ANPPowerState powerState) {
    WebCore::PluginView* pluginView = pluginViewForInstance(instance);
    PluginWidgetAndroid* pluginWidget = pluginView->platformPluginWidget();

    pluginWidget->setPowerState(powerState);
}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void ANPSystemInterfaceV0_Init(ANPInterface* v) {
    ANPSystemInterfaceV0* i = reinterpret_cast<ANPSystemInterfaceV0*>(v);

    ASSIGN(i, getApplicationDataDirectory);
    ASSIGN(i, loadJavaClass);
}

void ANPSystemInterfaceV1_Init(ANPInterface* v) {
    // initialize the functions from the previous interface
    ANPSystemInterfaceV0_Init(v);
    // add any new functions or override existing functions
    ANPSystemInterfaceV1* i = reinterpret_cast<ANPSystemInterfaceV1*>(v);
    ASSIGN(i, setPowerState);
}

void ANPSystemInterfaceV2_Init(ANPInterface* v) {
    // initialize the functions from the previous interface
    ANPSystemInterfaceV1_Init(v);
    // add any new functions or override existing functions
    ANPSystemInterfaceV2* i = reinterpret_cast<ANPSystemInterfaceV2*>(v);
    i->getApplicationDataDirectory = anp_getApplicationDataDirectoryV2;
}

///////////////////////////////////////////////////////////////////////////////

static bool isDirectory(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void removeDirectory(const char* path) {
    // create a pointer to a directory
    DIR *dir = NULL;
    dir = opendir(path);
    if (!dir)
        return;

    struct dirent* entry = 0;
    while ((entry = readdir(dir))) { // while there is still something in the directory to list
        if (!entry)
            return;

        if (!strcmp(".", entry->d_name) || !strcmp("..", entry->d_name)) {
            PLUGIN_LOG(". file: %s", entry->d_name);
            continue;
        }

        // concatenate the strings to get the complete path
        static const char separator[] = "/";
        char* file = (char*) malloc(strlen(path) + strlen(separator) + strlen(entry->d_name) + 1);
        strcpy(file, path);
        strcat(file, separator);
        strcat(file, entry->d_name);

        if (isDirectory(file) == true) {
            PLUGIN_LOG("remove dir: %s", file);
            removeDirectory(file);
        } else { // it's a file, we can use remove
            PLUGIN_LOG("remove file: %s", file);
            remove(file);
        }

        free(file);
    }

    // clean up
    closedir (dir); // close the directory
    rmdir(path); // delete the directory
}

void ANPSystemInterface_CleanupIncognito() {
    PLUGIN_LOG("cleanup incognito plugin directory");

    if (gApplicationDataDirIncognito == NULL)
        anp_getApplicationDataDirectory();
    if (gApplicationDataDirIncognito == NULL)
        return;

    // check to see if the directory exists and if so delete it
    if (isDirectory(gApplicationDataDirIncognito))
        removeDirectory(gApplicationDataDirIncognito);
}
