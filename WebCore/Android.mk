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

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# Set up the target identity before including Android.derived.mk.
# LOCAL_MODULE/_CLASS are required for local-intermediates-dir to work.
LOCAL_MODULE := libwebcore
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
intermediates := $(call local-intermediates-dir)

include $(LOCAL_PATH)/Android.derived.mk

# This comment block is read by tools/webkitsync/diff.cpp
# Don't remove it or move it. 
# If you edit it, keep it in alphabetical order
#
# The following files are intentionally not included
# LOCAL_SRC_FILES_EXCLUDED := \
#	WebCorePrefix.cpp \
#	bindings/js/JSCustomSQL*.cpp \
#	bindings/js/JSCustomVersionChangeCallback.cpp \
#	bindings/js/JSDatabaseCustom.cpp \
#	bindings/js/JSHTMLAudioElementConstructor.cpp \
#	bindings/js/JSSQLResultSetRowListCustom.cpp \
#	bindings/js/JSSVG*.cpp \
#	bindings/js/JSXSLTProcessor.cpp \
#	css/CSSGrammar.y \
#	editing/SmartReplaceCF.cpp \
#	html/HTMLAudioElement.cpp \
#	html/HTMLMediaElement.cpp \
#	html/HTMLSourceElement.cpp \
#	html/HTMLVideoElement.cpp \
#	loader/CachedResourceClientWalker.cpp \
#	loader/CachedXBLDocument.cpp \
#	loader/CachedXSLStyleSheet.cpp \
#	loader/FTP*.cpp \
#	loader/icon/IconDatabaseNone.cpp \
#	page/InspectorController.cpp \
#	platform/ThreadingNone.cpp \
#	platform/graphics/FloatPoint3D.cpp \
#	rendering/RenderSVG*.cpp \
#	rendering/RenderThemeWin.cpp \
#	rendering/RenderVideo.cpp \
#	rendering/SVG*.cpp \
#	storage/Database*.cpp \
#	storage/SQL*.cpp \
#	xml/NativeXPathNSResolver.cpp \
#	xml/XPath*.cpp \
#	xml/XSL*.cpp \

# This comment block is read by tools/webkitsync/diff.cpp
# Don't remove it or move it. 
# If you edit it, keep it in alphabetical order
#
# The following directory wildcard matches are intentionally not included
# If an entry starts with '/', any subdirectory may match
# If an entry starts with '^', the first directory must match
# LOCAL_DIR_WILDCARD_EXCLUDED := \
#	/cairo/* \
#	/cf/* \
#	/cg/* \
#	/curl/* \
#	/gtk/* \
#	/image-decoders/* \
#	^ksvg2/* \
#	/mac/* \
#	^manual-tests/* \
#	/objc/* \
#	/qt/* \
#	/svg/* \
#	/symbian/* \
#	/win/* \
#	/wx/* \


LOCAL_SRC_FILES := \
	bindings/js/GCController.cpp \
	bindings/js/JSAttrCustom.cpp \
	bindings/js/JSAudioConstructor.cpp \
	bindings/js/JSCSSRuleCustom.cpp \
	bindings/js/JSCSSStyleDeclarationCustom.cpp \
	bindings/js/JSCSSValueCustom.cpp \
	bindings/js/JSCanvasRenderingContext2DCustom.cpp \
	bindings/js/JSCustomVoidCallback.cpp \
	bindings/js/JSCustomXPathNSResolver.cpp \
	bindings/js/JSDOMWindowCustom.cpp \
	bindings/js/JSDocumentCustom.cpp \
	bindings/js/JSElementCustom.cpp \
	bindings/js/JSEventCustom.cpp \
	bindings/js/JSEventTargetBase.cpp \
	bindings/js/JSEventTargetNode.cpp \
	bindings/js/JSHTMLAppletElementCustom.cpp \
	bindings/js/JSHTMLCollectionCustom.cpp \
	bindings/js/JSHTMLDocumentCustom.cpp \
	bindings/js/JSHTMLElementCustom.cpp \
	bindings/js/JSHTMLElementWrapperFactory.cpp \
	bindings/js/JSHTMLEmbedElementCustom.cpp \
	bindings/js/JSHTMLFormElementCustom.cpp \
	bindings/js/JSHTMLFrameElementCustom.cpp \
	bindings/js/JSHTMLFrameSetElementCustom.cpp \
	bindings/js/JSHTMLIFrameElementCustom.cpp \
	bindings/js/JSHTMLInputElementBase.cpp \
	bindings/js/JSHTMLObjectElementCustom.cpp \
	bindings/js/JSHTMLOptionElementConstructor.cpp \
	bindings/js/JSHTMLOptionsCollectionCustom.cpp \
	bindings/js/JSHTMLSelectElementCustom.cpp \
	bindings/js/JSHistoryCustom.cpp \
	bindings/js/JSLocation.cpp \
	bindings/js/JSNamedNodeMapCustom.cpp \
	bindings/js/JSNamedNodesCollection.cpp \
	bindings/js/JSNodeCustom.cpp \
	bindings/js/JSNodeFilterCondition.cpp \
	bindings/js/JSNodeFilterCustom.cpp \
	bindings/js/JSNodeIteratorCustom.cpp \
	bindings/js/JSNodeListCustom.cpp \
	bindings/js/JSStyleSheetCustom.cpp \
	bindings/js/JSStyleSheetListCustom.cpp \
	bindings/js/JSTreeWalkerCustom.cpp \
	bindings/js/JSXMLHttpRequest.cpp \
	bindings/js/PausedTimeouts.cpp \
	bindings/js/ScheduledAction.cpp \
	bindings/js/kjs_binding.cpp \
	bindings/js/kjs_css.cpp \
	bindings/js/kjs_dom.cpp \
	bindings/js/kjs_events.cpp \
	bindings/js/kjs_html.cpp \
	bindings/js/kjs_navigator.cpp \
	bindings/js/kjs_proxy.cpp \
	bindings/js/kjs_window.cpp \
	 \
	css/CSSBorderImageValue.cpp \
	css/CSSCharsetRule.cpp \
	css/CSSComputedStyleDeclaration.cpp \
	css/CSSCursorImageValue.cpp \
	css/CSSFontFace.cpp \
	css/CSSFontFaceRule.cpp \
	css/CSSFontFaceSource.cpp \
	css/CSSFontFaceSrcValue.cpp \
	css/CSSFontSelector.cpp \
	css/CSSHelper.cpp \
	css/CSSImageValue.cpp \
	css/CSSImportRule.cpp \
	css/CSSInheritedValue.cpp \
	css/CSSInitialValue.cpp \
	css/CSSMediaRule.cpp \
	css/CSSMutableStyleDeclaration.cpp \
	css/CSSPageRule.cpp \
	css/CSSParser.cpp \
	css/CSSPrimitiveValue.cpp \
	css/CSSProperty.cpp \
	css/CSSRule.cpp \
	css/CSSRuleList.cpp \
	css/CSSSegmentedFontFace.cpp \
	css/CSSSelector.cpp \
	css/CSSStyleDeclaration.cpp \
	css/CSSStyleRule.cpp \
	css/CSSStyleSelector.cpp \
	css/CSSStyleSheet.cpp \
	css/CSSTimingFunctionValue.cpp \
	css/CSSTransformValue.cpp \
	css/CSSUnicodeRangeValue.cpp \
	css/CSSValueList.cpp \
	css/FontFamilyValue.cpp \
	css/FontValue.cpp \
	css/MediaFeatureNames.cpp \
	css/MediaList.cpp \
	css/MediaQuery.cpp \
	css/MediaQueryEvaluator.cpp \
	css/MediaQueryExp.cpp \
	css/ShadowValue.cpp \
	css/StyleBase.cpp \
	css/StyleList.cpp \
	css/StyleSheet.cpp \
	css/StyleSheetList.cpp \
	 \
	dom/Attr.cpp \
	dom/Attribute.cpp \
	dom/BeforeTextInsertedEvent.cpp \
	dom/BeforeUnloadEvent.cpp \
	dom/CDATASection.cpp \
	dom/CSSMappedAttributeDeclaration.cpp \
	dom/CharacterData.cpp \
	dom/ChildNodeList.cpp \
	dom/ClassNames.cpp \
	dom/ClassNodeList.cpp \
	dom/Clipboard.cpp \
	dom/ClipboardEvent.cpp \
	dom/Comment.cpp \
	dom/ContainerNode.cpp \
	dom/DOMImplementation.cpp \
	dom/Document.cpp \
	dom/DocumentFragment.cpp \
	dom/DocumentType.cpp \
	dom/DynamicNodeList.cpp \
	dom/EditingText.cpp \
	dom/Element.cpp \
	dom/Entity.cpp \
	dom/EntityReference.cpp \
	dom/Event.cpp \
	dom/EventNames.cpp \
	dom/EventTarget.cpp \
	dom/EventTargetNode.cpp \
	dom/ExceptionBase.cpp \
	dom/ExceptionCode.cpp \
	dom/KeyboardEvent.cpp \
	dom/MappedAttribute.cpp \
	dom/MessageEvent.cpp \
	dom/MouseEvent.cpp \
	dom/MouseRelatedEvent.cpp \
	dom/MutationEvent.cpp \
	dom/NameNodeList.cpp \
	dom/NamedAttrMap.cpp \
	dom/NamedMappedAttrMap.cpp \
	dom/Node.cpp \
	dom/NodeFilter.cpp \
	dom/NodeFilterCondition.cpp \
	dom/NodeIterator.cpp \
	dom/Notation.cpp \
	dom/OverflowEvent.cpp \
	dom/Position.cpp \
	dom/PositionIterator.cpp \
	dom/ProcessingInstruction.cpp \
	dom/ProgressEvent.cpp \
	dom/QualifiedName.cpp \
	dom/Range.cpp \
	dom/RegisteredEventListener.cpp \
	dom/SelectorNodeList.cpp \
	dom/StaticNodeList.cpp \
	dom/StyleElement.cpp \
	dom/StyledElement.cpp \
	dom/TagNodeList.cpp \
	dom/Text.cpp \
	dom/TextEvent.cpp \
	dom/Traversal.cpp \
	dom/TreeWalker.cpp \
	dom/UIEvent.cpp \
	dom/UIEventWithKeyState.cpp \
	dom/WheelEvent.cpp \
	dom/XMLTokenizer.cpp \
	 \
	editing/AppendNodeCommand.cpp \
	editing/ApplyStyleCommand.cpp \
	editing/BreakBlockquoteCommand.cpp \
	editing/CompositeEditCommand.cpp \
	editing/CreateLinkCommand.cpp \
	editing/DeleteButton.cpp \
	editing/DeleteButtonController.cpp \
	editing/DeleteFromTextNodeCommand.cpp \
	editing/DeleteSelectionCommand.cpp \
	editing/EditCommand.cpp \
	editing/Editor.cpp \
	editing/EditorCommand.cpp \
	editing/FormatBlockCommand.cpp \
	editing/HTMLInterchange.cpp \
	editing/IndentOutdentCommand.cpp \
	editing/InsertIntoTextNodeCommand.cpp \
	editing/InsertLineBreakCommand.cpp \
	editing/InsertListCommand.cpp \
	editing/InsertNodeBeforeCommand.cpp \
	editing/InsertParagraphSeparatorCommand.cpp \
	editing/InsertTextCommand.cpp \
	editing/JoinTextNodesCommand.cpp \
	editing/MergeIdenticalElementsCommand.cpp \
	editing/ModifySelectionListLevel.cpp \
	editing/MoveSelectionCommand.cpp \
	editing/RemoveCSSPropertyCommand.cpp \
	editing/RemoveFormatCommand.cpp \
	editing/RemoveNodeAttributeCommand.cpp \
	editing/RemoveNodeCommand.cpp \
	editing/RemoveNodePreservingChildrenCommand.cpp \
	editing/ReplaceSelectionCommand.cpp \
	editing/Selection.cpp \
	editing/SelectionController.cpp \
	editing/SetNodeAttributeCommand.cpp \
	editing/SplitElementCommand.cpp \
	editing/SplitTextNodeCommand.cpp \
	editing/SplitTextNodeContainingElementCommand.cpp \
	editing/TextIterator.cpp \
	editing/TypingCommand.cpp \
	editing/UnlinkCommand.cpp \
	editing/VisiblePosition.cpp \
	editing/WrapContentsInDummySpanCommand.cpp \
	editing/htmlediting.cpp \
	editing/markup.cpp \
	editing/visible_units.cpp \
	 \
	history/BackForwardList.cpp \
	history/CachedPage.cpp \
	history/HistoryItem.cpp \
	history/PageCache.cpp \
	\
	html/CanvasGradient.cpp \
	html/CanvasPattern.cpp \
	html/CanvasRenderingContext2D.cpp \
	html/CanvasStyle.cpp \
	html/FormDataList.cpp \
	html/HTMLAnchorElement.cpp \
	html/HTMLAppletElement.cpp \
	html/HTMLAreaElement.cpp \
	html/HTMLBRElement.cpp \
	html/HTMLBaseElement.cpp \
	html/HTMLBaseFontElement.cpp \
	html/HTMLBlockquoteElement.cpp \
	html/HTMLBodyElement.cpp \
	html/HTMLButtonElement.cpp \
	html/HTMLCanvasElement.cpp \
	html/HTMLCollection.cpp \
	html/HTMLDListElement.cpp \
	html/HTMLDirectoryElement.cpp \
	html/HTMLDivElement.cpp \
	html/HTMLDocument.cpp \
	html/HTMLElement.cpp \
	html/HTMLElementFactory.cpp \
	html/HTMLEmbedElement.cpp \
	html/HTMLFieldSetElement.cpp \
	html/HTMLFontElement.cpp \
	html/HTMLFormCollection.cpp \
	html/HTMLFormElement.cpp \
	html/HTMLFrameElement.cpp \
	html/HTMLFrameElementBase.cpp \
	html/HTMLFrameOwnerElement.cpp \
	html/HTMLFrameSetElement.cpp \
	html/HTMLGenericFormElement.cpp \
	html/HTMLHRElement.cpp \
	html/HTMLHeadElement.cpp \
	html/HTMLHeadingElement.cpp \
	html/HTMLHtmlElement.cpp \
	html/HTMLIFrameElement.cpp \
	html/HTMLImageElement.cpp \
	html/HTMLImageLoader.cpp \
	html/HTMLInputElement.cpp \
	html/HTMLIsIndexElement.cpp \
    html/HTMLKeygenElement.cpp \
	html/HTMLLIElement.cpp \
	html/HTMLLabelElement.cpp \
	html/HTMLLegendElement.cpp \
	html/HTMLLinkElement.cpp \
	html/HTMLMapElement.cpp \
	html/HTMLMarqueeElement.cpp \
	html/HTMLMenuElement.cpp \
	html/HTMLMetaElement.cpp \
	html/HTMLModElement.cpp \
	html/HTMLNameCollection.cpp \
	html/HTMLOListElement.cpp \
	html/HTMLObjectElement.cpp \
	html/HTMLOptGroupElement.cpp \
	html/HTMLOptionElement.cpp \
	html/HTMLOptionsCollection.cpp \
	html/HTMLParagraphElement.cpp \
	html/HTMLParamElement.cpp \
	html/HTMLParser.cpp \
	html/HTMLParserErrorCodes.cpp \
	html/HTMLPlugInElement.cpp \
	html/HTMLPreElement.cpp \
	html/HTMLQuoteElement.cpp \
	html/HTMLScriptElement.cpp \
	html/HTMLSelectElement.cpp \
	html/HTMLStyleElement.cpp \
	html/HTMLTableCaptionElement.cpp \
	html/HTMLTableCellElement.cpp \
	html/HTMLTableColElement.cpp \
	html/HTMLTableElement.cpp \
	html/HTMLTablePartElement.cpp \
	html/HTMLTableRowElement.cpp \
	html/HTMLTableRowsCollection.cpp \
	html/HTMLTableSectionElement.cpp \
	html/HTMLTextAreaElement.cpp \
	html/HTMLTextFieldInnerElement.cpp \
	html/HTMLTitleElement.cpp \
	html/HTMLTokenizer.cpp \
	html/HTMLUListElement.cpp \
	html/HTMLViewSourceDocument.cpp \
	html/PreloadScanner.cpp \
	 \
	html/TimeRanges.cpp \
	\
	loader/Cache.cpp \
	loader/CachedCSSStyleSheet.cpp \
	loader/CachedFont.cpp \
	loader/CachedImage.cpp \
	loader/CachedResource.cpp \
	loader/CachedResourceClientWalker.cpp \
	loader/CachedScript.cpp \
	loader/DocLoader.cpp \
	loader/DocumentLoader.cpp \
	loader/FormState.cpp \
	loader/FrameLoader.cpp \
	loader/ImageDocument.cpp \
	loader/MainResourceLoader.cpp \
	loader/NavigationAction.cpp \
	loader/NetscapePlugInStreamLoader.cpp \
	loader/PluginDocument.cpp \
	loader/ProgressTracker.cpp \
	loader/Request.cpp \
	loader/ResourceLoader.cpp \
	loader/SubresourceLoader.cpp \
	loader/TextDocument.cpp \
	loader/TextResourceDecoder.cpp \
	 \
	loader/android/DocumentLoaderAndroid.cpp \
	loader/icon/IconDatabase.cpp \
	loader/icon/IconLoader.cpp \
	loader/icon/IconRecord.cpp \
	loader/icon/PageURLRecord.cpp \
	 \
	loader/loader.cpp \
	 \
	page/AnimationController.cpp \
	page/BarInfo.cpp \
	page/Chrome.cpp \
	page/Console.cpp \
	page/ContextMenuController.cpp \
	page/DOMSelection.cpp \
	page/DOMWindow.cpp \
	page/DragController.cpp \
	page/EventHandler.cpp \
	page/FocusController.cpp \
	page/Frame.cpp \
	page/FrameTree.cpp \
	page/FrameView.cpp \
	page/History.cpp \
	page/MouseEventWithHitTestResults.cpp \
	page/Page.cpp \
	page/Screen.cpp \
	page/Settings.cpp \
	page/WindowFeatures.cpp \
	 \
	page/android/DragControllerAndroid.cpp \
	page/android/EventHandlerAndroid.cpp \
	page/android/FrameAndroid.cpp \
	page/android/InspectorControllerAndroid.cpp \
	 \
	platform/Arena.cpp \
	platform/ArrayImpl.cpp \
	platform/ContextMenu.cpp \
	platform/DeprecatedCString.cpp \
	platform/DeprecatedPtrListImpl.cpp \
	platform/DeprecatedString.cpp \
	platform/DeprecatedStringList.cpp \
	platform/DeprecatedValueListImpl.cpp \
	platform/DragData.cpp \
	platform/DragImage.cpp \
	platform/FileChooser.cpp \
	platform/KURL.cpp \
	platform/Logging.cpp \
	platform/MIMETypeRegistry.cpp \
	platform/ScrollBar.cpp \
	platform/SecurityOrigin.cpp \
	platform/SharedBuffer.cpp \
	platform/Timer.cpp \
	platform/Widget.cpp \
	 \
    platform/android/AndroidLog.cpp \
	platform/android/ChromeClientAndroid.cpp \
	platform/android/ContextMenuClientAndroid.cpp \
	platform/android/Cookie.cpp \
	platform/android/CursorAndroid.cpp \
	platform/android/ClipboardAndroid.cpp \
	platform/android/DragClientAndroid.cpp \
	platform/android/DragDataAndroid.cpp \
	platform/android/EditorAndroid.cpp \
	platform/android/EditorClientAndroid.cpp \
	platform/android/FileChooserAndroid.cpp \
	platform/android/FileSystemAndroid.cpp \
	platform/android/FrameLoaderClientAndroid.cpp \
	platform/android/KeyEventAndroid.cpp \
	platform/android/LocalizedStringsAndroid.cpp \
	platform/android/PlatformScrollBarAndroid.cpp \
	platform/android/PopupMenuAndroid.cpp \
	platform/android/RenderSkinAndroid.cpp \
	platform/android/RenderSkinButton.cpp \
	platform/android/RenderSkinCombo.cpp \
	platform/android/RenderSkinRadio.cpp \
	platform/android/RenderThemeAndroid.cpp \
	platform/android/ScreenAndroid.cpp \
	platform/android/ScrollViewAndroid.cpp \
	platform/android/SearchPopupMenuAndroid.cpp \
	platform/android/SharedTimerAndroid.cpp \
	platform/android/SystemTimeAndroid.cpp \
	platform/android/TemporaryLinkStubs.cpp \
	platform/android/TextBreakIteratorInternalICU.cpp \
	platform/android/ThreadingAndroid.cpp \
	platform/android/WidgetAndroid.cpp \
	platform/android/sort.cpp \
	 \
	platform/android/jni/JavaBridge.cpp \
	platform/android/jni/JavaSharedClient.cpp \
	platform/android/jni/WebCoreFrameBridge.cpp \
	platform/android/jni/WebCoreJni.cpp \
	platform/android/jni/WebCoreResourceLoader.cpp \
	platform/android/jni/WebHistory.cpp \
	platform/android/jni/WebIconDatabase.cpp \
	platform/android/jni/WebSettings.cpp \
	platform/android/jni/WebViewCore.cpp \
	\
	platform/android/nav/CacheBuilder.cpp \
	platform/android/nav/CachedFrame.cpp \
	platform/android/nav/CachedHistory.cpp \
	platform/android/nav/CachedNode.cpp \
	platform/android/nav/CachedRoot.cpp \
	platform/android/nav/SelectText.cpp \
	platform/android/nav/WebView.cpp \
	\
	platform/graphics/AffineTransform.cpp \
	platform/graphics/BitmapImage.cpp \
	platform/graphics/Color.cpp \
	platform/graphics/FloatPoint.cpp \
	platform/graphics/FloatRect.cpp \
	platform/graphics/FloatSize.cpp \
	platform/graphics/Font.cpp \
	platform/graphics/FontCache.cpp \
	platform/graphics/FontData.cpp \
	platform/graphics/FontFallbackList.cpp \
	platform/graphics/FontFamily.cpp \
	platform/graphics/GlyphPageTreeNode.cpp \
	platform/graphics/GlyphWidthMap.cpp \
	platform/graphics/GraphicsContext.cpp \
	platform/graphics/GraphicsTypes.cpp \
	platform/graphics/Image.cpp \
	platform/graphics/IntRect.cpp \
	platform/graphics/MediaPlayer.cpp \
	platform/graphics/Path.cpp \
	platform/graphics/PathTraversalState.cpp \
	platform/graphics/Pen.cpp \
	platform/graphics/SegmentedFontData.cpp \
	platform/graphics/SimpleFontData.cpp \
	platform/graphics/StringTruncator.cpp \
	platform/network/AuthenticationChallenge.cpp \
	platform/network/Credential.cpp \
	platform/network/FormData.cpp \
	platform/network/HTTPParsers.cpp \
	platform/network/ProtectionSpace.cpp \
	platform/network/ResourceError.cpp \
	platform/network/ResourceHandle.cpp \
	platform/network/ResourceRequestBase.cpp \
	platform/network/ResourceResponseBase.cpp \
	\
	platform/graphics/android/AffineTransformAndroid.cpp \
	platform/graphics/android/FontAndroid.cpp \
	platform/graphics/android/FontCacheAndroid.cpp \
	platform/graphics/android/FontCustomPlatformData.cpp \
	platform/graphics/android/FontDataAndroid.cpp \
	platform/graphics/android/FontPlatformDataAndroid.cpp \
	platform/graphics/android/GlyphMapAndroid.cpp \
	platform/graphics/android/GraphicsContextAndroid.cpp \
	platform/graphics/android/ImageAndroid.cpp \
	platform/graphics/android/ImageBufferAndroid.cpp \
	platform/graphics/android/ImageSourceAndroid.cpp \
	platform/graphics/android/PathAndroid.cpp \
	platform/graphics/android/PlatformGraphicsContext.cpp \
	platform/graphics/android/android_graphics.cpp \
	\
	platform/network/android/ResourceHandleAndroid.cpp \
	\
	platform/posix/FileSystemPOSIX.cpp \
	\
	platform/pthreads/ThreadingPthreads.cpp \
	\
	platform/sql/SQLValue.cpp \
	platform/sql/SQLiteAuthorizer.cpp \
	platform/sql/SQLiteDatabase.cpp \
	platform/sql/SQLiteStatement.cpp \
	platform/sql/SQLiteTransaction.cpp \
	\
	plugins/PluginInfoStore.cpp \
	\
	plugins/android/PluginDatabaseAndroid.cpp \
	plugins/android/PluginPackageAndroid.cpp \
	plugins/android/PluginViewAndroid.cpp \
	plugins/android/npapi.cpp \
	\
	platform/text/AtomicString.cpp \
	platform/text/Base64.cpp \
	platform/text/BidiContext.cpp \
	platform/text/CString.cpp \
	platform/text/RegularExpression.cpp \
	platform/text/SegmentedString.cpp \
	platform/text/String.cpp \
	platform/text/StringImpl.cpp \
	platform/text/TextBoundariesICU.cpp \
	platform/text/TextBreakIteratorICU.cpp \
	platform/text/TextCodec.cpp \
	platform/text/TextCodecICU.cpp \
	platform/text/TextCodecLatin1.cpp \
	platform/text/TextCodecUTF16.cpp \
	platform/text/TextCodecUserDefined.cpp \
	platform/text/TextDecoder.cpp \
	platform/text/TextEncoding.cpp \
	platform/text/TextEncodingRegistry.cpp \
	platform/text/TextStream.cpp \
	platform/text/UnicodeRange.cpp \
	\
	rendering/AutoTableLayout.cpp \
	rendering/CounterNode.cpp \
	rendering/EllipsisBox.cpp \
	rendering/FixedTableLayout.cpp \
	rendering/HitTestResult.cpp \
	rendering/InlineBox.cpp \
	rendering/InlineFlowBox.cpp \
	rendering/InlineTextBox.cpp \
	rendering/LayoutState.cpp \
	rendering/ListMarkerBox.cpp \
	rendering/MediaControlElements.cpp \
	rendering/PointerEventsHitRules.cpp \
	rendering/RenderApplet.cpp \
	rendering/RenderArena.cpp \
	rendering/RenderBR.cpp \
	rendering/RenderBlock.cpp \
	rendering/RenderBox.cpp \
	rendering/RenderButton.cpp \
	rendering/RenderContainer.cpp \
	rendering/RenderCounter.cpp \
	rendering/RenderFieldset.cpp \
	rendering/RenderFileUploadControl.cpp \
	rendering/RenderFlexibleBox.cpp \
	rendering/RenderFlow.cpp \
	rendering/RenderForeignObject.cpp \
	rendering/RenderFrame.cpp \
	rendering/RenderFrameSet.cpp \
	rendering/RenderHTMLCanvas.cpp \
	rendering/RenderImage.cpp \
	rendering/RenderInline.cpp \
	rendering/RenderLayer.cpp \
	rendering/RenderLegend.cpp \
	rendering/RenderListBox.cpp \
	rendering/RenderListItem.cpp \
	rendering/RenderListMarker.cpp \
	rendering/RenderMedia.cpp \
	rendering/RenderMenuList.cpp \
	rendering/RenderObject.cpp \
	rendering/RenderPart.cpp \
	rendering/RenderPartObject.cpp \
	rendering/RenderPath.cpp \
	rendering/RenderReplaced.cpp \
	rendering/RenderSlider.cpp \
	rendering/RenderStyle.cpp \
	rendering/RenderTable.cpp \
	rendering/RenderTableCell.cpp \
	rendering/RenderTableCol.cpp \
	rendering/RenderTableRow.cpp \
	rendering/RenderTableSection.cpp \
	rendering/RenderText.cpp \
	rendering/RenderTextControl.cpp \
	rendering/RenderTextFragment.cpp \
	rendering/RenderTheme.cpp \
	rendering/RenderThemeSafari.cpp \
	rendering/RenderTreeAsText.cpp \
	rendering/RenderView.cpp \
	rendering/RenderWidget.cpp \
	rendering/RenderWordBreak.cpp \
	rendering/RootInlineBox.cpp \
	rendering/bidi.cpp \
	rendering/break_lines.cpp \
	\
	xml/DOMParser.cpp \
	xml/XMLHttpRequest.cpp \
	xml/XMLSerializer.cpp \

LOCAL_CFLAGS += -U__APPLE__ -Wno-endif-labels -Wno-import -Wno-format
LOCAL_CFLAGS += -fno-strict-aliasing
LOCAL_CFLAGS += -include "WebCorePrefixAndroid.h"

ifeq ($(TARGET_ARCH),arm)
	LOCAL_CFLAGS += -fvisibility=hidden
	LOCAL_CFLAGS += -Darm -include "malloc.h"
endif

ifeq ($(TARGET_BUILD_TYPE),debug)
	LOCAL_CFLAGS += -DKJS_DEBUG_MEM
endif
    
java_script_core_dir := $(LOCAL_PATH)/../JavaScriptCore

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/.. \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/bindings/js \
	$(LOCAL_PATH)/bridge \
	$(LOCAL_PATH)/css \
	$(LOCAL_PATH)/dom \
	$(LOCAL_PATH)/editing \
	$(LOCAL_PATH)/history \
	$(LOCAL_PATH)/html \
	$(LOCAL_PATH)/kwq \
	$(LOCAL_PATH)/loader \
	$(LOCAL_PATH)/loader/icon \
	$(LOCAL_PATH)/page \
	$(LOCAL_PATH)/page/android \
	$(LOCAL_PATH)/platform \
	$(LOCAL_PATH)/platform/android \
	$(LOCAL_PATH)/platform/android/jni \
	$(LOCAL_PATH)/platform/android/nav \
	$(LOCAL_PATH)/platform/android/stl \
	$(LOCAL_PATH)/platform/graphics \
	$(LOCAL_PATH)/platform/graphics/android \
	$(LOCAL_PATH)/platform/image-decoders \
	$(LOCAL_PATH)/platform/network \
	$(LOCAL_PATH)/platform/network/android \
	$(LOCAL_PATH)/platform/sql \
	$(LOCAL_PATH)/platform/text \
	$(LOCAL_PATH)/plugins \
	$(LOCAL_PATH)/plugins/android \
	$(LOCAL_PATH)/rendering \
	$(LOCAL_PATH)/storage \
	$(LOCAL_PATH)/xml \
	$(LOCAL_PATH)/xpath \
	$(LOCAL_PATH)/ForwardingHeaders \
	$(call include-path-for, corecg graphics) \
	external/skia/libsgl/ports \
	$(JNI_H_INCLUDE) \
	frameworks/base/core/jni/android/graphics \
	$(java_script_core_dir) \
	$(java_script_core_dir)/ForwardingHeaders \
	$(java_script_core_dir)/bindings \
	$(java_script_core_dir)/kjs \
	$(java_script_core_dir)/wtf/unicode \
	external/sqlite/dist \
	external/libxml2/include \
	external/icu4c/common \

LOCAL_SHARED_LIBRARIES := \
    libandroid_runtime \
	libnativehelper \
	libutils \
	libsgl \
	libcorecg \
    libsqlite \
    libui \
    libcutils \
    libicuuc \
    libicudata \
    libicui18n

LOCAL_STATIC_LIBRARIES := \
	libkjs \
	libxml2

LOCAL_LDLIBS += -lpthread

ifeq ($(TARGET_OS),linux)
LOCAL_LDLIBS += -ldl
endif

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

include $(BUILD_SHARED_LIBRARY)
