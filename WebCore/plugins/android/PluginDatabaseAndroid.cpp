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
#include "PluginDatabaseAndroid.h"
#include "PluginPackageAndroid.h"
#include "PluginViewAndroid.h"
#include "Frame.h"
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>

#include "PluginDebug.h"

namespace WebCore {

String PluginDatabaseAndroid::s_defaultPluginsPath;

PluginDatabaseAndroid* PluginDatabaseAndroid::installedPlugins()
{
    static PluginDatabaseAndroid* plugins = 0;

    if (!plugins) {
        plugins = new PluginDatabaseAndroid;
        plugins->refresh();
    }

    return plugins;
}

void PluginDatabaseAndroid::setDefaultPluginsPath(const String& path)
{
    // Change the default plugins path and rescan.
    if(path != s_defaultPluginsPath) {
        s_defaultPluginsPath = path;
        installedPlugins()->refresh();
    }
}

PluginDatabaseAndroid::PluginDatabaseAndroid()
        : m_pluginsPaths()
        , m_plugins()
{
}

PluginDatabaseAndroid::~PluginDatabaseAndroid()
{
}

void PluginDatabaseAndroid::addExtraPluginPath(const String& path)
{
    m_pluginsPaths.append(path);
    refresh();
}

bool PluginDatabaseAndroid::addPluginFile(const String& path)
{
    PLUGIN_LOG("Adding plugin \"%s\"\n", path.utf8().data());

    // Check if the plugin is already loaded.
    PluginPackageAndroid* plugin = findPluginByPath(path);

    // Stat the file to check we can access it.
    struct stat statbuf;
    if(stat(path.utf8().data(), &statbuf)) {
        // Stat failed. We can't access this file.
        PLUGIN_LOG("Couldn't stat library \"%s\": %s\n",
                   path.utf8().data(), strerror(errno));
        // Well, can't load this plugin, then.
        if(plugin) {
            // If we previous succeeded loading it, unload it now.
            removePlugin(plugin);
        }
        // Failed to load.
        return false;
    }

    // Check the modification time.
    uint32 lastModified = statbuf.st_mtime;
    if(plugin) {
        // Compare to the currently loaded version.
        if(lastModified == plugin->getLastModified()) {
            // It's the same plugin. No new actions required.
            return true;
        } else {
            // It has been modified. Unload the old version.
            removePlugin(plugin);
        }
    }

    // If we get here, we either just unloaded an old plugin, or this
    // is a new one. Create a new package.
    PluginPackageAndroid* package =
            PluginPackageAndroid::createPackage(path, lastModified);
    if(package) {
        // Success, add to the vector of plugins.
        PLUGIN_LOG("Successfully added plugin \"%s\"\n", path.utf8().data());
        m_plugins.append(package);
        return true;
    } else {
        // Failed.
        return false;
    }
}

bool PluginDatabaseAndroid::addPluginsInDirectory(const String& path)
{
    // Open a directory iterator
    DIR* dir_it = opendir(path.utf8().data());
    if(!dir_it) {
        PLUGIN_LOG("Cannot open directory \"%s\"\n",
            path.utf8().data());
        return false;
    }
    // Scan the directory for "*.so" and "*.so.*" files.
    struct dirent* entry;
    while((entry = readdir(dir_it)) != NULL) {
        const char* name = entry->d_name;
        // Skip current and parent directory entries.
        if(!strcmp(name, ".") || !strcmp(name, ".."))
            continue;
        const char* so = strstr(name, ".so");
        if(so && (so[3] == 0 || so[3] == '.')) {
            // Matches our pattern, add it.
            addPluginFile(path + "/" + name);
        }
    }
    closedir(dir_it);
    return true;
}

void PluginDatabaseAndroid::removePlugin(PluginPackageAndroid* plugin)
{
    // Find this plugin in the array and remove it.
    for(unsigned i = 0; i < m_plugins.size(); ++i) {
        if(m_plugins[i] == plugin) {
            PLUGIN_LOG("Removed plugin \"%s\"\n",
                       plugin->path().utf8().data());
            // This will cause a reference count to decrement. When
            // the plugin actually stops getting used by all
            // documents, its instance will delete itself.
            m_plugins.remove(i);
            break;
        }
    }
    PLUGIN_LOG("Tried to remove plugin %p which didn't exist!\n", plugin);
}

bool PluginDatabaseAndroid::refresh()
{
    PLUGIN_LOG("PluginDatabaseAndroid::refresh()\n");

    // Don't delete existing plugins. The directory scan will add new
    // plugins and also refresh old plugins if their file is modified
    // since the last check.

    // Scan each directory and add any plugins found in them. This is
    // not recursive.
    if(s_defaultPluginsPath.length() > 0)
        addPluginsInDirectory(s_defaultPluginsPath);
    for(unsigned i = 0; i < m_pluginsPaths.size(); ++i)
        addPluginsInDirectory(m_pluginsPaths[i]);

    // Now stat() all plugins and remove any that we can't
    // access. This handles the case of a plugin being deleted at
    // runtime.
    for(unsigned i = 0; i < m_plugins.size();) {
        struct stat statbuf;
        if(stat(m_plugins[i]->path().utf8().data(), &statbuf)) {
            // It's gone away. Remove it from the list.
            m_plugins.remove(i);
        } else {
            ++i;
        }
    }

    return true;
}

Vector<PluginPackageAndroid*> PluginDatabaseAndroid::plugins() const
{
    Vector<PluginPackageAndroid*> result;

    for(PackageRefVector::const_iterator it = m_plugins.begin();
        it != m_plugins.end(); ++it)
        result.append(it->get());

    return result;
}

bool PluginDatabaseAndroid::isMIMETypeRegistered(const String& mimeType) const
{
    // Iterate through every package
    for(PackageRefVector::const_iterator it = m_plugins.begin();
        it != m_plugins.end(); ++it) {
        // Check if this package has the MIME type mapped to an extension
        const PluginPackageAndroid *package = it->get();
        const MIMEToExtensionsMap& mime_extension_map =
                package->mimeToExtensions();
        if(mime_extension_map.find(mimeType) != mime_extension_map.end()) {
            // Found it
            return true;
        }
    }
    // No package has this MIME type
    return false;
}

bool PluginDatabaseAndroid::isPluginBlacklisted(PluginPackageAndroid* plugin)
{
    // If there is ever a plugin in the wild which causes the latest
    // version of Android to crash, then stick a check here for it and
    // return true when matched.
    return false;
}

PluginPackageAndroid* PluginDatabaseAndroid::pluginForMIMEType(
        const String& mimeType)
{
    // Todo: these data structures are not right. This is inefficient.
    // Iterate through every package
    for(PackageRefVector::iterator it = m_plugins.begin();
        it != m_plugins.end(); ++it) {
        // Check if this package has the MIME type mapped to a description
        PluginPackageAndroid *package = it->get();
        const MIMEToDescriptionsMap& mime_desc_map =
                package->mimeToDescriptions();
        if(mime_desc_map.find(mimeType) != mime_desc_map.end()) {
            // Found it
            return package;
        }
    }
    // No package has this MIME type
    return NULL;
}

String PluginDatabaseAndroid::MIMETypeForExtension(const String& extension)
        const
{
    // Todo: these data structures are not right. This is inefficient.
    // Iterate through every package
    for(PackageRefVector::const_iterator it = m_plugins.begin();
        it != m_plugins.end(); ++it) {
        // Check if this package has the MIME type mapped to an extension
        PluginPackageAndroid *package = it->get();
        const MIMEToDescriptionsMap& mime_desc_map =
                package->mimeToDescriptions();
        for(MIMEToDescriptionsMap::const_iterator map_it =
                    mime_desc_map.begin();
            map_it != mime_desc_map.end(); ++map_it) {
            if(map_it->second == extension) {
                // Found it
                return map_it->first;
            }
        }
    }
    // No MIME type matches this extension
    return String("");
}

PluginPackageAndroid* PluginDatabaseAndroid::findPlugin(const KURL& url,
                                                        String& mimeType)
{
    PluginPackageAndroid* plugin = pluginForMIMEType(mimeType);
    String filename = url.string();

    if (!plugin) {
        String filename = url.lastPathComponent();
        if (!filename.endsWith("/")) {
            int extensionPos = filename.reverseFind('.');
            if (extensionPos != -1) {
                String extension = filename.substring(extensionPos + 1);

                mimeType = MIMETypeForExtension(extension);
                if (mimeType.length()) {
                    plugin = pluginForMIMEType(mimeType);
                }
            }
        }
    }
    // No MIME type matches this url
    return plugin;
}

PluginPackageAndroid* PluginDatabaseAndroid::findPluginByPath(
        const String& path)
{
    for(PackageRefVector::iterator it = m_plugins.begin();
            it != m_plugins.end(); ++it) {
        if((*it)->path() == path)
            return it->get(); // Found it
    }
    return 0;   // Not found.
}

PluginViewAndroid* PluginDatabaseAndroid::createPluginView(
        Frame* parentFrame,
        const IntSize& size,
        Element* element,
        const KURL& url,
        const Vector<String>& paramNames,
        const Vector<String>& paramValues,
        const String& mimeType,
        bool loadManually)
{
    // if we fail to find a plugin for this MIME type, findPlugin will search for
    // a plugin by the file extension and update the MIME type, so pass a mutable String
    String mimeTypeCopy = mimeType;
    PluginPackageAndroid* plugin = findPlugin(url, mimeTypeCopy);

    // No plugin was found, try refreshing the database and searching again
    if (!plugin && refresh()) {
        mimeTypeCopy = mimeType;
        plugin = findPlugin(url, mimeTypeCopy);
    }

    return new PluginViewAndroid(parentFrame, size, plugin, element, url, paramNames, paramValues, mimeTypeCopy, loadManually);
}

}

#endif
