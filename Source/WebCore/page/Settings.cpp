/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011 Apple Inc. All rights reserved.
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
#include "Settings.h"

#include "BackForwardController.h"
#include "CachedResourceLoader.h"
#include "CookieStorage.h"
#include "DOMTimer.h"
#include "Database.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HistoryItem.h"
#include "Page.h"
#include "PageCache.h"
#include "ResourceHandle.h"
#include "StorageMap.h"
#include <limits>

using namespace std;

namespace WebCore {

static void setLoadsImagesAutomaticallyInAllFrames(Page* page)
{
    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->document()->cachedResourceLoader()->setAutoLoadImages(page->settings()->loadsImagesAutomatically());
}

#if USE(SAFARI_THEME)
bool Settings::gShouldPaintNativeControls = true;
#endif

#if PLATFORM(WIN) || (OS(WINDOWS) && PLATFORM(WX))
bool Settings::gShouldUseHighResolutionTimers = true;
#endif

// NOTEs
//  1) EditingMacBehavior comprises Tiger, Leopard, SnowLeopard and iOS builds, as well QtWebKit and Chromium when built on Mac;
//  2) EditingWindowsBehavior comprises Win32 and WinCE builds, as well as QtWebKit and Chromium when built on Windows;
//  3) EditingUnixBehavior comprises all unix-based systems, but Darwin/MacOS (and then abusing the terminology);
// 99) MacEditingBehavior is used a fallback.
static EditingBehaviorType editingBehaviorTypeForPlatform()
{
    return
#if OS(DARWIN)
    EditingMacBehavior
#elif OS(WINDOWS)
    EditingWindowsBehavior
#elif OS(UNIX)
    EditingUnixBehavior
#else
    // Fallback
    EditingMacBehavior
#endif
    ;
}

Settings::Settings(Page* page)
    : m_page(page)
    , m_editableLinkBehavior(EditableLinkDefaultBehavior)
    , m_textDirectionSubmenuInclusionBehavior(TextDirectionSubmenuAutomaticallyIncluded)
    , m_minimumFontSize(0)
    , m_minimumLogicalFontSize(0)
    , m_defaultFontSize(0)
    , m_defaultFixedFontSize(0)
    , m_validationMessageTimerMagnification(50)
    , m_maximumDecodedImageSize(numeric_limits<size_t>::max())
#if ENABLE(DOM_STORAGE)
    , m_sessionStorageQuota(StorageMap::noQuota)
#endif
    , m_pluginAllowedRunTime(numeric_limits<unsigned>::max())
    , m_editingBehaviorType(editingBehaviorTypeForPlatform())
#ifdef ANDROID_LAYOUT
    , m_layoutAlgorithm(kLayoutFitColumnToScreen)
#endif
    , m_isSpatialNavigationEnabled(false)
    , m_isJavaEnabled(false)
    , m_loadsImagesAutomatically(false)
    , m_loadsSiteIconsIgnoringImageLoadingSetting(false)
    , m_privateBrowsingEnabled(false)
    , m_caretBrowsingEnabled(false)
    , m_areImagesEnabled(true)
    , m_isMediaEnabled(true)
    , m_arePluginsEnabled(false)
    , m_localStorageEnabled(false)
    , m_isJavaScriptEnabled(false)
    , m_isWebSecurityEnabled(true)
    , m_allowUniversalAccessFromFileURLs(true)
    , m_allowFileAccessFromFileURLs(true)
    , m_javaScriptCanOpenWindowsAutomatically(false)
    , m_javaScriptCanAccessClipboard(false)
    , m_shouldPrintBackgrounds(false)
    , m_textAreasAreResizable(false)
#if ENABLE(DASHBOARD_SUPPORT)
    , m_usesDashboardBackwardCompatibilityMode(false)
#endif
    , m_needsAdobeFrameReloadingQuirk(false)
    , m_needsKeyboardEventDisambiguationQuirks(false)
    , m_treatsAnyTextCSSLinkAsStylesheet(false)
    , m_needsLeopardMailQuirks(false)
    , m_needsTigerMailQuirks(false)
    , m_isDOMPasteAllowed(false)
    , m_shrinksStandaloneImagesToFit(true)
    , m_usesPageCache(false)
    , m_showsURLsInToolTips(false)
    , m_forceFTPDirectoryListings(false)
    , m_developerExtrasEnabled(false)
    , m_authorAndUserStylesEnabled(true)
    , m_needsSiteSpecificQuirks(false)
    , m_fontRenderingMode(0)
    , m_frameFlatteningEnabled(false)
    , m_webArchiveDebugModeEnabled(false)
    , m_localFileContentSniffingEnabled(false)
    , m_inApplicationChromeMode(false)
    , m_offlineWebApplicationCacheEnabled(false)
    , m_shouldPaintCustomScrollbars(false)
    , m_enforceCSSMIMETypeInNoQuirksMode(true)
    , m_usesEncodingDetector(false)
    , m_allowScriptsToCloseWindows(false)
    , m_canvasUsesAcceleratedDrawing(false)
    , m_acceleratedDrawingEnabled(false)
    // FIXME: This should really be disabled by default as it makes platforms that don't support the feature download files
    // they can't use by. Leaving enabled for now to not change existing behavior.
    , m_downloadableBinaryFontsEnabled(true)
    , m_xssAuditorEnabled(false)
    , m_acceleratedCompositingEnabled(true)
    , m_acceleratedCompositingFor3DTransformsEnabled(true)
    , m_acceleratedCompositingForVideoEnabled(true)
    , m_acceleratedCompositingForPluginsEnabled(true)
    , m_acceleratedCompositingForCanvasEnabled(true)
    , m_acceleratedCompositingForAnimationEnabled(true)
    , m_showDebugBorders(false)
    , m_showRepaintCounter(false)
    , m_experimentalNotificationsEnabled(false)
    , m_webGLEnabled(false)
    , m_openGLMultisamplingEnabled(true)
    , m_webAudioEnabled(false)
    , m_acceleratedCanvas2dEnabled(false)
    , m_loadDeferringEnabled(true)
    , m_tiledBackingStoreEnabled(false)
    , m_paginateDuringLayoutEnabled(false)
    , m_dnsPrefetchingEnabled(true)
#if ENABLE(FULLSCREEN_API)
    , m_fullScreenAPIEnabled(false)
#endif
    , m_asynchronousSpellCheckingEnabled(false)
    , m_memoryInfoEnabled(false)
    , m_interactiveFormValidation(false)
    , m_usePreHTML5ParserQuirks(false)
    , m_hyperlinkAuditingEnabled(false)
    , m_crossOriginCheckInGetMatchedCSSRulesDisabled(false)
    , m_useQuickLookResourceCachingQuirks(false)
    , m_forceCompositingMode(false)
    , m_shouldInjectUserScriptsInInitialEmptyDocument(false)
#ifdef ANDROID_LAYOUT
    , m_useWideViewport(false)
#endif
#ifdef ANDROID_MULTIPLE_WINDOWS
    , m_supportMultipleWindows(true)
#endif
#ifdef ANDROID_BLOCK_NETWORK_IMAGE
    , m_blockNetworkImage(false)
#endif
#if ENABLE(WEB_AUTOFILL)
    , m_autoFillEnabled(false)
#endif
#ifdef ANDROID_PLUGINS
    , m_pluginsOnDemand(false)
#endif
{
    // A Frame may not have been created yet, so we initialize the AtomicString 
    // hash before trying to use it.
    AtomicString::init();
#ifdef ANDROID_META_SUPPORT
    m_default_format_detection = true;
    resetMetadataSettings();
#endif
}

void Settings::setStandardFontFamily(const AtomicString& standardFontFamily)
{
    if (standardFontFamily == m_standardFontFamily)
        return;

    m_standardFontFamily = standardFontFamily;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setFixedFontFamily(const AtomicString& fixedFontFamily)
{
    if (m_fixedFontFamily == fixedFontFamily)
        return;
        
    m_fixedFontFamily = fixedFontFamily;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setSerifFontFamily(const AtomicString& serifFontFamily)
{
    if (m_serifFontFamily == serifFontFamily)
        return;
        
    m_serifFontFamily = serifFontFamily;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setSansSerifFontFamily(const AtomicString& sansSerifFontFamily)
{
    if (m_sansSerifFontFamily == sansSerifFontFamily)
        return;
        
    m_sansSerifFontFamily = sansSerifFontFamily; 
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setCursiveFontFamily(const AtomicString& cursiveFontFamily)
{
    if (m_cursiveFontFamily == cursiveFontFamily)
        return;
        
    m_cursiveFontFamily = cursiveFontFamily;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setFantasyFontFamily(const AtomicString& fantasyFontFamily)
{
    if (m_fantasyFontFamily == fantasyFontFamily)
        return;
        
    m_fantasyFontFamily = fantasyFontFamily;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setMinimumFontSize(int minimumFontSize)
{
    if (m_minimumFontSize == minimumFontSize)
        return;

    m_minimumFontSize = minimumFontSize;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setMinimumLogicalFontSize(int minimumLogicalFontSize)
{
    if (m_minimumLogicalFontSize == minimumLogicalFontSize)
        return;

    m_minimumLogicalFontSize = minimumLogicalFontSize;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setDefaultFontSize(int defaultFontSize)
{
    if (m_defaultFontSize == defaultFontSize)
        return;

    m_defaultFontSize = defaultFontSize;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setDefaultFixedFontSize(int defaultFontSize)
{
    if (m_defaultFixedFontSize == defaultFontSize)
        return;

    m_defaultFixedFontSize = defaultFontSize;
    m_page->setNeedsRecalcStyleInAllFrames();
}

#ifdef ANDROID_BLOCK_NETWORK_IMAGE
void Settings::setBlockNetworkImage(bool blockNetworkImage)
{
    m_blockNetworkImage = blockNetworkImage;
}
#endif

void Settings::setLoadsImagesAutomatically(bool loadsImagesAutomatically)
{
    m_loadsImagesAutomatically = loadsImagesAutomatically;
    setLoadsImagesAutomaticallyInAllFrames(m_page);
}

void Settings::setLoadsSiteIconsIgnoringImageLoadingSetting(bool loadsSiteIcons)
{
    m_loadsSiteIconsIgnoringImageLoadingSetting = loadsSiteIcons;
}

void Settings::setJavaScriptEnabled(bool isJavaScriptEnabled)
{
    m_isJavaScriptEnabled = isJavaScriptEnabled;
}

void Settings::setWebSecurityEnabled(bool isWebSecurityEnabled)
{
    m_isWebSecurityEnabled = isWebSecurityEnabled;
}

void Settings::setAllowUniversalAccessFromFileURLs(bool allowUniversalAccessFromFileURLs)
{
    m_allowUniversalAccessFromFileURLs = allowUniversalAccessFromFileURLs;
}

void Settings::setAllowFileAccessFromFileURLs(bool allowFileAccessFromFileURLs)
{
    m_allowFileAccessFromFileURLs = allowFileAccessFromFileURLs;
}

void Settings::setSpatialNavigationEnabled(bool isSpatialNavigationEnabled)
{
    m_isSpatialNavigationEnabled = isSpatialNavigationEnabled;
}

void Settings::setJavaEnabled(bool isJavaEnabled)
{
    m_isJavaEnabled = isJavaEnabled;
}

void Settings::setImagesEnabled(bool areImagesEnabled)
{
    m_areImagesEnabled = areImagesEnabled;
}

void Settings::setMediaEnabled(bool isMediaEnabled)
{
    m_isMediaEnabled = isMediaEnabled;
}

void Settings::setPluginsEnabled(bool arePluginsEnabled)
{
    m_arePluginsEnabled = arePluginsEnabled;
}

void Settings::setLocalStorageEnabled(bool localStorageEnabled)
{
    m_localStorageEnabled = localStorageEnabled;
}

#if ENABLE(DOM_STORAGE)
void Settings::setSessionStorageQuota(unsigned sessionStorageQuota)
{
    m_sessionStorageQuota = sessionStorageQuota;
}
#endif

void Settings::setPrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
    if (m_privateBrowsingEnabled == privateBrowsingEnabled)
        return;

#if USE(CFURLSTORAGESESSIONS)
    ResourceHandle::setPrivateBrowsingEnabled(privateBrowsingEnabled);
#endif

    // FIXME: We can only enable cookie private browsing mode globally, so it's misleading to have it as a per-page setting.
    setCookieStoragePrivateBrowsingEnabled(privateBrowsingEnabled);

    m_privateBrowsingEnabled = privateBrowsingEnabled;
    m_page->privateBrowsingStateChanged();
}

void Settings::setJavaScriptCanOpenWindowsAutomatically(bool javaScriptCanOpenWindowsAutomatically)
{
    m_javaScriptCanOpenWindowsAutomatically = javaScriptCanOpenWindowsAutomatically;
}

void Settings::setJavaScriptCanAccessClipboard(bool javaScriptCanAccessClipboard)
{
    m_javaScriptCanAccessClipboard = javaScriptCanAccessClipboard;
}

void Settings::setDefaultTextEncodingName(const String& defaultTextEncodingName)
{
    m_defaultTextEncodingName = defaultTextEncodingName;
}

void Settings::setUserStyleSheetLocation(const KURL& userStyleSheetLocation)
{
    if (m_userStyleSheetLocation == userStyleSheetLocation)
        return;

    m_userStyleSheetLocation = userStyleSheetLocation;

    m_page->userStyleSheetLocationChanged();
}

void Settings::setShouldPrintBackgrounds(bool shouldPrintBackgrounds)
{
    m_shouldPrintBackgrounds = shouldPrintBackgrounds;
}

void Settings::setTextAreasAreResizable(bool textAreasAreResizable)
{
    if (m_textAreasAreResizable == textAreasAreResizable)
        return;

    m_textAreasAreResizable = textAreasAreResizable;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setEditableLinkBehavior(EditableLinkBehavior editableLinkBehavior)
{
    m_editableLinkBehavior = editableLinkBehavior;
}

void Settings::setTextDirectionSubmenuInclusionBehavior(TextDirectionSubmenuInclusionBehavior behavior)
{
    m_textDirectionSubmenuInclusionBehavior = behavior;
}

#if ENABLE(DASHBOARD_SUPPORT)
void Settings::setUsesDashboardBackwardCompatibilityMode(bool usesDashboardBackwardCompatibilityMode)
{
    m_usesDashboardBackwardCompatibilityMode = usesDashboardBackwardCompatibilityMode;
}
#endif

// FIXME: This quirk is needed because of Radar 4674537 and 5211271. We need to phase it out once Adobe
// can fix the bug from their end.
void Settings::setNeedsAdobeFrameReloadingQuirk(bool shouldNotReloadIFramesForUnchangedSRC)
{
    m_needsAdobeFrameReloadingQuirk = shouldNotReloadIFramesForUnchangedSRC;
}

// This is a quirk we are pro-actively applying to old applications. It changes keyboard event dispatching,
// making keyIdentifier available on keypress events, making charCode available on keydown/keyup events,
// and getting keypress dispatched in more cases.
void Settings::setNeedsKeyboardEventDisambiguationQuirks(bool needsQuirks)
{
    m_needsKeyboardEventDisambiguationQuirks = needsQuirks;
}

void Settings::setTreatsAnyTextCSSLinkAsStylesheet(bool treatsAnyTextCSSLinkAsStylesheet)
{
    m_treatsAnyTextCSSLinkAsStylesheet = treatsAnyTextCSSLinkAsStylesheet;
}

void Settings::setNeedsLeopardMailQuirks(bool needsQuirks)
{
    m_needsLeopardMailQuirks = needsQuirks;
}

void Settings::setNeedsTigerMailQuirks(bool needsQuirks)
{
    m_needsTigerMailQuirks = needsQuirks;
}
    
void Settings::setDOMPasteAllowed(bool DOMPasteAllowed)
{
    m_isDOMPasteAllowed = DOMPasteAllowed;
}

void Settings::setDefaultMinDOMTimerInterval(double interval)
{
    DOMTimer::setDefaultMinTimerInterval(interval);
}

double Settings::defaultMinDOMTimerInterval()
{
    return DOMTimer::defaultMinTimerInterval();
}

void Settings::setMinDOMTimerInterval(double interval)
{
    m_page->setMinimumTimerInterval(interval);
}

double Settings::minDOMTimerInterval()
{
    return m_page->minimumTimerInterval();
}

void Settings::setUsesPageCache(bool usesPageCache)
{
    if (m_usesPageCache == usesPageCache)
        return;
        
    m_usesPageCache = usesPageCache;
    if (!m_usesPageCache) {
        int first = -m_page->backForward()->backCount();
        int last = m_page->backForward()->forwardCount();
        for (int i = first; i <= last; i++)
            pageCache()->remove(m_page->backForward()->itemAtIndex(i));
        pageCache()->releaseAutoreleasedPagesNow();
    }
}

void Settings::setShrinksStandaloneImagesToFit(bool shrinksStandaloneImagesToFit)
{
    m_shrinksStandaloneImagesToFit = shrinksStandaloneImagesToFit;
}

void Settings::setShowsURLsInToolTips(bool showsURLsInToolTips)
{
    m_showsURLsInToolTips = showsURLsInToolTips;
}

void Settings::setFTPDirectoryTemplatePath(const String& path)
{
    m_ftpDirectoryTemplatePath = path;
}

void Settings::setForceFTPDirectoryListings(bool force)
{
    m_forceFTPDirectoryListings = force;
}

void Settings::setDeveloperExtrasEnabled(bool developerExtrasEnabled)
{
    m_developerExtrasEnabled = developerExtrasEnabled;
}

#ifdef ANDROID_META_SUPPORT
void Settings::resetMetadataSettings()
{
    m_viewport_width = -1;
    m_viewport_height = -1;
    m_viewport_initial_scale = 0;
    m_viewport_minimum_scale = 0;
    m_viewport_maximum_scale = 0;
    m_viewport_user_scalable = true;
    m_viewport_target_densitydpi = -1;
    m_format_detection_telephone = m_default_format_detection;
    m_format_detection_address = m_default_format_detection;
    m_format_detection_email = m_default_format_detection;
}

void Settings::setMetadataSettings(const String& key, const String& value)
{
    if (key == "width") {
        if (value == "device-width") {
            m_viewport_width = 0;
        } else {
            int width = value.toInt();
            if (width <= 10000) {
                if (width <= 320) {
                    // This is a hack to accommodate the pages designed for the
                    // original iPhone. The new version, since 10/2007, is to
                    // use device-width which works for both portrait and
                    // landscape modes.
                    m_viewport_width = 0;
                } else {
                    m_viewport_width = width;
                }
            }
        }
    } else if (key == "height") {
        if (value == "device-height") {
            m_viewport_height = 0;
        } else {
            int height = value.toInt();
            if (height >= 200 && height <= 10000) {
                m_viewport_height = height;
            }
        }
    } else if (key == "initial-scale") {
        int scale = int(value.toFloat() * 100);
        if (scale >= 1 && scale <= 1000) {
            m_viewport_initial_scale = scale;
        }
    } else if (key == "minimum-scale") {
        int scale = int(value.toFloat() * 100);
        if (scale >= 1 && scale <= 1000) {
            m_viewport_minimum_scale = scale;
        }
    } else if (key == "maximum-scale") {
        int scale = int(value.toFloat() * 100);
        if (scale >= 1 && scale <= 1000) {
            m_viewport_maximum_scale = scale;
        }
    } else if (key == "user-scalable") {
        // even Apple doc says using "no", "0" is common in the real world, and
        // some sites, e.g. gomoviesapp.com, use "false".
        if (value == "no" || value == "0" || value == "false") {
            m_viewport_user_scalable = false;
        }
    } else if (key == "target-densitydpi") {
        if (value == "device-dpi") {
            m_viewport_target_densitydpi = 0;
        } else if (value == "low-dpi") {
            m_viewport_target_densitydpi = 120;
        } else if (value == "medium-dpi") {
            m_viewport_target_densitydpi = 160;
        } else if (value == "high-dpi") {
            m_viewport_target_densitydpi = 240;
        } else {
            int dpi = value.toInt();
            if (dpi >= 70 && dpi <= 400) {
                m_viewport_target_densitydpi = dpi;
            }
        }
    } else if (key == "telephone") {
        if (value == "no") {
            m_format_detection_telephone = false;
        }
    } else if (key == "address") {
        if (value == "no") {
            m_format_detection_address = false;
        }
    } else if (key == "email") {
        if (value == "no") {
            m_format_detection_email = false;
        }
    } else if (key == "format-detection") {
        // even Apple doc says "format-detection" should be the name of the
        // <meta> tag. In the real world, e.g. amazon.com, use
        // "format-detection=no" in the "viewport" <meta> tag to disable all
        // format detection.
        if (value == "no") {
            m_format_detection_telephone = false;
            m_format_detection_address = false;
            m_format_detection_email = false;
        }
    }
}

void Settings::setViewportWidth(int width)
{
    if (width < 0 || width > 10000)
        m_viewport_width = -1;
    else
        m_viewport_width = width;
}

void Settings::setViewportHeight(int height)
{
    if (height < 0 || height > 10000)
        m_viewport_height = -1;
    else
        m_viewport_height = height;
}

void Settings::setViewportInitialScale(int scale)
{
    if (scale < 1 || scale > 1000)
        m_viewport_initial_scale = 0;
    else
        m_viewport_initial_scale = scale;
}

void Settings::setViewportMinimumScale(int scale)
{
    if (scale < 1 || scale > 1000)
        m_viewport_minimum_scale = 0;
    else
        m_viewport_minimum_scale = scale;
}

void Settings::setViewportMaximumScale(int scale)
{
    if (scale < 1 || scale > 1000)
        m_viewport_maximum_scale = 0;
    else
        m_viewport_maximum_scale = scale;
}

void Settings::setViewportUserScalable(bool scalable)
{
    m_viewport_user_scalable = scalable;
}

void Settings::setViewportTargetDensityDpi(int dpi)
{
    if (dpi < 0 || dpi > 400)
        m_viewport_target_densitydpi = -1;
    else
        m_viewport_target_densitydpi = dpi;
}

void Settings::setFormatDetectionAddress(bool detect)
{
    m_format_detection_address = detect;
}

void Settings::setFormatDetectionEmail(bool detect)
{
    m_format_detection_email = detect;
}

void Settings::setFormatDetectionTelephone(bool detect)
{
    m_format_detection_telephone = detect;
}
#endif

void Settings::setAuthorAndUserStylesEnabled(bool authorAndUserStylesEnabled)
{
    if (m_authorAndUserStylesEnabled == authorAndUserStylesEnabled)
        return;

    m_authorAndUserStylesEnabled = authorAndUserStylesEnabled;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setFontRenderingMode(FontRenderingMode mode)
{
    if (fontRenderingMode() == mode)
        return;
    m_fontRenderingMode = mode;
    m_page->setNeedsRecalcStyleInAllFrames();
}

FontRenderingMode Settings::fontRenderingMode() const
{
    return static_cast<FontRenderingMode>(m_fontRenderingMode);
}

void Settings::setNeedsSiteSpecificQuirks(bool needsQuirks)
{
    m_needsSiteSpecificQuirks = needsQuirks;
}

void Settings::setFrameFlatteningEnabled(bool frameFlatteningEnabled)
{
    m_frameFlatteningEnabled = frameFlatteningEnabled;
}

#if ENABLE(WEB_ARCHIVE)
void Settings::setWebArchiveDebugModeEnabled(bool enabled)
{
    m_webArchiveDebugModeEnabled = enabled;
}
#endif

void Settings::setLocalFileContentSniffingEnabled(bool enabled)
{
    m_localFileContentSniffingEnabled = enabled;
}

void Settings::setLocalStorageDatabasePath(const String& path)
{
    m_localStorageDatabasePath = path;
}

void Settings::setApplicationChromeMode(bool mode)
{
    m_inApplicationChromeMode = mode;
}

void Settings::setOfflineWebApplicationCacheEnabled(bool enabled)
{
    m_offlineWebApplicationCacheEnabled = enabled;
}

void Settings::setShouldPaintCustomScrollbars(bool shouldPaintCustomScrollbars)
{
    m_shouldPaintCustomScrollbars = shouldPaintCustomScrollbars;
}

void Settings::setEnforceCSSMIMETypeInNoQuirksMode(bool enforceCSSMIMETypeInNoQuirksMode)
{
    m_enforceCSSMIMETypeInNoQuirksMode = enforceCSSMIMETypeInNoQuirksMode;
}

#if USE(SAFARI_THEME)
void Settings::setShouldPaintNativeControls(bool shouldPaintNativeControls)
{
    gShouldPaintNativeControls = shouldPaintNativeControls;
}
#endif

void Settings::setUsesEncodingDetector(bool usesEncodingDetector)
{
    m_usesEncodingDetector = usesEncodingDetector;
}

void Settings::setDNSPrefetchingEnabled(bool dnsPrefetchingEnabled)
{
    if (m_dnsPrefetchingEnabled == dnsPrefetchingEnabled)
        return;

    m_dnsPrefetchingEnabled = dnsPrefetchingEnabled;
    m_page->dnsPrefetchingStateChanged();
}

void Settings::setAllowScriptsToCloseWindows(bool allowScriptsToCloseWindows)
{
    m_allowScriptsToCloseWindows = allowScriptsToCloseWindows;
}

void Settings::setCaretBrowsingEnabled(bool caretBrowsingEnabled)
{
    m_caretBrowsingEnabled = caretBrowsingEnabled;
}

void Settings::setDownloadableBinaryFontsEnabled(bool downloadableBinaryFontsEnabled)
{
    m_downloadableBinaryFontsEnabled = downloadableBinaryFontsEnabled;
}

void Settings::setXSSAuditorEnabled(bool xssAuditorEnabled)
{
    m_xssAuditorEnabled = xssAuditorEnabled;
}

void Settings::setAcceleratedCompositingEnabled(bool enabled)
{
    if (m_acceleratedCompositingEnabled == enabled)
        return;
        
    m_acceleratedCompositingEnabled = enabled;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setCanvasUsesAcceleratedDrawing(bool enabled)
{
    m_canvasUsesAcceleratedDrawing = enabled;
}

void Settings::setAcceleratedDrawingEnabled(bool enabled)
{
    m_acceleratedDrawingEnabled = enabled;
}

void Settings::setAcceleratedCompositingFor3DTransformsEnabled(bool enabled)
{
    m_acceleratedCompositingFor3DTransformsEnabled = enabled;
}

void Settings::setAcceleratedCompositingForVideoEnabled(bool enabled)
{
    m_acceleratedCompositingForVideoEnabled = enabled;
}

void Settings::setAcceleratedCompositingForPluginsEnabled(bool enabled)
{
    m_acceleratedCompositingForPluginsEnabled = enabled;
}

void Settings::setAcceleratedCompositingForCanvasEnabled(bool enabled)
{
    m_acceleratedCompositingForCanvasEnabled = enabled;
}

void Settings::setAcceleratedCompositingForAnimationEnabled(bool enabled)
{
    m_acceleratedCompositingForAnimationEnabled = enabled;
}

void Settings::setShowDebugBorders(bool enabled)
{
    if (m_showDebugBorders == enabled)
        return;
        
    m_showDebugBorders = enabled;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setShowRepaintCounter(bool enabled)
{
    if (m_showRepaintCounter == enabled)
        return;
        
    m_showRepaintCounter = enabled;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setExperimentalNotificationsEnabled(bool enabled)
{
    m_experimentalNotificationsEnabled = enabled;
}

void Settings::setPluginAllowedRunTime(unsigned runTime)
{
    m_pluginAllowedRunTime = runTime;
    m_page->pluginAllowedRunTimeChanged();
}

#if PLATFORM(WIN) || (OS(WINDOWS) && PLATFORM(WX))
void Settings::setShouldUseHighResolutionTimers(bool shouldUseHighResolutionTimers)
{
    gShouldUseHighResolutionTimers = shouldUseHighResolutionTimers;
}
#endif

void Settings::setWebAudioEnabled(bool enabled)
{
    m_webAudioEnabled = enabled;
}

void Settings::setWebGLEnabled(bool enabled)
{
    m_webGLEnabled = enabled;
}

void Settings::setOpenGLMultisamplingEnabled(bool enabled)
{
    m_openGLMultisamplingEnabled = enabled;
}

void Settings::setAccelerated2dCanvasEnabled(bool enabled)
{
    m_acceleratedCanvas2dEnabled = enabled;
}

void Settings::setLoadDeferringEnabled(bool enabled)
{
    m_loadDeferringEnabled = enabled;
}

void Settings::setTiledBackingStoreEnabled(bool enabled)
{
    m_tiledBackingStoreEnabled = enabled;
#if ENABLE(TILED_BACKING_STORE)
    if (m_page->mainFrame())
        m_page->mainFrame()->setTiledBackingStoreEnabled(enabled);
#endif
}

} // namespace WebCore
