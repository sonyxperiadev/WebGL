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

# This comment block is read by tools/webkitsync/diff.cpp
# Don't remove it or move it. 
#
# The following files are intentionally not included
# LOCAL_SRC_FILES_EXCLUDED := \
#	css/RGBColor.idl \
#	dom/EventListener.idl \
#	dom/EventTarget.idl \
#	html/CanvasPixelArray.idl \
#	page/AbstractView.idl \
#	svg/ElementTimeControl.idl \
#	svg/SVGAnimatedPathData.idl \
#	svg/SVGAnimatedPoints.idl \
#	svg/SVGExternalResourcesRequired.idl \
#	svg/SVGFilterPrimitiveStandardAttributes.idl \
#	svg/SVGFitToViewBox.idl \
#	svg/SVGLangSpace.idl \
#	svg/SVGLocatable.idl \
#	svg/SVGStylable.idl \
#	svg/SVGTests.idl \
#	svg/SVGTransformable.idl \
#	svg/SVGURIReference.idl \
#	svg/SVGViewSpec.idl \
#	svg/SVGZoomAndPan.idl \

# This comment block is read by tools/webkitsync/diff.cpp
# Don't remove it or move it. 
#
# The following files are intentionally not generated
# LOCAL_GENERATED_FILES_EXCLUDED := \
#	WMLElementFactory.cpp \
#	WMLNames.cpp \
#	XLinkNames.cpp \

# This comment block is read by tools/webkitsync/diff.cpp
# Don't remove it or move it. 
#
# The following directory wildcard matches are intentionally not included
# If an entry starts with '/', any subdirectory may match
# If an entry starts with '^', the first directory must match
# LOCAL_DIR_WILDCARD_EXCLUDED :=
#

# This comment block is read by tools/webkitsync/diff.cpp
# Don't remove it or move it. 
# If you edit it, keep it in alphabetical order
#
# These files are Android extensions
# LOCAL_ANDROID_SRC_FILES_INCLUDED := \
#	dom/Touch*.idl \

LOCAL_SRC_FILES :=
# CSS property names and value keywords

GEN := $(intermediates)/css/CSSPropertyNames.h
$(GEN): SCRIPT := $(LOCAL_PATH)/css/makeprop.pl
$(GEN): $(intermediates)/%.h : $(LOCAL_PATH)/%.in $(LOCAL_PATH)/css/SVGCSSPropertyNames.in
	@echo "Generating CSSPropertyNames.h <= CSSPropertyNames.in"
	@mkdir -p $(dir $@)
	@cat $< > $(dir $@)/$(notdir $<)
ifeq ($(ENABLE_SVG),true)
	@cat $^ > $(@:%.h=%.in)
endif
	@cp -f $(SCRIPT) $(dir $@)
	@cd $(dir $@) ; perl ./$(notdir $(SCRIPT))
LOCAL_GENERATED_SOURCES += $(GEN)

GEN := $(intermediates)/css/CSSValueKeywords.h
$(GEN): SCRIPT := $(LOCAL_PATH)/css/makevalues.pl
$(GEN): $(intermediates)/%.h : $(LOCAL_PATH)/%.in $(LOCAL_PATH)/css/SVGCSSValueKeywords.in
	@echo "Generating CSSValueKeywords.h <= CSSValueKeywords.in"
	@mkdir -p $(dir $@)
	@cp -f $(SCRIPT) $(dir $@)
ifeq ($(ENABLE_SVG),true)    
	@perl -ne 'print lc' $^ > $(@:%.h=%.in)
else
	@perl -ne 'print lc' $< > $(@:%.h=%.in)
endif
	@cd $(dir $@); perl makevalues.pl
LOCAL_GENERATED_SOURCES += $(GEN)


# DOCTYPE strings

GEN := $(intermediates)/html/DocTypeStrings.cpp
$(GEN): PRIVATE_CUSTOM_TOOL = gperf -CEot -L ANSI-C -k "*" -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards $< > $@
$(GEN): $(LOCAL_PATH)/html/DocTypeStrings.gperf
	$(transform-generated-source)
# we have to do this dep by hand:
$(intermediates)/html/HTMLDocument.o : $(GEN)


# HTML entity names

GEN := $(intermediates)/html/HTMLEntityNames.c
$(GEN): PRIVATE_CUSTOM_TOOL = gperf -a -L ANSI-C -C -G -c -o -t -k '*' -N findEntity -D -s 2 $< > $@
$(GEN): $(LOCAL_PATH)/html/HTMLEntityNames.gperf
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)


# color names

GEN := $(intermediates)/platform/ColorData.c
$(GEN): PRIVATE_CUSTOM_TOOL = gperf -CDEot -L ANSI-C -k '*' -N findColor -D -s 2 $< > $@
$(GEN): $(LOCAL_PATH)/platform/ColorData.gperf
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)


# CSS tokenizer

GEN := $(intermediates)/css/tokenizer.cpp
$(GEN): PRIVATE_CUSTOM_TOOL = $(OLD_FLEX) -t $< | perl $(dir $<)/maketokenizer > $@
$(GEN): $(LOCAL_PATH)/css/tokenizer.flex $(LOCAL_PATH)/css/maketokenizer
	$(transform-generated-source)
# we have to do this dep by hand:
$(intermediates)/css/CSSParser.o : $(GEN)

# CSS grammar

GEN := $(intermediates)/CSSGrammar.cpp
$(GEN) : PRIVATE_YACCFLAGS := -p cssyy
$(GEN): $(LOCAL_PATH)/css/CSSGrammar.y
	$(call local-transform-y-to-cpp,.cpp)
$(GEN): $(LOCAL_BISON)

LOCAL_GENERATED_SOURCES += $(GEN)

# XPath grammar

GEN := $(intermediates)/XPathGrammar.cpp
$(GEN) : PRIVATE_YACCFLAGS := -p xpathyy
$(GEN): $(LOCAL_PATH)/xml/XPathGrammar.y
	$(call local-transform-y-to-cpp,.cpp)
$(GEN): $(LOCAL_BISON)

LOCAL_GENERATED_SOURCES += $(GEN)
	                         
# user agent style sheets

style_sheets := $(LOCAL_PATH)/css/html.css $(LOCAL_PATH)/css/quirks.css $(LOCAL_PATH)/css/view-source.css $(LOCAL_PATH)/css/mediaControls.css
ifeq ($(ENABLE_SVG), true)
style_sheets := $(style_sheets) $(LOCAL_PATH)/css/svg.css
endif
GEN := $(intermediates)/css/UserAgentStyleSheets.h
make_css_file_arrays := $(LOCAL_PATH)/css/make-css-file-arrays.pl
$(GEN): PRIVATE_CUSTOM_TOOL = $< $@ $(basename $@).cpp $(filter %.css,$^)
$(GEN): $(make_css_file_arrays) $(style_sheets)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# character set name table

#gen_inputs := \
		$(LOCAL_PATH)/platform/make-charset-table.pl \
		$(LOCAL_PATH)/platform/character-sets.txt \
		$(LOCAL_PATH)/platform/android/android-encodings.txt
#GEN := $(intermediates)/platform/CharsetData.cpp
#$(GEN): PRIVATE_CUSTOM_TOOL = $^ "android::Encoding::ENCODING_" > $@
#$(GEN): $(gen_inputs)
#	$(transform-generated-source)
#LOCAL_GENERATED_SOURCES += $(GEN)

# the above rule will make this build too
$(intermediates)/css/UserAgentStyleSheets.cpp : $(GEN)


# lookup tables for old-style JavaScript bindings
create_hash_table := $(LOCAL_PATH)/../JavaScriptCore/create_hash_table

GEN := $(addprefix $(intermediates)/, \
			bindings/js/JSDOMWindowBase.lut.h \
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

FEATURE_DEFINES := ANDROID_ORIENTATION_SUPPORT ENABLE_TOUCH_EVENTS=1 ENABLE_DATABASE=1 ENABLE_OFFLINE_WEB_APPLICATIONS=1 ENABLE_DOM_STORAGE=1 ENABLE_VIDEO=1 ENABLE_WORKERS=1 ENABLE_GEOLOCATION=1 ENABLE_CHANNEL_MESSAGING=1

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
    $(intermediates)/css/JSMedia.h \
    $(intermediates)/css/JSMediaList.h \
    $(intermediates)/css/JSRGBColor.h \
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
    $(intermediates)/dom/JSDocument.h \
    $(intermediates)/dom/JSDocumentFragment.h \
    $(intermediates)/dom/JSDocumentType.h \
    $(intermediates)/dom/JSElement.h \
    $(intermediates)/dom/JSEntity.h \
    $(intermediates)/dom/JSEntityReference.h \
    $(intermediates)/dom/JSErrorEvent.h \
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
    $(intermediates)/html/JSDataGridColumn.h \
    $(intermediates)/html/JSDataGridColumnList.h \
    $(intermediates)/html/JSFile.h \
    $(intermediates)/html/JSFileList.h \
    $(intermediates)/html/JSHTMLAnchorElement.h \
    $(intermediates)/html/JSHTMLAppletElement.h \
    $(intermediates)/html/JSHTMLAreaElement.h \
    $(intermediates)/html/JSHTMLAudioElement.h \
    $(intermediates)/html/JSHTMLBRElement.h \
    $(intermediates)/html/JSHTMLBaseElement.h \
    $(intermediates)/html/JSHTMLBaseFontElement.h \
    $(intermediates)/html/JSHTMLBlockquoteElement.h \
    $(intermediates)/html/JSHTMLBodyElement.h \
    $(intermediates)/html/JSHTMLButtonElement.h \
    $(intermediates)/html/JSHTMLCanvasElement.h \
	$(intermediates)/html/JSHTMLCollection.h \
    $(intermediates)/html/JSHTMLDataGridElement.h \
    $(intermediates)/html/JSHTMLDataGridCellElement.h \
    $(intermediates)/html/JSHTMLDataGridColElement.h \
    $(intermediates)/html/JSHTMLDataGridRowElement.h \
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
    $(intermediates)/html/JSHTMLMediaElement.h \
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
    $(intermediates)/html/JSValidityState.h \
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
	$(intermediates)/html/canvas/JSCanvasGradient.h \
	$(intermediates)/html/canvas/JSCanvasPattern.h \
    $(intermediates)/html/canvas/JSCanvasRenderingContext2D.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/html/canvas/JS%.h : $(LOCAL_PATH)/html/canvas/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/html/canvas/%.cpp : $(intermediates)/html/canvas/%.h

GEN := \
	$(intermediates)/inspector/JSInspectorBackend.h  \
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
    $(intermediates)/page/JSScreen.h \
    $(intermediates)/page/JSWebKitPoint.h \
    $(intermediates)/page/JSWorkerNavigator.h

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

# New section for Database storage API
GEN := \
    $(intermediates)/storage/JSDatabase.h \
    $(intermediates)/storage/JSSQLError.h \
    $(intermediates)/storage/JSSQLResultSet.h \
    $(intermediates)/storage/JSSQLResultSetRowList.h \
    $(intermediates)/storage/JSSQLTransaction.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/storage/JS%.h : $(LOCAL_PATH)/storage/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/storage/%.cpp : $(intermediates)/storage/%.h

# new section for DOM Storage APIs
GEN := \
    $(intermediates)/storage/JSStorage.h \
    $(intermediates)/storage/JSStorageEvent.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/storage/JS%.h : $(LOCAL_PATH)/storage/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/storage/%.cpp : $(intermediates)/storage/%.h

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

# new section for Workers
GEN := \
    $(intermediates)/workers/JSAbstractWorker.h \
    $(intermediates)/workers/JSDedicatedWorkerContext.h \
    $(intermediates)/workers/JSSharedWorker.h \
    $(intermediates)/workers/JSSharedWorkerContext.h \
	$(intermediates)/workers/JSWorker.h \
    $(intermediates)/workers/JSWorkerContext.h \
    $(intermediates)/workers/JSWorkerLocation.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/workers/JS%.h : $(LOCAL_PATH)/workers/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/workers/%.cpp : $(intermediates)/workers/%.h

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
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(PRIVATE_PATH)/bindings/scripts $< --tags $(html_tags) --attrs $(html_attrs)  --extraDefines "$(FEATURE_DEFINES)" --factory --wrapperFactory --output $(dir $@)
$(GEN): html_tags := $(LOCAL_PATH)/html/HTMLTagNames.in
$(GEN): html_attrs := $(LOCAL_PATH)/html/HTMLAttributeNames.in
$(GEN): $(LOCAL_PATH)/dom/make_names.pl $(html_tags) $(html_attrs)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# SVG tag and attribute names

ifeq ($(ENABLE_SVG), true)
GEN:= $(intermediates)/SVGNames.cpp  $(intermediates)/SVGElementFactory.cpp $(intermediates)/JSSVGElementWrapperFactory.cpp
SVG_FLAGS:=ENABLE_SVG_AS_IMAGE=1 ENABLE_SVG_FILTERS=1 ENABLE_SVG_FONTS=1 ENABLE_SVG_FOREIGN_OBJECT=1 ENABLE_SVG_USE=1
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(PRIVATE_PATH)/bindings/scripts $< --tags $(svg_tags) --attrs $(svg_attrs) --extraDefines "$(SVG_FLAGS)" --factory --wrapperFactory --output $(dir $@) 
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
