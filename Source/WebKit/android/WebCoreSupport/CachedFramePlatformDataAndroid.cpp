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

#include "config.h"

#include "CachedFramePlatformDataAndroid.h"
#include "Settings.h"

namespace android {

CachedFramePlatformDataAndroid::CachedFramePlatformDataAndroid(WebCore::Settings* settings)
{
#ifdef ANDROID_META_SUPPORT
    m_viewport_width = settings->viewportWidth();
    m_viewport_height = settings->viewportHeight();
    m_viewport_initial_scale = settings->viewportInitialScale();
    m_viewport_minimum_scale = settings->viewportMinimumScale();
    m_viewport_maximum_scale = settings->viewportMaximumScale();
    m_viewport_target_densitydpi = settings->viewportTargetDensityDpi();
    m_viewport_user_scalable = settings->viewportUserScalable();
    m_format_detection_address = settings->formatDetectionAddress();
    m_format_detection_email = settings->formatDetectionEmail();
    m_format_detection_telephone = settings->formatDetectionTelephone();
#endif

}

#ifdef ANDROID_META_SUPPORT
void CachedFramePlatformDataAndroid::restoreMetadata(WebCore::Settings* settings)
{
    settings->setViewportWidth(m_viewport_width);
    settings->setViewportHeight(m_viewport_height);
    settings->setViewportInitialScale(m_viewport_initial_scale);
    settings->setViewportMinimumScale(m_viewport_minimum_scale);
    settings->setViewportMaximumScale(m_viewport_maximum_scale);
    settings->setViewportTargetDensityDpi(m_viewport_target_densitydpi);
    settings->setViewportUserScalable(m_viewport_user_scalable);
    settings->setFormatDetectionAddress(m_format_detection_address);
    settings->setFormatDetectionEmail(m_format_detection_email);
    settings->setFormatDetectionTelephone(m_format_detection_telephone);
}
#endif

}
