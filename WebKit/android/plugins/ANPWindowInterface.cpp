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

static bool anp_lockRect(void* window, const ANPRectI* inval,
                         ANPBitmap* bitmap) {
    if (window) {
        // todo
        return true;
    }
    return false;
}

static bool anp_lockRegion(void* window, const ANPRegion* inval,
                           ANPBitmap* bitmap) {
    if (window) {
        // todo
        return true;
    }
    return false;
}

static void anp_unlock(void* window) {
}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void ANPWindowInterfaceV0_Init(ANPInterface* value) {
    ANPWindowInterfaceV0* i = reinterpret_cast<ANPWindowInterfaceV0*>(value);
    
    ASSIGN(i, lockRect);
    ASSIGN(i, lockRegion);
    ASSIGN(i, unlock);
}

