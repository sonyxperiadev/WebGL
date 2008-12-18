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

static ANPTypeface* anp_createFromName(const char name[], ANPTypefaceStyle s) {
    SkTypeface* tf = SkTypeface::Create(name,
                                        static_cast<SkTypeface::Style>(s));
    return reinterpret_cast<ANPTypeface*>(tf);
}

static ANPTypeface* anp_createFromTypeface(const ANPTypeface* family,
                                           ANPTypefaceStyle s) {
    SkTypeface* tf = SkTypeface::CreateFromTypeface(family,
                                          static_cast<SkTypeface::Style>(s));
    return reinterpret_cast<ANPTypeface*>(tf);
}

static int32_t anp_getRefCount(const ANPTypeface* tf) {
    return tf ? tf->getRefCnt() : 0;
}

static void anp_ref(ANPTypeface* tf) {
    tf->safeRef();
}

static void anp_unref(ANPTypeface* tf) {
    tf->safeUnref();
}

static ANPTypefaceStyle anp_getStyle(const ANPTypeface* tf) {
    SkTypeface::Style s = tf ? tf->getStyle() : SkTypeface::kNormal;
    return static_cast<ANPTypefaceStyle>(s);
}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void ANPTypefaceInterfaceV0_Init(ANPInterface* v) {
    ANPTypefaceInterfaceV0* i = reinterpret_cast<ANPTypefaceInterfaceV0*>(v);
    
    ASSIGN(i, createFromName);
    ASSIGN(i, createFromTypeface);
    ASSIGN(i, getRefCount);
    ASSIGN(i, ref);
    ASSIGN(i, unref);
    ASSIGN(i, getStyle);
}

