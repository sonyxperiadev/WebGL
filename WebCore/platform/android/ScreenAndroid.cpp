/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "Screen.h"

// This include must come first.
#undef LOG // FIXME: Still have to do this to get the log to show up
#include "utils/Log.h"

#include "FloatRect.h"
#include "Widget.h"
#include <ui/DisplayInfo.h>
#include <ui/PixelFormat.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#include "ScrollView.h"
#include "WebCoreViewBridge.h"

namespace WebCore {

int screenDepth(Widget* page)
{
    android::DisplayInfo info;
    android::SurfaceComposerClient::getDisplayInfo(android::DisplayID(0), &info);
    return info.pixelFormatInfo.bitsPerPixel;
}

int screenDepthPerComponent(Widget* page)
{
    android::DisplayInfo info;
    android::SurfaceComposerClient::getDisplayInfo(android::DisplayID(0), &info);
    return info.pixelFormatInfo.bitsPerPixel;
}

bool screenIsMonochrome(Widget* page)
{
    return false;
}

// The only place I have seen these values used is
// positioning popup windows. If we support multiple windows
// they will be most likely full screen. Therefore,
// the accuracy of these number are not too important.
FloatRect screenRect(Widget* page)
{
    IntRect rect = page->root()->platformWidget()->getBounds();
    return FloatRect(0.0, 0.0, rect.width(), rect.height());
}

// Similar screenRect, this function is commonly used by javascripts
// to position and resize windows (usually to full screen). 
FloatRect screenAvailableRect(Widget* page)
{
    IntRect rect = page->root()->platformWidget()->getBounds();
    return FloatRect(0.0, 0.0, rect.width(), rect.height());
}

} // namespace WebCore
