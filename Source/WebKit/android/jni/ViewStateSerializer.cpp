/*
 * Copyright 2011, The Android Open Source Project
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

#include "BaseLayerAndroid.h"
#include "CreateJavaOutputStreamAdaptor.h"
#include "PictureSet.h"
#include "SkPicture.h"

#include <JNIUtility.h>
#include <JNIHelp.h>
#include <jni.h>

namespace android {

static bool nativeSerializeViewState(JNIEnv* env, jobject, jint jbaseLayer,
                                     jobject jstream, jbyteArray jstorage)
{
    BaseLayerAndroid* baseLayer = (BaseLayerAndroid*) jbaseLayer;
    if (!baseLayer)
        return false;

    SkWStream *stream = CreateJavaOutputStreamAdaptor(env, jstream, jstorage);
#if USE(ACCELERATED_COMPOSITING)
    stream->write32(baseLayer->getBackgroundColor().rgb());
#else
    stream->write32(0);
#endif
    SkPicture picture;
    PictureSet* content = baseLayer->content();
    baseLayer->drawCanvas(picture.beginRecording(content->width(), content->height(),
            SkPicture::kUsePathBoundsForClip_RecordingFlag));
    picture.endRecording();
    if (!stream)
        return false;
    picture.serialize(stream);
    delete stream;
    return true;
}

static BaseLayerAndroid* nativeDeserializeViewState(JNIEnv* env, jobject, jobject jstream,
                                      jbyteArray jstorage)
{
    SkStream* stream = CreateJavaInputStreamAdaptor(env, jstream, jstorage);
    if (!stream)
        return 0;
    BaseLayerAndroid* layer = new BaseLayerAndroid();
    Color color = stream->readU32();
#if USE(ACCELERATED_COMPOSITING)
    layer->setBackgroundColor(color);
#endif
    SkPicture* picture = new SkPicture(stream);
    delete stream;
    layer->setContent(picture);
    return layer;
}

/*
 * JNI registration
 */
static JNINativeMethod gSerializerMethods[] = {
    { "nativeSerializeViewState", "(ILjava/io/OutputStream;[B)Z",
        (void*) nativeSerializeViewState },
    { "nativeDeserializeViewState", "(Ljava/io/InputStream;[B)I",
        (void*) nativeDeserializeViewState },
};

int registerViewStateSerializer(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, "android/webkit/ViewStateSerializer",
                                    gSerializerMethods, NELEM(gSerializerMethods));
}

}
