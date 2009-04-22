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

LOCAL_CFLAGS += -DWTF_USE_JSC

BINDING_C_INCLUDES := \
       $(BASE_PATH)/WebCore/bindings/js \
       $(BASE_PATH)/JavaScriptCore \
       $(BASE_PATH)/JavaScriptCore/API \
       $(BASE_PATH)/JavaScriptCore/assembler \
       $(BASE_PATH)/JavaScriptCore/bytecode \
       $(BASE_PATH)/JavaScriptCore/bytecompiler \
       $(BASE_PATH)/JavaScriptCore/debugger \
       $(BASE_PATH)/JavaScriptCore/parser \
       $(BASE_PATH)/JavaScriptCore/jit \
       $(BASE_PATH)/JavaScriptCore/interpreter \
       $(BASE_PATH)/JavaScriptCore/pcre \
       $(BASE_PATH)/JavaScriptCore/profiler \
       $(BASE_PATH)/JavaScriptCore/runtime \
       $(BASE_PATH)/JavaScriptCore/wrec \
       $(BASE_PATH)/JavaScriptCore/wtf \
       $(BASE_PATH)/JavaScriptCore/wtf/unicode \
       $(BASE_PATH)/JavaScriptCore/wtf/unicode/icu \
       $(BASE_PATH)/JavaScriptCore/ForwardingHeaders \
       $(base_intermediates)/JavaScriptCore \
       $(base_intermediates)/JavaScriptCore/parser \
       $(base_intermediates)/JavaScriptCore/runtime \
       $(base_intermediates)/WebCore/bindings/js


# This comment block is read by tools/webkitsync/diff.cpp
# Don't remove it or move it. 
#

# The following files are intentionally not included
# LOCAL_SRC_FILES_EXCLUDED := \
#	bindings/js/JSCustomSQL*.cpp \
#	bindings/js/JSCustomVersionChangeCallback.cpp \
#	bindings/js/JSDatabaseCustom.cpp \
#	bindings/js/JSHTMLAudioElementConstructor.cpp \
#	bindings/js/JSSQL*.cpp \
#	bindings/js/JSStorageCustom.cpp \
#	bindings/js/JSXSLTProcessor*.cpp \
#	bindings/js/JSWorker*.cpp \
#	bindings/js/Worker*.cpp \
#	bindings/js/*Gtk.cpp \
#	bindings/js/*Qt.cpp \
#	bindings/js/*Win.cpp \
#	bindings/js/*Wx.cpp \

LOCAL_SRC_FILES := \
	bindings/js/GCController.cpp \
	bindings/js/JSAttrCustom.cpp \
	bindings/js/JSAudioConstructor.cpp \
	bindings/js/JSCDATASectionCustom.cpp \
	bindings/js/JSCSSRuleCustom.cpp \
	bindings/js/JSCSSStyleDeclarationCustom.cpp \
	bindings/js/JSCSSValueCustom.cpp \
	bindings/js/JSCanvasRenderingContext2DCustom.cpp \
	bindings/js/JSClipboardCustom.cpp \
	bindings/js/JSConsoleCustom.cpp \
	bindings/js/JSCustomPositionCallback.cpp \
	bindings/js/JSCustomPositionErrorCallback.cpp \
	bindings/js/JSCustomVoidCallback.cpp \
	bindings/js/JSCustomXPathNSResolver.cpp \
	bindings/js/JSDOMApplicationCacheCustom.cpp \
	bindings/js/JSDOMBinding.cpp \
	bindings/js/JSDOMGlobalObject.cpp \
	bindings/js/JSDOMStringListCustom.cpp \
	bindings/js/JSDOMWindowBase.cpp \
	bindings/js/JSDOMWindowCustom.cpp \
	bindings/js/JSDOMWindowShell.cpp \
	bindings/js/JSDocumentCustom.cpp \
	bindings/js/JSDocumentFragmentCustom.cpp \
	bindings/js/JSElementCustom.cpp \
	bindings/js/JSEventCustom.cpp \
	bindings/js/JSEventListener.cpp \
	bindings/js/JSEventTarget.cpp \
	bindings/js/JSGeolocationCustom.cpp \
	bindings/js/JSHTMLAllCollection.cpp \
	bindings/js/JSHTMLAppletElementCustom.cpp \
	bindings/js/JSHTMLCollectionCustom.cpp \
	bindings/js/JSHTMLDocumentCustom.cpp \
	bindings/js/JSHTMLElementCustom.cpp \
	bindings/js/JSHTMLEmbedElementCustom.cpp \
	bindings/js/JSHTMLFormElementCustom.cpp \
	bindings/js/JSHTMLFrameElementCustom.cpp \
	bindings/js/JSHTMLFrameSetElementCustom.cpp \
	bindings/js/JSHTMLIFrameElementCustom.cpp \
	bindings/js/JSHTMLInputElementCustom.cpp \
	bindings/js/JSHTMLObjectElementCustom.cpp \
	bindings/js/JSHTMLOptionsCollectionCustom.cpp \
	bindings/js/JSHTMLSelectElementCustom.cpp \
	bindings/js/JSHistoryCustom.cpp \
	bindings/js/JSImageConstructor.cpp \
	bindings/js/JSImageDataCustom.cpp \
	bindings/js/JSInspectedObjectWrapper.cpp \
	bindings/js/JSInspectorCallbackWrapper.cpp \
	bindings/js/JSJavaScriptCallFrameCustom.cpp \
	bindings/js/JSLazyEventListener.cpp \
	bindings/js/JSLocationCustom.cpp \
	bindings/js/JSMessageChannelConstructor.cpp \
	bindings/js/JSMessageChannelCustom.cpp \
	bindings/js/JSMessagePortCustom.cpp \
	bindings/js/JSMimeTypeArrayCustom.cpp \
	bindings/js/JSNamedNodeMapCustom.cpp \
	bindings/js/JSNamedNodesCollection.cpp \
	bindings/js/JSNavigatorCustom.cpp \
	bindings/js/JSNodeCustom.cpp \
	bindings/js/JSNodeFilterCondition.cpp \
	bindings/js/JSNodeFilterCustom.cpp \
	bindings/js/JSNodeIteratorCustom.cpp \
	bindings/js/JSNodeListCustom.cpp \
	bindings/js/JSOptionConstructor.cpp \
	bindings/js/JSPluginArrayCustom.cpp \
	bindings/js/JSPluginCustom.cpp \
	bindings/js/JSPluginElementFunctions.cpp \
	bindings/js/JSQuarantinedObjectWrapper.cpp \
	bindings/js/JSRGBColor.cpp \
    
ifeq ($(ENABLE_SVG), true)
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	bindings/js/JSSVGElementInstanceCustom.cpp \
	bindings/js/JSSVGLengthCustom.cpp \
	bindings/js/JSSVGMatrixCustom.cpp \
	bindings/js/JSSVGPathSegCustom.cpp \
	bindings/js/JSSVGPathSegListCustom.cpp \
	bindings/js/JSSVGPointListCustom.cpp \
	bindings/js/JSSVGTransformListCustom.cpp
endif
    
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	bindings/js/JSStyleSheetCustom.cpp \
	bindings/js/JSStyleSheetListCustom.cpp \
	bindings/js/JSTextCustom.cpp \
	bindings/js/JSTreeWalkerCustom.cpp \
	bindings/js/JSWebKitCSSMatrixConstructor.cpp \
	bindings/js/JSWebKitPointConstructor.cpp \
	bindings/js/JSXMLHttpRequestConstructor.cpp \
	bindings/js/JSXMLHttpRequestCustom.cpp \
	bindings/js/JSXMLHttpRequestUploadCustom.cpp \
	bindings/js/ScheduledAction.cpp \
	bindings/js/ScriptCachedFrameData.cpp \
	bindings/js/ScriptCallFrame.cpp \
	bindings/js/ScriptCallStack.cpp \
	bindings/js/ScriptController.cpp \
	bindings/js/ScriptControllerAndroid.cpp \
	bindings/js/ScriptFunctionCall.cpp \
	bindings/js/ScriptObject.cpp \
	bindings/js/ScriptValue.cpp \
	\
	bridge/IdentifierRep.cpp \
	bridge/NP_jsobject.cpp \
	bridge/c/c_class.cpp \
	bridge/c/c_instance.cpp \
	bridge/c/c_runtime.cpp \
	bridge/c/c_utility.cpp \
	bridge/jni/jni_class.cpp \
	bridge/jni/jni_instance.cpp \
	bridge/jni/jni_runtime.cpp \
	bridge/jni/jni_utility.cpp \
	bridge/npruntime.cpp \
	bridge/runtime.cpp \
	bridge/runtime_array.cpp \
	bridge/runtime_method.cpp \
	bridge/runtime_object.cpp \
	bridge/runtime_root.cpp


# lookup tables for old-style JavaScript bindings
create_hash_table := $(LOCAL_PATH)/../JavaScriptCore/create_hash_table

GEN := $(addprefix $(intermediates)/, \
			bindings/js/JSDOMWindowBase.lut.h \
    		bindings/js/JSRGBColor.lut.h \
		)
$(GEN): PRIVATE_CUSTOM_TOOL = perl $(create_hash_table) $< > $@
$(GEN): $(intermediates)/bindings/js/%.lut.h: $(LOCAL_PATH)/bindings/js/%.cpp $(create_hash_table)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)


GEN := $(intermediates)/bindings/js/JSHTMLInputElementBaseTable.cpp
$(GEN): PRIVATE_CUSTOM_TOOL = perl $(create_hash_table) $< > $@
$(GEN): $(intermediates)/bindings/js/%Table.cpp: $(LOCAL_PATH)/bindings/js/%.cpp $(create_hash_table)
	$(transform-generated-source)
$(intermediates)/bindings/js/JSHTMLInputElementBase.o : $(GEN)

# lookup tables for old-style JavaScript bindings
js_binding_scripts := $(addprefix $(LOCAL_PATH)/,\
			bindings/scripts/CodeGenerator.pm \
			bindings/scripts/IDLParser.pm \
			bindings/scripts/IDLStructure.pm \
			bindings/scripts/generate-bindings.pl \
		)

FEATURE_DEFINES := ANDROID_ORIENTATION_SUPPORT ENABLE_TOUCH_EVENTS=1

GEN := \
    $(intermediates)/css/JSCSSCharsetRule.h \
    $(intermediates)/css/JSCSSFontFaceRule.h \
    $(intermediates)/css/JSCSSImportRule.h \
    $(intermediates)/css/JSCSSMediaRule.h \
    $(intermediates)/css/JSCSSPageRule.h \
    $(intermediates)/css/JSCSSPrimitiveValue.h \
    $(intermediates)/css/JSCSSRule.h \
    $(intermediates)/css/JSCSSRuleList.h \
    $(intermediates)/css/JSCSSStyleDeclaration.h \
    $(intermediates)/css/JSCSSStyleRule.h \
    $(intermediates)/css/JSCSSStyleSheet.h \
    $(intermediates)/css/JSCSSUnknownRule.h \
    $(intermediates)/css/JSCSSValue.h \
    $(intermediates)/css/JSCSSValueList.h \
    $(intermediates)/css/JSCSSVariablesDeclaration.h \
    $(intermediates)/css/JSCSSVariablesRule.h \
    $(intermediates)/css/JSCounter.h \
    $(intermediates)/css/JSMediaList.h \
    $(intermediates)/css/JSRect.h \
    $(intermediates)/css/JSStyleSheet.h \
    $(intermediates)/css/JSStyleSheetList.h \
    $(intermediates)/css/JSWebKitCSSKeyframeRule.h \
    $(intermediates)/css/JSWebKitCSSKeyframesRule.h \
    $(intermediates)/css/JSWebKitCSSMatrix.h \
    $(intermediates)/css/JSWebKitCSSTransformValue.h 
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/css/JS%.h : $(LOCAL_PATH)/css/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)


# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/css/%.cpp : $(intermediates)/css/%.h

# MANUAL MERGE : I took this out because compiling the result shows:
# out/.../JSEventTarget.cpp: In function 'JSC::JSValue* WebCore::jsEventTargetPrototypeFunctionAddEventListener(JSC::ExecState*, JSC::JSObject*, JSC::JSValue*, const JSC::ArgList&)':
# out/.../JSEventTarget.cpp:90: error: 'toEventListener' was not declared in this scope
# but I can't find toEventListener anywhere, nor can I figure out how toEventListener
# is generated
#    $(intermediates)/dom/JSEventTarget.h \

GEN := \
    $(intermediates)/dom/JSAttr.h \
    $(intermediates)/dom/JSCDATASection.h \
    $(intermediates)/dom/JSCharacterData.h \
    $(intermediates)/dom/JSClientRect.h \
    $(intermediates)/dom/JSClientRectList.h \
    $(intermediates)/dom/JSClipboard.h \
    $(intermediates)/dom/JSComment.h \
    $(intermediates)/dom/JSDOMCoreException.h \
    $(intermediates)/dom/JSDOMImplementation.h \
    $(intermediates)/dom/JSDOMStringList.h \
    $(intermediates)/dom/JSDocument.h \
    $(intermediates)/dom/JSDocumentFragment.h \
    $(intermediates)/dom/JSDocumentType.h \
    $(intermediates)/dom/JSElement.h \
    $(intermediates)/dom/JSEntity.h \
    $(intermediates)/dom/JSEntityReference.h \
    $(intermediates)/dom/JSEvent.h \
    $(intermediates)/dom/JSEventException.h \
    $(intermediates)/dom/JSKeyboardEvent.h \
    $(intermediates)/dom/JSMessageChannel.h \
    $(intermediates)/dom/JSMessageEvent.h \
    $(intermediates)/dom/JSMessagePort.h \
    $(intermediates)/dom/JSMouseEvent.h \
    $(intermediates)/dom/JSMutationEvent.h \
    $(intermediates)/dom/JSNamedNodeMap.h \
    $(intermediates)/dom/JSNode.h \
    $(intermediates)/dom/JSNodeFilter.h \
    $(intermediates)/dom/JSNodeIterator.h \
    $(intermediates)/dom/JSNodeList.h \
    $(intermediates)/dom/JSNotation.h \
    $(intermediates)/dom/JSOverflowEvent.h \
    $(intermediates)/dom/JSProcessingInstruction.h \
    $(intermediates)/dom/JSProgressEvent.h \
    $(intermediates)/dom/JSRange.h \
    $(intermediates)/dom/JSRangeException.h \
    $(intermediates)/dom/JSText.h \
    $(intermediates)/dom/JSTextEvent.h \
    $(intermediates)/dom/JSTouch.h \
    $(intermediates)/dom/JSTouchEvent.h \
    $(intermediates)/dom/JSTouchList.h \
    $(intermediates)/dom/JSTreeWalker.h \
    $(intermediates)/dom/JSUIEvent.h \
    $(intermediates)/dom/JSWebKitAnimationEvent.h \
    $(intermediates)/dom/JSWebKitTransitionEvent.h \
    $(intermediates)/dom/JSWheelEvent.h
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/dom/JS%.h : $(LOCAL_PATH)/dom/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/dom/%.cpp : $(intermediates)/dom/%.h


GEN := \
    $(intermediates)/html/JSCanvasGradient.h \
    $(intermediates)/html/JSCanvasPattern.h \
    $(intermediates)/html/JSCanvasRenderingContext2D.h \
    $(intermediates)/html/JSFile.h \
    $(intermediates)/html/JSFileList.h \
    $(intermediates)/html/JSHTMLAnchorElement.h \
    $(intermediates)/html/JSHTMLAppletElement.h \
    $(intermediates)/html/JSHTMLAreaElement.h \
    $(intermediates)/html/JSHTMLBRElement.h \
    $(intermediates)/html/JSHTMLBaseElement.h \
    $(intermediates)/html/JSHTMLBaseFontElement.h \
    $(intermediates)/html/JSHTMLBlockquoteElement.h \
    $(intermediates)/html/JSHTMLBodyElement.h \
    $(intermediates)/html/JSHTMLButtonElement.h \
    $(intermediates)/html/JSHTMLCanvasElement.h \
    $(intermediates)/html/JSHTMLCollection.h \
    $(intermediates)/html/JSHTMLDListElement.h \
    $(intermediates)/html/JSHTMLDirectoryElement.h \
    $(intermediates)/html/JSHTMLDivElement.h \
    $(intermediates)/html/JSHTMLDocument.h \
    $(intermediates)/html/JSHTMLElement.h \
    $(intermediates)/html/JSHTMLEmbedElement.h \
    $(intermediates)/html/JSHTMLFieldSetElement.h \
    $(intermediates)/html/JSHTMLFontElement.h \
    $(intermediates)/html/JSHTMLFormElement.h \
    $(intermediates)/html/JSHTMLFrameElement.h \
    $(intermediates)/html/JSHTMLFrameSetElement.h \
    $(intermediates)/html/JSHTMLHRElement.h \
    $(intermediates)/html/JSHTMLHeadElement.h \
    $(intermediates)/html/JSHTMLHeadingElement.h \
    $(intermediates)/html/JSHTMLHtmlElement.h \
    $(intermediates)/html/JSHTMLIFrameElement.h \
    $(intermediates)/html/JSHTMLImageElement.h \
    $(intermediates)/html/JSHTMLInputElement.h \
    $(intermediates)/html/JSHTMLIsIndexElement.h \
    $(intermediates)/html/JSHTMLLIElement.h \
    $(intermediates)/html/JSHTMLLabelElement.h \
    $(intermediates)/html/JSHTMLLegendElement.h \
    $(intermediates)/html/JSHTMLLinkElement.h \
    $(intermediates)/html/JSHTMLMapElement.h \
    $(intermediates)/html/JSHTMLMarqueeElement.h \
    $(intermediates)/html/JSHTMLMenuElement.h \
    $(intermediates)/html/JSHTMLMetaElement.h \
    $(intermediates)/html/JSHTMLModElement.h \
    $(intermediates)/html/JSHTMLOListElement.h \
    $(intermediates)/html/JSHTMLObjectElement.h \
    $(intermediates)/html/JSHTMLOptGroupElement.h \
    $(intermediates)/html/JSHTMLOptionElement.h \
    $(intermediates)/html/JSHTMLOptionsCollection.h \
    $(intermediates)/html/JSHTMLParagraphElement.h \
    $(intermediates)/html/JSHTMLParamElement.h \
    $(intermediates)/html/JSHTMLPreElement.h \
    $(intermediates)/html/JSHTMLQuoteElement.h \
    $(intermediates)/html/JSHTMLScriptElement.h \
    $(intermediates)/html/JSHTMLSelectElement.h \
    $(intermediates)/html/JSHTMLSourceElement.h \
    $(intermediates)/html/JSHTMLStyleElement.h \
    $(intermediates)/html/JSHTMLTableCaptionElement.h \
    $(intermediates)/html/JSHTMLTableCellElement.h \
    $(intermediates)/html/JSHTMLTableColElement.h \
    $(intermediates)/html/JSHTMLTableElement.h \
    $(intermediates)/html/JSHTMLTableRowElement.h \
    $(intermediates)/html/JSHTMLTableSectionElement.h \
    $(intermediates)/html/JSHTMLTextAreaElement.h \
    $(intermediates)/html/JSHTMLTitleElement.h \
    $(intermediates)/html/JSHTMLUListElement.h \
    $(intermediates)/html/JSHTMLVideoElement.h \
    $(intermediates)/html/JSImageData.h \
    $(intermediates)/html/JSMediaError.h \
    $(intermediates)/html/JSTextMetrics.h \
    $(intermediates)/html/JSTimeRanges.h \
    $(intermediates)/html/JSVoidCallback.h 

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/html/JS%.h : $(LOCAL_PATH)/html/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/html/%.cpp : $(intermediates)/html/%.h

GEN := \
    $(intermediates)/inspector/JSJavaScriptCallFrame.h 
    
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/inspector/JS%.h : $(LOCAL_PATH)/inspector/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/inspector/%.cpp : $(intermediates)/inspector/%.h

GEN := \
    $(intermediates)/loader/appcache/JSDOMApplicationCache.h 
    
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/loader/appcache/JS%.h : $(LOCAL_PATH)/loader/appcache/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/loader/appcache/%.cpp : $(intermediates)/loader/appcache/%.h

# MANUAL MERGE : I took this out because compiling the result shows:
# out/.../JSAbstractView.cpp:27:26: error: AbstractView.h: No such file or directory
# I can't find AbstractView.h anywhere
#    $(intermediates)/page/JSAbstractView.h \
#
# also:
#
# out/.../JSPositionCallback.cpp:145: error: no matching function for call to 'WebCore::PositionCallback::handleEvent(WebCore::Geoposition*&)'
#  note: candidates are: virtual void WebCore::PositionCallback::handleEvent(WebCore::Geoposition*, bool&)
#    $(intermediates)/page/JSPositionCallback.h \

GEN := \
    $(intermediates)/page/JSBarInfo.h \
    $(intermediates)/page/JSConsole.h \
    $(intermediates)/page/JSCoordinates.h \
    $(intermediates)/page/JSDOMSelection.h \
    $(intermediates)/page/JSDOMWindow.h \
    $(intermediates)/page/JSGeolocation.h \
    $(intermediates)/page/JSGeoposition.h \
    $(intermediates)/page/JSHistory.h \
    $(intermediates)/page/JSLocation.h \
    $(intermediates)/page/JSNavigator.h \
    $(intermediates)/page/JSPositionError.h \
    $(intermediates)/page/JSPositionErrorCallback.h \
    $(intermediates)/page/JSScreen.h \
    $(intermediates)/page/JSWebKitPoint.h
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/page/JS%.h : $(LOCAL_PATH)/page/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/page/%.cpp : $(intermediates)/page/%.h

GEN := \
    $(intermediates)/plugins/JSMimeType.h \
    $(intermediates)/plugins/JSMimeTypeArray.h \
    $(intermediates)/plugins/JSPlugin.h \
    $(intermediates)/plugins/JSPluginArray.h 
    
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/plugins/JS%.h : $(LOCAL_PATH)/plugins/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/plugins/%.cpp : $(intermediates)/plugins/%.h

#new section for svg
ifeq ($(ENABLE_SVG), true)
GEN := \
    $(intermediates)/svg/JSSVGAElement.h \
    $(intermediates)/svg/JSSVGAltGlyphElement.h \
    $(intermediates)/svg/JSSVGAngle.h \
    $(intermediates)/svg/JSSVGAnimateColorElement.h \
    $(intermediates)/svg/JSSVGAnimateElement.h \
    $(intermediates)/svg/JSSVGAnimateTransformElement.h \
    $(intermediates)/svg/JSSVGAnimatedAngle.h \
    $(intermediates)/svg/JSSVGAnimatedBoolean.h \
    $(intermediates)/svg/JSSVGAnimatedEnumeration.h \
    $(intermediates)/svg/JSSVGAnimatedInteger.h \
    $(intermediates)/svg/JSSVGAnimatedLength.h \
    $(intermediates)/svg/JSSVGAnimatedLengthList.h \
    $(intermediates)/svg/JSSVGAnimatedNumber.h \
    $(intermediates)/svg/JSSVGAnimatedNumberList.h \
    $(intermediates)/svg/JSSVGAnimatedPreserveAspectRatio.h \
    $(intermediates)/svg/JSSVGAnimatedRect.h \
    $(intermediates)/svg/JSSVGAnimatedString.h \
    $(intermediates)/svg/JSSVGAnimatedTransformList.h \
    $(intermediates)/svg/JSSVGAnimationElement.h \
    $(intermediates)/svg/JSSVGCircleElement.h \
    $(intermediates)/svg/JSSVGClipPathElement.h \
    $(intermediates)/svg/JSSVGColor.h \
    $(intermediates)/svg/JSSVGComponentTransferFunctionElement.h \
    $(intermediates)/svg/JSSVGCursorElement.h \
    $(intermediates)/svg/JSSVGDefinitionSrcElement.h \
    $(intermediates)/svg/JSSVGDefsElement.h \
    $(intermediates)/svg/JSSVGDescElement.h \
    $(intermediates)/svg/JSSVGDocument.h \
    $(intermediates)/svg/JSSVGElement.h \
    $(intermediates)/svg/JSSVGElementInstance.h \
    $(intermediates)/svg/JSSVGElementInstanceList.h \
    $(intermediates)/svg/JSSVGEllipseElement.h \
    $(intermediates)/svg/JSSVGException.h \
    $(intermediates)/svg/JSSVGFEBlendElement.h \
    $(intermediates)/svg/JSSVGFEColorMatrixElement.h \
    $(intermediates)/svg/JSSVGFEComponentTransferElement.h \
    $(intermediates)/svg/JSSVGFECompositeElement.h \
    $(intermediates)/svg/JSSVGFEDiffuseLightingElement.h \
    $(intermediates)/svg/JSSVGFEDisplacementMapElement.h \
    $(intermediates)/svg/JSSVGFEDistantLightElement.h \
    $(intermediates)/svg/JSSVGFEFloodElement.h \
    $(intermediates)/svg/JSSVGFEFuncAElement.h \
    $(intermediates)/svg/JSSVGFEFuncBElement.h \
    $(intermediates)/svg/JSSVGFEFuncGElement.h \
    $(intermediates)/svg/JSSVGFEFuncRElement.h \
    $(intermediates)/svg/JSSVGFEGaussianBlurElement.h \
    $(intermediates)/svg/JSSVGFEImageElement.h \
    $(intermediates)/svg/JSSVGFEMergeElement.h \
    $(intermediates)/svg/JSSVGFEMergeNodeElement.h \
    $(intermediates)/svg/JSSVGFEOffsetElement.h \
    $(intermediates)/svg/JSSVGFEPointLightElement.h \
    $(intermediates)/svg/JSSVGFESpecularLightingElement.h \
    $(intermediates)/svg/JSSVGFESpotLightElement.h \
    $(intermediates)/svg/JSSVGFETileElement.h \
    $(intermediates)/svg/JSSVGFETurbulenceElement.h \
    $(intermediates)/svg/JSSVGFilterElement.h \
    $(intermediates)/svg/JSSVGFontElement.h \
    $(intermediates)/svg/JSSVGFontFaceElement.h \
    $(intermediates)/svg/JSSVGFontFaceFormatElement.h \
    $(intermediates)/svg/JSSVGFontFaceNameElement.h \
    $(intermediates)/svg/JSSVGFontFaceSrcElement.h \
    $(intermediates)/svg/JSSVGFontFaceUriElement.h \
    $(intermediates)/svg/JSSVGForeignObjectElement.h \
    $(intermediates)/svg/JSSVGGElement.h \
    $(intermediates)/svg/JSSVGGlyphElement.h \
    $(intermediates)/svg/JSSVGGradientElement.h \
    $(intermediates)/svg/JSSVGHKernElement.h \
    $(intermediates)/svg/JSSVGImageElement.h \
    $(intermediates)/svg/JSSVGLength.h \
    $(intermediates)/svg/JSSVGLengthList.h \
    $(intermediates)/svg/JSSVGLineElement.h \
    $(intermediates)/svg/JSSVGLinearGradientElement.h \
    $(intermediates)/svg/JSSVGMarkerElement.h \
    $(intermediates)/svg/JSSVGMaskElement.h \
    $(intermediates)/svg/JSSVGMatrix.h \
    $(intermediates)/svg/JSSVGMetadataElement.h \
    $(intermediates)/svg/JSSVGMissingGlyphElement.h \
    $(intermediates)/svg/JSSVGNumber.h \
    $(intermediates)/svg/JSSVGNumberList.h \
    $(intermediates)/svg/JSSVGPaint.h \
    $(intermediates)/svg/JSSVGPathElement.h \
    $(intermediates)/svg/JSSVGPathSeg.h \
    $(intermediates)/svg/JSSVGPathSegArcAbs.h \
    $(intermediates)/svg/JSSVGPathSegArcRel.h \
    $(intermediates)/svg/JSSVGPathSegClosePath.h \
    $(intermediates)/svg/JSSVGPathSegCurvetoCubicAbs.h \
    $(intermediates)/svg/JSSVGPathSegCurvetoCubicRel.h \
    $(intermediates)/svg/JSSVGPathSegCurvetoCubicSmoothAbs.h \
    $(intermediates)/svg/JSSVGPathSegCurvetoCubicSmoothRel.h \
    $(intermediates)/svg/JSSVGPathSegCurvetoQuadraticAbs.h \
    $(intermediates)/svg/JSSVGPathSegCurvetoQuadraticRel.h \
    $(intermediates)/svg/JSSVGPathSegCurvetoQuadraticSmoothAbs.h \
    $(intermediates)/svg/JSSVGPathSegCurvetoQuadraticSmoothRel.h \
    $(intermediates)/svg/JSSVGPathSegLinetoAbs.h \
    $(intermediates)/svg/JSSVGPathSegLinetoHorizontalAbs.h \
    $(intermediates)/svg/JSSVGPathSegLinetoHorizontalRel.h \
    $(intermediates)/svg/JSSVGPathSegLinetoRel.h \
    $(intermediates)/svg/JSSVGPathSegLinetoVerticalAbs.h \
    $(intermediates)/svg/JSSVGPathSegLinetoVerticalRel.h \
    $(intermediates)/svg/JSSVGPathSegList.h \
    $(intermediates)/svg/JSSVGPathSegMovetoAbs.h \
    $(intermediates)/svg/JSSVGPathSegMovetoRel.h \
    $(intermediates)/svg/JSSVGPatternElement.h \
    $(intermediates)/svg/JSSVGPoint.h \
    $(intermediates)/svg/JSSVGPointList.h \
    $(intermediates)/svg/JSSVGPolygonElement.h \
    $(intermediates)/svg/JSSVGPolylineElement.h \
    $(intermediates)/svg/JSSVGPreserveAspectRatio.h \
    $(intermediates)/svg/JSSVGRadialGradientElement.h \
    $(intermediates)/svg/JSSVGRect.h \
    $(intermediates)/svg/JSSVGRectElement.h \
    $(intermediates)/svg/JSSVGRenderingIntent.h \
    $(intermediates)/svg/JSSVGSVGElement.h \
    $(intermediates)/svg/JSSVGScriptElement.h \
    $(intermediates)/svg/JSSVGSetElement.h \
    $(intermediates)/svg/JSSVGStopElement.h \
    $(intermediates)/svg/JSSVGStringList.h \
    $(intermediates)/svg/JSSVGStyleElement.h \
    $(intermediates)/svg/JSSVGSwitchElement.h \
    $(intermediates)/svg/JSSVGSymbolElement.h \
    $(intermediates)/svg/JSSVGTRefElement.h \
    $(intermediates)/svg/JSSVGTSpanElement.h \
    $(intermediates)/svg/JSSVGTextContentElement.h \
    $(intermediates)/svg/JSSVGTextElement.h \
    $(intermediates)/svg/JSSVGTextPathElement.h \
    $(intermediates)/svg/JSSVGTextPositioningElement.h \
    $(intermediates)/svg/JSSVGTitleElement.h \
    $(intermediates)/svg/JSSVGTransform.h \
    $(intermediates)/svg/JSSVGTransformList.h \
    $(intermediates)/svg/JSSVGUnitTypes.h \
    $(intermediates)/svg/JSSVGUseElement.h \
    $(intermediates)/svg/JSSVGViewElement.h \
    $(intermediates)/svg/JSSVGZoomEvent.h
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include external/webkit/WebCore/dom --include external/webkit/WebCore/html --include external/webkit/WebCore/svg --outputdir $(dir $@) $<
$(GEN): $(intermediates)/svg/JS%.h : $(LOCAL_PATH)/svg/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/svg/%.cpp : $(intermediates)/svg/%.h
endif

#new section for xml/DOMParser.idl
GEN := \
    $(intermediates)/xml/JSDOMParser.h \
    $(intermediates)/xml/JSXMLHttpRequest.h \
    $(intermediates)/xml/JSXMLHttpRequestException.h \
    $(intermediates)/xml/JSXMLHttpRequestProgressEvent.h \
    $(intermediates)/xml/JSXMLHttpRequestUpload.h \
    $(intermediates)/xml/JSXMLSerializer.h \
    $(intermediates)/xml/JSXPathEvaluator.h \
    $(intermediates)/xml/JSXPathException.h \
    $(intermediates)/xml/JSXPathExpression.h \
    $(intermediates)/xml/JSXPathNSResolver.h \
    $(intermediates)/xml/JSXPathResult.h  \
    $(intermediates)/xml/JSXSLTProcessor.h 
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/xml/JS%.h : $(LOCAL_PATH)/xml/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/xml/%.cpp : $(intermediates)/xml/%.h
#end

# HTML tag and attribute names

GEN:= $(intermediates)/HTMLNames.cpp $(intermediates)/HTMLElementFactory.cpp  $(intermediates)/JSHTMLElementWrapperFactory.cpp
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(PRIVATE_PATH)/bindings/scripts $< --tags $(html_tags) --attrs $(html_attrs) --factory --wrapperFactory --output $(dir $@) 
$(GEN): html_tags := $(LOCAL_PATH)/html/HTMLTagNames.in
$(GEN): html_attrs := $(LOCAL_PATH)/html/HTMLAttributeNames.in
$(GEN): $(LOCAL_PATH)/dom/make_names.pl $(html_tags) $(html_attrs)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# SVG tag and attribute names

ifeq ($(ENABLE_SVG), true)
GEN:= $(intermediates)/SVGNames.cpp  $(intermediates)/SVGElementFactory.cpp $(intermediates)/JSSVGElementWrapperFactory.cpp
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(PRIVATE_PATH)/bindings/scripts $< --tags $(svg_tags) --attrs $(svg_attrs) --factory --wrapperFactory --output $(dir $@) 
$(GEN): svg_tags := $(LOCAL_PATH)/svg/svgtags.in
$(GEN): svg_attrs := $(LOCAL_PATH)/svg/svgattrs.in
$(GEN): $(LOCAL_PATH)/dom/make_names.pl $(svg_tags) $(svg_attrs)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)
endif

# XML attribute names

GEN:= $(intermediates)/XMLNames.cpp
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(PRIVATE_PATH)/bindings/scripts $< --attrs $(xml_attrs) --output $(dir $@) 
$(GEN): xml_attrs := $(LOCAL_PATH)/xml/xmlattrs.in
$(GEN): $(LOCAL_PATH)/dom/make_names.pl $(xml_attrs)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# XLink attribute names

ifeq ($(ENABLE_SVG), true)
GEN:= $(intermediates)/XLinkNames.cpp
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(PRIVATE_PATH)/bindings/scripts $< --attrs $(xlink_attrs) --output $(dir $@) 
$(GEN): xlink_attrs := $(LOCAL_PATH)/svg/xlinkattrs.in
$(GEN): $(LOCAL_PATH)/dom/make_names.pl $(xlink_attrs)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)
endif

