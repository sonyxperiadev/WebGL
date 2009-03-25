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
# If you edit it, keep it in alphabetical order
#
# The following files are intentionally not included
# LOCAL_SRC_FILES_EXCLUDED := \
#	DerivedSources.cpp \
#	WebCorePrefix.cpp \
#	bindings/js/JSCustomSQL*.cpp \
#	bindings/js/JSCustomVersionChangeCallback.cpp \
#	bindings/js/JSDatabaseCustom.cpp \
#	bindings/js/JSEventTargetSVGElementInstance.cpp \
#	bindings/js/JSHTMLAudioElementConstructor.cpp \
#	bindings/js/JSSQL*.cpp \
#	bindings/js/JSSVG*.cpp \
#	bindings/js/JSStorageCustom.cpp \
#	bindings/js/JSXSLTProcessor*.cpp \
#	bindings/js/*Gtk.cpp \
#	bindings/js/*Qt.cpp \
#	bindings/js/*Win.cpp \
#	bindings/js/*Wx.cpp \
#	bridge/test*.cpp \
#	css/CSSGrammar.y \
#	css/SVG*.cpp \
#	dom/XMLTokenizerQt.cpp \
#	editing/SmartReplace*.cpp \
#	html/FileList.cpp \
#	html/HTMLAudioElement.cpp \
#	html/HTMLMediaElement.cpp \
#	html/HTMLSourceElement.cpp \
#	html/HTMLVideoElement.cpp \
#	loader/CachedResourceClientWalker.cpp \
#	loader/CachedXBLDocument.cpp \
#	loader/CachedXSLStyleSheet.cpp \
#	loader/FTP*.cpp \
#	loader/UserStyleSheetLoader.cpp \
#	loader/icon/IconDatabaseNone.cpp \
#	page/AXObjectCache.cpp \
#	page/Accessibility*.cpp \
#	page/InspectorController.cpp \
#	page/JavaScript*.cpp \
#	platform/ThreadingNone.cpp \
#	platform/graphics/FloatPoint3D.cpp \
#	rendering/RenderSVG*.cpp \
#	rendering/RenderThemeSafari.cpp \
#	rendering/RenderThemeWin.cpp \
#	rendering/RenderVideo.cpp \
#	rendering/SVG*.cpp \
#	xml/Access*.cpp \
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
#	^loader\/appcache/* \
#	^loader\/archive/* \
#	/mac/* \
#	^manual-tests/* \
#	/objc/* \
#	^platform\/graphics\/filters/* \
#	^platform\/network\/soup/* \
#	/qt/* \
#	^storage/* \
#	^svg/* \
#	/symbian/* \
#	/win/* \
#	/wx/* \


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
	bindings/js/JSEventTargetNodeCustom.cpp \
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
	bindings/js/JSStyleSheetCustom.cpp \
	bindings/js/JSStyleSheetListCustom.cpp \
	bindings/js/JSTextCustom.cpp \
	bindings/js/JSTreeWalkerCustom.cpp \
	bindings/js/JSWebKitCSSMatrixConstructor.cpp \
	bindings/js/JSXMLHttpRequestConstructor.cpp \
	bindings/js/JSXMLHttpRequestCustom.cpp \
	bindings/js/JSXMLHttpRequestUploadCustom.cpp \
	bindings/js/ScheduledAction.cpp \
	bindings/js/ScriptCachedFrameData.cpp \
	bindings/js/ScriptCallFrame.cpp \
	bindings/js/ScriptCallStack.cpp \
	bindings/js/ScriptController.cpp \
	bindings/js/ScriptControllerAndroid.cpp \
	bindings/js/ScriptValue.cpp \
	\
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
	bridge/runtime_root.cpp \
	css/CSSBorderImageValue.cpp \
	css/CSSCanvasValue.cpp \
	css/CSSCharsetRule.cpp \
	css/CSSComputedStyleDeclaration.cpp \
	css/CSSCursorImageValue.cpp \
	css/CSSFontFace.cpp \
	css/CSSFontFaceRule.cpp \
	css/CSSFontFaceSource.cpp \
	css/CSSFontFaceSrcValue.cpp \
	css/CSSFontSelector.cpp \
	css/CSSFunctionValue.cpp \
	css/CSSGradientValue.cpp \
	css/CSSHelper.cpp \
	css/CSSImageGeneratorValue.cpp \
	css/CSSImageValue.cpp \
	css/CSSImportRule.cpp \
	css/CSSInheritedValue.cpp \
	css/CSSInitialValue.cpp \
	css/CSSMediaRule.cpp \
	css/CSSMutableStyleDeclaration.cpp \
	css/CSSPageRule.cpp \
	css/CSSParser.cpp \
	css/CSSParserValues.cpp \
	css/CSSPrimitiveValue.cpp \
	css/CSSProperty.cpp \
	css/CSSPropertyLonghand.cpp \
	css/CSSReflectValue.cpp \
	css/CSSRule.cpp \
	css/CSSRuleList.cpp \
	css/CSSSegmentedFontFace.cpp \
	css/CSSSelector.cpp \
	css/CSSSelectorList.cpp \
	css/CSSStyleDeclaration.cpp \
	css/CSSStyleRule.cpp \
	css/CSSStyleSelector.cpp \
	css/CSSStyleSheet.cpp \
	css/CSSTimingFunctionValue.cpp \
	css/CSSUnicodeRangeValue.cpp \
	css/CSSValueList.cpp \
	css/CSSVariableDependentValue.cpp \
	css/CSSVariablesDeclaration.cpp \
	css/CSSVariablesRule.cpp \
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
	css/WebKitCSSKeyframeRule.cpp \
	css/WebKitCSSKeyframesRule.cpp \
	css/WebKitCSSMatrix.cpp \
	css/WebKitCSSTransformValue.cpp \
	\
	dom/ActiveDOMObject.cpp \
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
	dom/DOMStringList.cpp \
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
	dom/FormControlElement.cpp \
	dom/FormControlElementWithState.cpp \
	dom/InputElement.cpp \
	dom/KeyboardEvent.cpp \
	dom/MappedAttribute.cpp \
	dom/MessageChannel.cpp \
	dom/MessageEvent.cpp \
	dom/MessagePort.cpp \
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
	dom/OptionElement.cpp \
	dom/OptionGroupElement.cpp \
	dom/OverflowEvent.cpp \
	dom/Position.cpp \
	dom/PositionIterator.cpp \
	dom/ProcessingInstruction.cpp \
	dom/ProgressEvent.cpp \
	dom/QualifiedName.cpp \
	dom/Range.cpp \
	dom/RegisteredEventListener.cpp \
	dom/ScriptElement.cpp \
	dom/ScriptExecutionContext.cpp \
	dom/SelectorNodeList.cpp \
	dom/StaticNodeList.cpp \
	dom/StaticStringList.cpp \
	dom/StyleElement.cpp \
	dom/StyledElement.cpp \
	dom/TagNodeList.cpp \
	dom/Text.cpp \
	dom/TextEvent.cpp \
	dom/Touch.cpp \
	dom/TouchEvent.cpp \
	dom/TouchList.cpp \
	dom/Traversal.cpp \
	dom/TreeWalker.cpp \
	dom/UIEvent.cpp \
	dom/UIEventWithKeyState.cpp \
	dom/WebKitAnimationEvent.cpp \
	dom/WebKitTransitionEvent.cpp \
	dom/WheelEvent.cpp \
	dom/XMLTokenizer.cpp \
	dom/XMLTokenizerLibxml2.cpp \
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
	editing/android/EditorAndroid.cpp \
	\
	history/BackForwardList.cpp \
	history/CachedFrame.cpp \
	history/CachedPage.cpp \
	history/HistoryItem.cpp \
	history/PageCache.cpp \
	\
	html/CanvasGradient.cpp \
	html/CanvasPattern.cpp \
	html/CanvasPixelArray.cpp \
	html/CanvasRenderingContext2D.cpp \
	html/CanvasStyle.cpp \
	html/File.cpp \
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
	html/HTMLFormControlElement.cpp \
	html/HTMLFormElement.cpp \
	html/HTMLFrameElement.cpp \
	html/HTMLFrameElementBase.cpp \
	html/HTMLFrameOwnerElement.cpp \
	html/HTMLFrameSetElement.cpp \
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
	html/HTMLPlugInImageElement.cpp \
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
	html/HTMLTitleElement.cpp \
	html/HTMLTokenizer.cpp \
	html/HTMLUListElement.cpp \
	html/HTMLViewSourceDocument.cpp \
	\
	html/ImageData.cpp \
	html/PreloadScanner.cpp \
	html/TimeRanges.cpp \
	\
	loader/Cache.cpp \
	loader/CachedCSSStyleSheet.cpp \
	loader/CachedFont.cpp \
	loader/CachedImage.cpp \
	loader/CachedResource.cpp \
	loader/CachedResourceClientWalker.cpp \
	loader/CachedResourceHandle.cpp \
	loader/CachedScript.cpp \
	loader/DocLoader.cpp \
	loader/DocumentLoader.cpp \
	loader/DocumentThreadableLoader.cpp \
	loader/FormState.cpp \
	loader/FrameLoader.cpp \
	loader/FrameLoaderClient.cpp \
	loader/ImageDocument.cpp \
	loader/ImageLoader.cpp \
	loader/MainResourceLoader.cpp \
	loader/MediaDocument.cpp \
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
	loader/ThreadableLoader.cpp \
	loader/icon/IconDatabase.cpp \
	loader/icon/IconFetcher.cpp \
	loader/icon/IconLoader.cpp \
	loader/icon/IconRecord.cpp \
	loader/icon/PageURLRecord.cpp \
	\
	loader/loader.cpp \
	\
	page/BarInfo.cpp \
	page/Chrome.cpp \
	page/Console.cpp \
	page/ContextMenuController.cpp \
	page/DOMSelection.cpp \
	page/DOMTimer.cpp \
	page/DOMWindow.cpp \
	page/DragController.cpp \
	page/EventHandler.cpp \
	page/FocusController.cpp \
	page/Frame.cpp \
	page/FrameTree.cpp \
	page/FrameView.cpp \
	page/Geolocation.cpp \
	page/Geoposition.cpp \
	page/History.cpp \
	page/Location.cpp \
	page/MouseEventWithHitTestResults.cpp \
	page/Navigator.cpp \
	page/NavigatorBase.cpp \
	page/Page.cpp \
	page/PageGroup.cpp \
	page/PrintContext.cpp \
	page/Screen.cpp \
	page/SecurityOrigin.cpp \
	page/Settings.cpp \
	page/WindowFeatures.cpp \
	\
	page/android/DragControllerAndroid.cpp \
	page/android/EventHandlerAndroid.cpp \
	page/android/InspectorControllerAndroid.cpp \
	\
	page/animation/AnimationBase.cpp \
	page/animation/AnimationController.cpp \
	page/animation/CompositeAnimation.cpp \
	page/animation/ImplicitAnimation.cpp \
	page/animation/KeyframeAnimation.cpp \
	\
	platform/Arena.cpp \
	platform/ContextMenu.cpp \
	platform/DeprecatedPtrListImpl.cpp \
	platform/DragData.cpp \
	platform/DragImage.cpp \
	platform/FileChooser.cpp \
	platform/GeolocationService.cpp \
	platform/KURL.cpp \
	platform/Length.cpp \
	platform/LinkHash.cpp \
	platform/Logging.cpp \
	platform/MIMETypeRegistry.cpp \
	platform/ScrollView.cpp \
	platform/Scrollbar.cpp \
	platform/ScrollbarThemeComposite.cpp \
	platform/SharedBuffer.cpp \
	platform/Theme.cpp \
	platform/ThreadGlobalData.cpp \
	platform/Timer.cpp \
	platform/Widget.cpp \
	\
	platform/android/ClipboardAndroid.cpp \
	platform/android/CursorAndroid.cpp \
	platform/android/DragDataAndroid.cpp \
	platform/android/EventLoopAndroid.cpp \
	platform/android/FileChooserAndroid.cpp \
	platform/android/FileSystemAndroid.cpp \
	platform/android/KeyEventAndroid.cpp \
	platform/android/LocalizedStringsAndroid.cpp \
	platform/android/PopupMenuAndroid.cpp \
	platform/android/RenderThemeAndroid.cpp \
	platform/android/ScreenAndroid.cpp \
	platform/android/ScrollViewAndroid.cpp \
	platform/android/SearchPopupMenuAndroid.cpp \
	platform/android/SharedTimerAndroid.cpp \
	platform/android/SoundAndroid.cpp \
	platform/android/SystemTimeAndroid.cpp \
	platform/android/TemporaryLinkStubs.cpp \
	platform/android/TextBreakIteratorInternalICU.cpp \
	platform/android/WidgetAndroid.cpp \
	\
	platform/animation/Animation.cpp \
	platform/animation/AnimationList.cpp \
	\
	platform/graphics/BitmapImage.cpp \
	platform/graphics/Color.cpp \
	platform/graphics/FloatPoint.cpp \
	platform/graphics/FloatQuad.cpp \
	platform/graphics/FloatRect.cpp \
	platform/graphics/FloatSize.cpp \
	platform/graphics/Font.cpp \
	platform/graphics/FontCache.cpp \
	platform/graphics/FontData.cpp \
	platform/graphics/FontDescription.cpp \
	platform/graphics/FontFallbackList.cpp \
	platform/graphics/FontFamily.cpp \
	platform/graphics/FontFastPath.cpp \
	platform/graphics/GeneratedImage.cpp \
	platform/graphics/GlyphPageTreeNode.cpp \
	platform/graphics/GlyphWidthMap.cpp \
	platform/graphics/Gradient.cpp \
	platform/graphics/GraphicsContext.cpp \
	platform/graphics/GraphicsTypes.cpp \
	platform/graphics/Image.cpp \
	platform/graphics/IntRect.cpp \
	platform/graphics/MediaPlayer.cpp \
	platform/graphics/Path.cpp \
	platform/graphics/PathTraversalState.cpp \
	platform/graphics/Pattern.cpp \
	platform/graphics/Pen.cpp \
	platform/graphics/SegmentedFontData.cpp \
	platform/graphics/SimpleFontData.cpp \
	platform/graphics/StringTruncator.cpp \
	\
	platform/graphics/android/BitmapAllocatorAndroid.cpp \
	platform/graphics/android/FontAndroid.cpp \
	platform/graphics/android/FontCacheAndroid.cpp \
	platform/graphics/android/FontCustomPlatformData.cpp \
	platform/graphics/android/FontDataAndroid.cpp \
	platform/graphics/android/FontPlatformDataAndroid.cpp \
	platform/graphics/android/GlyphMapAndroid.cpp \
	platform/graphics/android/GraphicsContextAndroid.cpp \
	platform/graphics/android/GradientAndroid.cpp \
	platform/graphics/android/ImageAndroid.cpp \
	platform/graphics/android/ImageBufferAndroid.cpp \
	platform/graphics/android/ImageSourceAndroid.cpp \
	platform/graphics/android/PathAndroid.cpp \
	platform/graphics/android/PatternAndroid.cpp \
	platform/graphics/android/PlatformGraphicsContext.cpp \
	platform/graphics/android/SharedBufferStream.cpp \
	platform/graphics/android/TransformationMatrixAndroid.cpp \
	platform/graphics/android/android_graphics.cpp \
	\
	platform/graphics/WidthIterator.cpp \
	\
	platform/graphics/skia/NativeImageSkia.cpp \
	\
	platform/graphics/transforms/MatrixTransformOperation.cpp \
	platform/graphics/transforms/RotateTransformOperation.cpp \
	platform/graphics/transforms/ScaleTransformOperation.cpp \
	platform/graphics/transforms/SkewTransformOperation.cpp \
	platform/graphics/transforms/TransformOperations.cpp \
	platform/graphics/transforms/TransformationMatrix.cpp \
	platform/graphics/transforms/TranslateTransformOperation.cpp \
	\
	platform/image-decoders/skia/GIFImageDecoder.cpp \
	platform/image-decoders/skia/GIFImageReader.cpp \
	\
	platform/network/AuthenticationChallengeBase.cpp \
	platform/network/Credential.cpp \
	platform/network/FormData.cpp \
	platform/network/FormDataBuilder.cpp \
	platform/network/HTTPHeaderMap.cpp \
	platform/network/HTTPParsers.cpp \
	platform/network/NetworkStateNotifier.cpp \
	platform/network/ProtectionSpace.cpp \
	platform/network/ResourceErrorBase.cpp \
	platform/network/ResourceHandle.cpp \
	platform/network/ResourceRequestBase.cpp \
	platform/network/ResourceResponseBase.cpp \
	\
	platform/network/android/Cookie.cpp \
	platform/network/android/ResourceHandleAndroid.cpp \
	platform/network/android/NetworkStateNotifierAndroid.cpp \
	\
	platform/posix/FileSystemPOSIX.cpp \
	\
	platform/sql/SQLValue.cpp \
	platform/sql/SQLiteAuthorizer.cpp \
	platform/sql/SQLiteDatabase.cpp \
	platform/sql/SQLiteStatement.cpp \
	platform/sql/SQLiteTransaction.cpp \
	storage/DatabaseAuthorizer.cpp \
	\
	platform/text/AtomicString.cpp \
	platform/text/Base64.cpp \
	platform/text/BidiContext.cpp \
	platform/text/CString.cpp \
	platform/text/RegularExpression.cpp \
	platform/text/SegmentedString.cpp \
	platform/text/String.cpp \
	platform/text/StringBuilder.cpp \
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
	plugins/MimeType.cpp \
	plugins/MimeTypeArray.cpp \
	plugins/Plugin.cpp \
	plugins/PluginArray.cpp \
	plugins/PluginData.cpp \
	plugins/PluginDatabase.cpp \
	plugins/PluginInfoStore.cpp \
	plugins/PluginMainThreadScheduler.cpp \
	plugins/PluginPackage.cpp \
	plugins/PluginStream.cpp \
	plugins/PluginView.cpp \
	plugins/npapi.cpp \
	\
	plugins/android/PluginDataAndroid.cpp \
	plugins/android/PluginPackageAndroid.cpp \
	plugins/android/PluginViewAndroid.cpp \
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
	rendering/RenderImageGeneratedContent.cpp \
	rendering/RenderInline.cpp \
	rendering/RenderLayer.cpp \
	rendering/RenderLegend.cpp \
	rendering/RenderListBox.cpp \
	rendering/RenderListItem.cpp \
	rendering/RenderListMarker.cpp \
	rendering/RenderMarquee.cpp \
	rendering/RenderMedia.cpp \
	rendering/RenderMenuList.cpp \
	rendering/RenderObject.cpp \
	rendering/RenderPart.cpp \
	rendering/RenderPartObject.cpp \
	rendering/RenderPath.cpp \
	rendering/RenderReplaced.cpp \
	rendering/RenderReplica.cpp \
	rendering/RenderScrollbar.cpp \
	rendering/RenderScrollbarPart.cpp \
	rendering/RenderScrollbarTheme.cpp \
	rendering/RenderSlider.cpp \
	rendering/RenderTable.cpp \
	rendering/RenderTableCell.cpp \
	rendering/RenderTableCol.cpp \
	rendering/RenderTableRow.cpp \
	rendering/RenderTableSection.cpp \
	rendering/RenderText.cpp \
	rendering/RenderTextControl.cpp \
	rendering/RenderTextControlMultiLine.cpp \
	rendering/RenderTextControlSingleLine.cpp \
	rendering/RenderTextFragment.cpp \
	rendering/RenderTheme.cpp \
	rendering/RenderTreeAsText.cpp \
	rendering/RenderView.cpp \
	rendering/RenderWidget.cpp \
	rendering/RenderWordBreak.cpp \
	rendering/RootInlineBox.cpp \
	rendering/TextControlInnerElements.cpp \
	rendering/bidi.cpp \
	rendering/break_lines.cpp \
	\
	rendering/style/BindingURI.cpp \
	rendering/style/ContentData.cpp \
	rendering/style/CounterDirectives.cpp \
	rendering/style/FillLayer.cpp \
	rendering/style/KeyframeList.cpp \
	rendering/style/NinePieceImage.cpp \
	rendering/style/RenderStyle.cpp \
	rendering/style/SVGRenderStyle.cpp \
	rendering/style/SVGRenderStyleDefs.cpp \
	rendering/style/ShadowData.cpp \
	rendering/style/StyleBackgroundData.cpp \
	rendering/style/StyleBoxData.cpp \
	rendering/style/StyleCachedImage.cpp \
	rendering/style/StyleFlexibleBoxData.cpp \
	rendering/style/StyleGeneratedImage.cpp \
	rendering/style/StyleInheritedData.cpp \
	rendering/style/StyleMarqueeData.cpp \
	rendering/style/StyleMultiColData.cpp \
	rendering/style/StyleRareInheritedData.cpp \
	rendering/style/StyleRareNonInheritedData.cpp \
	rendering/style/StyleSurroundData.cpp \
	rendering/style/StyleTransformData.cpp \
	rendering/style/StyleVisualData.cpp \
	\
	xml/DOMParser.cpp \
	xml/XMLHttpRequest.cpp \
	xml/XMLHttpRequestUpload.cpp \
	xml/XMLSerializer.cpp
