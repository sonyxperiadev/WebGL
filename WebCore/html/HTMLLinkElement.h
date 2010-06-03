/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLLinkElement_h
#define HTMLLinkElement_h

#include "CSSStyleSheet.h"
#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "HTMLElement.h"
#include "Timer.h"

namespace WebCore {

class CachedCSSStyleSheet;
class CachedLinkPrefetch;
class KURL;

class HTMLLinkElement : public HTMLElement, public CachedResourceClient {
public:
    struct RelAttribute {
        bool m_isStyleSheet;
        bool m_isIcon;
        bool m_isAlternate;
        bool m_isDNSPrefetch;
#ifdef ANDROID_APPLE_TOUCH_ICON
        bool m_isTouchIcon;
        bool m_isPrecomposedTouchIcon;
#endif
#if ENABLE(LINK_PREFETCH)
        bool m_isLinkPrefetch;
#endif

        RelAttribute() : m_isStyleSheet(false), m_isIcon(false), m_isAlternate(false), m_isDNSPrefetch(false)
#ifdef ANDROID_APPLE_TOUCH_ICON
                , m_isTouchIcon(false), m_isPrecomposedTouchIcon(false)
#endif
#if ENABLE(LINK_PREFETCH)
            , m_isLinkPrefetch(false)
#endif
            { };
    };

    static PassRefPtr<HTMLLinkElement> create(const QualifiedName&, Document*, bool createdByParser);
    virtual ~HTMLLinkElement();

    bool disabled() const;
    void setDisabled(bool);

    String charset() const;
    void setCharset(const String&);

    KURL href() const;
    void setHref(const String&);

    String hreflang() const;
    void setHreflang(const String&);

    String media() const;
    void setMedia(const String&);

    String rel() const;
    void setRel(const String&);

    String rev() const;
    void setRev(const String&);

    virtual String target() const;
    void setTarget(const String&);

    String type() const;
    void setType(const String&);

    StyleSheet* sheet() const;

    bool isLoading() const;

    bool isDisabled() const { return m_disabledState == Disabled; }
    bool isEnabledViaScript() const { return m_disabledState == EnabledViaScript; }
    bool isIcon() const { return m_relAttribute.m_isIcon; }

private:
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual void parseMappedAttribute(Attribute*);

    void process();

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    // from CachedResourceClient
    virtual void setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CachedCSSStyleSheet* sheet);
<<<<<<< HEAD
    virtual void notifyFinished(CachedResource*);

    bool isLoading() const;
=======
>>>>>>> webkit.org at r60469
    virtual bool sheetLoaded();

    bool isAlternate() const { return m_disabledState == Unset && m_relAttribute.m_isAlternate; }
    
    void setDisabledState(bool _disabled);

    virtual bool isURLAttribute(Attribute*) const;
    
public:
    static void tokenizeRelAttribute(const AtomicString& value, RelAttribute&);

private:
    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;

    virtual void finishParsingChildren();

<<<<<<< HEAD
#ifdef ANDROID_INSTRUMENT
    // Overridden to resolve the ambiguous
    void* operator new(size_t size);
    void* operator new[](size_t size);
    void operator delete(void* p, size_t size);
    void operator delete[](void* p, size_t size);
#endif

protected:
    void timerFired(Timer<HTMLLinkElement>*);
=======
private:
    HTMLLinkElement(const QualifiedName&, Document*, bool createdByParser);
>>>>>>> webkit.org at r60469

    enum DisabledState {
        Unset,
        EnabledViaScript,
        Disabled
    };

    CachedResourceHandle<CachedCSSStyleSheet> m_cachedSheet;
    RefPtr<CSSStyleSheet> m_sheet;
#if ENABLE(LINK_PREFETCH)
    CachedResourceHandle<CachedLinkPrefetch> m_cachedLinkPrefetch;
#endif
    KURL m_url;
    String m_type;
    String m_media;
    DisabledState m_disabledState;
    RelAttribute m_relAttribute;
    bool m_loading;
    bool m_createdByParser;
    Timer<HTMLLinkElement> m_timer;
};

} //namespace

#endif
