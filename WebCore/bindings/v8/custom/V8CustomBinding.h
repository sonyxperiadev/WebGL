/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#ifndef V8CustomBinding_h
#define V8CustomBinding_h

#include "V8Index.h"
#include <v8.h>

namespace WebCore {
    class V8Custom {
    public:
#define DECLARE_CALLBACK(NAME) static v8::Handle<v8::Value> v8##NAME##Callback(const v8::Arguments& args)
#define USE_CALLBACK(NAME) V8Custom::v8##NAME##Callback

        DECLARE_CALLBACK(HTMLAudioElementConstructor);
        DECLARE_CALLBACK(HTMLImageElementConstructor);
        DECLARE_CALLBACK(HTMLOptionElementConstructor);
<<<<<<< HEAD
        DECLARE_CALLBACK(MessageChannelConstructor);
        DECLARE_CALLBACK(WebKitCSSMatrixConstructor);
        DECLARE_CALLBACK(WebKitPointConstructor);
        DECLARE_CALLBACK(XMLHttpRequestConstructor);
        DECLARE_CALLBACK(XMLSerializerConstructor);
        DECLARE_CALLBACK(XPathEvaluatorConstructor);
        DECLARE_CALLBACK(XSLTProcessorConstructor);

        DECLARE_CALLBACK(XSLTProcessorImportStylesheet);
        DECLARE_CALLBACK(XSLTProcessorTransformToFragment);
        DECLARE_CALLBACK(XSLTProcessorTransformToDocument);
        DECLARE_CALLBACK(XSLTProcessorSetParameter);
        DECLARE_CALLBACK(XSLTProcessorGetParameter);
        DECLARE_CALLBACK(XSLTProcessorRemoveParameter);

        DECLARE_CALLBACK(CSSPrimitiveValueGetRGBColorValue);

        DECLARE_CALLBACK(CanvasRenderingContext2DSetStrokeColor);
        DECLARE_CALLBACK(CanvasRenderingContext2DSetFillColor);
        DECLARE_CALLBACK(CanvasRenderingContext2DStrokeRect);
        DECLARE_CALLBACK(CanvasRenderingContext2DSetShadow);
        DECLARE_CALLBACK(CanvasRenderingContext2DDrawImage);
        DECLARE_CALLBACK(CanvasRenderingContext2DDrawImageFromRect);
        DECLARE_CALLBACK(CanvasRenderingContext2DCreatePattern);
        DECLARE_CALLBACK(CanvasRenderingContext2DFillText);
        DECLARE_CALLBACK(CanvasRenderingContext2DStrokeText);
        DECLARE_CALLBACK(CanvasRenderingContext2DPutImageData);

#if ENABLE(3D_CANVAS)
        DECLARE_CALLBACK(WebGLRenderingContextBufferData);
        DECLARE_CALLBACK(WebGLRenderingContextBufferSubData);
        DECLARE_CALLBACK(WebGLRenderingContextGetBufferParameter);
        DECLARE_CALLBACK(WebGLRenderingContextGetFramebufferAttachmentParameter);
        DECLARE_CALLBACK(WebGLRenderingContextGetParameter);
        DECLARE_CALLBACK(WebGLRenderingContextGetProgramParameter);
        DECLARE_CALLBACK(WebGLRenderingContextGetRenderbufferParameter);
        DECLARE_CALLBACK(WebGLRenderingContextGetShaderParameter);
        DECLARE_CALLBACK(WebGLRenderingContextGetTexParameter);
        DECLARE_CALLBACK(WebGLRenderingContextGetUniform);
        DECLARE_CALLBACK(WebGLRenderingContextGetVertexAttrib);
        DECLARE_CALLBACK(WebGLRenderingContextTexImage2D);
        DECLARE_CALLBACK(WebGLRenderingContextTexSubImage2D);
        DECLARE_CALLBACK(WebGLRenderingContextUniform1fv);
        DECLARE_CALLBACK(WebGLRenderingContextUniform1iv);
        DECLARE_CALLBACK(WebGLRenderingContextUniform2fv);
        DECLARE_CALLBACK(WebGLRenderingContextUniform2iv);
        DECLARE_CALLBACK(WebGLRenderingContextUniform3fv);
        DECLARE_CALLBACK(WebGLRenderingContextUniform3iv);
        DECLARE_CALLBACK(WebGLRenderingContextUniform4fv);
        DECLARE_CALLBACK(WebGLRenderingContextUniform4iv);
        DECLARE_CALLBACK(WebGLRenderingContextUniformMatrix2fv);
        DECLARE_CALLBACK(WebGLRenderingContextUniformMatrix3fv);
        DECLARE_CALLBACK(WebGLRenderingContextUniformMatrix4fv);
        DECLARE_CALLBACK(WebGLRenderingContextVertexAttrib1fv);
        DECLARE_CALLBACK(WebGLRenderingContextVertexAttrib2fv);
        DECLARE_CALLBACK(WebGLRenderingContextVertexAttrib3fv);
        DECLARE_CALLBACK(WebGLRenderingContextVertexAttrib4fv);

        DECLARE_CALLBACK(WebGLArrayBufferConstructor);
        DECLARE_CALLBACK(WebGLByteArrayConstructor);
        DECLARE_CALLBACK(WebGLFloatArrayConstructor);
        DECLARE_CALLBACK(WebGLIntArrayConstructor);
        DECLARE_CALLBACK(WebGLShortArrayConstructor);
        DECLARE_CALLBACK(WebGLUnsignedByteArrayConstructor);
        DECLARE_CALLBACK(WebGLUnsignedIntArrayConstructor);
        DECLARE_CALLBACK(WebGLUnsignedShortArrayConstructor);
#endif

        DECLARE_PROPERTY_ACCESSOR_GETTER(ClipboardTypes);
        DECLARE_CALLBACK(ClipboardClearData);
        DECLARE_CALLBACK(ClipboardGetData);
        DECLARE_CALLBACK(ClipboardSetData);
        DECLARE_CALLBACK(ClipboardSetDragImage);

        DECLARE_CALLBACK(ElementQuerySelector);
        DECLARE_CALLBACK(ElementQuerySelectorAll);
        DECLARE_CALLBACK(ElementSetAttribute);
        DECLARE_CALLBACK(ElementSetAttributeNode);
        DECLARE_CALLBACK(ElementSetAttributeNS);
        DECLARE_CALLBACK(ElementSetAttributeNodeNS);

        DECLARE_CALLBACK(HistoryPushState);
        DECLARE_CALLBACK(HistoryReplaceState);

        DECLARE_PROPERTY_ACCESSOR_SETTER(LocationProtocol);
        DECLARE_PROPERTY_ACCESSOR_SETTER(LocationHost);
        DECLARE_PROPERTY_ACCESSOR_SETTER(LocationHostname);
        DECLARE_PROPERTY_ACCESSOR_SETTER(LocationPort);
        DECLARE_PROPERTY_ACCESSOR_SETTER(LocationPathname);
        DECLARE_PROPERTY_ACCESSOR_SETTER(LocationSearch);
        DECLARE_PROPERTY_ACCESSOR_SETTER(LocationHash);
        DECLARE_PROPERTY_ACCESSOR_SETTER(LocationHref);
        DECLARE_PROPERTY_ACCESSOR_GETTER(LocationAssign);
        DECLARE_PROPERTY_ACCESSOR_GETTER(LocationReplace);
        DECLARE_PROPERTY_ACCESSOR_GETTER(LocationReload);
        DECLARE_CALLBACK(LocationAssign);
        DECLARE_CALLBACK(LocationReplace);
        DECLARE_CALLBACK(LocationReload);
        DECLARE_CALLBACK(LocationToString);
        DECLARE_CALLBACK(LocationValueOf);
        DECLARE_CALLBACK(NodeAddEventListener);
        DECLARE_CALLBACK(NodeRemoveEventListener);
        DECLARE_CALLBACK(NodeInsertBefore);
        DECLARE_CALLBACK(NodeReplaceChild);
        DECLARE_CALLBACK(NodeRemoveChild);
        DECLARE_CALLBACK(NodeAppendChild);

        // We actually only need this because WebKit has
        // navigator.appVersion as custom. Our version just
        // passes through.
        DECLARE_PROPERTY_ACCESSOR(NavigatorAppVersion);

        DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestOnabort);
        DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestOnerror);
        DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestOnload);
        DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestOnloadstart);
        DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestOnprogress);
        DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestOnreadystatechange);
        DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestResponseText);
        DECLARE_CALLBACK(XMLHttpRequestAddEventListener);
        DECLARE_CALLBACK(XMLHttpRequestRemoveEventListener);
        DECLARE_CALLBACK(XMLHttpRequestOpen);
        DECLARE_CALLBACK(XMLHttpRequestSend);
        DECLARE_CALLBACK(XMLHttpRequestSetRequestHeader);
        DECLARE_CALLBACK(XMLHttpRequestGetResponseHeader);
        DECLARE_CALLBACK(XMLHttpRequestOverrideMimeType);
        DECLARE_CALLBACK(XMLHttpRequestDispatchEvent);

        DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestUploadOnabort);
        DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestUploadOnerror);
        DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestUploadOnload);
        DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestUploadOnloadstart);
        DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestUploadOnprogress);
        DECLARE_CALLBACK(XMLHttpRequestUploadAddEventListener);
        DECLARE_CALLBACK(XMLHttpRequestUploadRemoveEventListener);
        DECLARE_CALLBACK(XMLHttpRequestUploadDispatchEvent);

        DECLARE_CALLBACK(TreeWalkerParentNode);
        DECLARE_CALLBACK(TreeWalkerFirstChild);
        DECLARE_CALLBACK(TreeWalkerLastChild);
        DECLARE_CALLBACK(TreeWalkerNextNode);
        DECLARE_CALLBACK(TreeWalkerPreviousNode);
        DECLARE_CALLBACK(TreeWalkerNextSibling);
        DECLARE_CALLBACK(TreeWalkerPreviousSibling);

        DECLARE_CALLBACK(InjectedScriptHostInspectedWindow);
        DECLARE_CALLBACK(InjectedScriptHostNodeForId);
        DECLARE_CALLBACK(InjectedScriptHostWrapObject);
        DECLARE_CALLBACK(InjectedScriptHostUnwrapObject);
        DECLARE_CALLBACK(InjectedScriptHostPushNodePathToFrontend);
        DECLARE_CALLBACK(InjectedScriptHostWrapCallback);
#if ENABLE(DATABASE)
        DECLARE_CALLBACK(InjectedScriptHostSelectDatabase);
        DECLARE_CALLBACK(InjectedScriptHostDatabaseForId);
#endif
#if ENABLE(DOM_STORAGE)
        DECLARE_CALLBACK(InjectedScriptHostSelectDOMStorage);
#endif

        DECLARE_CALLBACK(InspectorFrontendHostSearch);
        DECLARE_CALLBACK(InspectorFrontendHostShowContextMenu);

        DECLARE_CALLBACK(ConsoleProfile);
        DECLARE_CALLBACK(ConsoleProfileEnd);

        DECLARE_CALLBACK(NodeIteratorNextNode);
        DECLARE_CALLBACK(NodeIteratorPreviousNode);

        DECLARE_CALLBACK(NodeFilterAcceptNode);

        DECLARE_CALLBACK(HTMLFormElementSubmit);

        DECLARE_NAMED_PROPERTY_GETTER(DOMWindow);
        DECLARE_INDEXED_PROPERTY_GETTER(DOMWindow);
        DECLARE_NAMED_ACCESS_CHECK(DOMWindow);
        DECLARE_INDEXED_ACCESS_CHECK(DOMWindow);

        DECLARE_NAMED_PROPERTY_GETTER(HTMLFrameSetElement);
        DECLARE_NAMED_PROPERTY_GETTER(HTMLFormElement);
        DECLARE_NAMED_PROPERTY_GETTER(NodeList);
        DECLARE_NAMED_PROPERTY_GETTER(NamedNodeMap);
        DECLARE_NAMED_PROPERTY_GETTER(CSSStyleDeclaration);
        DECLARE_NAMED_PROPERTY_SETTER(CSSStyleDeclaration);
        DECLARE_NAMED_PROPERTY_GETTER(HTMLPlugInElement);
        DECLARE_NAMED_PROPERTY_SETTER(HTMLPlugInElement);
        DECLARE_INDEXED_PROPERTY_GETTER(HTMLPlugInElement);
        DECLARE_INDEXED_PROPERTY_SETTER(HTMLPlugInElement);

        DECLARE_CALLBACK(HTMLPlugInElement);

        DECLARE_NAMED_PROPERTY_GETTER(StyleSheetList);
        DECLARE_INDEXED_PROPERTY_GETTER(NamedNodeMap);
        DECLARE_INDEXED_PROPERTY_GETTER(HTMLFormElement);
        DECLARE_INDEXED_PROPERTY_GETTER(HTMLOptionsCollection);
        DECLARE_INDEXED_PROPERTY_SETTER(HTMLOptionsCollection);
        DECLARE_NAMED_PROPERTY_GETTER(HTMLSelectElementCollection);
        DECLARE_INDEXED_PROPERTY_SETTER(HTMLSelectElementCollection);
        DECLARE_NAMED_PROPERTY_GETTER(HTMLAllCollection);
        DECLARE_NAMED_PROPERTY_GETTER(HTMLCollection);

#if ENABLE(3D_CANVAS)
        DECLARE_CALLBACK(WebGLByteArrayGet);
        DECLARE_CALLBACK(WebGLByteArraySet);
        DECLARE_INDEXED_PROPERTY_GETTER(WebGLByteArray);
        DECLARE_INDEXED_PROPERTY_SETTER(WebGLByteArray);

        DECLARE_CALLBACK(WebGLFloatArrayGet);
        DECLARE_CALLBACK(WebGLFloatArraySet);
        DECLARE_INDEXED_PROPERTY_GETTER(WebGLFloatArray);
        DECLARE_INDEXED_PROPERTY_SETTER(WebGLFloatArray);

        DECLARE_CALLBACK(WebGLIntArrayGet);
        DECLARE_CALLBACK(WebGLIntArraySet);
        DECLARE_INDEXED_PROPERTY_GETTER(WebGLIntArray);
        DECLARE_INDEXED_PROPERTY_SETTER(WebGLIntArray);

        DECLARE_CALLBACK(WebGLShortArrayGet);
        DECLARE_CALLBACK(WebGLShortArraySet);
        DECLARE_INDEXED_PROPERTY_GETTER(WebGLShortArray);
        DECLARE_INDEXED_PROPERTY_SETTER(WebGLShortArray);

        DECLARE_CALLBACK(WebGLUnsignedByteArrayGet);
        DECLARE_CALLBACK(WebGLUnsignedByteArraySet);
        DECLARE_INDEXED_PROPERTY_GETTER(WebGLUnsignedByteArray);
        DECLARE_INDEXED_PROPERTY_SETTER(WebGLUnsignedByteArray);

        DECLARE_CALLBACK(WebGLUnsignedIntArrayGet);
        DECLARE_CALLBACK(WebGLUnsignedIntArraySet);
        DECLARE_INDEXED_PROPERTY_GETTER(WebGLUnsignedIntArray);
        DECLARE_INDEXED_PROPERTY_SETTER(WebGLUnsignedIntArray);

        DECLARE_CALLBACK(WebGLUnsignedShortArrayGet);
        DECLARE_CALLBACK(WebGLUnsignedShortArraySet);
        DECLARE_INDEXED_PROPERTY_GETTER(WebGLUnsignedShortArray);
        DECLARE_INDEXED_PROPERTY_SETTER(WebGLUnsignedShortArray);
#endif

        DECLARE_PROPERTY_ACCESSOR_GETTER(MessageEventPorts);
        DECLARE_CALLBACK(MessageEventInitMessageEvent);

        DECLARE_PROPERTY_ACCESSOR(MessagePortOnmessage);
        DECLARE_PROPERTY_ACCESSOR(MessagePortOnclose);
        DECLARE_CALLBACK(MessagePortAddEventListener);
        DECLARE_CALLBACK(MessagePortPostMessage);
        DECLARE_CALLBACK(MessagePortRemoveEventListener);
        DECLARE_CALLBACK(MessagePortStartConversation);

        DECLARE_CALLBACK(DatabaseChangeVersion);
        DECLARE_CALLBACK(DatabaseTransaction);
        DECLARE_CALLBACK(DatabaseReadTransaction);
        DECLARE_CALLBACK(SQLTransactionExecuteSql);
        DECLARE_CALLBACK(SQLResultSetRowListItem);

        DECLARE_INDEXED_PROPERTY_GETTER(ClientRectList);
        DECLARE_INDEXED_PROPERTY_GETTER(FileList);

#if ENABLE(DATAGRID)
        DECLARE_PROPERTY_ACCESSOR(HTMLDataGridElementDataSource);
        DECLARE_INDEXED_PROPERTY_GETTER(DataGridColumnList);
        DECLARE_NAMED_PROPERTY_GETTER(DataGridColumnList);
#endif

#if ENABLE(DATABASE)
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowOpenDatabase);
#endif

#if ENABLE(DOM_STORAGE)
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowLocalStorage);
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowSessionStorage);
        DECLARE_INDEXED_PROPERTY_GETTER(Storage);
        DECLARE_INDEXED_PROPERTY_SETTER(Storage);
        DECLARE_INDEXED_PROPERTY_DELETER(Storage);
        DECLARE_NAMED_PROPERTY_GETTER(Storage);
        DECLARE_NAMED_PROPERTY_SETTER(Storage);
        DECLARE_NAMED_PROPERTY_DELETER(Storage);
        static v8::Handle<v8::Array> v8StorageNamedPropertyEnumerator(const v8::AccessorInfo& info);
#endif

#if ENABLE(SVG)
        DECLARE_PROPERTY_ACCESSOR_GETTER(SVGLengthValue);
        DECLARE_CALLBACK(SVGLengthConvertToSpecifiedUnits);
        DECLARE_CALLBACK(SVGMatrixMultiply);
        DECLARE_CALLBACK(SVGMatrixInverse);
        DECLARE_CALLBACK(SVGMatrixRotateFromVector);
        DECLARE_CALLBACK(SVGElementInstanceAddEventListener);
        DECLARE_CALLBACK(SVGElementInstanceRemoveEventListener);
#endif

#if ENABLE(TOUCH_EVENTS)
        DECLARE_INDEXED_PROPERTY_GETTER(TouchList);
#endif

#if ENABLE(WORKERS)
        DECLARE_PROPERTY_ACCESSOR(AbstractWorkerOnerror);
        DECLARE_CALLBACK(AbstractWorkerAddEventListener);
        DECLARE_CALLBACK(AbstractWorkerRemoveEventListener);

        DECLARE_PROPERTY_ACCESSOR(DedicatedWorkerContextOnmessage);
        DECLARE_CALLBACK(DedicatedWorkerContextPostMessage);

        DECLARE_PROPERTY_ACCESSOR(WorkerOnmessage);
        DECLARE_CALLBACK(WorkerPostMessage);
        DECLARE_CALLBACK(WorkerConstructor);

        DECLARE_PROPERTY_ACCESSOR_GETTER(WorkerContextSelf);
        DECLARE_PROPERTY_ACCESSOR(WorkerContextOnerror);
        DECLARE_CALLBACK(WorkerContextImportScripts);
        DECLARE_CALLBACK(WorkerContextSetTimeout);
        DECLARE_CALLBACK(WorkerContextClearTimeout);
        DECLARE_CALLBACK(WorkerContextSetInterval);
        DECLARE_CALLBACK(WorkerContextClearInterval);
        DECLARE_CALLBACK(WorkerContextAddEventListener);
        DECLARE_CALLBACK(WorkerContextRemoveEventListener);

#if ENABLE(NOTIFICATIONS)
        DECLARE_ACCESSOR_RUNTIME_ENABLER(WorkerContextWebkitNotifications);
#endif
#endif // ENABLE(WORKERS)

#if ENABLE(NOTIFICATIONS)
        DECLARE_CALLBACK(NotificationCenterRequestPermission);
        DECLARE_CALLBACK(NotificationCenterCreateNotification);
        DECLARE_CALLBACK(NotificationCenterCreateHTMLNotification);

        DECLARE_CALLBACK(NotificationAddEventListener);
        DECLARE_CALLBACK(NotificationRemoveEventListener);
        DECLARE_PROPERTY_ACCESSOR(NotificationEventHandler);
#endif // ENABLE(NOTIFICATIONS)

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowApplicationCache);
        DECLARE_PROPERTY_ACCESSOR(DOMApplicationCacheEventHandler);
        DECLARE_CALLBACK(DOMApplicationCacheAddEventListener);
        DECLARE_CALLBACK(DOMApplicationCacheRemoveEventListener);
#endif

#if ENABLE(SHARED_WORKERS)
        DECLARE_CALLBACK(SharedWorkerConstructor);
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowSharedWorker);
#endif

#if ENABLE(NOTIFICATIONS)
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowWebkitNotifications);
#endif

#if ENABLE(WEB_SOCKETS)
        DECLARE_PROPERTY_ACCESSOR(WebSocketOnopen);
        DECLARE_PROPERTY_ACCESSOR(WebSocketOnmessage);
        DECLARE_PROPERTY_ACCESSOR(WebSocketOnclose);
        DECLARE_CALLBACK(WebSocketConstructor);
        DECLARE_CALLBACK(WebSocketAddEventListener);
        DECLARE_CALLBACK(WebSocketRemoveEventListener);
        DECLARE_CALLBACK(WebSocketSend);
        DECLARE_CALLBACK(WebSocketClose);
        DECLARE_ACCESSOR_RUNTIME_ENABLER(DOMWindowWebSocket);
#endif

        DECLARE_CALLBACK(GeolocationGetCurrentPosition);
        DECLARE_CALLBACK(GeolocationWatchPosition);
        DECLARE_PROPERTY_ACCESSOR_GETTER(CoordinatesAltitude);
        DECLARE_PROPERTY_ACCESSOR_GETTER(CoordinatesAltitudeAccuracy);
        DECLARE_PROPERTY_ACCESSOR_GETTER(CoordinatesHeading);
        DECLARE_PROPERTY_ACCESSOR_GETTER(CoordinatesSpeed);

#undef DECLARE_INDEXED_ACCESS_CHECK
#undef DECLARE_NAMED_ACCESS_CHECK

#undef DECLARE_PROPERTY_ACCESSOR_SETTER
#undef DECLARE_PROPERTY_ACCESSOR_GETTER
#undef DECLARE_PROPERTY_ACCESSOR

#undef DECLARE_NAMED_PROPERTY_GETTER
#undef DECLARE_NAMED_PROPERTY_SETTER
#undef DECLARE_NAMED_PROPERTY_DELETER

#undef DECLARE_INDEXED_PROPERTY_GETTER
#undef DECLARE_INDEXED_PROPERTY_SETTER
#undef DECLARE_INDEXED_PROPERTY_DELETER
=======
>>>>>>> webkit.org at r54127

#undef DECLARE_CALLBACK
    };
} // namespace WebCore

#endif // V8CustomBinding_h
