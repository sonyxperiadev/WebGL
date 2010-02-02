/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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
#include "V8Proxy.h"

#include "CSSMutableStyleDeclaration.h"
#include "DateExtension.h"
#include "DOMObjectsInclude.h"
#include "DocumentLoader.h"
#include "FrameLoaderClient.h"
#include "InspectorTimelineAgent.h"
#include "Page.h"
#include "PageGroup.h"
#include "ScriptController.h"
#include "StorageNamespace.h"
#include "V8Binding.h"
#include "V8BindingState.h"
#include "V8Collection.h"
#include "V8ConsoleMessage.h"
#include "V8CustomBinding.h"
#include "V8DOMMap.h"
#include "V8DOMWindow.h"
#include "V8HiddenPropertyName.h"
#include "V8Index.h"
#include "V8IsolatedContext.h"
#include "WorkerContextExecutionProxy.h"

#include <algorithm>
#include <stdio.h>
#include <utility>
#include <v8.h>
#include <v8-debug.h>
#include <wtf/Assertions.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/UnusedParam.h>

#if PLATFORM(CHROMIUM)
#include "ChromiumBridge.h"
#endif

#ifdef ANDROID_INSTRUMENT
#include "TimeCounter.h"
#endif

#if PLATFORM(ANDROID)
#include "CString.h"
#endif

namespace WebCore {

v8::Persistent<v8::Context> V8Proxy::m_utilityContext;

// Static list of registered extensions
V8Extensions V8Proxy::m_extensions;

void batchConfigureAttributes(v8::Handle<v8::ObjectTemplate> instance, 
                              v8::Handle<v8::ObjectTemplate> proto, 
                              const BatchedAttribute* attributes, 
                              size_t attributeCount)
{
    for (size_t i = 0; i < attributeCount; ++i)
        configureAttribute(instance, proto, attributes[i]);
}

void batchConfigureCallbacks(v8::Handle<v8::ObjectTemplate> proto, 
                             v8::Handle<v8::Signature> signature, 
                             v8::PropertyAttribute attributes,
                             const BatchedCallback* callbacks,
                             size_t callbackCount)
{
    for (size_t i = 0; i < callbackCount; ++i) {
        proto->Set(v8::String::New(callbacks[i].name),
                   v8::FunctionTemplate::New(callbacks[i].callback, 
                                             v8::Handle<v8::Value>(),
                                             signature),
                   attributes);
    }
}

void batchConfigureConstants(v8::Handle<v8::FunctionTemplate> functionDescriptor,
                             v8::Handle<v8::ObjectTemplate> proto,
                             const BatchedConstant* constants,
                             size_t constantCount)
{
    for (size_t i = 0; i < constantCount; ++i) {
        const BatchedConstant* constant = &constants[i];
        functionDescriptor->Set(v8::String::New(constant->name), v8::Integer::New(constant->value), v8::ReadOnly);
        proto->Set(v8::String::New(constant->name), v8::Integer::New(constant->value), v8::ReadOnly);
    }
}

typedef HashMap<Node*, v8::Object*> DOMNodeMap;
typedef HashMap<void*, v8::Object*> DOMObjectMap;

#if ENABLE(SVG)
// Map of SVG objects with contexts to their contexts
static HashMap<void*, SVGElement*>& svgObjectToContextMap()
{
    typedef HashMap<void*, SVGElement*> SvgObjectToContextMap;
    DEFINE_STATIC_LOCAL(SvgObjectToContextMap, staticSvgObjectToContextMap, ());
    return staticSvgObjectToContextMap;
}

void V8Proxy::setSVGContext(void* object, SVGElement* context)
{
    if (!object)
        return;

    SVGElement* oldContext = svgObjectToContextMap().get(object);

    if (oldContext == context)
        return;

    if (oldContext)
        oldContext->deref();

    if (context)
        context->ref();

    svgObjectToContextMap().set(object, context);
}

SVGElement* V8Proxy::svgContext(void* object)
{
    return svgObjectToContextMap().get(object);
}

#endif

typedef HashMap<int, v8::FunctionTemplate*> FunctionTemplateMap;

bool AllowAllocation::m_current = false;

void logInfo(Frame* frame, const String& message, const String& url)
{
    Page* page = frame->page();
    if (!page)
        return;
    V8ConsoleMessage consoleMessage(message, url, 0);
    consoleMessage.dispatchNow(page);
}

enum DelayReporting {
    ReportLater,
    ReportNow
};

void V8Proxy::reportUnsafeAccessTo(Frame* target, DelayReporting delay)
{
    ASSERT(target);
    Document* targetDocument = target->document();
    if (!targetDocument)
        return;

    Frame* source = V8Proxy::retrieveFrameForEnteredContext();
    if (!source || !source->document())
        return; // Ignore error if the source document is gone.

    Document* sourceDocument = source->document();

    // FIXME: This error message should contain more specifics of why the same
    // origin check has failed.
    String str = String::format("Unsafe JavaScript attempt to access frame "
                                "with URL %s from frame with URL %s. "
                                "Domains, protocols and ports must match.\n",
                                targetDocument->url().string().utf8().data(),
                                sourceDocument->url().string().utf8().data());

    // Build a console message with fake source ID and line number.
    const String kSourceID = "";
    const int kLineNumber = 1;
    V8ConsoleMessage message(str, kSourceID, kLineNumber);

    if (delay == ReportNow) {
        // NOTE: Safari prints the message in the target page, but it seems like
        // it should be in the source page. Even for delayed messages, we put it in
        // the source page; see V8ConsoleMessage::processDelayed().
        message.dispatchNow(source->page());
    } else {
        ASSERT(delay == ReportLater);
        // We cannot safely report the message eagerly, because this may cause
        // allocations and GCs internally in V8 and we cannot handle that at this
        // point. Therefore we delay the reporting.
        message.dispatchLater();
    }
}

static void handleFatalErrorInV8()
{
    // FIXME: We temporarily deal with V8 internal error situations
    // such as out-of-memory by crashing the renderer.
    CRASH();
}

V8Proxy::V8Proxy(Frame* frame)
    : m_frame(frame)
    , m_windowShell(V8DOMWindowShell::create(frame))
    , m_inlineCode(false)
    , m_timerCallback(false)
    , m_recursion(0)
{
}

V8Proxy::~V8Proxy()
{
    clearForClose();
    windowShell()->destroyGlobal();
}

v8::Handle<v8::Script> V8Proxy::compileScript(v8::Handle<v8::String> code, const String& fileName, int baseLine)
#ifdef ANDROID_INSTRUMENT
{
    android::TimeCounter::start(android::TimeCounter::JavaScriptParseTimeCounter);
    v8::Handle<v8::Script> script = compileScriptInternal(code, fileName, baseLine);
    android::TimeCounter::record(android::TimeCounter::JavaScriptParseTimeCounter, __FUNCTION__);
    return script;
}

v8::Handle<v8::Script> V8Proxy::compileScriptInternal(v8::Handle<v8::String> code, const String& fileName, int baseLine)
#endif
{
    const uint16_t* fileNameString = fromWebCoreString(fileName);
    v8::Handle<v8::String> name = v8::String::New(fileNameString, fileName.length());
    v8::Handle<v8::Integer> line = v8::Integer::New(baseLine);
    v8::ScriptOrigin origin(name, line);
    v8::Handle<v8::Script> script = v8::Script::Compile(code, &origin);
    return script;
}

bool V8Proxy::handleOutOfMemory()
{
    v8::Local<v8::Context> context = v8::Context::GetCurrent();

    if (!context->HasOutOfMemoryException())
        return false;

    // Warning, error, disable JS for this frame?
    Frame* frame = V8Proxy::retrieveFrame(context);

    V8Proxy* proxy = V8Proxy::retrieve(frame);
    if (proxy) {
        // Clean m_context, and event handlers.
        proxy->clearForClose();

        proxy->windowShell()->destroyGlobal();
    }

#if PLATFORM(CHROMIUM)
    // TODO (andreip): ChromeBridge -> BrowserBridge?
    ChromiumBridge::notifyJSOutOfMemory(frame);
#endif

    // Disable JS.
    Settings* settings = frame->settings();
    ASSERT(settings);
    settings->setJavaScriptEnabled(false);

    return true;
}

void V8Proxy::evaluateInIsolatedWorld(int worldID, const Vector<ScriptSourceCode>& sources, int extensionGroup)
{
    // FIXME: This will need to get reorganized once we have a windowShell for the isolated world.
    windowShell()->initContextIfNeeded();

    v8::HandleScope handleScope;
    V8IsolatedContext* isolatedContext = 0;

    if (worldID > 0) {
        IsolatedWorldMap::iterator iter = m_isolatedWorlds.find(worldID);
        if (iter != m_isolatedWorlds.end()) {
            isolatedContext = iter->second;
        } else {
            isolatedContext = new V8IsolatedContext(this, extensionGroup);
            if (isolatedContext->context().IsEmpty()) {
                delete isolatedContext;
                return;
            }

            // FIXME: We should change this to using window shells to match JSC.
            m_isolatedWorlds.set(worldID, isolatedContext);

            // Setup context id for JS debugger.
            if (!setInjectedScriptContextDebugId(isolatedContext->context())) {
                m_isolatedWorlds.take(worldID);
                delete isolatedContext;
                return;
            }
        }
    } else {
        isolatedContext = new V8IsolatedContext(this, extensionGroup);
        if (isolatedContext->context().IsEmpty()) {
            delete isolatedContext;
            return;
        }
    }

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolatedContext->context());
    v8::Context::Scope context_scope(context);
    for (size_t i = 0; i < sources.size(); ++i)
      evaluate(sources[i], 0);

    if (worldID == 0)
      isolatedContext->destroy();
}

bool V8Proxy::setInjectedScriptContextDebugId(v8::Handle<v8::Context> targetContext)
{
    // Setup context id for JS debugger.
    v8::Context::Scope contextScope(targetContext);
    v8::Handle<v8::Context> context = windowShell()->context();
    if (context.IsEmpty())
        return false;
    int debugId = contextDebugId(context);

    char buffer[32];
    if (debugId == -1)
        snprintf(buffer, sizeof(buffer), "injected");
    else
        snprintf(buffer, sizeof(buffer), "injected,%d", debugId);
    targetContext->SetData(v8::String::New(buffer));

    return true;
}

v8::Local<v8::Value> V8Proxy::evaluate(const ScriptSourceCode& source, Node* node)
{
    ASSERT(v8::Context::InContext());

    V8GCController::checkMemoryUsage();

#if ENABLE(INSPECTOR)
    if (InspectorTimelineAgent* timelineAgent = m_frame->page() ? m_frame->page()->inspectorTimelineAgent() : 0)
        timelineAgent->willEvaluateScript(source.url().isNull() ? String() : source.url().string(), source.startLine());
#endif

    v8::Local<v8::Value> result;
    {
        // Isolate exceptions that occur when compiling and executing
        // the code. These exceptions should not interfere with
        // javascript code we might evaluate from C++ when returning
        // from here.
        v8::TryCatch tryCatch;
        tryCatch.SetVerbose(true);

        // Compile the script.
        v8::Local<v8::String> code = v8ExternalString(source.source());
#if PLATFORM(CHROMIUM)
        // TODO(andreip): ChromeBridge->BrowserBridge?
        ChromiumBridge::traceEventBegin("v8.compile", node, "");
#endif

        // NOTE: For compatibility with WebCore, ScriptSourceCode's line starts at
        // 1, whereas v8 starts at 0.
        v8::Handle<v8::Script> script = compileScript(code, source.url(), source.startLine() - 1);
#if PLATFORM(CHROMIUM)
        // TODO(andreip): ChromeBridge->BrowserBridge?
        ChromiumBridge::traceEventEnd("v8.compile", node, "");

        ChromiumBridge::traceEventBegin("v8.run", node, "");
#endif
        // Set inlineCode to true for <a href="javascript:doSomething()">
        // and false for <script>doSomething</script>. We make a rough guess at
        // this based on whether the script source has a URL.
        result = runScript(script, source.url().string().isNull());
    }

#if ENABLE(INSPECTOR)
    if (InspectorTimelineAgent* timelineAgent = m_frame->page() ? m_frame->page()->inspectorTimelineAgent() : 0)
        timelineAgent->didEvaluateScript();
#endif

#if PLATFORM(CHROMIUM)
    // TODO(andreip): upstream CHROMIUM guards to webkit.org
    ChromiumBridge::traceEventEnd("v8.run", node, "");
#endif
    return result;
}

v8::Local<v8::Value> V8Proxy::runScript(v8::Handle<v8::Script> script, bool isInlineCode)
#ifdef ANDROID_INSTRUMENT
{
    android::TimeCounter::start(android::TimeCounter::JavaScriptExecuteTimeCounter);
    v8::Local<v8::Value> result = runScriptInternal(script, isInlineCode);
    android::TimeCounter::record(android::TimeCounter::JavaScriptExecuteTimeCounter, __FUNCTION__);
    return result;
}

v8::Local<v8::Value> V8Proxy::runScriptInternal(v8::Handle<v8::Script> script, bool isInlineCode)
#endif
{
    if (script.IsEmpty())
        return notHandledByInterceptor();

    V8GCController::checkMemoryUsage();
    // Compute the source string and prevent against infinite recursion.
    if (m_recursion >= kMaxRecursionDepth) {
        v8::Local<v8::String> code = v8ExternalString("throw RangeError('Recursion too deep')");
        // FIXME: Ideally, we should be able to re-use the origin of the
        // script passed to us as the argument instead of using an empty string
        // and 0 baseLine.
        script = compileScript(code, "", 0);
    }

    if (handleOutOfMemory())
        ASSERT(script.IsEmpty());

    if (script.IsEmpty())
        return notHandledByInterceptor();

    // Save the previous value of the inlineCode flag and update the flag for
    // the duration of the script invocation.
    bool previousInlineCode = inlineCode();
    setInlineCode(isInlineCode);

    // Run the script and keep track of the current recursion depth.
    v8::Local<v8::Value> result;
    {
        V8ConsoleMessage::Scope scope;

        // See comment in V8Proxy::callFunction.
        m_frame->keepAlive();

        m_recursion++;
        result = script->Run();
        m_recursion--;
    }

    // Release the storage mutex if applicable.
    releaseStorageMutex();

    if (handleOutOfMemory())
        ASSERT(result.IsEmpty());

    // Handle V8 internal error situation (Out-of-memory).
    if (result.IsEmpty())
        return notHandledByInterceptor();

    // Restore inlineCode flag.
    setInlineCode(previousInlineCode);

    if (v8::V8::IsDead())
        handleFatalErrorInV8();

    return result;
}

v8::Local<v8::Value> V8Proxy::callFunction(v8::Handle<v8::Function> function, v8::Handle<v8::Object> receiver, int argc, v8::Handle<v8::Value> args[])
{
#ifdef ANDROID_INSTRUMENT
    android::TimeCounter::start(android::TimeCounter::JavaScriptExecuteTimeCounter);
#endif
    V8GCController::checkMemoryUsage();
    v8::Local<v8::Value> result;
    {
        V8ConsoleMessage::Scope scope;

        if (m_recursion >= kMaxRecursionDepth) {
            v8::Local<v8::String> code = v8::String::New("throw new RangeError('Maximum call stack size exceeded.')");
            if (code.IsEmpty())
                return result;
            v8::Local<v8::Script> script = v8::Script::Compile(code);
            if (script.IsEmpty())
                return result;
            script->Run();
            return result;
        }

        // Evaluating the JavaScript could cause the frame to be deallocated,
        // so we start the keep alive timer here.
        // Frame::keepAlive method adds the ref count of the frame and sets a
        // timer to decrease the ref count. It assumes that the current JavaScript
        // execution finishs before firing the timer.
        m_frame->keepAlive();

        m_recursion++;
        result = function->Call(receiver, argc, args);
        m_recursion--;
    }

    // Release the storage mutex if applicable.
    releaseStorageMutex();

    if (v8::V8::IsDead())
        handleFatalErrorInV8();

#ifdef ANDROID_INSTRUMENT
    android::TimeCounter::record(android::TimeCounter::JavaScriptExecuteTimeCounter, __FUNCTION__);
#endif
    return result;
}

v8::Local<v8::Value> V8Proxy::newInstance(v8::Handle<v8::Function> constructor, int argc, v8::Handle<v8::Value> args[])
{
    // No artificial limitations on the depth of recursion, see comment in
    // V8Proxy::callFunction.
    v8::Local<v8::Value> result;
    {
        V8ConsoleMessage::Scope scope;

        // See comment in V8Proxy::callFunction.
        m_frame->keepAlive();

        result = constructor->NewInstance(argc, args);
    }

    if (v8::V8::IsDead())
        handleFatalErrorInV8();

    return result;
}

DOMWindow* V8Proxy::retrieveWindow(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    global = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, global);
    ASSERT(!global.IsEmpty());
    return V8DOMWindow::toNative(global);
}

Frame* V8Proxy::retrieveFrame(v8::Handle<v8::Context> context)
{
    DOMWindow* window = retrieveWindow(context);
    Frame* frame = window->frame();
    if (frame && frame->domWindow() == window)
        return frame;
    // We return 0 here because |context| is detached from the Frame.  If we
    // did return |frame| we could get in trouble because the frame could be
    // navigated to another security origin.
    return 0;
}

Frame* V8Proxy::retrieveFrameForEnteredContext()
{
    v8::Handle<v8::Context> context = v8::Context::GetEntered();
    if (context.IsEmpty())
        return 0;
    return retrieveFrame(context);
}

Frame* V8Proxy::retrieveFrameForCurrentContext()
{
    v8::Handle<v8::Context> context = v8::Context::GetCurrent();
    if (context.IsEmpty())
        return 0;
    return retrieveFrame(context);
}

Frame* V8Proxy::retrieveFrameForCallingContext()
{
    v8::Handle<v8::Context> context = v8::Context::GetCalling();
    if (context.IsEmpty())
        return 0;
    return retrieveFrame(context);
}

V8Proxy* V8Proxy::retrieve()
{
    DOMWindow* window = retrieveWindow(currentContext());
    ASSERT(window);
    return retrieve(window->frame());
}

V8Proxy* V8Proxy::retrieve(Frame* frame)
{
    if (!frame)
        return 0;
    return frame->script()->canExecuteScripts() ? frame->script()->proxy() : 0;
}

V8Proxy* V8Proxy::retrieve(ScriptExecutionContext* context)
{
    if (!context || !context->isDocument())
        return 0;
    return retrieve(static_cast<Document*>(context)->frame());
}

void V8Proxy::disconnectFrame()
{
}

void V8Proxy::releaseStorageMutex()
{
    // If we've just left a top level script context and local storage has been
    // instantiated, we must ensure that any storage locks have been freed.
    // Per http://dev.w3.org/html5/spec/Overview.html#storage-mutex
    if (m_recursion != 0)
        return;
    Page* page = m_frame->page();
    if (!page)
        return;
    if (page->group().hasLocalStorage())
        page->group().localStorage()->unlock();
}

void V8Proxy::resetIsolatedWorlds()
{
    for (IsolatedWorldMap::iterator iter = m_isolatedWorlds.begin();
         iter != m_isolatedWorlds.end(); ++iter) {
        iter->second->destroy();
    }
    m_isolatedWorlds.clear();
}

void V8Proxy::clearForClose()
{
    resetIsolatedWorlds();
    windowShell()->clearForClose();
}

void V8Proxy::clearForNavigation()
{
    resetIsolatedWorlds();
<<<<<<< HEAD

    if (!m_context.IsEmpty()) {
        v8::HandleScope handle;
        clearDocumentWrapper();

        v8::Context::Scope contextScope(m_context);

        // Clear the document wrapper cache before turning on access checks on
        // the old DOMWindow wrapper. This way, access to the document wrapper
        // will be protected by the security checks on the DOMWindow wrapper.
        clearDocumentWrapperCache();

        // Turn on access check on the old DOMWindow wrapper.
        v8::Handle<v8::Object> wrapper = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, m_global);
        ASSERT(!wrapper.IsEmpty());
        wrapper->TurnOnAccessCheck();

        // Separate the context from its global object.
        m_context->DetachGlobal();

        disposeContextHandles();
    }
}

void V8Proxy::setSecurityToken()
{
    Document* document = m_frame->document();
    // Setup security origin and security token.
    if (!document) {
        m_context->UseDefaultSecurityToken();
        return;
    }

    // Ask the document's SecurityOrigin to generate a security token.
    // If two tokens are equal, then the SecurityOrigins canAccess each other.
    // If two tokens are not equal, then we have to call canAccess.
    // Note: we can't use the HTTPOrigin if it was set from the DOM.
    SecurityOrigin* origin = document->securityOrigin();
    String token;
    if (!origin->domainWasSetInDOM())
        token = document->securityOrigin()->toString();

    // An empty or "null" token means we always have to call
    // canAccess. The toString method on securityOrigins returns the
    // string "null" for empty security origins and for security
    // origins that should only allow access to themselves. In this
    // case, we use the global object as the security token to avoid
    // calling canAccess when a script accesses its own objects.
    if (token.isEmpty() || token == "null") {
        m_context->UseDefaultSecurityToken();
        return;
    }

    CString utf8Token = token.utf8();
    // NOTE: V8 does identity comparison in fast path, must use a symbol
    // as the security token.
    m_context->SetSecurityToken(v8::String::NewSymbol(utf8Token.data(), utf8Token.length()));
}

void V8Proxy::updateDocument()
{
    if (!m_frame->document())
        return;

    if (m_global.IsEmpty())
        return;

    // There is an existing JavaScript wrapper for the global object
    // of this frame. JavaScript code in other frames might hold a
    // reference to this wrapper. We eagerly initialize the JavaScript
    // context for the new document to make property access on the
    // global object wrapper succeed.
    initContextIfNeeded();

    // Bail out if context initialization failed.
    if (m_context.IsEmpty())
        return;

    // We have a new document and we need to update the cache.
    updateDocumentWrapperCache();

    updateSecurityOrigin();
}

void V8Proxy::updateSecurityOrigin()
{
    v8::HandleScope scope;
    setSecurityToken();
}

// Same origin policy implementation:
//
// Same origin policy prevents JS code from domain A access JS & DOM objects
// in a different domain B. There are exceptions and several objects are
// accessible by cross-domain code. For example, the window.frames object is
// accessible by code from a different domain, but window.document is not.
//
// The binding code sets security check callbacks on a function template,
// and accessing instances of the template calls the callback function.
// The callback function checks same origin policy.
//
// Callback functions are expensive. V8 uses a security token string to do
// fast access checks for the common case where source and target are in the
// same domain. A security token is a string object that represents
// the protocol/url/port of a domain.
//
// There are special cases where a security token matching is not enough.
// For example, JavaScript can set its domain to a super domain by calling
// document.setDomain(...). In these cases, the binding code can reset
// a context's security token to its global object so that the fast access
// check will always fail.

// Check if the current execution context can access a target frame.
// First it checks same domain policy using the lexical context
//
// This is equivalent to KJS::Window::allowsAccessFrom(ExecState*, String&).
bool V8Proxy::canAccessPrivate(DOMWindow* targetWindow)
{
    ASSERT(targetWindow);

    String message;

    v8::Local<v8::Context> activeContext = v8::Context::GetCalling();
    if (activeContext.IsEmpty()) {
        // There is a single activation record on the stack, so that must
        // be the activeContext.
        activeContext = v8::Context::GetCurrent();
    }
    DOMWindow* activeWindow = retrieveWindow(activeContext);
    if (activeWindow == targetWindow)
        return true;

    if (!activeWindow)
        return false;

    const SecurityOrigin* activeSecurityOrigin = activeWindow->securityOrigin();
    const SecurityOrigin* targetSecurityOrigin = targetWindow->securityOrigin();

    // We have seen crashes were the security origin of the target has not been
    // initialized. Defend against that.
    if (!targetSecurityOrigin)
        return false;

    if (activeSecurityOrigin->canAccess(targetSecurityOrigin))
        return true;

    // Allow access to a "about:blank" page if the dynamic context is a
    // detached context of the same frame as the blank page.
    if (targetSecurityOrigin->isEmpty() && activeWindow->frame() == targetWindow->frame())
        return true;

    return false;
}

bool V8Proxy::canAccessFrame(Frame* target, bool reportError)
{
    // The subject is detached from a frame, deny accesses.
    if (!target)
        return false;

    if (!canAccessPrivate(target->domWindow())) {
        if (reportError)
            reportUnsafeAccessTo(target, ReportNow);
        return false;
    }
    return true;
}

bool V8Proxy::checkNodeSecurity(Node* node)
{
    if (!node)
        return false;

    Frame* target = node->document()->frame();

    if (!target)
        return false;

    return canAccessFrame(target, true);
}

v8::Persistent<v8::Context> V8Proxy::createNewContext(v8::Handle<v8::Object> global, int extensionGroup)
{
    v8::Persistent<v8::Context> result;

    // The activeDocumentLoader pointer could be NULL during frame shutdown.
    if (!m_frame->loader()->activeDocumentLoader())
        return result;

    // Create a new environment using an empty template for the shadow
    // object. Reuse the global object if one has been created earlier.
    v8::Persistent<v8::ObjectTemplate> globalTemplate = V8DOMWindow::GetShadowObjectTemplate();
    if (globalTemplate.IsEmpty())
        return result;

    // Install a security handler with V8.
    globalTemplate->SetAccessCheckCallbacks(V8Custom::v8DOMWindowNamedSecurityCheck, V8Custom::v8DOMWindowIndexedSecurityCheck, v8::Integer::New(V8ClassIndex::DOMWINDOW));
    globalTemplate->SetInternalFieldCount(V8Custom::kDOMWindowInternalFieldCount);

    // Used to avoid sleep calls in unload handlers.
    if (!registeredExtensionWithV8(DateExtension::get()))
        registerExtension(DateExtension::get(), String());

    // Dynamically tell v8 about our extensions now.
    OwnArrayPtr<const char*> extensionNames(new const char*[m_extensions.size()]);
    int index = 0;
    for (size_t i = 0; i < m_extensions.size(); ++i) {
        if (m_extensions[i].group && m_extensions[i].group != extensionGroup)
            continue;

        // Note: we check the loader URL here instead of the document URL
        // because we might be currently loading an URL into a blank page.
        // See http://code.google.com/p/chromium/issues/detail?id=10924
        if (m_extensions[i].scheme.length() > 0 && (m_extensions[i].scheme != m_frame->loader()->activeDocumentLoader()->url().protocol() || m_extensions[i].scheme != m_frame->page()->mainFrame()->loader()->activeDocumentLoader()->url().protocol()))
            continue;

        extensionNames[index++] = m_extensions[i].extension->name();
    }
    v8::ExtensionConfiguration extensions(index, extensionNames.get());
    result = v8::Context::New(&extensions, globalTemplate, global);

    return result;
}

bool V8Proxy::installDOMWindow(v8::Handle<v8::Context> context, DOMWindow* window)
{
    v8::Handle<v8::String> implicitProtoString = v8::String::New("__proto__");
    if (implicitProtoString.IsEmpty())
        return false;

    // Create a new JS window object and use it as the prototype for the  shadow global object.
    v8::Handle<v8::Function> windowConstructor = V8DOMWrapper::getConstructor(V8ClassIndex::DOMWINDOW, getHiddenObjectPrototype(context));
    v8::Local<v8::Object> jsWindow = SafeAllocation::newInstance(windowConstructor);
    // Bail out if allocation failed.
    if (jsWindow.IsEmpty())
        return false;

    // Wrap the window.
    V8DOMWrapper::setDOMWrapper(jsWindow, V8ClassIndex::ToInt(V8ClassIndex::DOMWINDOW), window);
    V8DOMWrapper::setDOMWrapper(v8::Handle<v8::Object>::Cast(jsWindow->GetPrototype()), V8ClassIndex::ToInt(V8ClassIndex::DOMWINDOW), window);

    window->ref();
    V8DOMWrapper::setJSWrapperForDOMObject(window, v8::Persistent<v8::Object>::New(jsWindow));

    // Insert the window instance as the prototype of the shadow object.
    v8::Handle<v8::Object> v8Global = context->Global();
    V8DOMWrapper::setDOMWrapper(v8::Handle<v8::Object>::Cast(v8Global->GetPrototype()), V8ClassIndex::ToInt(V8ClassIndex::DOMWINDOW), window);
    v8Global->Set(implicitProtoString, jsWindow);
    return true;
}

// Create a new environment and setup the global object.
//
// The global object corresponds to a DOMWindow instance. However, to
// allow properties of the JS DOMWindow instance to be shadowed, we
// use a shadow object as the global object and use the JS DOMWindow
// instance as the prototype for that shadow object. The JS DOMWindow
// instance is undetectable from javascript code because the __proto__
// accessors skip that object.
//
// The shadow object and the DOMWindow instance are seen as one object
// from javascript. The javascript object that corresponds to a
// DOMWindow instance is the shadow object. When mapping a DOMWindow
// instance to a V8 object, we return the shadow object.
//
// To implement split-window, see
//   1) https://bugs.webkit.org/show_bug.cgi?id=17249
//   2) https://wiki.mozilla.org/Gecko:SplitWindow
//   3) https://bugzilla.mozilla.org/show_bug.cgi?id=296639
// we need to split the shadow object further into two objects:
// an outer window and an inner window. The inner window is the hidden
// prototype of the outer window. The inner window is the default
// global object of the context. A variable declared in the global
// scope is a property of the inner window.
//
// The outer window sticks to a Frame, it is exposed to JavaScript
// via window.window, window.self, window.parent, etc. The outer window
// has a security token which is the domain. The outer window cannot
// have its own properties. window.foo = 'x' is delegated to the
// inner window.
//
// When a frame navigates to a new page, the inner window is cut off
// the outer window, and the outer window identify is preserved for
// the frame. However, a new inner window is created for the new page.
// If there are JS code holds a closure to the old inner window,
// it won't be able to reach the outer window via its global object.
void V8Proxy::initContextIfNeeded()
{
    // Bail out if the context has already been initialized.
    if (!m_context.IsEmpty())
        return;

#ifdef ANDROID_INSTRUMENT
    android::TimeCounter::start(android::TimeCounter::JavaScriptInitTimeCounter);
#endif
    // Create a handle scope for all local handles.
    v8::HandleScope handleScope;

    // Setup the security handlers and message listener. This only has
    // to be done once.
    static bool isV8Initialized = false;
    if (!isV8Initialized) {
        // Tells V8 not to call the default OOM handler, binding code
        // will handle it.
        v8::V8::IgnoreOutOfMemoryException();
        v8::V8::SetFatalErrorHandler(reportFatalErrorInV8);

        v8::V8::SetGlobalGCPrologueCallback(&V8GCController::gcPrologue);
        v8::V8::SetGlobalGCEpilogueCallback(&V8GCController::gcEpilogue);

        v8::V8::AddMessageListener(&V8ConsoleMessage::handler);

        v8::V8::SetFailedAccessCheckCallbackFunction(reportUnsafeJavaScriptAccess);

        isV8Initialized = true;
    }


    m_context = createNewContext(m_global, 0);
    if (m_context.IsEmpty())
        return;

    v8::Local<v8::Context> v8Context = v8::Local<v8::Context>::New(m_context);
    v8::Context::Scope contextScope(v8Context);

    // Store the first global object created so we can reuse it.
    if (m_global.IsEmpty()) {
        m_global = v8::Persistent<v8::Object>::New(v8Context->Global());
        // Bail out if allocation of the first global objects fails.
        if (m_global.IsEmpty()) {
            disposeContextHandles();
            return;
        }
#ifndef NDEBUG
        V8GCController::registerGlobalHandle(PROXY, this, m_global);
#endif
    }

    installHiddenObjectPrototype(v8Context);
    m_wrapperBoilerplates = v8::Persistent<v8::Array>::New(v8::Array::New(V8ClassIndex::WRAPPER_TYPE_COUNT));
    // Bail out if allocation failed.
    if (m_wrapperBoilerplates.IsEmpty()) {
        disposeContextHandles();
        return;
    }
#ifndef NDEBUG
    V8GCController::registerGlobalHandle(PROXY, this, m_wrapperBoilerplates);
#endif

    if (!installDOMWindow(v8Context, m_frame->domWindow()))
        disposeContextHandles();

    updateDocument();

    setSecurityToken();

    m_frame->loader()->client()->didCreateScriptContextForFrame();

    // FIXME: This is wrong. We should actually do this for the proper world once
    // we do isolated worlds the WebCore way.
    m_frame->loader()->dispatchDidClearWindowObjectInWorld(0);

#ifdef ANDROID_INSTRUMENT
    android::TimeCounter::record(android::TimeCounter::JavaScriptInitTimeCounter, __FUNCTION__);
#endif
=======
    windowShell()->clearForNavigation();
>>>>>>> webkit.org at r54127
}

void V8Proxy::setDOMException(int exceptionCode)
{
    if (exceptionCode <= 0)
        return;

    ExceptionCodeDescription description;
    getExceptionCodeDescription(exceptionCode, description);

    v8::Handle<v8::Value> exception;
    switch (description.type) {
    case DOMExceptionType:
        exception = V8DOMWrapper::convertToV8Object(V8ClassIndex::DOMCOREEXCEPTION, DOMCoreException::create(description));
        break;
    case RangeExceptionType:
        exception = V8DOMWrapper::convertToV8Object(V8ClassIndex::RANGEEXCEPTION, RangeException::create(description));
        break;
    case EventExceptionType:
        exception = V8DOMWrapper::convertToV8Object(V8ClassIndex::EVENTEXCEPTION, EventException::create(description));
        break;
    case XMLHttpRequestExceptionType:
        exception = V8DOMWrapper::convertToV8Object(V8ClassIndex::XMLHTTPREQUESTEXCEPTION, XMLHttpRequestException::create(description));
        break;
#if ENABLE(SVG)
    case SVGExceptionType:
        exception = V8DOMWrapper::convertToV8Object(V8ClassIndex::SVGEXCEPTION, SVGException::create(description));
        break;
#endif
#if ENABLE(XPATH)
    case XPathExceptionType:
        exception = V8DOMWrapper::convertToV8Object(V8ClassIndex::XPATHEXCEPTION, XPathException::create(description));
        break;
#endif
    }

    ASSERT(!exception.IsEmpty());
    v8::ThrowException(exception);
}

v8::Handle<v8::Value> V8Proxy::throwError(ErrorType type, const char* message)
{
    switch (type) {
    case RangeError:
        return v8::ThrowException(v8::Exception::RangeError(v8String(message)));
    case ReferenceError:
        return v8::ThrowException(v8::Exception::ReferenceError(v8String(message)));
    case SyntaxError:
        return v8::ThrowException(v8::Exception::SyntaxError(v8String(message)));
    case TypeError:
        return v8::ThrowException(v8::Exception::TypeError(v8String(message)));
    case GeneralError:
        return v8::ThrowException(v8::Exception::Error(v8String(message)));
    default:
        ASSERT_NOT_REACHED();
        return notHandledByInterceptor();
    }
}

v8::Local<v8::Context> V8Proxy::context(Frame* frame)
{
    v8::Local<v8::Context> context = V8Proxy::mainWorldContext(frame);
    if (context.IsEmpty())
        return v8::Local<v8::Context>();

    if (V8IsolatedContext* isolatedContext = V8IsolatedContext::getEntered()) {
        context = v8::Local<v8::Context>::New(isolatedContext->context());
        if (frame != V8Proxy::retrieveFrame(context))
            return v8::Local<v8::Context>();
    }

    return context;
}

v8::Local<v8::Context> V8Proxy::context()
{
    if (V8IsolatedContext* isolatedContext = V8IsolatedContext::getEntered()) {
        RefPtr<SharedPersistent<v8::Context> > context = isolatedContext->sharedContext();
        if (m_frame != V8Proxy::retrieveFrame(context->get()))
            return v8::Local<v8::Context>();
        return v8::Local<v8::Context>::New(context->get());
    }
    return mainWorldContext();
}

v8::Local<v8::Context> V8Proxy::mainWorldContext()
{
    windowShell()->initContextIfNeeded();
    return v8::Local<v8::Context>::New(windowShell()->context());
}

v8::Local<v8::Context> V8Proxy::mainWorldContext(Frame* frame)
{
    V8Proxy* proxy = retrieve(frame);
    if (!proxy)
        return v8::Local<v8::Context>();

    return proxy->mainWorldContext();
}

v8::Local<v8::Context> V8Proxy::currentContext()
{
    return v8::Context::GetCurrent();
}

v8::Handle<v8::Value> V8Proxy::checkNewLegal(const v8::Arguments& args)
{
    if (!AllowAllocation::m_current)
        return throwError(TypeError, "Illegal constructor");

    return args.This();
}

void V8Proxy::bindJsObjectToWindow(Frame* frame, const char* name, int type, v8::Handle<v8::FunctionTemplate> descriptor, void* impl)
{
    // Get environment.
    v8::Handle<v8::Context> v8Context = V8Proxy::mainWorldContext(frame);
    if (v8Context.IsEmpty())
        return; // JS not enabled.

    v8::Context::Scope scope(v8Context);
    v8::Handle<v8::Object> instance = descriptor->GetFunction();
    V8DOMWrapper::setDOMWrapper(instance, type, impl);

    v8::Handle<v8::Object> global = v8Context->Global();
    global->Set(v8::String::New(name), instance);
}

void V8Proxy::processConsoleMessages()
{
    V8ConsoleMessage::processDelayed();
}

// Create the utility context for holding JavaScript functions used internally
// which are not visible to JavaScript executing on the page.
void V8Proxy::createUtilityContext()
{
    ASSERT(m_utilityContext.IsEmpty());

    v8::HandleScope scope;
    v8::Handle<v8::ObjectTemplate> globalTemplate = v8::ObjectTemplate::New();
    m_utilityContext = v8::Context::New(0, globalTemplate);
    v8::Context::Scope contextScope(m_utilityContext);

    // Compile JavaScript function for retrieving the source line of the top
    // JavaScript stack frame.
    DEFINE_STATIC_LOCAL(const char*, frameSourceLineSource,
        ("function frameSourceLine(exec_state) {"
        "  return exec_state.frame(0).sourceLine();"
        "}"));
    v8::Script::Compile(v8::String::New(frameSourceLineSource))->Run();

    // Compile JavaScript function for retrieving the source name of the top
    // JavaScript stack frame.
    DEFINE_STATIC_LOCAL(const char*, frameSourceNameSource,
        ("function frameSourceName(exec_state) {"
        "  var frame = exec_state.frame(0);"
        "  if (frame.func().resolved() && "
        "      frame.func().script() && "
        "      frame.func().script().name()) {"
        "    return frame.func().script().name();"
        "  }"
        "}"));
    v8::Script::Compile(v8::String::New(frameSourceNameSource))->Run();
}

bool V8Proxy::sourceLineNumber(int& result)
{
#if PLATFORM(ANDROID)
    // TODO(andreip): consider V8's DEBUG flag here, rather than PLATFORM(ANDROID)
    // or, even better, the WEBKIT inspector flag.
    return 0;
#else
    v8::HandleScope scope;
    v8::Handle<v8::Context> v8UtilityContext = V8Proxy::utilityContext();
    if (v8UtilityContext.IsEmpty())
        return false;
    v8::Context::Scope contextScope(v8UtilityContext);
    v8::Handle<v8::Function> frameSourceLine;
    frameSourceLine = v8::Local<v8::Function>::Cast(v8UtilityContext->Global()->Get(v8::String::New("frameSourceLine")));
    if (frameSourceLine.IsEmpty())
        return false;
    v8::Handle<v8::Value> value = v8::Debug::Call(frameSourceLine);
    if (value.IsEmpty())
        return false;
    result = value->Int32Value();
    return true;
#endif
}

bool V8Proxy::sourceName(String& result)
{
#if PLATFORM(ANDROID)
    return false;
#else
    v8::HandleScope scope;
    v8::Handle<v8::Context> v8UtilityContext = utilityContext();
    if (v8UtilityContext.IsEmpty())
        return false;
    v8::Context::Scope contextScope(v8UtilityContext);
    v8::Handle<v8::Function> frameSourceName;
    frameSourceName = v8::Local<v8::Function>::Cast(v8UtilityContext->Global()->Get(v8::String::New("frameSourceName")));
    if (frameSourceName.IsEmpty())
        return false;
    v8::Handle<v8::Value> value = v8::Debug::Call(frameSourceName);
    if (value.IsEmpty())
        return false;
    result = toWebCoreString(value);
    return true;
#endif
}

void V8Proxy::registerExtensionWithV8(v8::Extension* extension)
{
    // If the extension exists in our list, it was already registered with V8.
    if (!registeredExtensionWithV8(extension))
        v8::RegisterExtension(extension);
}

bool V8Proxy::registeredExtensionWithV8(v8::Extension* extension)
{
    for (size_t i = 0; i < m_extensions.size(); ++i) {
        if (m_extensions[i].extension == extension)
            return true;
    }

    return false;
}

void V8Proxy::registerExtension(v8::Extension* extension, const String& schemeRestriction)
{
    registerExtensionWithV8(extension);
    V8ExtensionInfo info = {schemeRestriction, 0, extension};
    m_extensions.append(info);
}

void V8Proxy::registerExtension(v8::Extension* extension, int extensionGroup)
{
    registerExtensionWithV8(extension);
    V8ExtensionInfo info = {String(), extensionGroup, extension};
    m_extensions.append(info);
}

bool V8Proxy::setContextDebugId(int debugId)
{
    ASSERT(debugId > 0);
    v8::HandleScope scope;
    v8::Handle<v8::Context> context = windowShell()->context();
    if (context.IsEmpty())
        return false;
    if (!context->GetData()->IsUndefined())
        return false;

    v8::Context::Scope contextScope(context);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "page,%d", debugId);
    context->SetData(v8::String::New(buffer));

    return true;
}

int V8Proxy::contextDebugId(v8::Handle<v8::Context> context)
{
    v8::HandleScope scope;
    if (!context->GetData()->IsString())
        return -1;
    v8::String::AsciiValue ascii(context->GetData());
    char* comma = strnstr(*ascii, ",", ascii.length());
    if (!comma)
        return -1;
    return atoi(comma + 1);
}

v8::Local<v8::Context> toV8Context(ScriptExecutionContext* context, const WorldContextHandle& worldContext)
{
    if (context->isDocument()) {
        if (V8Proxy* proxy = V8Proxy::retrieve(context))
            return worldContext.adjustedContext(proxy);
    } else if (context->isWorkerContext()) {
        if (WorkerContextExecutionProxy* proxy = static_cast<WorkerContext*>(context)->script()->proxy())
            return proxy->context();
    }
    return v8::Local<v8::Context>();
}

}  // namespace WebCore
