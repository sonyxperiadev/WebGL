/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "config.h"

#include "RenderSkinNinePatch.h"
#include "NinePatchPeeker.h"
#include "SkCanvas.h"
#include "SkImageDecoder.h"
#include "SkNinePatch.h"
#include "SkRect.h"
#include "SkStream.h"
#include "SkTemplates.h"
#include <utils/Asset.h>
#include <utils/AssetManager.h>
#include <utils/Log.h>
#include <utils/ResourceTypes.h>

class SkPaint;
class SkRegion;

using namespace android;

extern void NinePatch_Draw(SkCanvas* canvas, const SkRect& bounds,
        const SkBitmap& bitmap, const Res_png_9patch& chunk,
        const SkPaint* paint, SkRegion** outRegion);

bool RenderSkinNinePatch::decodeAsset(AssetManager* am, const char* filename, NinePatch* ninepatch) {
    Asset* asset = am->open(filename, android::Asset::ACCESS_BUFFER);
    if (!asset) {
        asset = am->openNonAsset(filename, android::Asset::ACCESS_BUFFER);
        if (!asset) {
            return false;
        }
    }

    SkImageDecoder::Mode mode = SkImageDecoder::kDecodePixels_Mode;
    SkBitmap::Config prefConfig = SkBitmap::kNo_Config;
    SkMemoryStream stream(asset->getBuffer(false), asset->getLength());
    SkImageDecoder* decoder = SkImageDecoder::Factory(&stream);
    if (!decoder) {
        asset->close();
        LOGE("RenderSkinNinePatch::Failed to create an image decoder");
        return false;
    }

    decoder->setSampleSize(1);
    decoder->setDitherImage(true);
    decoder->setPreferQualityOverSpeed(false);

    NinePatchPeeker peeker(decoder);

    SkAutoTDelete<SkImageDecoder> add(decoder);

    decoder->setPeeker(&peeker);
    if (!decoder->decode(&stream, &ninepatch->m_bitmap, prefConfig, mode, true)) {
        asset->close();
        LOGE("RenderSkinNinePatch::Failed to decode nine patch asset");
        return false;
    }

    asset->close();
    if (!peeker.fPatchIsValid) {
        LOGE("RenderSkinNinePatch::Patch data not valid");
        return false;
    }
    void** data = &ninepatch->m_serializedPatchData;
    *data = malloc(peeker.fPatch->serializedSize());
    peeker.fPatch->serialize(*data);
    return true;
}

void RenderSkinNinePatch::DrawNinePatch(SkCanvas* canvas, const SkRect& bounds,
                                        const NinePatch& patch) {
    Res_png_9patch* data = Res_png_9patch::deserialize(patch.m_serializedPatchData);

    // if the NinePatch is bigger than the destination on a given axis the default
    // decoder will not stretch properly, therefore we fall back to skia's decoder
    // which if needed will down-sample and draw the bitmap as best as possible.
    if (patch.m_bitmap.width() >= bounds.width() || patch.m_bitmap.height() >= bounds.height()) {

        SkPaint defaultPaint;
        // matches default dither in NinePatchDrawable.java.
        defaultPaint.setDither(true);
        SkNinePatch::DrawMesh(canvas, bounds, patch.m_bitmap,
                              data->xDivs, data->numXDivs,
                              data->yDivs, data->numYDivs,
                              &defaultPaint);
    } else {
        NinePatch_Draw(canvas, bounds, patch.m_bitmap, *data, 0, 0);
    }
}
