/*
 * Copyright (C) 2004, 2005, 2006 Apple Inc.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H
#ifdef BUILDING_WITH_CMAKE
#include "cmakeconfig.h"
#else
#include "autotoolsconfig.h"
#endif
#endif

#include <wtf/Platform.h>

#if !PLATFORM(CHROMIUM) && OS(WINDOWS) && !defined(BUILDING_WX__) && !COMPILER(GCC)
#if defined(BUILDING_JavaScriptCore) || defined(BUILDING_WTF)
#define JS_EXPORTDATA __declspec(dllexport)
#else
#define JS_EXPORTDATA __declspec(dllimport)
#endif
#if defined(BUILDING_WebCore) || defined(BUILDING_WebKit)
#define WEBKIT_EXPORTDATA __declspec(dllexport)
#else
#define WEBKIT_EXPORTDATA __declspec(dllimport)
#endif
#define JS_EXPORTCLASS JS_EXPORTDATA
#else
#define JS_EXPORTDATA
#define JS_EXPORTCLASS
#define WEBKIT_EXPORTDATA
#endif

#ifdef __APPLE__
#define HAVE_FUNC_USLEEP 1
#endif /* __APPLE__ */

#if OS(WINDOWS)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

#ifndef WINVER
#define WINVER 0x0500
#endif

// If we don't define these, they get defined in windef.h.
// We want to use std::min and std::max.
#ifndef max
#define max max
#endif
#ifndef min
#define min min
#endif

// CURL needs winsock, so don't prevent inclusion of it
#if !USE(CURL)
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_ // Prevent inclusion of winsock.h in windows.h
#endif
#endif

#endif /* OS(WINDOWS) */

// ANDROID def should be after all PLATFORM to avoid override.
#if PLATFORM(ANDROID)
// Android uses a single set of include directories when building WebKit and
// JavaScriptCore. Since WebCore/ is included before JavaScriptCore/, Android
// includes JavaScriptCore/config.h explicitly here to make sure it gets picked
// up.
#include <JavaScriptCore/config.h>

#define WEBCORE_NAVIGATOR_VENDOR "Google Inc."
// This must be defined before we include FastMalloc.h, below.
#define USE_SYSTEM_MALLOC 1
#define LOG_DISABLED 1
#include <wtf/Assertions.h>
// Central place to set which optional features Android uses.
#define ENABLE_DOM_STORAGE 1
#undef ENABLE_FTPDIR  // Enabled by default in Platform.h
#define ENABLE_FTPDIR 0
#ifndef ENABLE_SVG
#define ENABLE_SVG 0
#endif
#define ENABLE_VIDEO 1

#if ENABLE_SVG
#if !defined(ENABLE_SVG_ANIMATION)
#define ENABLE_SVG_ANIMATION 0 // to enable:
    // fix error: no matching function for call to 'sort(WebCore::SVGSMILElement**, WebCore::SVGSMILElement**, WebCore::PriorityCompare)'
    // fix error: no matching function for call to 'sort(WebCore::SMILTime*, WebCore::SMILTime*)'
    // add ENABLE_SVG_ANIMATION=1 to SVG_FLAGS in JavaScriptCore.derived.mk
#endif
#define ENABLE_SVG_AS_IMAGE 1
#define ENABLE_SVG_FILTERS 1
#define ENABLE_SVG_FONTS 1
#define ENABLE_SVG_FOREIGN_OBJECT 1
#define ENABLE_SVG_USE 1
#endif

#define ENABLE_XBL 0
#define ENABLE_XHTMLMP 0
#define ENABLE_XPATH 1
#define ENABLE_XSLT 1
#undef ENABLE_ARCHIVE // Enabled by default in Platform.h
#define ENABLE_ARCHIVE 1
#define ENABLE_OFFLINE_WEB_APPLICATIONS 1
#define ENABLE_TOUCH_EVENTS 1
#undef ENABLE_GEOLOCATION  // Disabled by default in Platform.h
#define ENABLE_GEOLOCATION 1
#undef ENABLE_INSPECTOR  // Enabled by default in Platform.h
#define ENABLE_INSPECTOR 0
#define ENABLE_EVENT_SOURCE 0
#define ENABLE_DEVICE_ORIENTATION 1

#undef ENABLE_APPLICATION_INSTALLED
#define ENABLE_APPLICATION_INSTALLED 1

#define ENABLE_ANDROID_INSTALLABLE_WEB_APPS 1

// Uses composited RenderLayers for fixed elements
#undef ENABLE_COMPOSITED_FIXED_ELEMENTS // Disabled by default in Platform.h
#define ENABLE_COMPOSITED_FIXED_ELEMENTS 1

#define ENABLE_FILE_READER 1
#define ENABLE_BLOB 1

#define ANDROID_FLATTEN_FRAMESET

#define ANDROID_LAYOUT

#define ANDROID_FIX

#define ANDROID_DISABLE_ROUNDING_HACKS

// Ensure that the fixed elements are always relative to the top document.
#define ANDROID_FIXED_ELEMENTS

// Passes the webkit-originated changes of a focused textfield to our UI
// thread
#define ANDROID_ACCEPT_CHANGES_TO_FOCUSED_TEXTFIELDS

// Allow us to turn off the blinking caret as desired.
#define ANDROID_ALLOW_TURNING_OFF_CARET

#define ANDROID_META_SUPPORT

// Converts ListBoxes to dropdown popup lists.
#define ENABLE_NO_LISTBOX_RENDERING 1

#define ANDROID_MULTIPLE_WINDOWS
#define ANDROID_CSS_RING
#define ANDROID_CSS_TAP_HIGHLIGHT_COLOR

#define ANDROID_BLOCK_NETWORK_IMAGE

// Changes needed to support native plugins (npapi.h). If the change is generic,
// it may be under a different #define (see: PLUGIN_PLATFORM_SETVALUE,
// PLUGIN_SCHEDULE_TIMER)
#define ANDROID_PLUGINS

// This enables a portable implementation of NPN_[Un]ScheduleTimer
// Will submit this as a patch to apple
#define PLUGIN_SCHEDULE_TIMER

// This adds platformInit() and platformSetValue() to pluginview
// Will submit this as a patch to apple
#define PLUGIN_PLATFORM_SETVALUE

// This enables logging the DOM tree, Render tree even for the release build
#define ANDROID_DOM_LOGGING

// Notify WebViewCore when a clipped out rectangle is drawn,
// so that all invals are captured by the display tree.
#define ANDROID_CAPTURE_OFFSCREEN_PAINTS

// Enable dumping the display tree to a file (triggered in WebView.java)
#define ANDROID_DUMP_DISPLAY_TREE

// Animated GIF support.
#define ANDROID_ANIMATED_GIF

// apple-touch-icon support in <link> tags
#define ANDROID_APPLE_TOUCH_ICON

// track changes to the style that may change what is drawn
#define ANDROID_STYLE_VERSION

// Enable prefetching when specified via the rel element of <link> elements.
#define ENABLE_LINK_PREFETCH 1

// Enable scrollable divs in separate layers.  This might be upstreamed to
// webkit.org but for now, it is just an Android feature.
#define ENABLE_ANDROID_OVERFLOW_SCROLL 1

#if !defined(WTF_USE_CHROME_NETWORK_STACK)
#define WTF_USE_CHROME_NETWORK_STACK 0
#endif /* !defined(WTF_USE_CHROME_NETWORK_STACK) */

#endif /* PLATFORM(ANDROID) */

#ifdef __cplusplus

// These undefs match up with defines in WebCorePrefix.h for Mac OS X.
// Helps us catch if anyone uses new or delete by accident in code and doesn't include "config.h".
#undef new
#undef delete
#include <wtf/FastMalloc.h>

#endif

// On MSW, wx headers need to be included before windows.h is.
// The only way we can always ensure this is if we include wx here.
#if PLATFORM(WX)
#include <wx/defs.h>
#endif

// this breaks compilation of <QFontDatabase>, at least, so turn it off for now
// Also generates errors on wx on Windows, presumably because these functions
// are used from wx headers.
#if !PLATFORM(QT) && !PLATFORM(WX) && !PLATFORM(CHROMIUM)
#include <wtf/DisallowCType.h>
#endif

#if COMPILER(MSVC)
#define SKIP_STATIC_CONSTRUCTORS_ON_MSVC 1
#elif !COMPILER(WINSCW)
#define SKIP_STATIC_CONSTRUCTORS_ON_GCC 1
#endif

#if PLATFORM(WIN)
#if defined(WIN_CAIRO)
#undef WTF_PLATFORM_CG
#define WTF_PLATFORM_CAIRO 1
#undef WTF_USE_CFNETWORK
#define WTF_USE_CURL 1
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_ // Prevent inclusion of winsock.h in windows.h
#endif
#elif !OS(WINCE)
#define WTF_PLATFORM_CG 1
#undef WTF_PLATFORM_CAIRO
#define WTF_USE_CFNETWORK 1
#undef WTF_USE_CURL
#endif
#endif

#if PLATFORM(MAC)
// New theme
#define WTF_USE_NEW_THEME 1
#endif // PLATFORM(MAC)

#if OS(SYMBIAN)
#define USE_SYSTEM_MALLOC 1
#endif

#if PLATFORM(CHROMIUM)

#if !OS(DARWIN)
// Define SKIA on non-Mac.
#define WTF_PLATFORM_SKIA 1
#endif /* !OS(DARWIN) */

// Chromium uses this file instead of JavaScriptCore/config.h to compile
// JavaScriptCore/wtf (chromium doesn't compile the rest of JSC). Therefore,
// this define is required.
#define WTF_CHANGES 1

#define WTF_USE_GOOGLEURL 1

#if !defined(WTF_USE_V8)
#define WTF_USE_V8 1
#endif

#undef WTF_USE_CFNETWORK

#endif /* PLATFORM(CHROMIUM) */

#if !defined(WTF_USE_V8)
#define WTF_USE_V8 0
#endif /* !defined(WTF_USE_V8) */

/* Using V8 implies not using JSC and vice versa */
#if !defined(WTF_USE_JSC)
#define WTF_USE_JSC !WTF_USE_V8
#endif

#if PLATFORM(CG)
#ifndef CGFLOAT_DEFINED
#ifdef __LP64__
typedef double CGFloat;
#else
typedef float CGFloat;
#endif
#define CGFLOAT_DEFINED 1
#endif
#endif /* PLATFORM(CG) */

#ifdef BUILDING_ON_TIGER
#undef ENABLE_FTPDIR
#define ENABLE_FTPDIR 0
#endif

#if PLATFORM(WIN) && PLATFORM(CG)
#define WTF_USE_SAFARI_THEME 1
#endif

#if PLATFORM(QT) && USE(V8) && defined(Q_WS_X11)
/* protect ourselves from evil X11 defines */
#include <bridge/npruntime_internal.h>
#endif
