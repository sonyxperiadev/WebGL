/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
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

#define LOG_TAG "WebCore"

#include "config.h"
#include "LocalizedStrings.h"

#include "NotImplemented.h"
#include "PlatformString.h"

namespace WebCore {

// *** The following strings should be localized *** //

String contextMenuItemTagInspectElement()
{
    return String("Inspect Element");
}

String unknownFileSizeText()
{
    return String("Unknown");
}

String imageTitle(const String& filename, const IntSize& size)
{
    notImplemented();
    return String();
}

// The following functions are used to fetch localized text for HTML form
// elements submit and reset. These strings are used when the page author
// has not specified any text for these buttons.
String submitButtonDefaultLabel()
{
    verifiedOk();
    return String("Submit");
}

String resetButtonDefaultLabel()
{
    verifiedOk();
    return String("Reset");
}

// The alt text for an input element is not used visually, but rather is
// used for accessability - eg reading the web page. See
// HTMLInputElement::altText() for more information.
String inputElementAltText()
{
    notImplemented();
    return String();
}

// This is the string that appears before an input box when the HTML element
// <ISINDEX> is used. The returned string is used if no PROMPT attribute is
// provided.
// note: Safari and FireFox use (too long for us imho) "This is a searchable index. Enter search keywords:"
String searchableIndexIntroduction()
{
    verifiedOk();
    return String("Enter search:");
}

String contextMenuItemTagOpenLinkInNewWindow()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagDownloadLinkToDisk()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagCopyLinkToClipboard()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagOpenImageInNewWindow()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagDownloadImageToDisk()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagCopyImageToClipboard()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagOpenFrameInNewWindow()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagCopy()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagGoBack()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagGoForward()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagStop()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagReload()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagCut()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagPaste()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagNoGuessesFound()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagIgnoreSpelling()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagLearnSpelling()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagSearchWeb()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagLookUpInDictionary()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagOpenLink()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagIgnoreGrammar()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagSpellingMenu()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagShowSpellingPanel(bool)
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagCheckSpelling()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagCheckSpellingWhileTyping()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagCheckGrammarWithSpelling()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagFontMenu()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagBold()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagItalic()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagUnderline()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagOutline()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagWritingDirectionMenu()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagDefaultDirection()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagLeftToRight()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagRightToLeft()
{
    ASSERT_NOT_REACHED();
    return String();
}

String contextMenuItemTagTextDirectionMenu()
{
    ASSERT_NOT_REACHED();
    return String();
}

} // namespace WebCore
