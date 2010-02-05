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

#include "config.h"
#include "V8DOMWrapper.h"

#include "CSSMutableStyleDeclaration.h"
#include "DOMDataStore.h"
#include "DOMObjectsInclude.h"
#include "DocumentLoader.h"
#include "FrameLoaderClient.h"
#include "Notification.h"
#include "SVGElementInstance.h"
#include "SVGPathSeg.h"
#include "ScriptController.h"
#include "V8AbstractEventListener.h"
#include "V8Binding.h"
#include "V8Collection.h"
#include "V8CustomEventListener.h"
#include "V8DOMApplicationCache.h"
#include "V8DOMMap.h"
#include "V8DOMWindow.h"
#include "V8EventListenerList.h"
#include "V8HTMLCollection.h"
#include "V8HTMLDocument.h"
#include "V8Index.h"
#include "V8IsolatedContext.h"
#include "V8Location.h"
#include "V8MessageChannel.h"
#include "V8NamedNodeMap.h"
#include "V8Node.h"
#include "V8NodeList.h"
#include "V8Notification.h"
#include "V8Proxy.h"
#include "V8SVGElementInstance.h"
#include "V8SharedWorker.h"
#include "V8StyleSheet.h"
#include "V8WebSocket.h"
#include "V8Worker.h"
#include "WebGLArray.h"
#include "WebGLContextAttributes.h"
#include "WebGLUniformLocation.h"
#include "WorkerContextExecutionProxy.h"

#include <algorithm>
#include <utility>
#include <v8.h>
#include <v8-debug.h>
#include <wtf/Assertions.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

typedef HashMap<Node*, v8::Object*> DOMNodeMap;
typedef HashMap<void*, v8::Object*> DOMObjectMap;

#if ENABLE(3D_CANVAS)
void V8DOMWrapper::setIndexedPropertiesToExternalArray(v8::Handle<v8::Object> wrapper,
                                                       int index,
                                                       void* address,
                                                       int length)
{
    v8::ExternalArrayType array_type = v8::kExternalByteArray;
    V8ClassIndex::V8WrapperType classIndex = V8ClassIndex::FromInt(index);
    switch (classIndex) {
    case V8ClassIndex::WEBGLBYTEARRAY:
        array_type = v8::kExternalByteArray;
        break;
    case V8ClassIndex::WEBGLUNSIGNEDBYTEARRAY:
        array_type = v8::kExternalUnsignedByteArray;
        break;
    case V8ClassIndex::WEBGLSHORTARRAY:
        array_type = v8::kExternalShortArray;
        break;
    case V8ClassIndex::WEBGLUNSIGNEDSHORTARRAY:
        array_type = v8::kExternalUnsignedShortArray;
        break;
    case V8ClassIndex::WEBGLINTARRAY:
        array_type = v8::kExternalIntArray;
        break;
    case V8ClassIndex::WEBGLUNSIGNEDINTARRAY:
        array_type = v8::kExternalUnsignedIntArray;
        break;
    case V8ClassIndex::WEBGLFLOATARRAY:
        array_type = v8::kExternalFloatArray;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    wrapper->SetIndexedPropertiesToExternalArrayData(address,
                                                     array_type,
                                                     length);
}
#endif

// The caller must have increased obj's ref count.
void V8DOMWrapper::setJSWrapperForDOMObject(void* object, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
#ifndef NDEBUG
    V8ClassIndex::V8WrapperType type = V8DOMWrapper::domWrapperType(wrapper);
    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE:
        ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
        ASSERT_NOT_REACHED();
#undef MAKE_CASE
    default:
        break;
    }
#endif
    getDOMObjectMap().set(object, wrapper);
}

// The caller must have increased obj's ref count.
void V8DOMWrapper::setJSWrapperForActiveDOMObject(void* object, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
#ifndef NDEBUG
    V8ClassIndex::V8WrapperType type = V8DOMWrapper::domWrapperType(wrapper);
    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE: break;
        ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
    default: 
        ASSERT_NOT_REACHED();
#undef MAKE_CASE
    }
#endif
    getActiveDOMObjectMap().set(object, wrapper);
}

// The caller must have increased node's ref count.
void V8DOMWrapper::setJSWrapperForDOMNode(Node* node, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
    getDOMNodeMap().set(node, wrapper);
}

v8::Persistent<v8::FunctionTemplate> V8DOMWrapper::getTemplate(V8ClassIndex::V8WrapperType type)
{
    v8::Persistent<v8::FunctionTemplate>* cacheCell = V8ClassIndex::GetCache(type);
    if (!cacheCell->IsEmpty())
        return *cacheCell;

    // Not in the cache.
    FunctionTemplateFactory factory = V8ClassIndex::GetFactory(type);
    v8::Persistent<v8::FunctionTemplate> descriptor = factory();

    *cacheCell = descriptor;
    return descriptor;
}

v8::Local<v8::Function> V8DOMWrapper::getConstructor(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Value> objectPrototype)
{
    // A DOM constructor is a function instance created from a DOM constructor
    // template. There is one instance per context. A DOM constructor is
    // different from a normal function in two ways:
    //   1) it cannot be called as constructor (aka, used to create a DOM object)
    //   2) its __proto__ points to Object.prototype rather than
    //      Function.prototype.
    // The reason for 2) is that, in Safari, a DOM constructor is a normal JS
    // object, but not a function. Hotmail relies on the fact that, in Safari,
    // HTMLElement.__proto__ == Object.prototype.
    v8::Handle<v8::FunctionTemplate> functionTemplate = getTemplate(type);
    // Getting the function might fail if we're running out of
    // stack or memory.
    v8::TryCatch tryCatch;
    v8::Local<v8::Function> value = functionTemplate->GetFunction();
    if (value.IsEmpty())
        return v8::Local<v8::Function>();
    // Hotmail fix, see comments above.
    if (!objectPrototype.IsEmpty())
        value->Set(v8::String::New("__proto__"), objectPrototype);
    return value;
}

v8::Local<v8::Function> V8DOMWrapper::getConstructorForContext(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Context> context)
{
    // Enter the scope for this context to get the correct constructor.
    v8::Context::Scope scope(context);

    return getConstructor(type, V8DOMWindowShell::getHiddenObjectPrototype(context));
}

v8::Local<v8::Function> V8DOMWrapper::getConstructor(V8ClassIndex::V8WrapperType type, DOMWindow* window)
{
    Frame* frame = window->frame();
    if (!frame)
        return v8::Local<v8::Function>();

    v8::Handle<v8::Context> context = V8Proxy::context(frame);
    if (context.IsEmpty())
        return v8::Local<v8::Function>();

    return getConstructorForContext(type, context);
}

v8::Local<v8::Function> V8DOMWrapper::getConstructor(V8ClassIndex::V8WrapperType type, WorkerContext*)
{
    WorkerContextExecutionProxy* proxy = WorkerContextExecutionProxy::retrieve();
    if (!proxy)
        return v8::Local<v8::Function>();

    v8::Handle<v8::Context> context = proxy->context();
    if (context.IsEmpty())
        return v8::Local<v8::Function>();

    return getConstructorForContext(type, context);
}

void V8DOMWrapper::setHiddenWindowReference(Frame* frame, const int internalIndex, v8::Handle<v8::Object> jsObject)
{
    // Get DOMWindow
    if (!frame)
        return; // Object might be detached from window
    v8::Handle<v8::Context> context = V8Proxy::context(frame);
    if (context.IsEmpty())
        return;

    ASSERT(internalIndex < V8DOMWindow::internalFieldCount);

    v8::Handle<v8::Object> global = context->Global();
    // Look for real DOM wrapper.
    global = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, global);
    ASSERT(!global.IsEmpty());
    ASSERT(global->GetInternalField(internalIndex)->IsUndefined());
    global->SetInternalField(internalIndex, jsObject);
}

V8ClassIndex::V8WrapperType V8DOMWrapper::domWrapperType(v8::Handle<v8::Object> object)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(object));
    v8::Handle<v8::Value> type = object->GetInternalField(v8DOMWrapperTypeIndex);
    return V8ClassIndex::FromInt(type->Int32Value());
}

PassRefPtr<NodeFilter> V8DOMWrapper::wrapNativeNodeFilter(v8::Handle<v8::Value> filter)
{
    // A NodeFilter is used when walking through a DOM tree or iterating tree
    // nodes.
    // FIXME: we may want to cache NodeFilterCondition and NodeFilter
    // object, but it is minor.
    // NodeFilter is passed to NodeIterator that has a ref counted pointer
    // to NodeFilter. NodeFilter has a ref counted pointer to NodeFilterCondition.
    // In NodeFilterCondition, filter object is persisted in its constructor,
    // and disposed in its destructor.
    if (!filter->IsFunction())
        return 0;

    NodeFilterCondition* condition = new V8NodeFilterCondition(filter);
    return NodeFilter::create(condition);
}

v8::Local<v8::Object> V8DOMWrapper::instantiateV8Object(V8Proxy* proxy, V8ClassIndex::V8WrapperType type, void* impl)
{
    if (V8IsolatedContext::getEntered()) {
        // This effectively disables the wrapper cache for isolated worlds.
        proxy = 0;
        // FIXME: Do we need a wrapper cache for the isolated world?  We should
        //        see if the performance gains are worth while.
        // We'll get one once we give the isolated context a proper window shell.
    } else if (!proxy)
        proxy = V8Proxy::retrieve();

    v8::Local<v8::Object> instance;
    if (proxy)
        // FIXME: Fix this to work properly with isolated worlds (see above).
        instance = proxy->windowShell()->createWrapperFromCache(type);
    else {
        v8::Local<v8::Function> function = getTemplate(type)->GetFunction();
        instance = SafeAllocation::newInstance(function);
    }
    if (!instance.IsEmpty()) {
        // Avoid setting the DOM wrapper for failed allocations.
        setDOMWrapper(instance, V8ClassIndex::ToInt(type), impl);
    }
    return instance;
}

#ifndef NDEBUG
bool V8DOMWrapper::maybeDOMWrapper(v8::Handle<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
    if (!object->InternalFieldCount())
        return false;

    ASSERT(object->InternalFieldCount() >= v8DefaultWrapperInternalFieldCount);

    v8::Handle<v8::Value> type = object->GetInternalField(v8DOMWrapperTypeIndex);
    ASSERT(type->IsInt32());
    ASSERT(V8ClassIndex::INVALID_CLASS_INDEX < type->Int32Value() && type->Int32Value() < V8ClassIndex::CLASSINDEX_END);

    v8::Handle<v8::Value> wrapper = object->GetInternalField(v8DOMWrapperObjectIndex);
    ASSERT(wrapper->IsNumber() || wrapper->IsExternal());

    return true;
}
#endif

bool V8DOMWrapper::isWrapperOfType(v8::Handle<v8::Value> value, V8ClassIndex::V8WrapperType classType)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
    if (!object->InternalFieldCount())
        return false;

    ASSERT(object->InternalFieldCount() >= v8DefaultWrapperInternalFieldCount);

    v8::Handle<v8::Value> wrapper = object->GetInternalField(v8DOMWrapperObjectIndex);
    ASSERT(wrapper->IsNumber() || wrapper->IsExternal());

    v8::Handle<v8::Value> type = object->GetInternalField(v8DOMWrapperTypeIndex);
    ASSERT(type->IsInt32());
    ASSERT(V8ClassIndex::INVALID_CLASS_INDEX < type->Int32Value() && type->Int32Value() < V8ClassIndex::CLASSINDEX_END);

    return V8ClassIndex::FromInt(type->Int32Value()) == classType;
}

<<<<<<< HEAD
#if ENABLE(VIDEO)
#define FOR_EACH_VIDEO_TAG(macro)                  \
    macro(audio, AUDIO)                            \
    macro(source, SOURCE)                          \
    macro(video, VIDEO)
#else
#define FOR_EACH_VIDEO_TAG(macro)
#endif

#if ENABLE(DATAGRID)
#define FOR_EACH_DATAGRID_TAG(macro)               \
    macro(datagrid, DATAGRID)                        \
    macro(dcell, DATAGRIDCELL)                       \
    macro(dcol, DATAGRIDCOL)                         \
    macro(drow, DATAGRIDROW)
#else
#define FOR_EACH_DATAGRID_TAG(macro)
#endif

#define FOR_EACH_TAG(macro)                        \
    FOR_EACH_DATAGRID_TAG(macro)                   \
    macro(a, ANCHOR)                               \
    macro(applet, APPLET)                          \
    macro(area, AREA)                              \
    macro(base, BASE)                              \
    macro(basefont, BASEFONT)                      \
    macro(blockquote, BLOCKQUOTE)                  \
    macro(body, BODY)                              \
    macro(br, BR)                                  \
    macro(button, BUTTON)                          \
    macro(caption, TABLECAPTION)                   \
    macro(col, TABLECOL)                           \
    macro(colgroup, TABLECOL)                      \
    macro(del, MOD)                                \
    macro(canvas, CANVAS)                          \
    macro(dir, DIRECTORY)                          \
    macro(div, DIV)                                \
    macro(dl, DLIST)                               \
    macro(embed, EMBED)                            \
    macro(fieldset, FIELDSET)                      \
    macro(font, FONT)                              \
    macro(form, FORM)                              \
    macro(frame, FRAME)                            \
    macro(frameset, FRAMESET)                      \
    macro(h1, HEADING)                             \
    macro(h2, HEADING)                             \
    macro(h3, HEADING)                             \
    macro(h4, HEADING)                             \
    macro(h5, HEADING)                             \
    macro(h6, HEADING)                             \
    macro(head, HEAD)                              \
    macro(hr, HR)                                  \
    macro(html, HTML)                              \
    macro(img, IMAGE)                              \
    macro(iframe, IFRAME)                          \
    macro(image, IMAGE)                            \
    macro(input, INPUT)                            \
    macro(ins, MOD)                                \
    macro(isindex, ISINDEX)                        \
    macro(keygen, SELECT)                          \
    macro(label, LABEL)                            \
    macro(legend, LEGEND)                          \
    macro(li, LI)                                  \
    macro(link, LINK)                              \
    macro(listing, PRE)                            \
    macro(map, MAP)                                \
    macro(marquee, MARQUEE)                        \
    macro(menu, MENU)                              \
    macro(meta, META)                              \
    macro(object, OBJECT)                          \
    macro(ol, OLIST)                               \
    macro(optgroup, OPTGROUP)                      \
    macro(option, OPTION)                          \
    macro(p, PARAGRAPH)                            \
    macro(param, PARAM)                            \
    macro(pre, PRE)                                \
    macro(q, QUOTE)                                \
    macro(script, SCRIPT)                          \
    macro(select, SELECT)                          \
    macro(style, STYLE)                            \
    macro(table, TABLE)                            \
    macro(thead, TABLESECTION)                     \
    macro(tbody, TABLESECTION)                     \
    macro(tfoot, TABLESECTION)                     \
    macro(td, TABLECELL)                           \
    macro(th, TABLECELL)                           \
    macro(tr, TABLEROW)                            \
    macro(textarea, TEXTAREA)                      \
    macro(title, TITLE)                            \
    macro(ul, ULIST)                               \
    macro(xmp, PRE)

V8ClassIndex::V8WrapperType V8DOMWrapper::htmlElementType(HTMLElement* element)
{
    typedef HashMap<String, V8ClassIndex::V8WrapperType> WrapperTypeMap;
    DEFINE_STATIC_LOCAL(WrapperTypeMap, wrapperTypeMap, ());
    if (wrapperTypeMap.isEmpty()) {
#define ADD_TO_HASH_MAP(tag, name) \
        wrapperTypeMap.set(#tag, V8ClassIndex::HTML##name##ELEMENT);
        FOR_EACH_TAG(ADD_TO_HASH_MAP)
#if ENABLE(VIDEO)
        if (MediaPlayer::isAvailable()) {
            FOR_EACH_VIDEO_TAG(ADD_TO_HASH_MAP)
        }
#endif
#undef ADD_TO_HASH_MAP
    }

    V8ClassIndex::V8WrapperType type = wrapperTypeMap.get(element->localName().impl());
    if (!type)
        return V8ClassIndex::HTMLELEMENT;
    return type;
}
#undef FOR_EACH_TAG

#if ENABLE(SVG)

#if ENABLE(SVG_ANIMATION)
#define FOR_EACH_ANIMATION_TAG(macro) \
    macro(animateColor, ANIMATECOLOR) \
    macro(animate, ANIMATE) \
    macro(animateTransform, ANIMATETRANSFORM) \
    macro(set, SET)
#else
#define FOR_EACH_ANIMATION_TAG(macro)
#endif

#if ENABLE(SVG) && ENABLE(FILTERS)
#define FOR_EACH_FILTERS_TAG(macro) \
    macro(feBlend, FEBLEND) \
    macro(feColorMatrix, FECOLORMATRIX) \
    macro(feComponentTransfer, FECOMPONENTTRANSFER) \
    macro(feComposite, FECOMPOSITE) \
    macro(feDiffuseLighting, FEDIFFUSELIGHTING) \
    macro(feDisplacementMap, FEDISPLACEMENTMAP) \
    macro(feDistantLight, FEDISTANTLIGHT) \
    macro(feFlood, FEFLOOD) \
    macro(feFuncA, FEFUNCA) \
    macro(feFuncB, FEFUNCB) \
    macro(feFuncG, FEFUNCG) \
    macro(feFuncR, FEFUNCR) \
    macro(feGaussianBlur, FEGAUSSIANBLUR) \
    macro(feImage, FEIMAGE) \
    macro(feMerge, FEMERGE) \
    macro(feMergeNode, FEMERGENODE) \
    macro(feMorphology, FEMORPHOLOGY) \
    macro(feOffset, FEOFFSET) \
    macro(fePointLight, FEPOINTLIGHT) \
    macro(feSpecularLighting, FESPECULARLIGHTING) \
    macro(feSpotLight, FESPOTLIGHT) \
    macro(feTile, FETILE) \
    macro(feTurbulence, FETURBULENCE) \
    macro(filter, FILTER)
#else
#define FOR_EACH_FILTERS_TAG(macro)
#endif

#if ENABLE(SVG_FONTS)
#define FOR_EACH_FONTS_TAG(macro) \
    macro(font-face, FONTFACE) \
    macro(font-face-format, FONTFACEFORMAT) \
    macro(font-face-name, FONTFACENAME) \
    macro(font-face-src, FONTFACESRC) \
    macro(font-face-uri, FONTFACEURI)
#else
#define FOR_EACH_FONTS_TAG(marco)
#endif

#if ENABLE(SVG_FOREIGN_OBJECT)
#define FOR_EACH_FOREIGN_OBJECT_TAG(macro) \
    macro(foreignObject, FOREIGNOBJECT)
#else
#define FOR_EACH_FOREIGN_OBJECT_TAG(macro)
#endif

#if ENABLE(SVG_USE)
#define FOR_EACH_USE_TAG(macro) \
    macro(use, USE)
#else
#define FOR_EACH_USE_TAG(macro)
#endif

#define FOR_EACH_TAG(macro) \
    FOR_EACH_ANIMATION_TAG(macro) \
    FOR_EACH_FILTERS_TAG(macro) \
    FOR_EACH_FONTS_TAG(macro) \
    FOR_EACH_FOREIGN_OBJECT_TAG(macro) \
    FOR_EACH_USE_TAG(macro) \
    macro(a, A) \
    macro(altGlyph, ALTGLYPH) \
    macro(circle, CIRCLE) \
    macro(clipPath, CLIPPATH) \
    macro(cursor, CURSOR) \
    macro(defs, DEFS) \
    macro(desc, DESC) \
    macro(ellipse, ELLIPSE) \
    macro(g, G) \
    macro(glyph, GLYPH) \
    macro(image, IMAGE) \
    macro(linearGradient, LINEARGRADIENT) \
    macro(line, LINE) \
    macro(marker, MARKER) \
    macro(mask, MASK) \
    macro(metadata, METADATA) \
    macro(path, PATH) \
    macro(pattern, PATTERN) \
    macro(polyline, POLYLINE) \
    macro(polygon, POLYGON) \
    macro(radialGradient, RADIALGRADIENT) \
    macro(rect, RECT) \
    macro(script, SCRIPT) \
    macro(stop, STOP) \
    macro(style, STYLE) \
    macro(svg, SVG) \
    macro(switch, SWITCH) \
    macro(symbol, SYMBOL) \
    macro(text, TEXT) \
    macro(textPath, TEXTPATH) \
    macro(title, TITLE) \
    macro(tref, TREF) \
    macro(tspan, TSPAN) \
    macro(view, VIEW) \
    // end of macro

V8ClassIndex::V8WrapperType V8DOMWrapper::svgElementType(SVGElement* element)
{
    typedef HashMap<String, V8ClassIndex::V8WrapperType> WrapperTypeMap;
    DEFINE_STATIC_LOCAL(WrapperTypeMap, wrapperTypeMap, ());
    if (wrapperTypeMap.isEmpty()) {
#define ADD_TO_HASH_MAP(tag, name) \
        wrapperTypeMap.set(#tag, V8ClassIndex::SVG##name##ELEMENT);
        FOR_EACH_TAG(ADD_TO_HASH_MAP)
#undef ADD_TO_HASH_MAP
    }

    V8ClassIndex::V8WrapperType type = wrapperTypeMap.get(element->localName().impl());
    if (!type)
        return V8ClassIndex::SVGELEMENT;
    return type;
}
#undef FOR_EACH_TAG

#endif // ENABLE(SVG)

v8::Handle<v8::Value> V8DOMWrapper::convertEventToV8Object(Event* event)
{
    if (!event)
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(event);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type = V8ClassIndex::EVENT;

    if (event->isUIEvent()) {
        if (event->isKeyboardEvent())
            type = V8ClassIndex::KEYBOARDEVENT;
        else if (event->isTextEvent())
            type = V8ClassIndex::TEXTEVENT;
        else if (event->isMouseEvent())
            type = V8ClassIndex::MOUSEEVENT;
        else if (event->isWheelEvent())
            type = V8ClassIndex::WHEELEVENT;
// ANDROID: Upstream TOUCH_EVENTS.
#if ENABLE(TOUCH_EVENTS)
        else if (event->isTouchEvent())
            type = V8ClassIndex::TOUCHEVENT;
#endif
#if ENABLE(SVG)
        else if (event->isSVGZoomEvent())
            type = V8ClassIndex::SVGZOOMEVENT;
#endif
        else if (event->isCompositionEvent())
            type = V8ClassIndex::COMPOSITIONEVENT;
        else
            type = V8ClassIndex::UIEVENT;
    } else if (event->isMutationEvent())
        type = V8ClassIndex::MUTATIONEVENT;
    else if (event->isOverflowEvent())
        type = V8ClassIndex::OVERFLOWEVENT;
    else if (event->isMessageEvent())
        type = V8ClassIndex::MESSAGEEVENT;
    else if (event->isPageTransitionEvent())
        type = V8ClassIndex::PAGETRANSITIONEVENT;
    else if (event->isPopStateEvent())
        type = V8ClassIndex::POPSTATEEVENT;
    else if (event->isProgressEvent()) {
        if (event->isXMLHttpRequestProgressEvent())
            type = V8ClassIndex::XMLHTTPREQUESTPROGRESSEVENT;
        else
            type = V8ClassIndex::PROGRESSEVENT;
    } else if (event->isWebKitAnimationEvent())
        type = V8ClassIndex::WEBKITANIMATIONEVENT;
    else if (event->isWebKitTransitionEvent())
        type = V8ClassIndex::WEBKITTRANSITIONEVENT;
#if ENABLE(WORKERS)
    else if (event->isErrorEvent())
        type = V8ClassIndex::ERROREVENT;
#endif
#if ENABLE(DOM_STORAGE)
    else if (event->isStorageEvent())
        type = V8ClassIndex::STORAGEEVENT;
#endif
    else if (event->isBeforeLoadEvent())
        type = V8ClassIndex::BEFORELOADEVENT;


    v8::Handle<v8::Object> result = instantiateV8Object(type, V8ClassIndex::EVENT, event);
    if (result.IsEmpty()) {
        // Instantiation failed. Avoid updating the DOM object map and
        // return null which is already handled by callers of this function
        // in case the event is NULL.
        return v8::Null();
    }

    event->ref(); // fast ref
    setJSWrapperForDOMObject(event, v8::Persistent<v8::Object>::New(result));

    return result;
}

static const V8ClassIndex::V8WrapperType mapping[] = {
    V8ClassIndex::INVALID_CLASS_INDEX,    // NONE
    V8ClassIndex::INVALID_CLASS_INDEX,    // ELEMENT_NODE needs special treatment
    V8ClassIndex::ATTR,                   // ATTRIBUTE_NODE
    V8ClassIndex::TEXT,                   // TEXT_NODE
    V8ClassIndex::CDATASECTION,           // CDATA_SECTION_NODE
    V8ClassIndex::ENTITYREFERENCE,        // ENTITY_REFERENCE_NODE
    V8ClassIndex::ENTITY,                 // ENTITY_NODE
    V8ClassIndex::PROCESSINGINSTRUCTION,  // PROCESSING_INSTRUCTION_NODE
    V8ClassIndex::COMMENT,                // COMMENT_NODE
    V8ClassIndex::INVALID_CLASS_INDEX,    // DOCUMENT_NODE needs special treatment
    V8ClassIndex::DOCUMENTTYPE,           // DOCUMENT_TYPE_NODE
    V8ClassIndex::DOCUMENTFRAGMENT,       // DOCUMENT_FRAGMENT_NODE
    V8ClassIndex::NOTATION,               // NOTATION_NODE
    V8ClassIndex::NODE,                   // XPATH_NAMESPACE_NODE
};

v8::Handle<v8::Value> V8DOMWrapper::convertDocumentToV8Object(Document* document)
{
    // Find a proxy for this node.
    //
    // Note that if proxy is found, we might initialize the context which can
    // instantiate a document wrapper.  Therefore, we get the proxy before
    // checking if the node already has a wrapper.
    V8Proxy* proxy = V8Proxy::retrieve(document->frame());
    if (proxy)
        proxy->windowShell()->initContextIfNeeded();

    DOMNodeMapping& domNodeMap = getDOMNodeMap();
    v8::Handle<v8::Object> wrapper = domNodeMap.get(document);
    if (wrapper.IsEmpty())
        return convertNewNodeToV8Object(document, proxy, domNodeMap);

    return wrapper;
}

static v8::Handle<v8::Value> getWrapper(Node* node)
=======
v8::Handle<v8::Object> V8DOMWrapper::getWrapper(Node* node)
>>>>>>> webkit.org at r54340
{
    ASSERT(WTF::isMainThread());
    V8IsolatedContext* context = V8IsolatedContext::getEntered();
    if (LIKELY(!context)) {
        v8::Persistent<v8::Object>* wrapper = node->wrapper();
        if (!wrapper)
            return v8::Handle<v8::Object>();
        return *wrapper;
    }

    DOMNodeMapping& domNodeMap = context->world()->domDataStore()->domNodeMap();
    return domNodeMap.get(node);
}

// A JS object of type EventTarget is limited to a small number of possible classes.
// Check EventTarget.h for new type conversion methods
v8::Handle<v8::Value> V8DOMWrapper::convertEventTargetToV8Object(EventTarget* target)
{
    if (!target)
        return v8::Null();

#if ENABLE(SVG)
    if (SVGElementInstance* instance = target->toSVGElementInstance())
        return toV8(instance);
#endif

#if ENABLE(WORKERS)
    if (Worker* worker = target->toWorker())
        return toV8(worker);
#endif // WORKERS

#if ENABLE(SHARED_WORKERS)
    if (SharedWorker* sharedWorker = target->toSharedWorker())
        return toV8(sharedWorker);
#endif // SHARED_WORKERS

#if ENABLE(NOTIFICATIONS)
    if (Notification* notification = target->toNotification())
        return toV8(notification);
#endif

#if ENABLE(WEB_SOCKETS)
    if (WebSocket* webSocket = target->toWebSocket())
        return toV8(webSocket);
#endif

    if (Node* node = target->toNode())
        return toV8(node);

    if (DOMWindow* domWindow = target->toDOMWindow())
        return toV8(domWindow);

    // XMLHttpRequest is created within its JS counterpart.
    if (XMLHttpRequest* xmlHttpRequest = target->toXMLHttpRequest()) {
        v8::Handle<v8::Object> wrapper = getActiveDOMObjectMap().get(xmlHttpRequest);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

    // MessagePort is created within its JS counterpart
    if (MessagePort* port = target->toMessagePort()) {
        v8::Handle<v8::Object> wrapper = getActiveDOMObjectMap().get(port);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

    if (XMLHttpRequestUpload* upload = target->toXMLHttpRequestUpload()) {
        v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(upload);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (DOMApplicationCache* domAppCache = target->toDOMApplicationCache())
        return toV8(domAppCache);
#endif

#if ENABLE(EVENTSOURCE)
    if (EventSource* eventSource = target->toEventSource())
        return toV8(eventSource);
#endif

    ASSERT(0);
    return notHandledByInterceptor();
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(Node* node, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
}

#if ENABLE(SVG)
PassRefPtr<EventListener> V8DOMWrapper::getEventListener(SVGElementInstance* element, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    return getEventListener(element->correspondingElement(), value, isAttribute, lookup);
}
#endif

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(AbstractWorker* worker, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (worker->scriptExecutionContext()->isWorkerContext()) {
        WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
        ASSERT(workerContextProxy);
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
    }

    return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
}

#if ENABLE(NOTIFICATIONS)
PassRefPtr<EventListener> V8DOMWrapper::getEventListener(Notification* notification, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (notification->scriptExecutionContext()->isWorkerContext()) {
        WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
        ASSERT(workerContextProxy);
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
    }

    return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
}
#endif

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(WorkerContext* workerContext, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    WorkerContextExecutionProxy* workerContextProxy = workerContext->script()->proxy();
    if (workerContextProxy)
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);

    return 0;
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(XMLHttpRequestUpload* upload, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    return getEventListener(upload->associatedXMLHttpRequest(), value, isAttribute, lookup);
}

#if ENABLE(EVENTSOURCE)
PassRefPtr<EventListener> V8DOMWrapper::getEventListener(EventSource* eventSource, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (V8Proxy::retrieve(eventSource->scriptExecutionContext()))
        return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);

#if ENABLE(WORKERS)
    WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
    if (workerContextProxy)
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
#endif

    return 0;
}
#endif

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(EventTarget* eventTarget, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (V8Proxy::retrieve(eventTarget->scriptExecutionContext()))
        return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);

#if ENABLE(WORKERS)
    WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
    if (workerContextProxy)
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
#endif

    return 0;
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(V8Proxy* proxy, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
}

}  // namespace WebCore
