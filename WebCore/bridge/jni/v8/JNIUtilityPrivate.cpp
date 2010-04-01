/*
 * Copyright 2010, The Android Open Source Project
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

#include "config.h"
#include "JNIUtilityPrivate.h"

#include "JavaInstanceV8.h"
#include "JavaNPObjectV8.h"
#include "npruntime_impl.h"

namespace JSC {

namespace Bindings {

jvalue convertNPVariantToJValue(NPVariant value, JNIType jniType, const char* javaClassName)
{
    jvalue result;
    NPVariantType type = value.type;

    switch (jniType) {
    case array_type:
        {
            JNIEnv* env = getJNIEnv();
            jobject javaArray;
            NPObject* object = NPVARIANT_IS_OBJECT(value) ? NPVARIANT_TO_OBJECT(value) : 0;
            NPVariant npvLength;
            bool success = _NPN_GetProperty(0, object, _NPN_GetStringIdentifier("length"), &npvLength);
            if (!success) {
                // No length property so we don't know how many elements to put into the array.
                // Treat this as an error.
#ifdef EMULATE_JSC_BINDINGS
                // JSC sends null for an array that is not an array of strings or basic types,
                // do this also in the unknown length case.
                memset(&result, 0, sizeof(jvalue));
#else
                // Sending NULL as JSC does seems dangerous. (Imagine the java method that asks
                // for the length of the array it was passed). Here we send a 0 length array.
                jclass objectClass = env->FindClass("java/lang/Object");
                javaArray = env->NewObjectArray(0, objectClass, 0);
                env->DeleteLocalRef(objectClass);
#endif
                break;
            }

            jsize length = static_cast<jsize>(NPVARIANT_TO_INT32(npvLength));

            if (!strcmp(javaClassName, "[Ljava.lang.String;")) {
                // Match JSC behavior by only allowing Object arrays if they are Strings.
                jclass stringClass = env->FindClass("java/lang/String");
                javaArray = env->NewObjectArray(length, stringClass, 0);
                for (jsize i = 0; i < length; i++) {
                    NPVariant npvValue;
                    _NPN_GetProperty(0, object, _NPN_GetIntIdentifier(i), &npvValue);
                    if(NPVARIANT_IS_STRING(npvValue)) {
                        NPString str = NPVARIANT_TO_STRING(npvValue);
                        env->SetObjectArrayElement(static_cast<jobjectArray>(javaArray), i, env->NewStringUTF(str.UTF8Characters));
                    }
                }

                env->DeleteLocalRef(stringClass);
            } else if (!strcmp(javaClassName, "[B")) {
                // array of bytes
                javaArray = env->NewByteArray(length);
                // Now iterate over each element and add to the array.
                for (jsize i = 0; i < length; i++) {
                    NPVariant npvValue;
                    _NPN_GetProperty(0, object, _NPN_GetIntIdentifier(i), &npvValue);
                    if (NPVARIANT_IS_INT32(npvValue)) {
                        jbyte bVal = static_cast<jbyte>(NPVARIANT_TO_INT32(npvValue));
                        env->SetByteArrayRegion(static_cast<jbyteArray>(javaArray), i, 1, &bVal);
                    }
                }
             } else if (!strcmp(javaClassName, "[C")) {
                // array of chars
                javaArray = env->NewCharArray(length);
                // Now iterate over each element and add to the array.
                for (jsize i = 0; i < length; i++) {
                    NPVariant npvValue;
                    _NPN_GetProperty(0, object, _NPN_GetIntIdentifier(i), &npvValue);
                    jchar cVal = 0;
                    if (NPVARIANT_IS_INT32(npvValue)) {
                        cVal = static_cast<jchar>(NPVARIANT_TO_INT32(npvValue));
                    } else if (NPVARIANT_IS_STRING(npvValue)) {
                        NPString str = NPVARIANT_TO_STRING(npvValue);
                        cVal = str.UTF8Characters[0];
                    }
                    env->SetCharArrayRegion(static_cast<jcharArray>(javaArray), i, 1, &cVal);
                }
             } else if (!strcmp(javaClassName, "[D")) {
                // array of doubles
                javaArray = env->NewDoubleArray(length);
                // Now iterate over each element and add to the array.
                for (jsize i = 0; i < length; i++) {
                    NPVariant npvValue;
                    _NPN_GetProperty(0, object, _NPN_GetIntIdentifier(i), &npvValue);
                    if (NPVARIANT_IS_DOUBLE(npvValue)) {
                        jdouble dVal = NPVARIANT_TO_DOUBLE(npvValue);
                        env->SetDoubleArrayRegion(static_cast<jdoubleArray>(javaArray), i, 1, &dVal);
                    }
                }
             } else if (!strcmp(javaClassName, "[F")) {
                // array of floats
                javaArray = env->NewFloatArray(length);
                // Now iterate over each element and add to the array.
                for (jsize i = 0; i < length; i++) {
                    NPVariant npvValue;
                    _NPN_GetProperty(0, object, _NPN_GetIntIdentifier(i), &npvValue);
                    if (NPVARIANT_IS_DOUBLE(npvValue)) {
                        jfloat fVal = static_cast<jfloat>(NPVARIANT_TO_DOUBLE(npvValue));
                        env->SetFloatArrayRegion(static_cast<jfloatArray>(javaArray), i, 1, &fVal);
                    }
                }
             } else if (!strcmp(javaClassName, "[I")) {
                // array of ints
                javaArray = env->NewIntArray(length);
                // Now iterate over each element and add to the array.
                for (jsize i = 0; i < length; i++) {
                    NPVariant npvValue;
                    _NPN_GetProperty(0, object, _NPN_GetIntIdentifier(i), &npvValue);
                    if (NPVARIANT_IS_INT32(npvValue)) {
                        jint iVal = NPVARIANT_TO_INT32(npvValue);
                        env->SetIntArrayRegion(static_cast<jintArray>(javaArray), i, 1, &iVal);
                    }
                }
             } else if (!strcmp(javaClassName, "[J")) {
                // array of longs
                javaArray = env->NewLongArray(length);
                // Now iterate over each element and add to the array.
                for (jsize i = 0; i < length; i++) {
                    NPVariant npvValue;
                    _NPN_GetProperty(0, object, _NPN_GetIntIdentifier(i), &npvValue);
                    if (NPVARIANT_IS_INT32(npvValue)) {
                        jlong jVal = static_cast<jlong>(NPVARIANT_TO_INT32(npvValue));
                        env->SetLongArrayRegion(static_cast<jlongArray>(javaArray), i, 1, &jVal);
                    }
                }
             } else if (!strcmp(javaClassName, "[S")) {
                // array of shorts
                javaArray = env->NewShortArray(length);
                // Now iterate over each element and add to the array.
                for (jsize i = 0; i < length; i++) {
                    NPVariant npvValue;
                    _NPN_GetProperty(0, object, _NPN_GetIntIdentifier(i), &npvValue);
                    if (NPVARIANT_IS_INT32(npvValue)) {
                        jshort sVal = static_cast<jshort>(NPVARIANT_TO_INT32(npvValue));
                        env->SetShortArrayRegion(static_cast<jshortArray>(javaArray), i, 1, &sVal);
                    }
                }
             } else if (!strcmp(javaClassName, "[Z")) {
                // array of booleans
                javaArray = env->NewBooleanArray(length);
                // Now iterate over each element and add to the array.
                for (jsize i = 0; i < length; i++) {
                    NPVariant npvValue;
                    _NPN_GetProperty(0, object, _NPN_GetIntIdentifier(i), &npvValue);
                    if (NPVARIANT_IS_BOOLEAN(npvValue)) {
                        jboolean zVal = NPVARIANT_TO_BOOLEAN(npvValue);
                        env->SetBooleanArrayRegion(static_cast<jbooleanArray>(javaArray), i, 1, &zVal);
                    }
                }
            } else {
#ifdef EMULATE_JSC_BINDINGS
                // JSC sends null for an array that is not an array of strings or basic types.
                memset(&result, 0, sizeof(jvalue));
                break;
#else
                // Sending NULL as JSC does seems dangerous. (Imagine the java method that asks
                // for the length of the array it was passed). Here we send a 0 length array.
                jclass objectClass = env->FindClass("java/lang/Object");
                javaArray = env->NewObjectArray(0, objectClass, 0);
                env->DeleteLocalRef(objectClass);
#endif
            }

            result.l = javaArray;
        }
        break;

    case object_type:
        {
            JNIEnv* env = getJNIEnv();
            result.l = static_cast<jobject>(0);
            jobject javaString;

            // First see if we have a Java instance.
            if (type == NPVariantType_Object) {
                NPObject* objectImp = NPVARIANT_TO_OBJECT(value);
                if (JavaInstance* instance = ExtractJavaInstance(objectImp))
                    result.l = instance->javaInstance();
            }

            // Now convert value to a string if the target type is a java.lang.string, and we're not
            // converting from a Null.
            if (!result.l && !strcmp(javaClassName, "java.lang.String")) {
#ifdef CONVERT_NULL_TO_EMPTY_STRING
                if (type == NPVariantType_Null) {
                    jchar buf[2];
                    jobject javaString = env->functions->NewString(env, buf, 0);
                    result.l = javaString;
                } else
#else
                if (type == NPVariantType_String)
#endif
                {
                    NPString src = NPVARIANT_TO_STRING(value);
                    javaString = env->NewStringUTF(src.UTF8Characters);
                    result.l = javaString;
                } else if (type == NPVariantType_Int32) {
                    jint src = NPVARIANT_TO_INT32(value);
                    jclass integerClass = env->FindClass("java/lang/Integer");
                    jmethodID toString = env->GetStaticMethodID(integerClass, "toString", "(I)Ljava/lang/String;");
                    javaString = env->CallStaticObjectMethod(integerClass, toString, src);
                    result.l = javaString;
                    env->DeleteLocalRef(integerClass);
                } else if (type == NPVariantType_Bool) {
                    jboolean src = NPVARIANT_TO_BOOLEAN(value);
                    jclass booleanClass = env->FindClass("java/lang/Boolean");
                    jmethodID toString = env->GetStaticMethodID(booleanClass, "toString", "(Z)Ljava/lang/String;");
                    javaString = env->CallStaticObjectMethod(booleanClass, toString, src);
                    result.l = javaString;
                    env->DeleteLocalRef(booleanClass);
                } else if (type == NPVariantType_Double) {
                    jdouble src = NPVARIANT_TO_DOUBLE(value);
                    jclass doubleClass = env->FindClass("java/lang/Double");
                    jmethodID toString = env->GetStaticMethodID(doubleClass, "toString", "(D)Ljava/lang/String;");
                    javaString = env->CallStaticObjectMethod(doubleClass, toString, src);
                    result.l = javaString;
                    env->DeleteLocalRef(doubleClass);
                }
#ifdef EMULATE_JSC_BINDINGS
                // For the undefined value, JSC sends the String "undefined". Feels to me like we
                // should send null in this case.
                else if (!NPVARIANT_IS_NULL(value)) {
                  javaString = env->NewStringUTF("undefined");
                  result.l = javaString;
                }
#endif
            } else if (!result.l)
                memset(&result, 0, sizeof(jvalue)); // Handle it the same as a void case
        }
        break;

    case boolean_type:
        {
            if (type == NPVariantType_Bool)
                result.z = NPVARIANT_TO_BOOLEAN(value);
            else
                memset(&result, 0, sizeof(jvalue)); // as void case
        }
        break;

    case byte_type:
        {
            if (type == NPVariantType_Int32)
                result.b = static_cast<char>(NPVARIANT_TO_INT32(value));
            else
                memset(&result, 0, sizeof(jvalue));
        }
        break;

    case char_type:
        {
            if (type == NPVariantType_Int32)
                result.c = static_cast<char>(NPVARIANT_TO_INT32(value));
#ifndef EMULATE_JSC_BINDINGS
            // There is no char type in JavaScript - just strings 1 character
            // long. So just converting it to an int above doesn't work. Again,
            // we emulate the behavior for now for maximum compatability.
            else if (type == NPVariantType_String) {
                NPString str = NPVARIANT_TO_STRING(value);
                result.c = str.UTF8Characters[0];
            }
#endif
            else
                memset(&result, 0, sizeof(jvalue));
        }
        break;

    case short_type:
        {
            if (type == NPVariantType_Int32)
                result.s = static_cast<jshort>(NPVARIANT_TO_INT32(value));
            else
                memset(&result, 0, sizeof(jvalue));
        }
        break;

    case int_type:
        {
            if (type == NPVariantType_Int32)
                result.i = static_cast<jint>(NPVARIANT_TO_INT32(value));
            else
                memset(&result, 0, sizeof(jvalue));
        }
        break;

    case long_type:
        {
            if (type == NPVariantType_Int32)
                result.j = static_cast<jlong>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.j = static_cast<jlong>(NPVARIANT_TO_DOUBLE(value));
            else
                memset(&result, 0, sizeof(jvalue));
        }
        break;

    case float_type:
        {
            if (type == NPVariantType_Int32)
                result.f = static_cast<jfloat>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.f = static_cast<jfloat>(NPVARIANT_TO_DOUBLE(value));
            else
                memset(&result, 0, sizeof(jvalue));
        }
        break;

    case double_type:
        {
            if (type == NPVariantType_Int32)
                result.d = static_cast<jdouble>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.d = static_cast<jdouble>(NPVARIANT_TO_DOUBLE(value));
            else
                memset(&result, 0, sizeof(jvalue));
        }
        break;

    case invalid_type:
    default:
    case void_type:
        {
            memset(&result, 0, sizeof(jvalue));
        }
        break;
    }
    return result;
}


void convertJValueToNPVariant(jvalue value, JNIType jniType, const char* javaTypeName, NPVariant* result)
{
    switch (jniType) {
    case void_type:
        {
            VOID_TO_NPVARIANT(*result);
        }
        break;

    case object_type:
        {
            if (value.l) {
                if (!strcmp(javaTypeName, "java.lang.String")) {
                    const char* v = getCharactersFromJString(static_cast<jstring>(value.l));
                    // s is freed in NPN_ReleaseVariantValue (see npruntime.cpp)
                    const char* s = strdup(v);
                    releaseCharactersForJString(static_cast<jstring>(value.l), v);
                    STRINGZ_TO_NPVARIANT(s, *result);
                } else
                    OBJECT_TO_NPVARIANT(JavaInstanceToNPObject(new JavaInstance(value.l)), *result);
            } else
                VOID_TO_NPVARIANT(*result);
        }
        break;

    case boolean_type:
        {
            BOOLEAN_TO_NPVARIANT(value.z, *result);
        }
        break;

    case byte_type:
        {
            INT32_TO_NPVARIANT(value.b, *result);
        }
        break;

    case char_type:
        {
#ifndef EMULATE_JSC_BINDINGS
            // There is no char type in JavaScript - just strings 1 character
            // long. So just converting it to an int above doesn't work. Again,
            // we emulate the behavior for now for maximum compatability.
            if (!strcmp(javaTypeName, "char")) {
                const char c = value.c;
                const char* v = strndup(&c, 1);
                STRINGZ_TO_NPVARIANT(v, *result);
            } else
#endif
                INT32_TO_NPVARIANT(value.c, *result);
        }
        break;

    case short_type:
        {
            INT32_TO_NPVARIANT(value.s, *result);
        }
        break;

    case int_type:
        {
            INT32_TO_NPVARIANT(value.i, *result);
        }
        break;

        // TODO: Check if cast to double is needed.
    case long_type:
        {
            DOUBLE_TO_NPVARIANT(value.j, *result);
        }
        break;

    case float_type:
        {
            DOUBLE_TO_NPVARIANT(value.f, *result);
        }
        break;

    case double_type:
        {
            DOUBLE_TO_NPVARIANT(value.d, *result);
        }
        break;

    case invalid_type:
    default:
        {
            VOID_TO_NPVARIANT(*result);
        }
        break;
    }
}

} // end of namespace Bindings

} // end of namespace JSC
