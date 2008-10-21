/*
 *
 * Copyright (C) 2006 The Android Open Source Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include "jni/jni_utility.h"
#include "jni/jni_runtime.h"

#if defined ANDROID
bool KJS::Bindings::dispatchJNICall (const void *targetAppletView, jobject obj, bool isStatic, JNIType returnType, jmethodID methodID, jvalue *args, jvalue &result, const char*, JSValue *&exceptionDescription)
{
    bzero (&result, sizeof(jvalue));
    return false;

}
#endif

