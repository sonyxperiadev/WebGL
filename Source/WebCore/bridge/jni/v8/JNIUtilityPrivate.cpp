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

#if ENABLE(JAVA_BRIDGE)

#include "JavaInstanceJobjectV8.h"
#include "JavaNPObjectV8.h"
#if PLATFORM(ANDROID)
#include "npruntime_impl.h"
#endif // PLATFORM(ANDROID)
#include "JavaValueV8.h"
#include <wtf/text/CString.h>

namespace JSC {

namespace Bindings {

JavaValue convertNPVariantToJavaValue(NPVariant value, const String& javaClass)
{
    CString javaClassName = javaClass.utf8();
    JavaType javaType = javaTypeFromClassName(javaClassName.data());
    JavaValue result;
    result.m_type = javaType;
    NPVariantType type = value.type;

    switch (javaType) {
    case JavaTypeArray:
#if PLATFORM(ANDROID)
        // If we're converting to an array, see if the NPVariant has a length
        // property. If so, create a JNI array of the relevant length and to get
        // the elements of the NPVariant. When we interpret the JavaValue later,
        // we know that the array is represented as a JNI array.
        //
        // FIXME: This is a hack. We should not be using JNI here. We should
        // represent the JavaValue without JNI.
        {
            JNIEnv* env = getJNIEnv();
            jobject javaArray;
            NPObject* object = NPVARIANT_IS_OBJECT(value) ? NPVARIANT_TO_OBJECT(value) : 0;
            NPVariant npvLength;
            bool success = _NPN_GetProperty(0, object, _NPN_GetStringIdentifier("length"), &npvLength);
            if (!success) {
                // No length property so we don't know how many elements to put into the array.
                // Treat this as an error.
                // JSC sends null for an array that is not an array of strings or basic types,
                // do this also in the unknown length case.
                break;
            }

            jsize length = 0;
            if (NPVARIANT_IS_INT32(npvLength))
                length = static_cast<jsize>(NPVARIANT_TO_INT32(npvLength));
            else if (NPVARIANT_IS_DOUBLE(npvLength))
                length = static_cast<jsize>(NPVARIANT_TO_DOUBLE(npvLength));

            if (!strcmp(javaClassName.data(), "[Ljava.lang.String;")) {
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
            } else if (!strcmp(javaClassName.data(), "[B")) {
                // array of bytes
                javaArray = env->NewByteArray(length);
                // Now iterate over each element and add to the array.
                for (jsize i = 0; i < length; i++) {
                    NPVariant npvValue;
                    _NPN_GetProperty(0, object, _NPN_GetIntIdentifier(i), &npvValue);
                    jbyte bVal = 0;
                    if (NPVARIANT_IS_INT32(npvValue)) {
                        bVal = static_cast<jbyte>(NPVARIANT_TO_INT32(npvValue));
                    } else if (NPVARIANT_IS_DOUBLE(npvValue)) {
                        bVal = static_cast<jbyte>(NPVARIANT_TO_DOUBLE(npvValue));
                    }
                    env->SetByteArrayRegion(static_cast<jbyteArray>(javaArray), i, 1, &bVal);
                }
            } else if (!strcmp(javaClassName.data(), "[C")) {
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
            } else if (!strcmp(javaClassName.data(), "[D")) {
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
            } else if (!strcmp(javaClassName.data(), "[F")) {
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
            } else if (!strcmp(javaClassName.data(), "[I")) {
                // array of ints
                javaArray = env->NewIntArray(length);
                // Now iterate over each element and add to the array.
                for (jsize i = 0; i < length; i++) {
                    NPVariant npvValue;
                    _NPN_GetProperty(0, object, _NPN_GetIntIdentifier(i), &npvValue);
                    jint iVal = 0;
                    if (NPVARIANT_IS_INT32(npvValue)) {
                        iVal = NPVARIANT_TO_INT32(npvValue);
                    } else if (NPVARIANT_IS_DOUBLE(npvValue)) {
                        iVal = static_cast<jint>(NPVARIANT_TO_DOUBLE(npvValue));
                    }
                    env->SetIntArrayRegion(static_cast<jintArray>(javaArray), i, 1, &iVal);
                }
            } else if (!strcmp(javaClassName.data(), "[J")) {
                // array of longs
                javaArray = env->NewLongArray(length);
                // Now iterate over each element and add to the array.
                for (jsize i = 0; i < length; i++) {
                    NPVariant npvValue;
                    _NPN_GetProperty(0, object, _NPN_GetIntIdentifier(i), &npvValue);
                    jlong jVal = 0;
                    if (NPVARIANT_IS_INT32(npvValue)) {
                        jVal = static_cast<jlong>(NPVARIANT_TO_INT32(npvValue));
                    } else if (NPVARIANT_IS_DOUBLE(npvValue)) {
                        jVal = static_cast<jlong>(NPVARIANT_TO_DOUBLE(npvValue));
                    }
                    env->SetLongArrayRegion(static_cast<jlongArray>(javaArray), i, 1, &jVal);
                }
            } else if (!strcmp(javaClassName.data(), "[S")) {
                // array of shorts
                javaArray = env->NewShortArray(length);
                // Now iterate over each element and add to the array.
                for (jsize i = 0; i < length; i++) {
                    NPVariant npvValue;
                    _NPN_GetProperty(0, object, _NPN_GetIntIdentifier(i), &npvValue);
                    jshort sVal = 0;
                    if (NPVARIANT_IS_INT32(npvValue)) {
                        sVal = static_cast<jshort>(NPVARIANT_TO_INT32(npvValue));
                    } else if (NPVARIANT_IS_DOUBLE(npvValue)) {
                        sVal = static_cast<jshort>(NPVARIANT_TO_DOUBLE(npvValue));
                    }
                    env->SetShortArrayRegion(static_cast<jshortArray>(javaArray), i, 1, &sVal);
                }
            } else if (!strcmp(javaClassName.data(), "[Z")) {
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
                // JSC sends null for an array that is not an array of strings or basic types.
                break;
            }

            result.m_objectValue = adoptRef(new JavaInstanceJobject(javaArray));
            env->DeleteLocalRef(javaArray);
        }
        break;
#endif // PLATFORM(ANDROID)

    case JavaTypeObject:
        {
            // See if we have a Java instance.
            if (type == NPVariantType_Object) {
                NPObject* objectImp = NPVARIANT_TO_OBJECT(value);
                result.m_objectValue = ExtractJavaInstance(objectImp);
            }
        }
        break;

    case JavaTypeString:
        {
#ifdef CONVERT_NULL_TO_EMPTY_STRING
            if (type == NPVariantType_Null) {
                result.m_type = JavaTypeString;
                result.m_stringValue = String::fromUTF8("");
            } else
#else
            if (type == NPVariantType_String)
#endif
            {
                NPString src = NPVARIANT_TO_STRING(value);
                result.m_type = JavaTypeString;
                result.m_stringValue = String::fromUTF8(src.UTF8Characters);
            }
#if PLATFORM(ANDROID)
            else if (type == NPVariantType_Int32) {
                result.m_type = JavaTypeString;
                result.m_stringValue = String::number(NPVARIANT_TO_INT32(value));
            } else if (type == NPVariantType_Bool) {
                result.m_type = JavaTypeString;
                result.m_stringValue = NPVARIANT_TO_BOOLEAN(value) ? "true" : "false";
            } else if (type == NPVariantType_Double) {
                result.m_type = JavaTypeString;
                result.m_stringValue = String::number(NPVARIANT_TO_DOUBLE(value));
            } else if (!NPVARIANT_IS_NULL(value)) {
                result.m_type = JavaTypeString;
                result.m_stringValue = "undefined";
            }
#endif // PLATFORM(ANDROID)
        }
        break;

    case JavaTypeBoolean:
        {
            if (type == NPVariantType_Bool)
                result.m_booleanValue = NPVARIANT_TO_BOOLEAN(value);
        }
        break;

    case JavaTypeByte:
        {
            if (type == NPVariantType_Int32)
                result.m_byteValue = static_cast<signed char>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.m_byteValue = static_cast<signed char>(NPVARIANT_TO_DOUBLE(value));
        }
        break;

    case JavaTypeChar:
        {
            if (type == NPVariantType_Int32)
                result.m_charValue = static_cast<unsigned short>(NPVARIANT_TO_INT32(value));
        }
        break;

    case JavaTypeShort:
        {
            if (type == NPVariantType_Int32)
                result.m_shortValue = static_cast<short>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.m_shortValue = static_cast<short>(NPVARIANT_TO_DOUBLE(value));
        }
        break;

    case JavaTypeInt:
        {
            if (type == NPVariantType_Int32)
                result.m_intValue = static_cast<int>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.m_intValue = static_cast<int>(NPVARIANT_TO_DOUBLE(value));
        }
        break;

    case JavaTypeLong:
        {
            if (type == NPVariantType_Int32)
                result.m_longValue = static_cast<long long>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.m_longValue = static_cast<long long>(NPVARIANT_TO_DOUBLE(value));
        }
        break;

    case JavaTypeFloat:
        {
            if (type == NPVariantType_Int32)
                result.m_floatValue = static_cast<float>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.m_floatValue = static_cast<float>(NPVARIANT_TO_DOUBLE(value));
        }
        break;

    case JavaTypeDouble:
        {
            if (type == NPVariantType_Int32)
                result.m_doubleValue = static_cast<double>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.m_doubleValue = static_cast<double>(NPVARIANT_TO_DOUBLE(value));
        }
        break;
    default:
        break;
    }
    return result;
}


void convertJavaValueToNPVariant(JavaValue value, NPVariant* result)
{
    switch (value.m_type) {
    case JavaTypeVoid:
        {
            VOID_TO_NPVARIANT(*result);
        }
        break;

    case JavaTypeObject:
        {
            // If the JavaValue is a String object, it should have type JavaTypeString.
            if (value.m_objectValue)
                OBJECT_TO_NPVARIANT(JavaInstanceToNPObject(value.m_objectValue.get()), *result);
            else
                VOID_TO_NPVARIANT(*result);
        }
        break;

    case JavaTypeString:
        {
#if PLATFORM(ANDROID)
            // This entire file will likely be removed usptream soon.
            if (value.m_stringValue.isNull()) {
                VOID_TO_NPVARIANT(*result);
                break;
            }
#endif
            const char* utf8String = strdup(value.m_stringValue.utf8().data());
            // The copied string is freed in NPN_ReleaseVariantValue (see npruntime.cpp)
            STRINGZ_TO_NPVARIANT(utf8String, *result);
        }
        break;

    case JavaTypeBoolean:
        {
            BOOLEAN_TO_NPVARIANT(value.m_booleanValue, *result);
        }
        break;

    case JavaTypeByte:
        {
            INT32_TO_NPVARIANT(value.m_byteValue, *result);
        }
        break;

    case JavaTypeChar:
        {
            INT32_TO_NPVARIANT(value.m_charValue, *result);
        }
        break;

    case JavaTypeShort:
        {
            INT32_TO_NPVARIANT(value.m_shortValue, *result);
        }
        break;

    case JavaTypeInt:
        {
            INT32_TO_NPVARIANT(value.m_intValue, *result);
        }
        break;

        // TODO: Check if cast to double is needed.
    case JavaTypeLong:
        {
            DOUBLE_TO_NPVARIANT(value.m_longValue, *result);
        }
        break;

    case JavaTypeFloat:
        {
            DOUBLE_TO_NPVARIANT(value.m_floatValue, *result);
        }
        break;

    case JavaTypeDouble:
        {
            DOUBLE_TO_NPVARIANT(value.m_doubleValue, *result);
        }
        break;

    case JavaTypeInvalid:
    default:
        {
            VOID_TO_NPVARIANT(*result);
        }
        break;
    }
}

JavaValue jvalueToJavaValue(const jvalue& value, const JavaType& type)
{
    JavaValue result;
    result.m_type = type;
    switch (result.m_type) {
    case JavaTypeVoid:
        break;
    case JavaTypeObject:
        result.m_objectValue = new JavaInstanceJobject(value.l);
        break;
    case JavaTypeString:
        {
            jstring javaString = static_cast<jstring>(value.l);
            if (!javaString) {
                // result.m_stringValue is null by default
                break;
            }
            const UChar* characters = getUCharactersFromJStringInEnv(getJNIEnv(), javaString);
            // We take a copy to allow the Java String to be released.
            result.m_stringValue = String(characters, getJNIEnv()->GetStringLength(javaString));
            releaseUCharactersForJStringInEnv(getJNIEnv(), javaString, characters);
        }
        break;
    case JavaTypeBoolean:
        result.m_booleanValue = value.z == JNI_FALSE ? false : true;
        break;
    case JavaTypeByte:
        result.m_byteValue = value.b;
        break;
    case JavaTypeChar:
        result.m_charValue = value.c;
        break;
    case JavaTypeShort:
        result.m_shortValue = value.s;
        break;
    case JavaTypeInt:
        result.m_intValue = value.i;
        break;
    case JavaTypeLong:
        result.m_longValue = value.j;
        break;
    case JavaTypeFloat:
        result.m_floatValue = value.f;
        break;
    case JavaTypeDouble:
        result.m_doubleValue = value.d;
        break;
    default:
        ASSERT(false);
    }
    return result;
}

jvalue javaValueToJvalue(const JavaValue& value)
{
    jvalue result;
    memset(&result, 0, sizeof(jvalue));
    switch (value.m_type) {
    case JavaTypeVoid:
        break;
#if PLATFORM(ANDROID)
    case JavaTypeArray:
#endif
    case JavaTypeObject:
        if (value.m_objectValue) {
            // This method is used only by JavaInstanceJobject, so we know the
            // derived type of the object.
            result.l = static_cast<JavaInstanceJobject*>(value.m_objectValue.get())->javaInstance();
        }
        break;
    case JavaTypeString:
        // This creates a local reference to a new String object, which will
        // be released when the call stack returns to Java. Note that this
        // may cause leaks if invoked from a native message loop, as is the
        // case in workers.
        if (value.m_stringValue.isNull()) {
            // result.l is null by default.
            break;
        }
        result.l = getJNIEnv()->NewString(value.m_stringValue.characters(), value.m_stringValue.length());
        break;
    case JavaTypeBoolean:
        result.z = value.m_booleanValue ? JNI_TRUE : JNI_FALSE;
        break;
    case JavaTypeByte:
        result.b = value.m_byteValue;
        break;
    case JavaTypeChar:
        result.c = value.m_charValue;
        break;
    case JavaTypeShort:
        result.s = value.m_shortValue;
        break;
    case JavaTypeInt:
        result.i = value.m_intValue;
        break;
    case JavaTypeLong:
        result.j = value.m_longValue;
        break;
    case JavaTypeFloat:
        result.f = value.m_floatValue;
        break;
    case JavaTypeDouble:
        result.d = value.m_doubleValue;
        break;
    default:
        ASSERT(false);
    }
    return result;
}

} // namespace Bindings

} // namespace JSC

#endif // ENABLE(JAVA_BRIDGE)
