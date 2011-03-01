/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ANPSystem_npapi_H
#define ANPSystem_npapi_H

#include "android_npapi.h"
#include <jni.h>

struct ANPSystemInterfaceV0 : ANPInterface {
    /** Return the path name for the current Application's plugin data directory,
        or NULL if not supported
     */
    const char* (*getApplicationDataDirectory)();

    /** A helper function to load java classes from the plugin's apk.  The
        function looks for a class given the fully qualified and null terminated
        string representing the className. For example,

        const char* className = "com.android.mypackage.MyClass";

        If the class cannot be found or there is a problem loading the class
        NULL will be returned.
     */
    jclass (*loadJavaClass)(NPP instance, const char* className);
};

enum ANPPowerStates {
    kDefault_ANPPowerState  = 0,
    kScreenOn_ANPPowerState = 1
};
typedef int32_t ANPPowerState;

struct ANPSystemInterfaceV1 : ANPSystemInterfaceV0 {
    void (*setPowerState)(NPP instance, ANPPowerState powerState);
};

struct ANPSystemInterfaceV2 : ANPInterface {
    /** Return the path name for the current Application's plugin data directory,
        or NULL if not supported. This directory will change depending on whether
        or not the plugin is found within an incognito tab.
     */
    const char* (*getApplicationDataDirectory)(NPP instance);

    // redeclaration of existing features
    jclass (*loadJavaClass)(NPP instance, const char* className);
    void (*setPowerState)(NPP instance, ANPPowerState powerState);
};

#endif //ANPSystem_npapi_H
