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
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkImageDecoder.h"
#include "SkStream.h"
#include "SkTemplates.h"
#include <utils/Asset.h>
#include <utils/Log.h>
#include <utils/ResourceTypes.h>

bool RenderSkinNinePatch::decodeAsset(android::Asset* asset, SkBitmap* bitmap, void** data) {
    SkImageDecoder::Mode mode = SkImageDecoder::kDecodePixels_Mode;
    SkBitmap::Config prefConfig = SkBitmap::kNo_Config;
    SkStream* stream = new SkMemoryStream(asset->getBuffer(false), asset->getLength());
    SkImageDecoder* decoder = SkImageDecoder::Factory(stream);
    if (!decoder) {
        LOGE("RenderSkinNinePatch::Failed to create an image decoder");
        return false;
    }

    decoder->setSampleSize(1);
    decoder->setDitherImage(true);
    decoder->setPreferQualityOverSpeed(false);

    NinePatchPeeker peeker(decoder);

    SkAutoTDelete<SkImageDecoder> add(decoder);

    decoder->setPeeker(&peeker);
    if (!decoder->decode(stream, bitmap, prefConfig, mode, true)) {
        LOGE("RenderSkinNinePatch::Failed to decode nine patch asset");
        return false;
    }

    if (!peeker.fPatchIsValid) {
        LOGE("RenderSkinNinePatch::Patch data not valid");
        return false;
    }
    *data = malloc(peeker.fPatch->serializedSize());
    peeker.fPatch->serialize(*data);
    return true;
}
