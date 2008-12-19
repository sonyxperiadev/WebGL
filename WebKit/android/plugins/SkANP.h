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

#ifndef SkANP_DEFINED
#define SkANP_DEFINED

#include "android_npapi.h"
#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkPath.h"
#include "SkTypeface.h"

struct ANPPath : SkPath {
};

struct ANPPaint : SkPaint {
};

struct ANPTypeface : SkTypeface {
};

struct ANPCanvas {
    SkCanvas* skcanvas;

    // draw into the specified bitmap
    explicit ANPCanvas(const SkBitmap& bm) {
        skcanvas = new SkCanvas(bm);
    }

    // redirect all drawing to the specific SkCanvas
    explicit ANPCanvas(SkCanvas* other) {
        skcanvas = other;
        skcanvas->ref();
    }

    ~ANPCanvas() {
        skcanvas->unref();
    }
};

class SkANP {
public:
    static SkRect* SetRect(SkRect* dst, const ANPRectF& src);
    static SkIRect* SetRect(SkIRect* dst, const ANPRectI& src);
    static ANPRectI* SetRect(ANPRectI* dst, const SkIRect& src);
    static SkBitmap* SetBitmap(SkBitmap* dst, const ANPBitmap& src);
    static bool SetBitmap(ANPBitmap* dst, const SkBitmap& src);
    
    static void InitEvent(ANPEvent* event, ANPEventType et);
};

#endif

