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
	android/WebCoreSupport/ChromeClientAndroid.cpp \
	android/WebCoreSupport/ContextMenuClientAndroid.cpp \
	android/WebCoreSupport/DragClientAndroid.cpp \
	android/WebCoreSupport/EditorClientAndroid.cpp \
	android/WebCoreSupport/FrameLoaderClientAndroid.cpp \
	android/WebCoreSupport/MediaPlayerPrivateAndroid.cpp \
	android/WebCoreSupport/PlatformBridge.cpp \
	android/WebCoreSupport/GeolocationPermissions.cpp \
	\
	android/RenderSkinAndroid.cpp \
	android/RenderSkinButton.cpp \
	android/RenderSkinCombo.cpp \
	android/RenderSkinRadio.cpp \
	android/TimeCounter.cpp \
	android/sort.cpp \
	\
	android/icu/unicode/ucnv.cpp \
	\
	android/jni/GeolocationPermissionsBridge.cpp \
	android/jni/JavaBridge.cpp \
	android/jni/JavaSharedClient.cpp \
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
	android/nav/CachedFrame.cpp \
	android/nav/CachedHistory.cpp \
	android/nav/CachedNode.cpp \
	android/nav/CachedRoot.cpp \
	android/nav/FindCanvas.cpp \
	android/nav/SelectText.cpp \
	android/nav/WebView.cpp \
	\
	android/plugins/ANPBitmapInterface.cpp \
	android/plugins/ANPCanvasInterface.cpp \
	android/plugins/ANPLogInterface.cpp \
	android/plugins/ANPMatrixInterface.cpp \
	android/plugins/ANPPaintInterface.cpp \
	android/plugins/ANPPathInterface.cpp \
	android/plugins/ANPSoundInterface.cpp \
	android/plugins/ANPSurfaceInterface.cpp \
	android/plugins/ANPSystemInterface.cpp \
	android/plugins/ANPTypefaceInterface.cpp \
	android/plugins/ANPWindowInterface.cpp \
	android/plugins/PluginTimer.cpp \
	android/plugins/PluginViewBridgeAndroid.cpp \
	android/plugins/PluginWidgetAndroid.cpp \
	android/plugins/SkANP.cpp \
	\
	android/wds/Command.cpp \
	android/wds/Connection.cpp \
	android/wds/DebugServer.cpp
