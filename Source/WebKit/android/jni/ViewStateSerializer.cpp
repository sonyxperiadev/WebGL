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
#include "ImagesManager.h"
#include "Layer.h"
#include "LayerAndroid.h"
#include "PictureSet.h"
#include "ScrollableLayerAndroid.h"
#include "SkPicture.h"
#include "TilesManager.h"

#include <JNIUtility.h>
#include <JNIHelp.h>
#include <jni.h>

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "ViewStateSerializer", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace android {

enum LayerTypes {
    LTNone = 0,
    LTLayerAndroid = 1,
    LTScrollableLayerAndroid = 2,
};

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
    int childCount = baseLayer->countChildren();
    XLOG("BaseLayer has %d child(ren)", childCount);
    stream->write32(childCount);
    for (int i = 0; i < childCount; i++) {
        LayerAndroid* layer = static_cast<LayerAndroid*>(baseLayer->getChild(i));
        serializeLayer(layer, stream);
    }
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
    layer->setContent(picture);
    SkSafeUnref(picture);
    int childCount = stream->readS32();
    for (int i = 0; i < childCount; i++) {
        LayerAndroid* childLayer = deserializeLayer(stream);
        if (childLayer)
            layer->addChild(childLayer);
    }
    delete stream;
    return layer;
}

// Serialization helpers

void writeMatrix(SkWStream *stream, const SkMatrix& matrix)
{
    for (int i = 0; i < 9; i++)
        stream->writeScalar(matrix[i]);
}

SkMatrix readMatrix(SkStream *stream)
{
    SkMatrix matrix;
    for (int i = 0; i < 9; i++)
        matrix.set(i, stream->readScalar());
    return matrix;
}

void writeSkLength(SkWStream *stream, SkLength length)
{
    stream->write32(length.type);
    stream->writeScalar(length.value);
}

SkLength readSkLength(SkStream *stream)
{
    SkLength len;
    len.type = (SkLength::SkLengthType) stream->readU32();
    len.value = stream->readScalar();
    return len;
}

void writeSkRect(SkWStream *stream, SkRect rect)
{
    stream->writeScalar(rect.fLeft);
    stream->writeScalar(rect.fTop);
    stream->writeScalar(rect.fRight);
    stream->writeScalar(rect.fBottom);
}

SkRect readSkRect(SkStream *stream)
{
    SkRect rect;
    rect.fLeft = stream->readScalar();
    rect.fTop = stream->readScalar();
    rect.fRight = stream->readScalar();
    rect.fBottom = stream->readScalar();
    return rect;
}

void writeTransformationMatrix(SkWStream *stream, TransformationMatrix& matrix)
{
    double value;
    int dsize = sizeof(double);
    value = matrix.m11();
    stream->write(&value, dsize);
    value = matrix.m12();
    stream->write(&value, dsize);
    value = matrix.m13();
    stream->write(&value, dsize);
    value = matrix.m14();
    stream->write(&value, dsize);
    value = matrix.m21();
    stream->write(&value, dsize);
    value = matrix.m22();
    stream->write(&value, dsize);
    value = matrix.m23();
    stream->write(&value, dsize);
    value = matrix.m24();
    stream->write(&value, dsize);
    value = matrix.m31();
    stream->write(&value, dsize);
    value = matrix.m32();
    stream->write(&value, dsize);
    value = matrix.m33();
    stream->write(&value, dsize);
    value = matrix.m34();
    stream->write(&value, dsize);
    value = matrix.m41();
    stream->write(&value, dsize);
    value = matrix.m42();
    stream->write(&value, dsize);
    value = matrix.m43();
    stream->write(&value, dsize);
    value = matrix.m44();
    stream->write(&value, dsize);
}

void readTransformationMatrix(SkStream *stream, TransformationMatrix& matrix)
{
    double value;
    int dsize = sizeof(double);
    stream->read(&value, dsize);
    matrix.setM11(value);
    stream->read(&value, dsize);
    matrix.setM12(value);
    stream->read(&value, dsize);
    matrix.setM13(value);
    stream->read(&value, dsize);
    matrix.setM14(value);
    stream->read(&value, dsize);
    matrix.setM21(value);
    stream->read(&value, dsize);
    matrix.setM22(value);
    stream->read(&value, dsize);
    matrix.setM23(value);
    stream->read(&value, dsize);
    matrix.setM24(value);
    stream->read(&value, dsize);
    matrix.setM31(value);
    stream->read(&value, dsize);
    matrix.setM32(value);
    stream->read(&value, dsize);
    matrix.setM33(value);
    stream->read(&value, dsize);
    matrix.setM34(value);
    stream->read(&value, dsize);
    matrix.setM41(value);
    stream->read(&value, dsize);
    matrix.setM42(value);
    stream->read(&value, dsize);
    matrix.setM43(value);
    stream->read(&value, dsize);
    matrix.setM44(value);
}

void serializeLayer(LayerAndroid* layer, SkWStream* stream)
{
    if (!layer) {
        XLOG("NULL layer!");
        stream->write8(LTNone);
        return;
    }
    if (layer->isMedia() || layer->isVideo()) {
        XLOG("Layer isn't supported for serialization: isMedia: %s, isVideo: %s",
             layer->isMedia() ? "true" : "false",
             layer->isVideo() ? "true" : "false");
        stream->write8(LTNone);
        return;
    }
    LayerTypes type = layer->contentIsScrollable()
            ? LTScrollableLayerAndroid
            : LTLayerAndroid;
    stream->write8(type);

    // Start with Layer fields
    stream->writeBool(layer->shouldInheritFromRootTransform());
    stream->writeScalar(layer->getOpacity());
    stream->writeScalar(layer->getSize().width());
    stream->writeScalar(layer->getSize().height());
    stream->writeScalar(layer->getPosition().x());
    stream->writeScalar(layer->getPosition().y());
    stream->writeScalar(layer->getAnchorPoint().x());
    stream->writeScalar(layer->getAnchorPoint().y());
    writeMatrix(stream, layer->getMatrix());
    writeMatrix(stream, layer->getChildrenMatrix());

    // Next up, LayerAndroid fields
    stream->writeBool(layer->m_haveClip);
    stream->writeBool(layer->m_isFixed);
    stream->writeBool(layer->m_backgroundColorSet);
    stream->writeBool(layer->m_isIframe);
    writeSkLength(stream, layer->m_fixedLeft);
    writeSkLength(stream, layer->m_fixedTop);
    writeSkLength(stream, layer->m_fixedRight);
    writeSkLength(stream, layer->m_fixedBottom);
    writeSkLength(stream, layer->m_fixedMarginLeft);
    writeSkLength(stream, layer->m_fixedMarginTop);
    writeSkLength(stream, layer->m_fixedMarginRight);
    writeSkLength(stream, layer->m_fixedMarginBottom);
    writeSkRect(stream, layer->m_fixedRect);
    stream->write32(layer->m_renderLayerPos.x());
    stream->write32(layer->m_renderLayerPos.y());
    stream->writeBool(layer->m_backfaceVisibility);
    stream->writeBool(layer->m_visible);
    stream->write32(layer->m_backgroundColor);
    stream->writeBool(layer->m_preserves3D);
    stream->writeScalar(layer->m_anchorPointZ);
    stream->writeScalar(layer->m_drawOpacity);
    bool hasContentsImage = layer->m_imageCRC != 0;
    stream->writeBool(hasContentsImage);
    if (hasContentsImage) {
        SkFlattenableWriteBuffer buffer(1024);
        buffer.setFlags(SkFlattenableWriteBuffer::kCrossProcess_Flag);
        ImageTexture* imagetexture =
                ImagesManager::instance()->retainImage(layer->m_imageCRC);
        if (imagetexture && imagetexture->bitmap())
            imagetexture->bitmap()->flatten(buffer);
        ImagesManager::instance()->releaseImage(layer->m_imageCRC);
        stream->write32(buffer.size());
        buffer.writeToStream(stream);
    }
    bool hasRecordingPicture = layer->m_recordingPicture != 0;
    stream->writeBool(hasRecordingPicture);
    if (hasRecordingPicture)
        layer->m_recordingPicture->serialize(stream);
    // TODO: support m_animations (maybe?)
    stream->write32(0); // placeholder for m_animations.size();
    writeTransformationMatrix(stream, layer->m_transform);
    writeTransformationMatrix(stream, layer->m_childrenTransform);
    if (type == LTScrollableLayerAndroid) {
        ScrollableLayerAndroid* scrollableLayer =
                static_cast<ScrollableLayerAndroid*>(layer);
        stream->writeScalar(scrollableLayer->m_scrollLimits.fLeft);
        stream->writeScalar(scrollableLayer->m_scrollLimits.fTop);
        stream->writeScalar(scrollableLayer->m_scrollLimits.width());
        stream->writeScalar(scrollableLayer->m_scrollLimits.height());
    }
    int childCount = layer->countChildren();
    stream->write32(childCount);
    for (int i = 0; i < childCount; i++)
        serializeLayer(layer->getChild(i), stream);
}

LayerAndroid* deserializeLayer(SkStream* stream)
{
    int type = stream->readU8();
    if (type == LTNone)
        return 0;
    // Cast is to disambiguate between ctors.
    LayerAndroid *layer;
    if (type == LTLayerAndroid)
        layer = new LayerAndroid((RenderLayer*) 0);
    else if (type == LTScrollableLayerAndroid)
        layer = new ScrollableLayerAndroid((RenderLayer*) 0);
    else {
        XLOG("Unexpected layer type: %d, aborting!", type);
        return 0;
    }

    // Layer fields
    layer->setShouldInheritFromRootTransform(stream->readBool());
    layer->setOpacity(stream->readScalar());
    layer->setSize(stream->readScalar(), stream->readScalar());
    layer->setPosition(stream->readScalar(), stream->readScalar());
    layer->setAnchorPoint(stream->readScalar(), stream->readScalar());
    layer->setMatrix(readMatrix(stream));
    layer->setChildrenMatrix(readMatrix(stream));

    // LayerAndroid fields
    layer->m_haveClip = stream->readBool();
    layer->m_isFixed = stream->readBool();
    layer->m_backgroundColorSet = stream->readBool();
    layer->m_isIframe = stream->readBool();
    layer->m_fixedLeft = readSkLength(stream);
    layer->m_fixedTop = readSkLength(stream);
    layer->m_fixedRight = readSkLength(stream);
    layer->m_fixedBottom = readSkLength(stream);
    layer->m_fixedMarginLeft = readSkLength(stream);
    layer->m_fixedMarginTop = readSkLength(stream);
    layer->m_fixedMarginRight = readSkLength(stream);
    layer->m_fixedMarginBottom = readSkLength(stream);
    layer->m_fixedRect = readSkRect(stream);
    layer->m_renderLayerPos.setX(stream->readS32());
    layer->m_renderLayerPos.setY(stream->readS32());
    layer->m_backfaceVisibility = stream->readBool();
    layer->m_visible = stream->readBool();
    layer->m_backgroundColor = stream->readU32();
    layer->m_preserves3D = stream->readBool();
    layer->m_anchorPointZ = stream->readScalar();
    layer->m_drawOpacity = stream->readScalar();
    bool hasContentsImage = stream->readBool();
    if (hasContentsImage) {
        int size = stream->readU32();
        SkAutoMalloc storage(size);
        stream->read(storage.get(), size);
        SkFlattenableReadBuffer buffer(storage.get(), size);
        SkBitmap contentsImage;
        contentsImage.unflatten(buffer);
        SkBitmapRef* imageRef = new SkBitmapRef(contentsImage);
        layer->setContentsImage(imageRef);
        delete imageRef;
    }
    bool hasRecordingPicture = stream->readBool();
    if (hasRecordingPicture) {
        layer->m_recordingPicture = new SkPicture(stream);
    }
    int animationCount = stream->readU32(); // TODO: Support (maybe?)
    readTransformationMatrix(stream, layer->m_transform);
    readTransformationMatrix(stream, layer->m_childrenTransform);
    if (type == LTScrollableLayerAndroid) {
        ScrollableLayerAndroid* scrollableLayer =
                static_cast<ScrollableLayerAndroid*>(layer);
        scrollableLayer->m_scrollLimits.set(
                stream->readScalar(),
                stream->readScalar(),
                stream->readScalar(),
                stream->readScalar());
    }
    int childCount = stream->readU32();
    for (int i = 0; i < childCount; i++) {
        LayerAndroid *childLayer = deserializeLayer(stream);
        if (childLayer)
            layer->addChild(childLayer);
    }
    layer->needsRepaint();
    XLOG("Created layer with id %d", layer->uniqueId());
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
