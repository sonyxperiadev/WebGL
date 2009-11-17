##
##
## Copyright 2007, The Android Open Source Project
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

LOCAL_CFLAGS += -DWTF_USE_V8=1

v8binding_dir := $(LOCAL_PATH)

BINDING_C_INCLUDES := \
	$(LOCAL_PATH)/v8/include \
	$(WEBCORE_PATH)/bindings/v8 \
	$(WEBCORE_PATH)/bindings/v8/custom \
	$(LOCAL_PATH)/binding \
	$(WEBCORE_PATH)/bridge \
	$(LOCAL_PATH)/jni \
	$(JAVASCRIPTCORE_PATH)/wtf \
	$(JAVASCRIPTCORE_PATH)

WEBCORE_SRC_FILES := \
	bindings/ScriptControllerBase.cpp \
	\
	bindings/v8/ChildThreadDOMData.cpp \
	bindings/v8/DateExtension.cpp \
	bindings/v8/DOMData.cpp \
	bindings/v8/DOMDataStore.cpp \
	bindings/v8/DerivedSourcesAllInOne.cpp \
	bindings/v8/MainThreadDOMData.cpp \
	bindings/v8/NPV8Object.cpp \
	bindings/v8/RuntimeEnabledFeatures.cpp \
	bindings/v8/ScheduledAction.cpp \
	bindings/v8/ScopedDOMDataStore.cpp \
	bindings/v8/ScriptArray.cpp \
	bindings/v8/ScriptCallFrame.cpp \
	bindings/v8/ScriptCallStack.cpp \
	bindings/v8/ScriptController.cpp \
	bindings/v8/ScriptEventListener.cpp \
	bindings/v8/ScriptFunctionCall.cpp \
	bindings/v8/ScriptInstance.cpp \
	bindings/v8/ScriptObject.cpp \
	bindings/v8/ScriptScope.cpp \
	bindings/v8/ScriptState.cpp \
	bindings/v8/ScriptStringImpl.cpp \
	bindings/v8/ScriptValue.cpp \
	bindings/v8/StaticDOMDataStore.cpp \
	bindings/v8/V8AbstractEventListener.cpp \
	bindings/v8/V8Binding.cpp \
	bindings/v8/V8Collection.cpp \
	bindings/v8/V8ConsoleMessage.cpp \
	bindings/v8/V8DOMMap.cpp \
	bindings/v8/V8DOMWrapper.cpp \
	bindings/v8/V8DataGridDataSource.cpp \
	bindings/v8/V8EventListenerList.cpp \
	bindings/v8/V8GCController.cpp \
	bindings/v8/V8Helpers.cpp \
	bindings/v8/V8HiddenPropertyName.cpp \
	bindings/v8/V8Index.cpp \
	bindings/v8/V8IsolatedWorld.cpp \
	bindings/v8/V8LazyEventListener.cpp \
	bindings/v8/V8NPObject.cpp \
	bindings/v8/V8NPUtils.cpp \
	bindings/v8/V8NodeFilterCondition.cpp \
	bindings/v8/V8Proxy.cpp \
	bindings/v8/V8Utilities.cpp \
	bindings/v8/V8WorkerContextEventListener.cpp \
	bindings/v8/WorkerContextExecutionProxy.cpp \
	bindings/v8/WorkerScriptController.cpp \
	\
	bindings/v8/npruntime.cpp \
	\
	bindings/v8/custom/V8AbstractWorkerCustom.cpp \
	bindings/v8/custom/V8AttrCustom.cpp \
	bindings/v8/custom/V8CSSStyleDeclarationCustom.cpp \
	bindings/v8/custom/V8CanvasArrayBufferCustom.cpp \
	bindings/v8/custom/V8CanvasByteArrayCustom.cpp \
	bindings/v8/custom/V8CanvasFloatArrayCustom.cpp \
	bindings/v8/custom/V8CanvasIntArrayCustom.cpp \
	bindings/v8/custom/V8CanvasPixelArrayCustom.cpp \
	bindings/v8/custom/V8CanvasRenderingContext2DCustom.cpp \
	bindings/v8/custom/V8CanvasRenderingContext3DCustom.cpp \
	bindings/v8/custom/V8CanvasShortArrayCustom.cpp \
	bindings/v8/custom/V8CanvasUnsignedByteArrayCustom.cpp \
	bindings/v8/custom/V8CanvasUnsignedIntArrayCustom.cpp \
	bindings/v8/custom/V8CanvasUnsignedShortArrayCustom.cpp \
	bindings/v8/custom/V8ClientRectListCustom.cpp \
	bindings/v8/custom/V8ClipboardCustom.cpp \
	bindings/v8/custom/V8CoordinatesCustom.cpp \
	bindings/v8/custom/V8CustomBinding.cpp \
	bindings/v8/custom/V8CustomEventListener.cpp \
	bindings/v8/custom/V8CustomPositionCallback.cpp \
	bindings/v8/custom/V8CustomPositionErrorCallback.cpp \
	bindings/v8/custom/V8CustomSQLStatementCallback.cpp \
	bindings/v8/custom/V8CustomSQLStatementErrorCallback.cpp \
	bindings/v8/custom/V8CustomSQLTransactionCallback.cpp \
	bindings/v8/custom/V8CustomSQLTransactionErrorCallback.cpp \
	bindings/v8/custom/V8CustomVoidCallback.cpp \
	bindings/v8/custom/V8DOMApplicationCacheCustom.cpp \
	bindings/v8/custom/V8DOMParserConstructor.cpp \
	bindings/v8/custom/V8DOMWindowCustom.cpp \
	bindings/v8/custom/V8DataGridColumnListCustom.cpp \
	bindings/v8/custom/V8DatabaseCustom.cpp \
	bindings/v8/custom/V8DedicatedWorkerContextCustom.cpp \
	bindings/v8/custom/V8DocumentCustom.cpp \
	bindings/v8/custom/V8DocumentLocationCustom.cpp \
	bindings/v8/custom/V8ElementCustom.cpp \
	bindings/v8/custom/V8EventCustom.cpp \
	bindings/v8/custom/V8FileListCustom.cpp \
	bindings/v8/custom/V8GeolocationCustom.cpp \
	bindings/v8/custom/V8HTMLAllCollectionCustom.cpp \
	bindings/v8/custom/V8HTMLAudioElementConstructor.cpp \
	bindings/v8/custom/V8HTMLCanvasElementCustom.cpp \
	bindings/v8/custom/V8HTMLCollectionCustom.cpp \
	bindings/v8/custom/V8HTMLDataGridElementCustom.cpp \
	bindings/v8/custom/V8HTMLDocumentCustom.cpp \
	bindings/v8/custom/V8HTMLFormElementCustom.cpp \
	bindings/v8/custom/V8HTMLFrameElementCustom.cpp \
	bindings/v8/custom/V8HTMLFrameSetElementCustom.cpp \
	bindings/v8/custom/V8HTMLIFrameElementCustom.cpp \
	bindings/v8/custom/V8HTMLImageElementConstructor.cpp \
	bindings/v8/custom/V8HTMLInputElementCustom.cpp \
	bindings/v8/custom/V8HTMLOptionElementConstructor.cpp \
	bindings/v8/custom/V8HTMLOptionsCollectionCustom.cpp \
	bindings/v8/custom/V8HTMLPlugInElementCustom.cpp \
	bindings/v8/custom/V8HTMLSelectElementCollectionCustom.cpp \
	bindings/v8/custom/V8HTMLSelectElementCustom.cpp \
	bindings/v8/custom/V8LocationCustom.cpp \
	bindings/v8/custom/V8MessageChannelConstructor.cpp \
	bindings/v8/custom/V8MessagePortCustom.cpp \
	bindings/v8/custom/V8MessageEventCustom.cpp \
	bindings/v8/custom/V8NamedNodeMapCustom.cpp \
	bindings/v8/custom/V8NamedNodesCollection.cpp \
	bindings/v8/custom/V8NavigatorCustom.cpp \
	bindings/v8/custom/V8NodeCustom.cpp \
	bindings/v8/custom/V8NodeFilterCustom.cpp \
	bindings/v8/custom/V8NodeIteratorCustom.cpp \
	bindings/v8/custom/V8NodeListCustom.cpp \
	bindings/v8/custom/V8SQLResultSetRowListCustom.cpp \
	bindings/v8/custom/V8SQLTransactionCustom.cpp \
	bindings/v8/custom/V8WebSocketCustom.cpp

ifeq ($(ENABLE_SVG), true)
WEBCORE_SRC_FILES := $(WEBCORE_SRC_FILES) \
	bindings/v8/custom/V8SVGElementInstanceCustom.cpp \
	bindings/v8/custom/V8SVGLengthCustom.cpp \
	bindings/v8/custom/V8SVGMatrixCustom.cpp
endif

WEBCORE_SRC_FILES := $(WEBCORE_SRC_FILES) \
	bindings/v8/custom/V8SharedWorkerCustom.cpp \
	bindings/v8/custom/V8StorageCustom.cpp \
	bindings/v8/custom/V8StyleSheetListCustom.cpp \
	bindings/v8/custom/V8TouchListCustom.cpp \
	bindings/v8/custom/V8TreeWalkerCustom.cpp \
	bindings/v8/custom/V8WebKitCSSMatrixConstructor.cpp \
	bindings/v8/custom/V8WebKitPointConstructor.cpp \
	bindings/v8/custom/V8WorkerContextCustom.cpp \
	bindings/v8/custom/V8WorkerCustom.cpp \
	bindings/v8/custom/V8XMLHttpRequestConstructor.cpp \
	bindings/v8/custom/V8XMLHttpRequestCustom.cpp \
	bindings/v8/custom/V8XMLHttpRequestUploadCustom.cpp \
	bindings/v8/custom/V8XMLSerializerConstructor.cpp

LOCAL_SRC_FILES := \
	jni/jni_class.cpp \
	jni/jni_instance.cpp \
	jni/jni_npobject.cpp \
	jni/jni_runtime.cpp \
	jni/jni_utility.cpp

LOCAL_SHARED_LIBRARIES += libv8

js_binding_scripts := \
	$(WEBCORE_PATH)/bindings/scripts/CodeGenerator.pm \
	$(WEBCORE_PATH)/bindings/scripts/CodeGeneratorV8.pm \
	$(WEBCORE_PATH)/bindings/scripts/IDLParser.pm \
	$(WEBCORE_PATH)/bindings/scripts/IDLStructure.pm \
	$(WEBCORE_PATH)/bindings/scripts/generate-bindings.pl

FEATURE_DEFINES := ANDROID_ORIENTATION_SUPPORT ENABLE_TOUCH_EVENTS=1 V8_BINDING ENABLE_DATABASE=1 ENABLE_OFFLINE_WEB_APPLICATIONS=1 ENABLE_DOM_STORAGE=1 ENABLE_WORKERS=1 ENABLE_VIDEO=1 ENABLE_GEOLOCATION=1

GEN := \
    $(intermediates)/bindings/V8CSSCharsetRule.h \
    $(intermediates)/bindings/V8CSSFontFaceRule.h \
    $(intermediates)/bindings/V8CSSImportRule.h \
    $(intermediates)/bindings/V8CSSMediaRule.h \
    $(intermediates)/bindings/V8CSSPageRule.h \
    $(intermediates)/bindings/V8CSSPrimitiveValue.h \
    $(intermediates)/bindings/V8CSSRule.h \
    $(intermediates)/bindings/V8CSSRuleList.h \
    $(intermediates)/bindings/V8CSSStyleDeclaration.h \
    $(intermediates)/bindings/V8CSSStyleRule.h \
    $(intermediates)/bindings/V8CSSStyleSheet.h \
    $(intermediates)/bindings/V8CSSUnknownRule.h \
    $(intermediates)/bindings/V8CSSValue.h \
    $(intermediates)/bindings/V8CSSValueList.h \
    $(intermediates)/bindings/V8CSSVariablesDeclaration.h \
    $(intermediates)/bindings/V8CSSVariablesRule.h \
    $(intermediates)/bindings/V8Counter.h \
    $(intermediates)/bindings/V8Media.h \
    $(intermediates)/bindings/V8MediaList.h \
    $(intermediates)/bindings/V8Rect.h \
    $(intermediates)/bindings/V8RGBColor.h \
    $(intermediates)/bindings/V8StyleSheet.h \
    $(intermediates)/bindings/V8StyleSheetList.h  \
    $(intermediates)/bindings/V8WebKitCSSKeyframeRule.h \
    $(intermediates)/bindings/V8WebKitCSSKeyframesRule.h \
    $(intermediates)/bindings/V8WebKitCSSMatrix.h \
    $(intermediates)/bindings/V8WebKitCSSTransformValue.h 
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(WEBCORE_PATH)/bindings/scripts $(WEBCORE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include css --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(WEBCORE_PATH)/css/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)


# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

GEN := \
    $(intermediates)/bindings/V8Attr.h \
    $(intermediates)/bindings/V8BeforeLoadEvent.h \
    $(intermediates)/bindings/V8CDATASection.h \
    $(intermediates)/bindings/V8CharacterData.h \
    $(intermediates)/bindings/V8ClientRect.h \
    $(intermediates)/bindings/V8ClientRectList.h \
    $(intermediates)/bindings/V8Clipboard.h \
    $(intermediates)/bindings/V8Comment.h \
    $(intermediates)/bindings/V8DOMCoreException.h \
    $(intermediates)/bindings/V8DOMImplementation.h \
    $(intermediates)/bindings/V8Document.h \
    $(intermediates)/bindings/V8DocumentFragment.h \
    $(intermediates)/bindings/V8DocumentType.h \
    $(intermediates)/bindings/V8Element.h \
    $(intermediates)/bindings/V8Entity.h \
    $(intermediates)/bindings/V8EntityReference.h \
    $(intermediates)/bindings/V8ErrorEvent.h \
    $(intermediates)/bindings/V8Event.h \
    $(intermediates)/bindings/V8EventException.h \
    $(intermediates)/bindings/V8KeyboardEvent.h \
    $(intermediates)/bindings/V8MessageChannel.h \
    $(intermediates)/bindings/V8MessageEvent.h \
    $(intermediates)/bindings/V8MessagePort.h \
    $(intermediates)/bindings/V8MouseEvent.h \
    $(intermediates)/bindings/V8MutationEvent.h \
    $(intermediates)/bindings/V8NamedNodeMap.h \
    $(intermediates)/bindings/V8Node.h \
    $(intermediates)/bindings/V8NodeFilter.h \
    $(intermediates)/bindings/V8NodeIterator.h \
    $(intermediates)/bindings/V8NodeList.h \
    $(intermediates)/bindings/V8Notation.h \
    $(intermediates)/bindings/V8OverflowEvent.h \
    $(intermediates)/bindings/V8PageTransitionEvent.h \
    $(intermediates)/bindings/V8ProcessingInstruction.h \
    $(intermediates)/bindings/V8ProgressEvent.h \
    $(intermediates)/bindings/V8Range.h \
    $(intermediates)/bindings/V8RangeException.h \
    $(intermediates)/bindings/V8Text.h \
    $(intermediates)/bindings/V8TextEvent.h \
    $(intermediates)/bindings/V8Touch.h \
    $(intermediates)/bindings/V8TouchEvent.h \
    $(intermediates)/bindings/V8TouchList.h \
    $(intermediates)/bindings/V8TreeWalker.h \
    $(intermediates)/bindings/V8UIEvent.h \
    $(intermediates)/bindings/V8WebKitAnimationEvent.h \
    $(intermediates)/bindings/V8WebKitTransitionEvent.h \
    $(intermediates)/bindings/V8WheelEvent.h
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(WEBCORE_PATH)/bindings/scripts $(WEBCORE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(WEBCORE_PATH)/dom/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h


GEN := \
    $(intermediates)/bindings/V8DataGridColumn.h \
    $(intermediates)/bindings/V8DataGridColumnList.h \
    $(intermediates)/bindings/V8File.h \
    $(intermediates)/bindings/V8FileList.h \
    $(intermediates)/bindings/V8HTMLAllCollection.h \
    $(intermediates)/bindings/V8HTMLAnchorElement.h \
    $(intermediates)/bindings/V8HTMLAppletElement.h \
    $(intermediates)/bindings/V8HTMLAreaElement.h \
    $(intermediates)/bindings/V8HTMLAudioElement.h \
    $(intermediates)/bindings/V8HTMLBRElement.h \
    $(intermediates)/bindings/V8HTMLBaseElement.h \
    $(intermediates)/bindings/V8HTMLBaseFontElement.h \
    $(intermediates)/bindings/V8HTMLBlockquoteElement.h \
    $(intermediates)/bindings/V8HTMLBodyElement.h \
    $(intermediates)/bindings/V8HTMLButtonElement.h \
    $(intermediates)/bindings/V8HTMLCanvasElement.h \
    $(intermediates)/bindings/V8HTMLCollection.h \
    $(intermediates)/bindings/V8HTMLDataGridCellElement.h \
    $(intermediates)/bindings/V8HTMLDataGridColElement.h \
    $(intermediates)/bindings/V8HTMLDataGridElement.h \
    $(intermediates)/bindings/V8HTMLDataGridRowElement.h \
    $(intermediates)/bindings/V8HTMLDataListElement.h \
    $(intermediates)/bindings/V8HTMLDListElement.h \
    $(intermediates)/bindings/V8HTMLDirectoryElement.h \
    $(intermediates)/bindings/V8HTMLDivElement.h \
    $(intermediates)/bindings/V8HTMLDocument.h \
    $(intermediates)/bindings/V8HTMLElement.h \
    $(intermediates)/bindings/V8HTMLEmbedElement.h \
    $(intermediates)/bindings/V8HTMLFieldSetElement.h \
    $(intermediates)/bindings/V8HTMLFontElement.h \
    $(intermediates)/bindings/V8HTMLFormElement.h \
    $(intermediates)/bindings/V8HTMLFrameElement.h \
    $(intermediates)/bindings/V8HTMLFrameSetElement.h \
    $(intermediates)/bindings/V8HTMLHRElement.h \
    $(intermediates)/bindings/V8HTMLHeadElement.h \
    $(intermediates)/bindings/V8HTMLHeadingElement.h \
    $(intermediates)/bindings/V8HTMLHtmlElement.h \
    $(intermediates)/bindings/V8HTMLIFrameElement.h \
    $(intermediates)/bindings/V8HTMLImageElement.h \
    $(intermediates)/bindings/V8HTMLInputElement.h \
    $(intermediates)/bindings/V8HTMLIsIndexElement.h \
    $(intermediates)/bindings/V8HTMLLIElement.h \
    $(intermediates)/bindings/V8HTMLLabelElement.h \
    $(intermediates)/bindings/V8HTMLLegendElement.h \
    $(intermediates)/bindings/V8HTMLLinkElement.h \
    $(intermediates)/bindings/V8HTMLMapElement.h \
    $(intermediates)/bindings/V8HTMLMarqueeElement.h \
    $(intermediates)/bindings/V8HTMLMediaElement.h \
    $(intermediates)/bindings/V8HTMLMenuElement.h \
    $(intermediates)/bindings/V8HTMLMetaElement.h \
    $(intermediates)/bindings/V8HTMLModElement.h \
    $(intermediates)/bindings/V8HTMLOListElement.h \
    $(intermediates)/bindings/V8HTMLObjectElement.h \
    $(intermediates)/bindings/V8HTMLOptGroupElement.h \
    $(intermediates)/bindings/V8HTMLOptionElement.h \
    $(intermediates)/bindings/V8HTMLOptionsCollection.h \
    $(intermediates)/bindings/V8HTMLParagraphElement.h \
    $(intermediates)/bindings/V8HTMLParamElement.h \
    $(intermediates)/bindings/V8HTMLPreElement.h \
    $(intermediates)/bindings/V8HTMLQuoteElement.h \
    $(intermediates)/bindings/V8HTMLScriptElement.h \
    $(intermediates)/bindings/V8HTMLSelectElement.h \
    $(intermediates)/bindings/V8HTMLSourceElement.h \
    $(intermediates)/bindings/V8HTMLStyleElement.h \
    $(intermediates)/bindings/V8HTMLTableCaptionElement.h \
    $(intermediates)/bindings/V8HTMLTableCellElement.h \
    $(intermediates)/bindings/V8HTMLTableColElement.h \
    $(intermediates)/bindings/V8HTMLTableElement.h \
    $(intermediates)/bindings/V8HTMLTableRowElement.h \
    $(intermediates)/bindings/V8HTMLTableSectionElement.h \
    $(intermediates)/bindings/V8HTMLTextAreaElement.h \
    $(intermediates)/bindings/V8HTMLTitleElement.h \
    $(intermediates)/bindings/V8HTMLUListElement.h \
    $(intermediates)/bindings/V8HTMLVideoElement.h \
    $(intermediates)/bindings/V8ImageData.h \
    $(intermediates)/bindings/V8MediaError.h \
    $(intermediates)/bindings/V8TextMetrics.h \
    $(intermediates)/bindings/V8TimeRanges.h \
    $(intermediates)/bindings/V8ValidityState.h \
    $(intermediates)/bindings/V8VoidCallback.h

$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(WEBCORE_PATH)/bindings/scripts $(WEBCORE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(WEBCORE_PATH)/html/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

GEN := \
    $(intermediates)/bindings/V8CanvasActiveInfo.h \
    $(intermediates)/bindings/V8CanvasArray.h \
    $(intermediates)/bindings/V8CanvasArrayBuffer.h \
    $(intermediates)/bindings/V8CanvasBuffer.h \
    $(intermediates)/bindings/V8CanvasByteArray.h \
    $(intermediates)/bindings/V8CanvasFloatArray.h \
    $(intermediates)/bindings/V8CanvasFramebuffer.h \
    $(intermediates)/bindings/V8CanvasGradient.h \
    $(intermediates)/bindings/V8CanvasIntArray.h \
    $(intermediates)/bindings/V8CanvasNumberArray.h \
    $(intermediates)/bindings/V8CanvasPattern.h \
    $(intermediates)/bindings/V8CanvasPixelArray.h \
    $(intermediates)/bindings/V8CanvasProgram.h \
    $(intermediates)/bindings/V8CanvasRenderbuffer.h \
    $(intermediates)/bindings/V8CanvasRenderingContext.h \
    $(intermediates)/bindings/V8CanvasRenderingContext2D.h \
    $(intermediates)/bindings/V8CanvasRenderingContext3D.h \
    $(intermediates)/bindings/V8CanvasShader.h \
    $(intermediates)/bindings/V8CanvasShortArray.h \
    $(intermediates)/bindings/V8CanvasTexture.h \
    $(intermediates)/bindings/V8CanvasUnsignedByteArray.h \
    $(intermediates)/bindings/V8CanvasUnsignedIntArray.h \
    $(intermediates)/bindings/V8CanvasUnsignedShortArray.h

$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(WEBCORE_PATH)/bindings/scripts $(WEBCORE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --include html/canvas --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(WEBCORE_PATH)/html/canvas/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

GEN := \
    $(intermediates)/bindings/V8DOMApplicationCache.h

$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(WEBCORE_PATH)/bindings/scripts $(WEBCORE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(WEBCORE_PATH)/loader/appcache/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

GEN := \
    $(intermediates)/bindings/V8BarInfo.h \
    $(intermediates)/bindings/V8Console.h \
    $(intermediates)/bindings/V8Coordinates.h \
    $(intermediates)/bindings/V8DOMSelection.h \
    $(intermediates)/bindings/V8DOMWindow.h \
    $(intermediates)/bindings/V8Geolocation.h \
    $(intermediates)/bindings/V8Geoposition.h \
    $(intermediates)/bindings/V8History.h \
    $(intermediates)/bindings/V8Location.h \
    $(intermediates)/bindings/V8Navigator.h \
    $(intermediates)/bindings/V8PositionError.h \
    $(intermediates)/bindings/V8Screen.h \
    $(intermediates)/bindings/V8WebKitPoint.h \
    $(intermediates)/bindings/V8WorkerNavigator.h
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(WEBCORE_PATH)/bindings/scripts $(WEBCORE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(WEBCORE_PATH)/page/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

GEN := \
    $(intermediates)/bindings/V8MimeType.h \
    $(intermediates)/bindings/V8MimeTypeArray.h \
    $(intermediates)/bindings/V8Plugin.h \
    $(intermediates)/bindings/V8PluginArray.h 
    
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(WEBCORE_PATH)/bindings/scripts $(WEBCORE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(WEBCORE_PATH)/plugins/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# Database support
GEN := \
    $(intermediates)/bindings/V8Database.h \
    $(intermediates)/bindings/V8SQLError.h \
    $(intermediates)/bindings/V8SQLResultSet.h \
    $(intermediates)/bindings/V8SQLResultSetRowList.h \
    $(intermediates)/bindings/V8SQLTransaction.h
    
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(WEBCORE_PATH)/bindings/scripts $(WEBCORE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(WEBCORE_PATH)/storage/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# DOM Storage support
GEN := \
    $(intermediates)/bindings/V8Storage.h \
    $(intermediates)/bindings/V8StorageEvent.h

$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(WEBCORE_PATH)/bindings/scripts $(WEBCORE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(WEBCORE_PATH)/storage/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# Workers support
GEN := \
	$(intermediates)/bindings/V8AbstractWorker.h \
	$(intermediates)/bindings/V8DedicatedWorkerContext.h \
	$(intermediates)/bindings/V8SharedWorker.h \
	$(intermediates)/bindings/V8SharedWorkerContext.h \
	$(intermediates)/bindings/V8Worker.h \
	$(intermediates)/bindings/V8WorkerContext.h \
	$(intermediates)/bindings/V8WorkerLocation.h

$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(WEBCORE_PATH)/bindings/scripts $(WEBCORE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --include workers --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(WEBCORE_PATH)/workers/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

#new section for svg
ifeq ($(ENABLE_SVG), true)
GEN := \
    $(intermediates)/bindings/V8SVGAElement.h \
    $(intermediates)/bindings/V8SVGAltGlyphElement.h \
    $(intermediates)/bindings/V8SVGAngle.h \
    $(intermediates)/bindings/V8SVGCircleElement.h \
    $(intermediates)/bindings/V8SVGClipPathElement.h \
    $(intermediates)/bindings/V8SVGColor.h \
    $(intermediates)/bindings/V8SVGComponentTransferFunctionElement.h \
    $(intermediates)/bindings/V8SVGCursorElement.h \
    $(intermediates)/bindings/V8SVGDefsElement.h \
    $(intermediates)/bindings/V8SVGDescElement.h \
    $(intermediates)/bindings/V8SVGDocument.h \
    $(intermediates)/bindings/V8SVGElement.h \
    $(intermediates)/bindings/V8SVGElementInstance.h \
    $(intermediates)/bindings/V8SVGElementInstanceList.h \
    $(intermediates)/bindings/V8SVGEllipseElement.h \
    $(intermediates)/bindings/V8SVGException.h \
    $(intermediates)/bindings/V8SVGFEBlendElement.h \
    $(intermediates)/bindings/V8SVGFEColorMatrixElement.h \
    $(intermediates)/bindings/V8SVGFEComponentTransferElement.h \
    $(intermediates)/bindings/V8SVGFECompositeElement.h \
    $(intermediates)/bindings/V8SVGFEDiffuseLightingElement.h \
    $(intermediates)/bindings/V8SVGFEDisplacementMapElement.h \
    $(intermediates)/bindings/V8SVGFEDistantLightElement.h \
    $(intermediates)/bindings/V8SVGFEFloodElement.h \
    $(intermediates)/bindings/V8SVGFEFuncAElement.h \
    $(intermediates)/bindings/V8SVGFEFuncBElement.h \
    $(intermediates)/bindings/V8SVGFEFuncGElement.h \
    $(intermediates)/bindings/V8SVGFEFuncRElement.h \
    $(intermediates)/bindings/V8SVGFEGaussianBlurElement.h \
    $(intermediates)/bindings/V8SVGFEImageElement.h \
    $(intermediates)/bindings/V8SVGFEMergeElement.h \
    $(intermediates)/bindings/V8SVGFEMergeNodeElement.h \
    $(intermediates)/bindings/V8SVGFEOffsetElement.h \
    $(intermediates)/bindings/V8SVGFEPointLightElement.h \
    $(intermediates)/bindings/V8SVGFESpecularLightingElement.h \
    $(intermediates)/bindings/V8SVGFESpotLightElement.h \
    $(intermediates)/bindings/V8SVGFETileElement.h \
    $(intermediates)/bindings/V8SVGFETurbulenceElement.h \
    $(intermediates)/bindings/V8SVGFilterElement.h \
    $(intermediates)/bindings/V8SVGFontElement.h \
    $(intermediates)/bindings/V8SVGFontFaceElement.h \
    $(intermediates)/bindings/V8SVGFontFaceFormatElement.h \
    $(intermediates)/bindings/V8SVGFontFaceNameElement.h \
    $(intermediates)/bindings/V8SVGFontFaceSrcElement.h \
    $(intermediates)/bindings/V8SVGFontFaceUriElement.h \
    $(intermediates)/bindings/V8SVGForeignObjectElement.h \
    $(intermediates)/bindings/V8SVGGElement.h \
    $(intermediates)/bindings/V8SVGGlyphElement.h \
    $(intermediates)/bindings/V8SVGGradientElement.h \
    $(intermediates)/bindings/V8SVGHKernElement.h \
    $(intermediates)/bindings/V8SVGImageElement.h \
    $(intermediates)/bindings/V8SVGLength.h \
    $(intermediates)/bindings/V8SVGLengthList.h \
    $(intermediates)/bindings/V8SVGLineElement.h \
    $(intermediates)/bindings/V8SVGLinearGradientElement.h \
    $(intermediates)/bindings/V8SVGMarkerElement.h \
    $(intermediates)/bindings/V8SVGMaskElement.h \
    $(intermediates)/bindings/V8SVGMatrix.h \
    $(intermediates)/bindings/V8SVGMetadataElement.h \
    $(intermediates)/bindings/V8SVGMissingGlyphElement.h \
    $(intermediates)/bindings/V8SVGNumber.h \
    $(intermediates)/bindings/V8SVGNumberList.h \
    $(intermediates)/bindings/V8SVGPaint.h \
    $(intermediates)/bindings/V8SVGPathElement.h \
    $(intermediates)/bindings/V8SVGPathSeg.h \
    $(intermediates)/bindings/V8SVGPathSegArcAbs.h \
    $(intermediates)/bindings/V8SVGPathSegArcRel.h \
    $(intermediates)/bindings/V8SVGPathSegClosePath.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoCubicAbs.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoCubicRel.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoCubicSmoothAbs.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoCubicSmoothRel.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoQuadraticAbs.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoQuadraticRel.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoQuadraticSmoothAbs.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoQuadraticSmoothRel.h \
    $(intermediates)/bindings/V8SVGPathSegLinetoAbs.h \
    $(intermediates)/bindings/V8SVGPathSegLinetoHorizontalAbs.h \
    $(intermediates)/bindings/V8SVGPathSegLinetoHorizontalRel.h \
    $(intermediates)/bindings/V8SVGPathSegLinetoRel.h \
    $(intermediates)/bindings/V8SVGPathSegLinetoVerticalAbs.h \
    $(intermediates)/bindings/V8SVGPathSegLinetoVerticalRel.h \
    $(intermediates)/bindings/V8SVGPathSegList.h \
    $(intermediates)/bindings/V8SVGPathSegMovetoAbs.h \
    $(intermediates)/bindings/V8SVGPathSegMovetoRel.h \
    $(intermediates)/bindings/V8SVGPatternElement.h \
    $(intermediates)/bindings/V8SVGPoint.h \
    $(intermediates)/bindings/V8SVGPointList.h \
    $(intermediates)/bindings/V8SVGPolygonElement.h \
    $(intermediates)/bindings/V8SVGPolylineElement.h \
    $(intermediates)/bindings/V8SVGPreserveAspectRatio.h \
    $(intermediates)/bindings/V8SVGRadialGradientElement.h \
    $(intermediates)/bindings/V8SVGRect.h \
    $(intermediates)/bindings/V8SVGRectElement.h \
    $(intermediates)/bindings/V8SVGRenderingIntent.h \
    $(intermediates)/bindings/V8SVGSVGElement.h \
    $(intermediates)/bindings/V8SVGScriptElement.h \
    $(intermediates)/bindings/V8SVGStopElement.h \
    $(intermediates)/bindings/V8SVGStringList.h \
    $(intermediates)/bindings/V8SVGStyleElement.h \
    $(intermediates)/bindings/V8SVGSwitchElement.h \
    $(intermediates)/bindings/V8SVGSymbolElement.h \
    $(intermediates)/bindings/V8SVGTRefElement.h \
    $(intermediates)/bindings/V8SVGTSpanElement.h \
    $(intermediates)/bindings/V8SVGTextContentElement.h \
    $(intermediates)/bindings/V8SVGTextElement.h \
    $(intermediates)/bindings/V8SVGTextPathElement.h \
    $(intermediates)/bindings/V8SVGTextPositioningElement.h \
    $(intermediates)/bindings/V8SVGTitleElement.h \
    $(intermediates)/bindings/V8SVGTransform.h \
    $(intermediates)/bindings/V8SVGTransformList.h \
    $(intermediates)/bindings/V8SVGURIReference.h \
    $(intermediates)/bindings/V8SVGUnitTypes.h \
    $(intermediates)/bindings/V8SVGUseElement.h \
    $(intermediates)/bindings/V8SVGViewElement.h \
    $(intermediates)/bindings/V8SVGZoomEvent.h \
    \
    $(intermediates)/bindings/V8SVGAnimatedAngle.h \
    $(intermediates)/bindings/V8SVGAnimatedEnumeration.h \
    $(intermediates)/bindings/V8SVGAnimatedBoolean.h \
    $(intermediates)/bindings/V8SVGAnimatedInteger.h \
    $(intermediates)/bindings/V8SVGAnimatedLength.h \
    $(intermediates)/bindings/V8SVGAnimatedLengthList.h \
    $(intermediates)/bindings/V8SVGAnimatedNumber.h \
    $(intermediates)/bindings/V8SVGAnimatedNumberList.h \
    $(intermediates)/bindings/V8SVGAnimatedPoints.h \
    $(intermediates)/bindings/V8SVGAnimatedPreserveAspectRatio.h \
    $(intermediates)/bindings/V8SVGAnimatedRect.h \
    $(intermediates)/bindings/V8SVGAnimatedString.h \
    $(intermediates)/bindings/V8SVGAnimatedTransformList.h

ifeq ($(ENABLE_SVG_ANIMATION), true)
GEN += \
    $(intermediates)/bindings/V8SVGAnimateColorElement.h \
    $(intermediates)/bindings/V8SVGAnimateElement.h \
    $(intermediates)/bindings/V8SVGAnimateTransformElement.h \
    $(intermediates)/bindings/V8SVGAnimationElement.h \
    $(intermediates)/bindings/V8SVGSetElement.h
endif

$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(WEBCORE_PATH)/bindings/scripts $(WEBCORE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include css --include dom --include html --include svg --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(WEBCORE_PATH)/svg/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h
endif

#new section for xml/DOMParser.idl
GEN := \
    $(intermediates)/bindings/V8DOMParser.h \
    $(intermediates)/bindings/V8XMLHttpRequest.h \
    $(intermediates)/bindings/V8XMLHttpRequestException.h \
    $(intermediates)/bindings/V8XMLHttpRequestProgressEvent.h \
    $(intermediates)/bindings/V8XMLHttpRequestUpload.h \
    $(intermediates)/bindings/V8XMLSerializer.h

GEN += \
    $(intermediates)/bindings/V8XPathNSResolver.h

$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(WEBCORE_PATH)/bindings/scripts $(WEBCORE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(WEBCORE_PATH)/xml/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h
#end

# HTML tag and attribute names

GEN:= $(intermediates)/HTMLNames.cpp $(intermediates)/HTMLElementFactory.cpp
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(WEBCORE_PATH)/bindings/scripts $< --tags $(html_tags) --attrs $(html_attrs) --factory --wrapperFactory --output $(dir $@) 
$(GEN): html_tags := $(WEBCORE_PATH)/html/HTMLTagNames.in
$(GEN): html_attrs := $(WEBCORE_PATH)/html/HTMLAttributeNames.in
$(GEN): $(WEBCORE_PATH)/dom/make_names.pl $(html_tags) $(html_attrs)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# SVG tag and attribute names

ifeq ($(ENABLE_SVG), true)
GEN:= $(intermediates)/SVGNames.cpp  $(intermediates)/SVGElementFactory.cpp
SVG_FLAGS:=ENABLE_SVG_AS_IMAGE=1 ENABLE_SVG_FILTERS=1 ENABLE_SVG_FONTS=1 ENABLE_SVG_FOREIGN_OBJECT=1 ENABLE_SVG_USE=1
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(WEBCORE_PATH)/bindings/scripts $< --tags $(svg_tags) --attrs $(svg_attrs) --extraDefines "$(SVG_FLAGS)" --factory --wrapperFactory --output $(dir $@)
$(GEN): svg_tags := $(WEBCORE_PATH)/svg/svgtags.in
$(GEN): svg_attrs := $(WEBCORE_PATH)/svg/svgattrs.in
$(GEN): $(WEBCORE_PATH)/dom/make_names.pl $(svg_tags) $(svg_attrs)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)
endif

# XLink attribute names

ifeq ($(ENABLE_SVG), true)
GEN:= $(intermediates)/XLinkNames.cpp
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(WEBCORE_PATH)/bindings/scripts $< --attrs $(xlink_attrs) --output $(dir $@)
$(GEN): xlink_attrs := $(WEBCORE_PATH)/svg/xlinkattrs.in
$(GEN): $(WEBCORE_PATH)/dom/make_names.pl $(xlink_attrs)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)
endif
