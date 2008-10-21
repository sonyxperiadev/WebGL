/* libs/WebKitLib/WebKit/WebCore/rendering/RenderSkinAndroid.cpp
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#include "config.h"
#include "RenderSkinAndroid.h"
#include "RenderSkinButton.h"
#include "RenderSkinCombo.h"
#include "RenderSkinRadio.h"
#include "SkImageDecoder.h"

#define LOG_TAG "WebCore"
#undef LOG
#include "utils/Log.h"
#include "utils/AssetManager.h"
#include "utils/Asset.h"

namespace WebCore {
 
RenderSkinAndroid::RenderSkinAndroid()
    : m_height(0)
    , m_width(0)
{}

void RenderSkinAndroid::Init(android::AssetManager* am)
{
    RenderSkinButton::Init(am);
    RenderSkinCombo::Init(am);
    RenderSkinRadio::Init(am);
}

bool RenderSkinAndroid::DecodeBitmap(android::AssetManager* am, const char* fileName, SkBitmap* bitmap)
{
    android::Asset* asset = am->open(fileName, android::Asset::ACCESS_BUFFER);
    if (!asset) {
        asset = am->openNonAsset(fileName, android::Asset::ACCESS_BUFFER);
        if (!asset) {
            LOGD("RenderSkinAndroid: File \"%s\" not found.\n", fileName);
            return false;
        }
    }
    
    bool success = SkImageDecoder::DecodeMemory(asset->getBuffer(false), asset->getLength(), bitmap);
    if (!success) {
        LOGD("RenderSkinAndroid: Failed to decode %s\n", fileName);
    }

    delete asset;
    return success;
}

} // namespace WebCore
