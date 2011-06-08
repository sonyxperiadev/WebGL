/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * All rights reserved.
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
#include "FileSystem.h"

#include "PlatformBridge.h"
#include "cutils/log.h"
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

// Global static used to store the base to the plugin path.
// This is set in WebSettings.cpp
String sPluginPath;

CString fileSystemRepresentation(const String& path)
{
    // If the path is a content:// URI then ask Java to resolve it for us.
    if (path.startsWith("content://"))
        return PlatformBridge::resolveFilePathForContentUri(path).utf8();
    else if (path.startsWith("file://"))
        return path.substring(strlen("file://")).utf8();

    return path.utf8();
}

String openTemporaryFile(const String& prefix, PlatformFileHandle& handle)
{
    int number = rand() % 10000 + 1;
    String filename;
    do {
        StringBuilder builder;
        builder.append(sPluginPath);
        builder.append('/');
        builder.append(prefix);
        builder.append(String::number(number));
        filename = builder.toString();
        handle = open(filename.utf8().data(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        number++;
        
        if (handle != -1)
            return filename;
    } while (errno == EEXIST);
    
    return String();
}

bool unloadModule(PlatformModule module)
{
    return !dlclose(module);
}

String homeDirectoryPath() 
{
    return sPluginPath;
}

// We define our own pathGetFileName rather than use the POSIX versions as we
// may get passed a content URI representing the path to the file. We pass
// the input through fileSystemRepresentation before using it to resolve it if
// it is a content URI.
String pathGetFileName(const String& path)
{
    CString fsRep = fileSystemRepresentation(path);
    String fsPath = String(fsRep.data());
    return fsPath.substring(fsPath.reverseFind('/') + 1);
}


} // namespace WebCore
