/*
 * Copyright 2010, The Android Open Source Project
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
#include "jni_runtime.h"

#include "JNIUtility.h"

using namespace JSC::Bindings;

JavaParameter::JavaParameter(JNIEnv* env, jstring type)
{
    m_type = JavaString(env, type);
    m_JNIType = JNITypeFromClassName(m_type.UTF8String());
}

JavaField::JavaField(JNIEnv* env, jobject aField)
{
    // Get field type
    jobject fieldType = callJNIMethod<jobject>(aField, "getType", "()Ljava/lang/Class;");
    jstring fieldTypeName = static_cast<jstring>(callJNIMethod<jobject>(fieldType, "getName", "()Ljava/lang/String;"));
    m_type = JavaString(env, fieldTypeName);
    m_JNIType = JNITypeFromClassName(m_type.UTF8String());

    // Get field name
    jstring fieldName = static_cast<jstring>(callJNIMethod<jobject>(aField, "getName", "()Ljava/lang/String;"));
    m_name = JavaString(env, fieldName);

    m_field = new JObjectWrapper(aField);
}

JavaMethod::JavaMethod(JNIEnv* env, jobject aMethod)
{
    // Get return type
    jobject returnType = callJNIMethod<jobject>(aMethod, "getReturnType", "()Ljava/lang/Class;");
    jstring returnTypeName = static_cast<jstring>(callJNIMethod<jobject>(returnType, "getName", "()Ljava/lang/String;"));
    m_returnType = JavaString(env, returnTypeName);
    m_JNIReturnType = JNITypeFromClassName(m_returnType.UTF8String());
    env->DeleteLocalRef(returnType);
    env->DeleteLocalRef(returnTypeName);

    // Get method name
    jstring methodName = static_cast<jstring>(callJNIMethod<jobject>(aMethod, "getName", "()Ljava/lang/String;"));
    m_name = JavaString(env, methodName);
    env->DeleteLocalRef(methodName);

    // Get parameters
    jarray jparameters = static_cast<jarray>(callJNIMethod<jobject>(aMethod, "getParameterTypes", "()[Ljava/lang/Class;"));
    m_numParameters = env->GetArrayLength(jparameters);
    m_parameters = new JavaParameter[m_numParameters];

    for (int i = 0; i < m_numParameters; i++) {
        jobject aParameter = env->GetObjectArrayElement(static_cast<jobjectArray>(jparameters), i);
        jstring parameterName = static_cast<jstring>(callJNIMethod<jobject>(aParameter, "getName", "()Ljava/lang/String;"));
        m_parameters[i] = JavaParameter(env, parameterName);
        env->DeleteLocalRef(aParameter);
        env->DeleteLocalRef(parameterName);
    }
    env->DeleteLocalRef(jparameters);

    // Created lazily.
    m_signature = 0;
    m_methodID = 0;

    jclass modifierClass = env->FindClass("java/lang/reflect/Modifier");
    int modifiers = callJNIMethod<jint>(aMethod, "getModifiers", "()I");
    m_isStatic = static_cast<bool>(callJNIStaticMethod<jboolean>(modifierClass, "isStatic", "(I)Z", modifiers));
    env->DeleteLocalRef(modifierClass);
}

JavaMethod::~JavaMethod()
{
    if (m_signature)
        free(m_signature);
    delete[] m_parameters;
};


class SignatureBuilder {
public:
    explicit SignatureBuilder(int init_size) {
        if (init_size <= 0)
            init_size = 16;
        m_size = init_size;
        m_length = 0;
        m_storage = (char*)malloc(m_size * sizeof(char));
    }

    ~SignatureBuilder() {
        free(m_storage);
    }

    void append(const char* s) {
        int l = strlen(s);
        expandIfNeeded(l);
        memcpy(m_storage + m_length, s, l);
        m_length += l;
    }

    // JNI method signatures use '/' between components of a class name, but
    // we get '.' between components from the reflection API.
    void appendClassName(const char* className) {
       int l = strlen(className);
       expandIfNeeded(l);

       char* sp = m_storage + m_length;
       const char* cp = className;

       while (*cp) {
           if (*cp == '.')
               *sp = '/';
           else
               *sp = *cp;

           cp++;
           sp++;
       }

       m_length += l;
    }

    // make a duplicated copy of the content.
    char* ascii() {
        if (m_length == 0)
            return NULL;
        m_storage[m_length] = '\0';
        return strndup(m_storage, m_length);
    }

private:
    void expandIfNeeded(int l) {
        // expand storage if needed
        if (l + m_length >= m_size) {
            int new_size = 2 * m_size;
            if (l + m_length >= new_size)
                new_size = l + m_length + 1;

            char* new_storage = (char*)malloc(new_size * sizeof(char));
            memcpy(new_storage, m_storage, m_length);
            m_size = new_size;
            free(m_storage);
            m_storage = new_storage;
        }
    }

    int m_size;
    int m_length;
    char* m_storage;
};

const char* JavaMethod::signature() const
{
    if (!m_signature) {
        SignatureBuilder signatureBuilder(64);
        signatureBuilder.append("(");
        for (int i = 0; i < m_numParameters; i++) {
            JavaParameter* aParameter = parameterAt(i);
            JNIType type = aParameter->getJNIType();
            if (type == array_type)
                signatureBuilder.appendClassName(aParameter->type());
            else {
                signatureBuilder.append(signatureFromPrimitiveType(type));
                if (type == object_type) {
                    signatureBuilder.appendClassName(aParameter->type());
                    signatureBuilder.append(";");
                }
            }
        }
        signatureBuilder.append(")");

        const char* returnType = m_returnType.UTF8String();
        if (m_JNIReturnType == array_type)
            signatureBuilder.appendClassName(returnType);
        else {
            signatureBuilder.append(signatureFromPrimitiveType(m_JNIReturnType));
            if (m_JNIReturnType == object_type) {
                signatureBuilder.appendClassName(returnType);
                signatureBuilder.append(";");
            }
        }

        m_signature = signatureBuilder.ascii();
    }

    return m_signature;
}

JNIType JavaMethod::JNIReturnType() const
{
    return m_JNIReturnType;
}

jmethodID JavaMethod::methodID(jobject obj) const
{
    if (!m_methodID)
        m_methodID = getMethodID(obj, m_name.UTF8String(), signature());
    return m_methodID;
}
