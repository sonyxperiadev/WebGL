/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef DocLoader_h
#define DocLoader_h

#include "CachedResource.h"
#include "CachePolicy.h"
#include "StringHash.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#ifdef ANDROID_PRELOAD_CHANGES
#include <wtf/ListHashSet.h>
#endif

namespace WebCore {

class CachedCSSStyleSheet;
class CachedFont;
class CachedImage;
class CachedScript;
class CachedXSLStyleSheet;
class Document;
class Frame;
class HTMLImageLoader;
class KURL;

// The DocLoader manages the loading of scripts/images/stylesheets for a single document.
class DocLoader
{
friend class Cache;
friend class HTMLImageLoader;

public:
    DocLoader(Frame*, Document*);
    ~DocLoader();

    CachedImage* requestImage(const String& url);
    CachedCSSStyleSheet* requestCSSStyleSheet(const String& url, const String& charset, bool isUserStyleSheet = false);
    CachedCSSStyleSheet* requestUserCSSStyleSheet(const String& url, const String& charset);
    CachedScript* requestScript(const String& url, const String& charset);
    CachedFont* requestFont(const String& url);

#if ENABLE(XSLT)
    CachedXSLStyleSheet* requestXSLStyleSheet(const String& url);
#endif
#if ENABLE(XBL)
    CachedXBLDocument* requestXBLDocument(const String &url);
#endif

    CachedResource* cachedResource(const String& url) const { return m_docResources.get(url); }
    const HashMap<String, CachedResource*>& allCachedResources() const { return m_docResources; }

    bool autoLoadImages() const { return m_autoLoadImages; }
    void setAutoLoadImages(bool);
    
#ifdef ANDROID_BLOCK_NETWORK_IMAGE
    bool blockNetworkImage() const { return m_blockNetworkImage; }
    void setBlockNetworkImage(bool);
    bool shouldBlockNetworkImage(const String& url) const;
#endif

    CachePolicy cachePolicy() const { return m_cachePolicy; }
    void setCachePolicy(CachePolicy);
    
    Frame* frame() const { return m_frame; }
    Document* doc() const { return m_doc; }

    void removeCachedResource(CachedResource*) const;

    void setLoadInProgress(bool);
    bool loadInProgress() const { return m_loadInProgress; }
    
    void setAllowStaleResources(bool allowStaleResources) { m_allowStaleResources = allowStaleResources; }

#if USE(LOW_BANDWIDTH_DISPLAY)
    void replaceDocument(Document* doc) { m_doc = doc; }
#endif

    void incrementRequestCount();
    void decrementRequestCount();
    int requestCount();

#ifdef ANDROID_PRELOAD_CHANGES
    void clearPreloads();
    void preload(CachedResource::Type type, const String& url, const String& charset, bool inBody);
    void checkForPendingPreloads();
    void printPreloadStats();
#endif
private:
#ifdef ANDROID_PRELOAD_CHANGES
    CachedResource* requestResource(CachedResource::Type, const String& url, const String* charset = 0, bool skipCanLoadCheck = false, bool sendResourceLoadCallbacks = true, bool isPreload = false);
    void requestPreload(CachedResource::Type type, const String& url, const String& charset);
#else
    CachedResource* requestResource(CachedResource::Type, const String& url, const String* charset = 0, bool skipCanLoadCheck = false, bool sendResourceLoadCallbacks = true);
#endif
    void checkForReload(const KURL&);
    void checkCacheObjectStatus(CachedResource*);
    
    Cache* m_cache;
    HashSet<String> m_reloadedURLs;
    mutable HashMap<String, CachedResource*> m_docResources;
    CachePolicy m_cachePolicy;
    Frame* m_frame;
    Document *m_doc;
    
    int m_requestCount;

#ifdef ANDROID_PRELOAD_CHANGES
    ListHashSet<CachedResource*> m_preloads;
    struct PendingPreload {
        CachedResource::Type m_type;
        String m_url;
        String m_charset;
    };
    Vector<PendingPreload> m_pendingPreloads;
#endif
    
    //29 bits left
#ifdef ANDROID_BLOCK_NETWORK_IMAGE
    bool m_blockNetworkImage : 1;
#endif
    bool m_autoLoadImages : 1;
    bool m_loadInProgress : 1;
    bool m_allowStaleResources : 1;
};

}

#endif
