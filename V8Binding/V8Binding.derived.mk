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

LOCAL_CFLAGS += -DWTF_USE_V8

v8binding_dir := $(LOCAL_PATH)

BINDING_C_INCLUDES := \
  $(BASE_PATH)/v8/include \
	$(WEBCORE_PATH)/bindings/v8 \
  $(LOCAL_PATH)/v8 \
	$(JAVASCRIPTCORE_PATH)/wtf \
	$(JAVASCRIPTCORE_PATH)

WEBCORE_SRC_FILES := \
  bindings/v8/ScheduledAction.cpp \
	bindings/v8/ScriptCallFrame.cpp \
	bindings/v8/ScriptCallStack.cpp \
	bindings/v8/ScriptInstance.cpp \
	bindings/v8/ScriptValue.cpp \
	bindings/v8/V8AbstractEventListener.cpp \
	bindings/v8/V8DOMMap.cpp \
	bindings/v8/V8LazyEventListener.cpp \
	bindings/v8/V8NodeFilterCondition.cpp \
	bindings/v8/V8ObjectEventListener.cpp \
	bindings/v8/V8WorkerContextEventListener.cpp \
	bindings/v8/V8XMLHttpRequestUtilities.cpp \
	bindings/v8/WorkerContextExecutionProxy.cpp \
	bindings/v8/WorkerScriptController.cpp \
	\
	bindings/v8/custom/V8AttrCustom.cpp \
	bindings/v8/custom/V8CSSStyleDeclarationCustom.cpp \
	bindings/v8/custom/V8CanvasRenderingContext2DCustom.cpp \
	bindings/v8/custom/V8ClipboardCustom.cpp \
	bindings/v8/custom/V8CustomBinding.cpp \
	bindings/v8/custom/V8CustomEventListener.cpp \
	bindings/v8/custom/V8CustomSQLStatementCallback.cpp \
	bindings/v8/custom/V8CustomSQLStatementErrorCallback.cpp \
	bindings/v8/custom/V8CustomSQLTransactionCallback.cpp \
	bindings/v8/custom/V8CustomSQLTransactionErrorCallback.cpp \
	bindings/v8/custom/V8CustomVoidCallback.cpp \
	bindings/v8/custom/V8DOMParserConstructor.cpp \
	bindings/v8/custom/V8DOMStringListCustom.cpp \
	bindings/v8/custom/V8DatabaseCustom.cpp \
	bindings/v8/custom/V8DocumentCustom.cpp \
	bindings/v8/custom/V8ElementCustom.cpp \
	bindings/v8/custom/V8EventCustom.cpp \
	bindings/v8/custom/V8HTMLCanvasElementCustom.cpp \
	bindings/v8/custom/V8HTMLCollectionCustom.cpp \
	bindings/v8/custom/V8HTMLDocumentCustom.cpp \
	bindings/v8/custom/V8HTMLFormElementCustom.cpp \
	bindings/v8/custom/V8HTMLFrameElementCustom.cpp \
	bindings/v8/custom/V8HTMLFrameSetElementCustom.cpp \
	bindings/v8/custom/V8HTMLIFrameElementCustom.cpp \
	bindings/v8/custom/V8HTMLInputElementCustom.cpp \
	bindings/v8/custom/V8HTMLOptionsCollectionCustom.cpp \
	bindings/v8/custom/V8HTMLPlugInElementCustom.cpp \
	bindings/v8/custom/V8HTMLSelectElementCustom.cpp \
	bindings/v8/custom/V8InspectorControllerCustom.cpp \
	bindings/v8/custom/V8LocationCustom.cpp \
	bindings/v8/custom/V8MessageChannelConstructor.cpp \
	bindings/v8/custom/V8NamedNodeMapCustom.cpp \
	bindings/v8/custom/V8NamedNodesCollection.cpp \
	bindings/v8/custom/V8NavigatorCustom.cpp \
	bindings/v8/custom/V8NodeCustom.cpp \
	bindings/v8/custom/V8NodeFilterCustom.cpp \
	bindings/v8/custom/V8NodeIteratorCustom.cpp \
	bindings/v8/custom/V8NodeListCustom.cpp \
	bindings/v8/custom/V8SQLResultSetRowListCustom.cpp \
	bindings/v8/custom/V8SQLTransactionCustom.cpp

ifeq ($(ENABLE_SVG), true)
WEBCORE_SRC_FILES := $(WEBCORE_SRC_FILES) \
	bindings/v8/custom/V8SVGElementInstanceCustom.cpp \
	bindings/v8/custom/V8SVGLengthCustom.cpp \
	bindings/v8/custom/V8SVGMatrixCustom.cpp
endif

WEBCORE_SRC_FILES := $(WEBCORE_SRC_FILES) \
	bindings/v8/custom/V8StyleSheetListCustom.cpp \
	bindings/v8/custom/V8TreeWalkerCustom.cpp \
	bindings/v8/custom/V8WebKitCSSMatrixConstructor.cpp \
	bindings/v8/custom/V8XMLHttpRequestConstructor.cpp \
	bindings/v8/custom/V8XMLHttpRequestCustom.cpp \
	bindings/v8/custom/V8XMLHttpRequestUploadCustom.cpp \
	bindings/v8/custom/V8XMLSerializerConstructor.cpp \
	bindings/v8/custom/V8XPathEvaluatorConstructor.cpp \
	bindings/v8/custom/V8XSLTProcessorCustom.cpp \
  \
  bridge/jni/jni_utility.cpp

LOCAL_SRC_FILES := \
	v8/JSDOMBinding.cpp \
	v8/JSXPathNSResolver.cpp \
	v8/NPV8Object.cpp \
	v8/RGBColor.cpp \
	v8/ScriptController.cpp \
	v8/V8CanvasPixelArrayCustom.cpp \
	v8/V8MessagePortCustom.cpp \
	v8/V8NPObject.cpp \
	v8/V8NPUtils.cpp \
	v8/V8WorkerContextCustom.cpp \
	v8/V8WorkerCustom.cpp \
	v8/npruntime.cpp \
	v8/v8_binding.cpp \
	v8/v8_custom.cpp \
	v8/v8_helpers.cpp \
	v8/v8_index.cpp \
	v8/v8_proxy.cpp

LOCAL_SHARED_LIBRARIES += libv8

js_binding_scripts := \
  $(LOCAL_PATH)/scripts/CodeGenerator.pm \
  $(LOCAL_PATH)/scripts/CodeGeneratorV8.pm \
  $(LOCAL_PATH)/scripts/IDLParser.pm \
  $(WEBCORE_PATH)/bindings/scripts/IDLStructure.pm \
	$(LOCAL_PATH)/scripts/generate-bindings.pl

FEATURE_DEFINES := ANDROID_ORIENTATION_SUPPORT ENABLE_TOUCH_EVENTS=1 V8_BINDING

GEN := \
    $(intermediates)/css/V8CSSCharsetRule.h \
    $(intermediates)/css/V8CSSFontFaceRule.h \
    $(intermediates)/css/V8CSSImportRule.h \
    $(intermediates)/css/V8CSSMediaRule.h \
    $(intermediates)/css/V8CSSPageRule.h \
    $(intermediates)/css/V8CSSPrimitiveValue.h \
    $(intermediates)/css/V8CSSRule.h \
    $(intermediates)/css/V8CSSRuleList.h \
    $(intermediates)/css/V8CSSStyleDeclaration.h \
    $(intermediates)/css/V8CSSStyleRule.h \
    $(intermediates)/css/V8CSSStyleSheet.h \
    $(intermediates)/css/V8CSSUnknownRule.h \
    $(intermediates)/css/V8CSSValue.h \
    $(intermediates)/css/V8CSSValueList.h \
    $(intermediates)/css/V8CSSVariablesDeclaration.h \
    $(intermediates)/css/V8CSSVariablesRule.h \
    $(intermediates)/css/V8Counter.h \
    $(intermediates)/css/V8MediaList.h \
    $(intermediates)/css/V8Rect.h \
    $(intermediates)/css/V8RGBColor.h \
    $(intermediates)/css/V8StyleSheet.h \
    $(intermediates)/css/V8StyleSheetList.h  \
    $(intermediates)/css/V8WebKitCSSKeyframeRule.h \
    $(intermediates)/css/V8WebKitCSSKeyframesRule.h \
    $(intermediates)/css/V8WebKitCSSTransformValue.h 
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(v8binding_dir)/scripts -I$(WEBCORE_PATH)/bindings/scripts $(v8binding_dir)/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include css --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/css/V8%.h : $(WEBCORE_PATH)/css/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)


# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/css/%.cpp : $(intermediates)/css/%.h

GEN := \
    $(intermediates)/dom/V8Attr.h \
    $(intermediates)/dom/V8CDATASection.h \
    $(intermediates)/dom/V8CharacterData.h \
    $(intermediates)/dom/V8ClientRect.h \
    $(intermediates)/dom/V8ClientRectList.h \
    $(intermediates)/dom/V8Clipboard.h \
    $(intermediates)/dom/V8Comment.h \
    $(intermediates)/dom/V8DOMCoreException.h \
    $(intermediates)/dom/V8DOMImplementation.h \
    $(intermediates)/dom/V8DOMStringList.h \
    $(intermediates)/dom/V8Document.h \
    $(intermediates)/dom/V8DocumentFragment.h \
    $(intermediates)/dom/V8DocumentType.h \
    $(intermediates)/dom/V8Element.h \
    $(intermediates)/dom/V8Entity.h \
    $(intermediates)/dom/V8EntityReference.h \
    $(intermediates)/dom/V8Event.h \
    $(intermediates)/dom/V8EventException.h \
    $(intermediates)/dom/V8KeyboardEvent.h \
    $(intermediates)/dom/V8MessageChannel.h \
    $(intermediates)/dom/V8MessageEvent.h \
    $(intermediates)/dom/V8MessagePort.h \
    $(intermediates)/dom/V8MouseEvent.h \
    $(intermediates)/dom/V8MutationEvent.h \
    $(intermediates)/dom/V8NamedNodeMap.h \
    $(intermediates)/dom/V8Node.h \
    $(intermediates)/dom/V8NodeFilter.h \
    $(intermediates)/dom/V8NodeIterator.h \
    $(intermediates)/dom/V8NodeList.h \
    $(intermediates)/dom/V8Notation.h \
    $(intermediates)/dom/V8OverflowEvent.h \
    $(intermediates)/dom/V8ProcessingInstruction.h \
    $(intermediates)/dom/V8ProgressEvent.h \
    $(intermediates)/dom/V8Range.h \
    $(intermediates)/dom/V8RangeException.h \
    $(intermediates)/dom/V8Text.h \
    $(intermediates)/dom/V8TextEvent.h \
    $(intermediates)/dom/V8Touch.h \
    $(intermediates)/dom/V8TouchEvent.h \
    $(intermediates)/dom/V8TouchList.h \
    $(intermediates)/dom/V8TreeWalker.h \
    $(intermediates)/dom/V8UIEvent.h \
    $(intermediates)/dom/V8WebKitAnimationEvent.h \
    $(intermediates)/dom/V8WebKitTransitionEvent.h \
    $(intermediates)/dom/V8WheelEvent.h
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(v8binding_dir)/scripts -I$(WEBCORE_PATH)/bindings/scripts $(v8binding_dir)/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/dom/V8%.h : $(WEBCORE_PATH)/dom/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/dom/%.cpp : $(intermediates)/dom/%.h


GEN := \
    $(intermediates)/html/V8CanvasGradient.h \
    $(intermediates)/html/V8CanvasPattern.h \
    $(intermediates)/html/V8CanvasPixelArray.h \
    $(intermediates)/html/V8CanvasRenderingContext2D.h \
    $(intermediates)/html/V8File.h \
    $(intermediates)/html/V8FileList.h \
    $(intermediates)/html/V8HTMLAnchorElement.h \
    $(intermediates)/html/V8HTMLAppletElement.h \
    $(intermediates)/html/V8HTMLAreaElement.h \
    $(intermediates)/html/V8HTMLBRElement.h \
    $(intermediates)/html/V8HTMLBaseElement.h \
    $(intermediates)/html/V8HTMLBaseFontElement.h \
    $(intermediates)/html/V8HTMLBlockquoteElement.h \
    $(intermediates)/html/V8HTMLBodyElement.h \
    $(intermediates)/html/V8HTMLButtonElement.h \
    $(intermediates)/html/V8HTMLCanvasElement.h \
    $(intermediates)/html/V8HTMLCollection.h \
    $(intermediates)/html/V8HTMLDListElement.h \
    $(intermediates)/html/V8HTMLDirectoryElement.h \
    $(intermediates)/html/V8HTMLDivElement.h \
    $(intermediates)/html/V8HTMLDocument.h \
    $(intermediates)/html/V8HTMLElement.h \
    $(intermediates)/html/V8HTMLEmbedElement.h \
    $(intermediates)/html/V8HTMLFieldSetElement.h \
    $(intermediates)/html/V8HTMLFontElement.h \
    $(intermediates)/html/V8HTMLFormElement.h \
    $(intermediates)/html/V8HTMLFrameElement.h \
    $(intermediates)/html/V8HTMLFrameSetElement.h \
    $(intermediates)/html/V8HTMLHRElement.h \
    $(intermediates)/html/V8HTMLHeadElement.h \
    $(intermediates)/html/V8HTMLHeadingElement.h \
    $(intermediates)/html/V8HTMLHtmlElement.h \
    $(intermediates)/html/V8HTMLIFrameElement.h \
    $(intermediates)/html/V8HTMLImageElement.h \
    $(intermediates)/html/V8HTMLInputElement.h \
    $(intermediates)/html/V8HTMLIsIndexElement.h \
    $(intermediates)/html/V8HTMLLIElement.h \
    $(intermediates)/html/V8HTMLLabelElement.h \
    $(intermediates)/html/V8HTMLLegendElement.h \
    $(intermediates)/html/V8HTMLLinkElement.h \
    $(intermediates)/html/V8HTMLMapElement.h \
    $(intermediates)/html/V8HTMLMarqueeElement.h \
    $(intermediates)/html/V8HTMLMenuElement.h \
    $(intermediates)/html/V8HTMLMetaElement.h \
    $(intermediates)/html/V8HTMLModElement.h \
    $(intermediates)/html/V8HTMLOListElement.h \
    $(intermediates)/html/V8HTMLObjectElement.h \
    $(intermediates)/html/V8HTMLOptGroupElement.h \
    $(intermediates)/html/V8HTMLOptionElement.h \
    $(intermediates)/html/V8HTMLOptionsCollection.h \
    $(intermediates)/html/V8HTMLParagraphElement.h \
    $(intermediates)/html/V8HTMLParamElement.h \
    $(intermediates)/html/V8HTMLPreElement.h \
    $(intermediates)/html/V8HTMLQuoteElement.h \
    $(intermediates)/html/V8HTMLScriptElement.h \
    $(intermediates)/html/V8HTMLSelectElement.h \
    $(intermediates)/html/V8HTMLSourceElement.h \
    $(intermediates)/html/V8HTMLStyleElement.h \
    $(intermediates)/html/V8HTMLTableCaptionElement.h \
    $(intermediates)/html/V8HTMLTableCellElement.h \
    $(intermediates)/html/V8HTMLTableColElement.h \
    $(intermediates)/html/V8HTMLTableElement.h \
    $(intermediates)/html/V8HTMLTableRowElement.h \
    $(intermediates)/html/V8HTMLTableSectionElement.h \
    $(intermediates)/html/V8HTMLTextAreaElement.h \
    $(intermediates)/html/V8HTMLTitleElement.h \
    $(intermediates)/html/V8HTMLUListElement.h \
    $(intermediates)/html/V8HTMLVideoElement.h \
    $(intermediates)/html/V8ImageData.h \
    $(intermediates)/html/V8MediaError.h \
    $(intermediates)/html/V8TextMetrics.h \
    $(intermediates)/html/V8TimeRanges.h \
    $(intermediates)/html/V8VoidCallback.h 

$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(v8binding_dir)/scripts -I$(WEBCORE_PATH)/bindings/scripts $(v8binding_dir)/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/html/V8%.h : $(WEBCORE_PATH)/html/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/html/%.cpp : $(intermediates)/html/%.h


GEN := \
    $(intermediates)/page/V8BarInfo.h \
    $(intermediates)/page/V8Console.h \
		$(intermediates)/page/V8Coordinates.h \
    $(intermediates)/page/V8DOMSelection.h \
    $(intermediates)/page/V8DOMWindow.h \
    $(intermediates)/page/V8Geolocation.h \
    $(intermediates)/page/V8Geoposition.h \
    $(intermediates)/page/V8History.h \
    $(intermediates)/page/V8Location.h \
    $(intermediates)/page/V8Navigator.h \
    $(intermediates)/page/V8PositionError.h \
    $(intermediates)/page/V8PositionErrorCallback.h \
    $(intermediates)/page/V8Screen.h \
		$(intermediates)/page/V8WebKitPoint.h
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(v8binding_dir)/scripts -I$(WEBCORE_PATH)/bindings/scripts $(v8binding_dir)/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/page/V8%.h : $(WEBCORE_PATH)/page/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/page/%.cpp : $(intermediates)/page/%.h

GEN := \
    $(intermediates)/plugins/V8MimeType.h \
    $(intermediates)/plugins/V8MimeTypeArray.h \
    $(intermediates)/plugins/V8Plugin.h \
    $(intermediates)/plugins/V8PluginArray.h 
    
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(v8binding_dir)/scripts -I$(WEBCORE_PATH)/bindings/scripts $(v8binding_dir)/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/plugins/V8%.h : $(WEBCORE_PATH)/plugins/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/plugins/%.cpp : $(intermediates)/plugins/%.h

#new section for xml/DOMParser.idl
GEN := \
    $(intermediates)/xml/V8DOMParser.h \
    $(intermediates)/xml/V8XMLHttpRequest.h \
    $(intermediates)/xml/V8XMLHttpRequestException.h \
    $(intermediates)/xml/V8XMLHttpRequestProgressEvent.h \
    $(intermediates)/xml/V8XMLHttpRequestUpload.h \
    $(intermediates)/xml/V8XMLSerializer.h \
    $(intermediates)/xml/V8XPathEvaluator.h \
    $(intermediates)/xml/V8XPathExpression.h \
    $(intermediates)/xml/V8XPathNSResolver.h \
    $(intermediates)/xml/V8XPathResult.h  \
    $(intermediates)/xml/V8XSLTProcessor.h

$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(v8binding_dir)/scripts -I$(WEBCORE_PATH)/bindings/scripts $(v8binding_dir)/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/xml/V8%.h : $(WEBCORE_PATH)/xml/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/xml/%.cpp : $(intermediates)/xml/%.h
#end

#new section for UndetectableHTMLCollection 
GEN := \
    $(intermediates)/html/V8UndetectableHTMLCollection.h

$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(WEBCORE_PATH) perl -I$(v8binding_dir)/scripts -I$(WEBCORE_PATH)/bindings/scripts $(v8binding_dir)/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/html/V8%.h : $(v8binding_dir)/v8/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/html/%.cpp : $(intermediates)/html/%.h
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

