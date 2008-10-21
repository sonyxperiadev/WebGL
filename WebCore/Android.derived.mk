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

# CSS property names and value keywords

GEN := $(intermediates)/css/CSSPropertyNames.h
$(GEN): SCRIPT := $(LOCAL_PATH)/css/makeprop.pl
$(GEN): $(intermediates)/%.h : $(LOCAL_PATH)/%.in
	@echo "Generating CSSPropertyNames.h <= CSSPropertyNames.in"
	@mkdir -p $(dir $@)
	@cp -f $< $(dir $@)
	@cp -f $(SCRIPT) $(dir $@)
	@cd $(dir $@) ; perl ./$(notdir $(SCRIPT))
LOCAL_GENERATED_SOURCES += $(GEN)

GEN := $(intermediates)/css/CSSValueKeywords.h
$(GEN): SCRIPT := $(LOCAL_PATH)/css/makevalues.pl
$(GEN): $(intermediates)/%.h : $(LOCAL_PATH)/%.in
	@echo "Generating CSSValueKeywords.h <= CSSValueKeywords.in"
	@mkdir -p $(dir $@)
	@cp -f $< $(dir $@)
	@cp -f $(SCRIPT) $(dir $@)
	@cd $(dir $@) ; perl ./$(notdir $(SCRIPT))
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
	$(call transform-y-to-cpp,.cpp)

LOCAL_GENERATED_SOURCES += $(GEN)

# XPath grammar

GEN := $(intermediates)/XPathGrammar.cpp
$(GEN) : PRIVATE_YACCFLAGS := -p xpathyy
$(GEN): $(LOCAL_PATH)/xml/XPathGrammar.y
	$(call transform-y-to-cpp,.cpp)

LOCAL_GENERATED_SOURCES += $(GEN)
	                         
# user agent style sheets

style_sheets := $(LOCAL_PATH)/css/html4.css $(LOCAL_PATH)/css/quirks.css $(LOCAL_PATH)/css/view-source.css
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
create_hash_table := $(LOCAL_PATH)/../JavaScriptCore/kjs/create_hash_table

GEN := $(addprefix $(intermediates)/, \
			bindings/js/JSEventTargetBase.lut.h \
			bindings/js/JSEventTargetNode.lut.h \
			bindings/js/JSLocation.lut.h \
			bindings/js/JSXMLHttpRequest.lut.h \
			bindings/js/kjs_css.lut.h \
			bindings/js/kjs_events.lut.h \
			bindings/js/kjs_navigator.lut.h \
			bindings/js/kjs_window.lut.h \
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
			bindings/scripts/CodeGeneratorJS.pm \
			bindings/scripts/IDLParser.pm \
			bindings/scripts/IDLStructure.pm \
			bindings/scripts/generate-bindings.pl \
		)

FEATURE_DEFINES := ANDROID_ORIENTATION_SUPPORT

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
    $(intermediates)/css/JSCSSValue.h \
    $(intermediates)/css/JSCSSValueList.h \
    $(intermediates)/css/JSCounter.h \
    $(intermediates)/css/JSMediaList.h \
    $(intermediates)/css/JSRect.h \
    $(intermediates)/css/JSStyleSheet.h \
    $(intermediates)/css/JSStyleSheetList.h 
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/css/JS%.h : $(LOCAL_PATH)/css/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)


# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/css/%.cpp : $(intermediates)/css/%.h


GEN := \
    $(intermediates)/dom/JSAttr.h \
    $(intermediates)/dom/JSCDATASection.h \
    $(intermediates)/dom/JSCharacterData.h \
    $(intermediates)/dom/JSComment.h \
    $(intermediates)/dom/JSDOMCoreException.h \
    $(intermediates)/dom/JSDOMImplementation.h \
    $(intermediates)/dom/JSDocument.h \
    $(intermediates)/dom/JSDocumentFragment.h \
    $(intermediates)/dom/JSDocumentType.h \
    $(intermediates)/dom/JSElement.h \
    $(intermediates)/dom/JSEntity.h \
    $(intermediates)/dom/JSEntityReference.h \
    $(intermediates)/dom/JSEvent.h \
    $(intermediates)/dom/JSEventException.h \
    $(intermediates)/dom/JSKeyboardEvent.h \
    $(intermediates)/dom/JSMessageEvent.h \
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
    $(intermediates)/dom/JSTreeWalker.h \
    $(intermediates)/dom/JSUIEvent.h \
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
    $(intermediates)/html/JSMediaError.h \
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
    $(intermediates)/page/JSBarInfo.h \
    $(intermediates)/page/JSConsole.h \
    $(intermediates)/page/JSDOMSelection.h \
    $(intermediates)/page/JSDOMWindow.h \
    $(intermediates)/page/JSHistory.h \
    $(intermediates)/page/JSScreen.h 
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/page/JS%.h : $(LOCAL_PATH)/page/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/page/%.cpp : $(intermediates)/page/%.h

#new section for xml/DOMParser.idl
GEN := \
    $(intermediates)/xml/JSDOMParser.h \
    $(intermediates)/xml/JSXMLHttpRequestException.h \
    $(intermediates)/xml/JSXMLSerializer.h \
    $(intermediates)/xml/JSXPathEvaluator.h \
    $(intermediates)/xml/JSXPathExpression.h \
    $(intermediates)/xml/JSXPathNSResolver.h \
    $(intermediates)/xml/JSXPathResult.h 
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

GEN:= $(intermediates)/HTMLNames.cpp
$(GEN): PRIVATE_CUSTOM_TOOL = perl $< --tags $(html_tags) --attrs $(html_attrs) --namespace HTML --namespacePrefix xhtml --cppNamespace WebCore --namespaceURI "http://www.w3.org/1999/xhtml" --attrsNullNamespace --output $(dir $@) 
$(GEN): html_tags := $(LOCAL_PATH)/html/HTMLTagNames.in
$(GEN): html_attrs := $(LOCAL_PATH)/html/HTMLAttributeNames.in
$(GEN): $(LOCAL_PATH)/dom/make_names.pl $(html_tags) $(html_attrs)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# XML attribute names

GEN:= $(intermediates)/XMLNames.cpp
$(GEN): PRIVATE_CUSTOM_TOOL = perl $< --attrs $(xml_attrs) --namespace XML --cppNamespace WebCore --namespaceURI "http://www.w3.org/XML/1998/namespace" --output $(dir $@) 
$(GEN): xml_attrs := $(LOCAL_PATH)/xml/xmlattrs.in
$(GEN): $(LOCAL_PATH)/dom/make_names.pl $(xml_attrs)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

GEN:= $(intermediates)/ksvgcssproperties.h
$(GEN): 
	@echo > $@
LOCAL_GENERATED_SOURCES += $(GEN)

LOCAL_C_INCLUDES += \
		$(intermediates)/bindings/js \
		$(intermediates)/css \
		$(intermediates)/dom \
		$(intermediates)/html \
		$(intermediates)/page \
		$(intermediates)/platform \
		$(intermediates)/storage \
		$(intermediates)/xml

