/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// must include config.h first for webkit to fiddle with new/delete
#include "config.h"
#include "SkANP.h"

SkRect* SkANP::SetRect(SkRect* dst, const ANPRectF& src) {
    dst->set(SkFloatToScalar(src.left),
             SkFloatToScalar(src.top),
             SkFloatToScalar(src.right),
             SkFloatToScalar(src.bottom));
    return dst;
}

SkIRect* SkANP::SetRect(SkIRect* dst, const ANPRectI& src) {
    dst->set(src.left, src.top, src.right, src.bottom);
    return dst;
}

ANPRectI* SkANP::SetRect(ANPRectI* dst, const SkIRect& src) {
    dst->left = src.fLeft;
    dst->top = src.fTop;
    dst->right = src.fRight;
    dst->bottom = src.fBottom;
    return dst;
}

SkBitmap* SkANP::SetBitmap(SkBitmap* dst, const ANPBitmap& src) {
    SkBitmap::Config config = SkBitmap::kNo_Config;
    
    switch (src.format) {
        case kRGBA_8888_ANPBitmapFormat:
            config = SkBitmap::kARGB_8888_Config;
            break;
        case kRGB_565_ANPBitmapFormat:
            config = SkBitmap::kRGB_565_Config;
            break;
        default:
            break;
    }
    
    dst->setConfig(config, src.width, src.height, src.rowBytes);
    dst->setPixels(src.baseAddr);
    return dst;
}

bool SkANP::SetBitmap(ANPBitmap* dst, const SkBitmap& src) {
    if (!(dst->baseAddr = src.getPixels())) {
        SkDebugf("SkANP::SetBitmap - getPixels() returned null\n");
        return false;
    }

    switch (src.config()) {
        case SkBitmap::kARGB_8888_Config:
            dst->format = kRGBA_8888_ANPBitmapFormat;
            break;
        case SkBitmap::kRGB_565_Config:
            dst->format = kRGB_565_ANPBitmapFormat;
            break;
        default:
            SkDebugf("SkANP::SetBitmap - unsupported src.config %d\n", src.config());
            return false;
    }
    
    dst->width    = src.width();
    dst->height   = src.height();
    dst->rowBytes = src.rowBytes();
    return true;
}

void SkANP::InitEvent(ANPEvent* event, ANPEventType et) {
    event->inSize = sizeof(ANPEvent);
    event->eventType = et;
}

