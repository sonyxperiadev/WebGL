/* 
**
** Copyright 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "config.h"

#define ANDROID_COMPILE_HACK

#include <stdio.h>
#include <stdlib.h>
#include "AXObjectCache.h"
#include "CachedPage.h"
#include "CachedResource.h"
#include "CookieJar.h"
#include "ContextMenu.h"
#include "ContextMenuItem.h"
#include "Clipboard.h"
#include "CString.h"
#include "Cursor.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "EditCommand.h"
#include "Editor.h"
#include "Font.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLKeygenElement.h"
#include "History.h"
#include "Icon.h"
#include "IconDatabase.h"
#include "IconLoader.h"
#include "IntPoint.h"
#include "KURL.h"
#include "Language.h"
#include "loader.h"
#include "LocalizedStrings.h"
#include "MainResourceLoader.h"
#include "MIMETypeRegistry.h"
#include "Node.h"
#include "PageCache.h"
#include "Pasteboard.h"
#include "Path.h"
#include "PlatformScrollBar.h"
#include "PluginInfoStore.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceLoader.h"
#include "Screen.h"
#include "ScrollBar.h"
#include "SmartReplace.h"
#include "Widget.h"

#undef LOG
#define LOG_TAG "WebCore"
#include "utils/Log.h"

using namespace WebCore;

//static inline void notImplemented() { LOGV("LinkStubs: Not Implemented %s\n", __PRETTY_FUNCTION__); }
//static inline void notImplemented(const char name[]) { LOGV("LinkStubs: Not Implemented %s\n", name); }
static inline void verifiedOk() { } // not a problem that it's not implemented

// This function is called when the frame view has changed the state of it's border. 
// iFrames, which are have a FrameView, are drawn with a 1px left/right border and 2px top/bottom border
// Check function _shouldDrawBorder in WebFrameView.mm
// We don't draw borders unless css draws them.
// void FrameView::updateBorder() { verifiedOk(); }

//int WebCore::screenDepthPerComponent(Widget*) { ASSERT(0); notImplemented(); return 0; }
//bool WebCore::screenIsMonochrome(Widget*) { ASSERT(0); notImplemented(); return false; }

/********************************************************/
/* Completely empty stubs (mostly to allow DRT to run): */
/********************************************************/
// This value turns on or off the Mac specific Accessibility support.
// bool AXObjectCache::gAccessibilityEnabled = false;

// This function is used by Javascript to find out what the default language
// the user has selected. It is used by the JS object Navigator.language
// I guess this information should be mapped with the Accept-Language: HTTP header.
String WebCore::defaultLanguage() { verifiedOk(); return "en"; }

namespace WebCore {

#if !defined(ANDROID_PLUGINS)
// If plugins support is turned on, don't use these stubs.

// Except for supportsMIMEType(), these Plugin functions are used by javascript's 
// navigator.plugins[] object to provide the list of available plugins. This is most 
// often used with to check to see if the browser supports Flash or which video
// codec to use.
// The supportsMIMEType() is used by the Frame to determine if a full screen instance
// of a plugin can be used to render a mimetype that is not native to the browser.
PluginInfo* PluginInfoStore::createPluginInfoForPluginAtIndex(unsigned) { ASSERT(0); return 0;}
unsigned PluginInfoStore::pluginCount() const { verifiedOk(); return 0; }
// FIXME, return false for now.
String PluginInfoStore::pluginNameForMIMEType(const String& ) { notImplemented(); return String(); }
bool PluginInfoStore::supportsMIMEType(const String& ) { verifiedOk(); return false; }
void refreshPlugins(bool) { verifiedOk(); }
#endif // !defined(ANDROID_PLUGINS)

// This function tells the bridge that a resource was loaded from the cache and thus
// the app may update progress with the amount of data loaded.
void CheckCacheObjectStatus(DocLoader*, CachedResource*) { ASSERT(0); notImplemented();}

// This class is used in conjunction with the File Upload form element, and
// therefore relates to the above. When a file has been selected, an icon
// representing the file type can be rendered next to the filename on the
// web page. The icon for the file is encapsulated within this class.
Icon::Icon() { notImplemented(); }
Icon::~Icon() { }
PassRefPtr<Icon> Icon::newIconForFile(const String& filename){ notImplemented(); return PassRefPtr<Icon>(new Icon()); }
void Icon::paint(GraphicsContext*, const IntRect&) { }

// *** The following strings should be localized *** //

// The following functions are used to fetch localized text for HTML form
// elements submit and reset. These strings are used when the page author
// has not specified any text for these buttons.
String submitButtonDefaultLabel() { verifiedOk(); return "Submit"; }
String resetButtonDefaultLabel() { verifiedOk(); return "Reset"; }

// The alt text for an input element is not used visually, but rather is
// used for accessability - eg reading the web page. See 
// HTMLInputElement::altText() for more information.
String inputElementAltText() { notImplemented(); return String(); }

// This is the string that appears before an input box when the HTML element
// <ISINDEX> is used. The returned string is used if no PROMPT attribute is 
// provided.
// note: Safari and FireFox use (too long for us imho) "This is a searchable index. Enter search keywords:"
String searchableIndexIntroduction() { verifiedOk(); return String("Enter search:"); }

}

// This function provides the default value for the CSS property:
// -webkit-focus-ring-color
// It is also related to the CSS property outline-color:
Color WebCore::focusRingColor() { verifiedOk(); return 0xFF0000FF; }

// The following functions are used on the Mac to register for and handle
// platform colour changes. The later function simply tells the view to
// reapply style and relayout.
void WebCore::setFocusRingColorChangeFunction(void (*)()) { verifiedOk(); }

// LocalizedStrings
String WebCore::contextMenuItemTagOpenLinkInNewWindow() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagDownloadLinkToDisk() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagCopyLinkToClipboard() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagOpenImageInNewWindow() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagDownloadImageToDisk() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagCopyImageToClipboard() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagOpenFrameInNewWindow() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagCopy() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagGoBack() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagGoForward() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagStop() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagReload() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagCut() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagPaste() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagNoGuessesFound() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagIgnoreSpelling() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagLearnSpelling() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagSearchWeb() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagLookUpInDictionary() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagOpenLink() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagIgnoreGrammar() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagSpellingMenu() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagShowSpellingPanel(bool show) { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagCheckSpelling() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagCheckSpellingWhileTyping() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagCheckGrammarWithSpelling() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagFontMenu() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagBold() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagItalic() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagUnderline() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagOutline() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagWritingDirectionMenu() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagDefaultDirection() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagLeftToRight() { ASSERT(0); return String(); }
String WebCore::contextMenuItemTagRightToLeft() { ASSERT(0); return String(); }

// FIXME, no support for spelling yet.
Pasteboard* Pasteboard::generalPasteboard() { return new Pasteboard(); }
void Pasteboard::writeSelection(Range*, bool canSmartCopyOrDelete, Frame*) { notImplemented(); }
void Pasteboard::writeURL(const KURL&, const String&, Frame*) { notImplemented(); }
void Pasteboard::clear() { notImplemented(); }
bool Pasteboard::canSmartReplace() { notImplemented(); return false; }
PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame*, PassRefPtr<Range>, bool allowPlainText, bool& chosePlainText) { notImplemented(); return 0; }
String Pasteboard::plainText(Frame* frame) { notImplemented(); return String(); }
Pasteboard::Pasteboard() { notImplemented(); }
Pasteboard::~Pasteboard() { notImplemented(); }

ContextMenu::ContextMenu(const HitTestResult& result) : m_hitTestResult(result) { ASSERT(0); notImplemented(); }
ContextMenu::~ContextMenu() { ASSERT(0); notImplemented(); }
void ContextMenu::appendItem(ContextMenuItem&) { ASSERT(0); notImplemented(); }
void ContextMenu::setPlatformDescription(PlatformMenuDescription menu) { ASSERT(0); m_platformDescription = menu; }
PlatformMenuDescription ContextMenu::platformDescription() const  { ASSERT(0); return m_platformDescription; }

ContextMenuItem::ContextMenuItem(PlatformMenuItemDescription) { ASSERT(0); notImplemented(); }
ContextMenuItem::ContextMenuItem(ContextMenu*) { ASSERT(0); notImplemented(); }
ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* subMenu) { ASSERT(0); notImplemented(); }
ContextMenuItem::~ContextMenuItem() { ASSERT(0); notImplemented(); }
PlatformMenuItemDescription ContextMenuItem::releasePlatformDescription() { ASSERT(0); notImplemented(); return m_platformDescription; }
ContextMenuItemType ContextMenuItem::type() const { ASSERT(0); notImplemented(); return ActionType; }
void ContextMenuItem::setType(ContextMenuItemType) { ASSERT(0); notImplemented(); }
ContextMenuAction ContextMenuItem::action() const { ASSERT(0); notImplemented(); return ContextMenuItemTagNoAction; }
void ContextMenuItem::setAction(ContextMenuAction) { ASSERT(0); notImplemented(); }
String ContextMenuItem::title() const { ASSERT(0); notImplemented(); return String(); }
void ContextMenuItem::setTitle(const String&) { ASSERT(0); notImplemented(); }
PlatformMenuDescription ContextMenuItem::platformSubMenu() const { ASSERT(0); notImplemented(); return 0; }
void ContextMenuItem::setSubMenu(ContextMenu*) { ASSERT(0); notImplemented(); }
void ContextMenuItem::setChecked(bool) { ASSERT(0); notImplemented(); }
void ContextMenuItem::setEnabled(bool) { ASSERT(0); notImplemented(); }

namespace WebCore {
float userIdleTime() { notImplemented(); return 0; }
// systemBeep() is called by the Editor to indicate that there was nothing to copy, and may be called from 
// other places too.
void systemBeep() { notImplemented(); }
}

// functions new to Jun-07 tip of tree merge:

// void WebCore::CachedPage::close() {}

//void WebCore::Frame::print() {}
// void WebCore::Frame::issueTransposeCommand() {}
//void WebCore::Frame::cleanupPlatformScriptObjects() {}
void WebCore::Frame::dashboardRegionsChanged() {}
//bool WebCore::Frame::isCharacterSmartReplaceExempt(unsigned short, bool) { return false; }
void* WebCore::Frame::dragImageForSelection() { return NULL; }

WebCore::String WebCore::MIMETypeRegistry::getMIMETypeForExtension(WebCore::String const&) {
    return WebCore::String();
}

void WebCore::Pasteboard::writeImage(WebCore::Node*, WebCore::KURL const&, WebCore::String const&) {}

namespace WebCore {
IntSize dragImageSize(void*) { return IntSize(0, 0); }
void deleteDragImage(void*) {}
void* createDragImageFromImage(Image*) { return NULL; }
void* dissolveDragImageToFraction(void*, float) { return NULL; }
void* createDragImageIconForCachedImage(CachedImage*) { return NULL; }
Cursor dummyCursor;
const Cursor& zoomInCursor() { return dummyCursor; }
const Cursor& zoomOutCursor() { return dummyCursor; }
const Cursor& notAllowedCursor() { return dummyCursor; }
void* scaleDragImage(void*, FloatSize) { return NULL; }
String searchMenuRecentSearchesText() { return String(); }
String searchMenuNoRecentSearchesText() { return String(); }
String searchMenuClearRecentSearchesText() { return String(); }
Vector<String> supportedKeySizes() { notImplemented(); return Vector<String>(); }
String signedPublicKeyAndChallengeString(unsigned int, String const&, WebCore::KURL const&) { return String(); }

}

// added for Nov-16-07 ToT integration
namespace WebCore {
void Frame::clearPlatformScriptObjects() { notImplemented(); }
}

// functions new to Feb-19 tip of tree merge:
namespace WebCore {
// isCharacterSmartReplaceExempt is defined in SmartReplaceICU.cpp; in theory, we could use that one
//      but we don't support all of the required icu functions 
bool isCharacterSmartReplaceExempt(UChar32 , bool ) { notImplemented(); return false; }
}

int MakeDataExecutable;

// functions new to Mar-2 tip of tree merge:
String KURL::fileSystemPath() const { notImplemented(); return String(); }
