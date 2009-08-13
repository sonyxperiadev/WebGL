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
#	dom/Worker*.idl \
#	html/CanvasPixelArray.idl \
#	page/AbstractView.idl \
#	page/Worker*.idl \
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
#	JSWorkerContextBase.lut.h \
#	WMLElementFactory.cpp \
#	WMLNames.cpp \
#	XLinkNames.cpp \

# This comment block is read by tools/webkitsync/diff.cpp
# Don't remove it or move it. 
#
# The following directory wildcard matches are intentionally not included
# If an entry starts with '/', any subdirectory may match
# If an entry starts with '^', the first directory must match
# LOCAL_DIR_WILDCARD_EXCLUDED := \

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

# XML attribute names

GEN:= $(intermediates)/XMLNames.cpp
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(PRIVATE_PATH)/bindings/scripts $< --attrs $(xml_attrs) --output $(dir $@) 
$(GEN): xml_attrs := $(LOCAL_PATH)/xml/xmlattrs.in
$(GEN): $(LOCAL_PATH)/dom/make_names.pl $(xml_attrs)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

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
