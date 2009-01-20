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

#include "config.h"
#include "ClipboardAndroid.h"

#include "CachedImage.h"
#include "Element.h"
#include "Frame.h"
#include "Range.h"

namespace WebCore {

// format string for
static const char szShellDotUrlTemplate[] = "[InternetShortcut]\r\nURL=%s\r\n";

// We provide the IE clipboard types (URL and Text), and the clipboard types specified in the WHATWG Web Applications 1.0 draft
// see http://www.whatwg.org/specs/web-apps/current-work/ Section 6.3.5.3

enum ClipboardDataType { ClipboardDataTypeNone, ClipboardDataTypeURL, ClipboardDataTypeText };

static ClipboardDataType clipboardTypeFromMIMEType(const String& type)
{
    String qType = type.stripWhiteSpace().lower();

    // two special cases for IE compatibility
    if (qType == "text" || qType == "text/plain" || qType.startsWith("text/plain;"))
        return ClipboardDataTypeText;
    if (qType == "url" || qType == "text/uri-list")
        return ClipboardDataTypeURL;

    return ClipboardDataTypeNone;
}

ClipboardAndroid::ClipboardAndroid(ClipboardAccessPolicy policy, bool isForDragging)
    : Clipboard(policy, isForDragging)
{
}

ClipboardAndroid::~ClipboardAndroid()
{
}

void ClipboardAndroid::clearData(const String& type)
{
    //FIXME: Need to be able to write to the system clipboard <rdar://problem/5015941>
    ASSERT(isForDragging());
    if (policy() != ClipboardWritable)
        return;

    ClipboardDataType dataType = clipboardTypeFromMIMEType(type);

    if (dataType == ClipboardDataTypeURL) {
           }
    if (dataType == ClipboardDataTypeText) {
       
    }

}

void ClipboardAndroid::clearAllData()
{
    //FIXME: Need to be able to write to the system clipboard <rdar://problem/5015941>
    ASSERT(isForDragging());
    if (policy() != ClipboardWritable)
        return;
    
}

String ClipboardAndroid::getData(const String& type, bool& success) const
{     
    success = false;
    if (policy() != ClipboardReadable) {
        return "";
    }

    /*
    ClipboardDataType dataType = clipboardTypeFromMIMEType(type);
    if (dataType == ClipboardDataTypeText)
        return getPlainText(m_dataObject.get(), success);
    else if (dataType == ClipboardDataTypeURL) 
        return getURL(m_dataObject.get(), success);
    */
    return "";
}

bool ClipboardAndroid::setData(const String &type, const String &data)
{
    //FIXME: Need to be able to write to the system clipboard <rdar://problem/5015941>
    ASSERT(isForDragging());
    if (policy() != ClipboardWritable)
        return false;

    ClipboardDataType platformType = clipboardTypeFromMIMEType(type);

    if (platformType == ClipboardDataTypeURL) {
        KURL url = KURL(data);
#if 0 && defined ANDROID // FIXME HACK : KURL no longer defines a public method isValid()
        if (!url.isValid())
            return false;
#endif
        return false; // WebCore::writeURL(m_writableDataObject.get(), url, String(), false, true);
    } else if ( platformType == ClipboardDataTypeText) {
        return false;
    }
    return false;
}


// extensions beyond IE's API
HashSet<String> ClipboardAndroid::types() const
{ 
    HashSet<String> results; 
    if (policy() != ClipboardReadable && policy() != ClipboardTypesReadable)
        return results;

    return results;
}

void ClipboardAndroid::setDragImage(CachedImage* image, Node *node, const IntPoint &loc)
{
    if (policy() != ClipboardImageWritable && policy() != ClipboardWritable) 
        return;
        
    if (m_dragImage)
        m_dragImage->removeClient(this);
    m_dragImage = image;
    if (m_dragImage)
        m_dragImage->addClient(this);

    m_dragLoc = loc;
    m_dragImageElement = node;
}

void ClipboardAndroid::setDragImage(CachedImage* img, const IntPoint &loc)
{
    setDragImage(img, 0, loc);
}

void ClipboardAndroid::setDragImageElement(Node *node, const IntPoint &loc)
{
    setDragImage(0, node, loc);
}

DragImageRef ClipboardAndroid::createDragImage(IntPoint& loc) const
{
    void* result = 0;
    //FIXME: Need to be able to draw element <rdar://problem/5015942>
    if (m_dragImage) {
        result = createDragImageFromImage(m_dragImage->image());        
        loc = m_dragLoc;
    }
    return result;
}


void ClipboardAndroid::declareAndWriteDragImage(Element* element, const KURL& url, const String& title, Frame* frame)
{

}

void ClipboardAndroid::writeURL(const KURL& kurl, const String& titleStr, Frame*)
{

}

void ClipboardAndroid::writeRange(Range* selectedRange, Frame* frame)
{
    ASSERT(selectedRange);
}

bool ClipboardAndroid::hasData()
{

    return false;
}

} // namespace WebCore
