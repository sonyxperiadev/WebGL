## 
##
## Copyright 2008, The Android Open Source Project
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##

LOCAL_SRC_FILES := \
	android/WebCoreSupport/CachedFramePlatformDataAndroid.cpp \
	android/WebCoreSupport/ChromeClientAndroid.cpp \
	android/WebCoreSupport/ContextMenuClientAndroid.cpp \
	android/WebCoreSupport/DeviceMotionClientAndroid.cpp \
	android/WebCoreSupport/DeviceOrientationClientAndroid.cpp \
	android/WebCoreSupport/DragClientAndroid.cpp \
	android/WebCoreSupport/EditorClientAndroid.cpp \
	android/WebCoreSupport/FrameLoaderClientAndroid.cpp \
	android/WebCoreSupport/FrameNetworkingContextAndroid.cpp \
	android/WebCoreSupport/GeolocationPermissions.cpp \
	android/WebCoreSupport/MediaPlayerPrivateAndroid.cpp \
	android/WebCoreSupport/MemoryUsage.cpp \
	android/WebCoreSupport/PlatformBridge.cpp \
	android/WebCoreSupport/ResourceLoaderAndroid.cpp \
	android/WebCoreSupport/UrlInterceptResponse.cpp \
	android/WebCoreSupport/V8Counters.cpp

ifeq ($(HTTP_STACK),chrome)
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	android/WebCoreSupport/ChromiumInit.cpp \
	android/WebCoreSupport/CacheResult.cpp \
	android/WebCoreSupport/WebCache.cpp \
	android/WebCoreSupport/WebCookieJar.cpp \
	android/WebCoreSupport/WebUrlLoader.cpp \
	android/WebCoreSupport/WebUrlLoaderClient.cpp \
	android/WebCoreSupport/WebRequest.cpp \
	android/WebCoreSupport/WebRequestContext.cpp \
	android/WebCoreSupport/WebResourceRequest.cpp \
	android/WebCoreSupport/WebResponse.cpp \
	android/WebCoreSupport/WebViewClientError.cpp
endif # HTTP_STACK == chrome

LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	android/RenderSkinAndroid.cpp \
	android/RenderSkinButton.cpp \
	android/RenderSkinCombo.cpp \
	android/RenderSkinMediaButton.cpp \
	android/RenderSkinRadio.cpp \
	android/TimeCounter.cpp \
	\
	android/benchmark/Intercept.cpp \
	android/benchmark/MyJavaVM.cpp \
	\
	android/icu/unicode/ucnv.cpp \
	\
	android/jni/CacheManager.cpp \
	android/jni/CookieManager.cpp \
	android/jni/DeviceMotionAndOrientationManager.cpp \
	android/jni/DeviceMotionClientImpl.cpp \
	android/jni/DeviceOrientationClientImpl.cpp \
	android/jni/GeolocationPermissionsBridge.cpp \
	android/jni/JavaBridge.cpp \
	android/jni/JavaSharedClient.cpp \
	android/jni/JniUtil.cpp \
	android/jni/MIMETypeRegistry.cpp \
	android/jni/MockGeolocation.cpp \
	android/jni/PictureSet.cpp \
	android/jni/WebCoreFrameBridge.cpp \
	android/jni/WebCoreJni.cpp \
	android/jni/WebCoreResourceLoader.cpp \
	android/jni/WebFrameView.cpp \
	android/jni/WebHistory.cpp \
	android/jni/WebIconDatabase.cpp \
	android/jni/WebStorage.cpp \
	android/jni/WebSettings.cpp \
	android/jni/WebViewCore.cpp \
	\
	android/nav/CacheBuilder.cpp \
	android/nav/CachedColor.cpp \
	android/nav/CachedFrame.cpp \
	android/nav/CachedHistory.cpp \
	android/nav/CachedInput.cpp \
	android/nav/CachedLayer.cpp \
	android/nav/CachedNode.cpp \
	android/nav/CachedRoot.cpp \
	android/nav/FindCanvas.cpp \
	android/nav/SelectText.cpp \
	android/nav/WebView.cpp \
	\
	android/plugins/ANPBitmapInterface.cpp \
	android/plugins/ANPCanvasInterface.cpp \
	android/plugins/ANPEventInterface.cpp \
	android/plugins/ANPLogInterface.cpp \
	android/plugins/ANPMatrixInterface.cpp \
	android/plugins/ANPOpenGLInterface.cpp \
	android/plugins/ANPPaintInterface.cpp \
	android/plugins/ANPPathInterface.cpp \
	android/plugins/ANPSoundInterface.cpp \
	android/plugins/ANPSurfaceInterface.cpp \
	android/plugins/ANPSystemInterface.cpp \
	android/plugins/ANPTypefaceInterface.cpp \
	android/plugins/ANPVideoInterface.cpp \
	android/plugins/ANPWindowInterface.cpp \
	android/plugins/PluginDebugAndroid.cpp \
	android/plugins/PluginTimer.cpp \
	android/plugins/PluginViewBridgeAndroid.cpp \
	android/plugins/PluginWidgetAndroid.cpp \
	android/plugins/SkANP.cpp \
	\
	android/wds/Command.cpp \
	android/wds/Connection.cpp \
	android/wds/DebugServer.cpp

# Needed for autofill.
ifeq ($(ENABLE_AUTOFILL),true)
LOCAL_CFLAGS += -DENABLE_WEB_AUTOFILL

LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	android/WebCoreSupport/autofill/AutoFillHostAndroid.cpp \
	android/WebCoreSupport/autofill/FormFieldAndroid.cpp \
	android/WebCoreSupport/autofill/FormManagerAndroid.cpp \
	android/WebCoreSupport/autofill/WebAutoFill.cpp
endif # ENABLE_AUTOFILL == true
