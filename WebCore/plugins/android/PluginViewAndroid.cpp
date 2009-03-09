/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define LOG_TAG "WebCore"

#include "config.h"
#include "PluginView.h"

#include "Document.h"
#include "Element.h"
#include "EventNames.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "Image.h"
#include "KeyboardEvent.h"
#include "MIMETypeRegistry.h"
#include "MouseEvent.h"
#include "NetworkStateNotifier.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformGraphicsContext.h"
#include "PlatformKeyboardEvent.h"
#include "PluginMainThreadScheduler.h"
#include "PluginPackage.h"
// #include "kjs_binding.h"
// #include "kjs_proxy.h"
#include "android_graphics.h"
#include "SkCanvas.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include "utils/SystemClock.h"
#include "ScriptController.h"
#include "Settings.h"
#include <runtime/JSLock.h>
// #include <kjs/value.h>
#include <wtf/ASCIICType.h>
#include "runtime.h"
#include "WebViewCore.h"

#include "PluginDebug.h"
#include "PluginDebugAndroid.h"
#include "PluginViewBridgeAndroid.h"
#include "PluginWidgetAndroid.h"

#include "android_npapi.h"
#include "SkANP.h"
#include "SkFlipPixelRef.h"

///////////////////////////////////////////////////////////////////////////////

extern void ANPAudioTrackInterfaceV0_Init(ANPInterface* value);
extern void ANPCanvasInterfaceV0_Init(ANPInterface* value);
extern void ANPLogInterfaceV0_Init(ANPInterface* value);
extern void ANPMatrixInterfaceV0_Init(ANPInterface* value);
extern void ANPOffscreenInterfaceV0_Init(ANPInterface* value);
extern void ANPPaintInterfaceV0_Init(ANPInterface* value);
extern void ANPTypefaceInterfaceV0_Init(ANPInterface* value);
extern void ANPWindowInterfaceV0_Init(ANPInterface* value);

struct VarProcPair {
    int         enumValue;
    size_t      size;
    void        (*proc)(ANPInterface*);
};

#define VARPROCLINE(name)   \
    k##name##_ANPGetValue, sizeof(ANP##name), ANP##name##_Init

static const VarProcPair gVarProcs[] = {
    { VARPROCLINE(AudioTrackInterfaceV0)    },
    { VARPROCLINE(LogInterfaceV0)           },
    { VARPROCLINE(CanvasInterfaceV0)        },
    { VARPROCLINE(MatrixInterfaceV0)        },
    { VARPROCLINE(PaintInterfaceV0)         },
    { VARPROCLINE(TypefaceInterfaceV0)      },
    { VARPROCLINE(WindowInterfaceV0)        },
};

/*  return true if var was an interface request (error will be set accordingly)
    return false if var is not a recognized interface (and ignore error param)
 */
static bool anp_getInterface(NPNVariable var, void* value, NPError* error) {
    const VarProcPair* iter = gVarProcs;
    const VarProcPair* stop = gVarProcs + SK_ARRAY_COUNT(gVarProcs);
    while (iter < stop) {
        if (iter->enumValue == var) {
            ANPInterface* i = reinterpret_cast<ANPInterface*>(value);
            if (i->inSize < iter->size) {
                SkDebugf("------- interface %d, expected size %d, allocated %d\n",
                         var, iter->size, i->inSize);
                *error = NPERR_INCOMPATIBLE_VERSION_ERROR;
            } else {
                iter->proc(i);
                *error = NPERR_NO_ERROR;
            }
            return true;
        }
        iter += 1;
    }
    SkDebugf("------ unknown NPNVariable %d\n", var);
    return false;
}

///////////////////////////////////////////////////////////////////////////////

using JSC::ExecState;
using JSC::Interpreter;
using JSC::JSLock;
using JSC::JSObject;
using JSC::UString;

using std::min;

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

void PluginView::platformInit()
{
    setPlatformWidget(new PluginViewBridgeAndroid());

    m_isWindowed = false;   // we don't support windowed yet

    m_window = new PluginWidgetAndroid(this);
    
    m_npWindow.type = NPWindowTypeDrawable;
    m_npWindow.window = 0;
}
    
PluginView::~PluginView()
{
    stop();
    
    deleteAllValues(m_requests);
    
    freeStringArray(m_paramNames, m_paramCount);
    freeStringArray(m_paramValues, m_paramCount);
    
    m_parentFrame->script()->cleanupScriptObjectsForPlugin(this);
  
// Since we have no legacy plugins to check, we ignore the quirks check
//    if (m_plugin && !m_plugin->quirks().contains(PluginQuirkDontUnloadPlugin))
    if (m_plugin) {
        m_plugin->unload();
    }
    delete m_window;
}

void PluginView::init()
{
    if (m_haveInitialized)
        return;
    m_haveInitialized = true;

    android::WebViewCore* c = android::WebViewCore::getWebViewCore(this->parent());
    m_window->init(c);

    if (!m_plugin) {
        ASSERT(m_status == PluginStatusCanNotFindPlugin);
        return;
    }
    
    if (!m_plugin->load()) {
        m_plugin = 0;
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }
    
    if (!start()) {
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }
    
    m_status = PluginStatusLoadedSuccessfully;
}
    
void PluginView::handleMouseEvent(MouseEvent* event)
{
    const AtomicString& type = event->type();
    bool isDown = (eventNames().mousedownEvent == type);
    bool isUp = (eventNames().mouseupEvent == type);
    if (!isDown && !isUp) {
        return;
    }
    
    ANPEvent    evt;
    SkANP::InitEvent(&evt, kTouch_ANPEventType);

    evt.data.touch.action = isDown ? kDown_ANPTouchAction : kUp_ANPTouchAction;
    evt.data.touch.modifiers = 0;   // todo
    // these are relative to plugin
    evt.data.touch.x = event->pageX() - m_npWindow.x; 
    evt.data.touch.y = event->pageY() - m_npWindow.y; 
    if (m_plugin->pluginFuncs()->event(m_instance, &evt)) {
        event->setDefaultHandled();
    }
}

static ANPKeyModifier make_modifiers(bool shift, bool alt) {
    ANPKeyModifier mod = 0;
    if (shift) {
        mod |= kShift_ANPKeyModifier;
    }
    if (alt) {
        mod |= kAlt_ANPKeyModifier;
    }
    return mod;
}

void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    const PlatformKeyboardEvent* pke = event->keyEvent();
    if (NULL == pke) {
        return;
    }

    ANPEvent evt;
    SkANP::InitEvent(&evt, kKey_ANPEventType);

    switch (pke->type()) {
        case PlatformKeyboardEvent::KeyDown:
#ifdef TRACE_KEY_EVENTS
            SkDebugf("--------- KeyDown, ignore\n");
#endif
            return;
        case PlatformKeyboardEvent::RawKeyDown:
            evt.data.key.action = kDown_ANPKeyAction;
            break;
        case PlatformKeyboardEvent::Char:
#ifdef TRACE_KEY_EVENTS
            SkDebugf("--------- Char, ignore\n");
#endif
            return;
        case PlatformKeyboardEvent::KeyUp:
            evt.data.key.action = kUp_ANPKeyAction;
            break;
        default:
#ifdef TRACE_KEY_EVENTS
            SkDebugf("------ unexpected keyevent type %d\n", pke->type());
#endif
            return;
    }
    evt.data.key.nativeCode = pke->nativeVirtualKeyCode();
    evt.data.key.virtualCode = pke->windowsVirtualKeyCode();
    evt.data.key.repeatCount = pke->repeatCount();
    evt.data.key.modifiers = make_modifiers(pke->shiftKey(), pke->altKey());
    evt.data.key.unichar = pke->unichar();
    
    if (m_plugin->pluginFuncs()->event(m_instance, &evt)) {
        event->setDefaultHandled();
    }
}

NPError PluginView::handlePostReadFile(Vector<char>& buffer, uint32 len, const char* buf)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

NPError PluginView::getValueStatic(NPNVariable variable, void* value)
{
    // our interface query is valid with no NPP instance
    NPError error = NPERR_GENERIC_ERROR;
    if ((value != NULL) && (variable == NPNVisOfflineBool)) {
      bool* retValue = static_cast<bool*>(value);
      *retValue = !networkStateNotifier().onLine();
      return NPERR_NO_ERROR;
    }
    (void)anp_getInterface(variable, value, &error);
    return error;
}

void PluginView::setParent(ScrollView* parent)
{
    Widget::setParent(parent);

    if (parent)
        init();
}

void PluginView::setNPWindowRect(const IntRect& rect)
{
    if (!m_isStarted)
        return;
    
    const int width = rect.width();
    const int height = rect.height();

    // the rect is relative to the frameview's (0,0), so use convertToContainingWindow
    IntPoint p = parent()->convertToContainingWindow(rect.location());
    m_npWindow.x = p.x();
    m_npWindow.y = p.y();
    
    m_npWindow.width = width;
    m_npWindow.height = height;
    
    m_npWindow.clipRect.left = 0;
    m_npWindow.clipRect.top = 0;
    m_npWindow.clipRect.right = width;
    m_npWindow.clipRect.bottom = height;

    if (m_plugin->pluginFuncs()->setwindow) {
        JSC::JSLock::DropAllLocks dropAllLocks(false);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
        setCallingPlugin(false);
    }
    
    m_window->setWindow(m_npWindow.x, m_npWindow.y, width, height,
                        m_isTransparent);
}

void PluginView::stop()
{
    if (!m_isStarted)
        return;

    HashSet<RefPtr<PluginStream> > streams = m_streams;
    HashSet<RefPtr<PluginStream> >::iterator end = streams.end();
    for (HashSet<RefPtr<PluginStream> >::iterator it = streams.begin(); it != end; ++it) {
        (*it)->stop();
        disconnectStream((*it).get());
    }

    ASSERT(m_streams.isEmpty());

    m_isStarted = false;

    JSC::JSLock::DropAllLocks dropAllLocks(false);

    PluginMainThreadScheduler::scheduler().unregisterPlugin(m_instance);

    // Destroy the plugin
    NPSavedData* savedData = 0;
    setCallingPlugin(true);
    NPError npErr = m_plugin->pluginFuncs()->destroy(m_instance, &savedData);
    setCallingPlugin(false);
    LOG_NPERROR(npErr);

    if (savedData) {
        if (savedData->buf)
            NPN_MemFree(savedData->buf);
        NPN_MemFree(savedData);
    }

    m_instance->pdata = 0;
}

const char* PluginView::userAgentStatic()
{
    return 0;
}

const char* PluginView::userAgent()
{
    if (m_userAgent.isNull())
        m_userAgent = m_parentFrame->loader()->userAgent(m_url).utf8();
    return m_userAgent.data();
}

NPError PluginView::getValue(NPNVariable variable, void* value)
{
    switch (variable) {
        case NPNVWindowNPObject: {
            NPObject* windowScriptObject =
                    m_parentFrame->script()->windowScriptNPObject();

            // Return value is expected to be retained, as described
            // here:
            // <http://www.mozilla.org/projects/plugin/npruntime.html>
            if (windowScriptObject)
                _NPN_RetainObject(windowScriptObject);

            void** v = (void**)value;
            *v = windowScriptObject;

            return NPERR_NO_ERROR;
        }

        case NPNVPluginElementNPObject: {
            NPObject* pluginScriptObject = 0;

            if (m_element->hasTagName(appletTag) ||
                m_element->hasTagName(embedTag) ||
                m_element->hasTagName(objectTag)) {
                HTMLPlugInElement* pluginElement =
                        static_cast<HTMLPlugInElement*>(m_element);
                pluginScriptObject = pluginElement->getNPObject();
            }

            // Return value is expected to be retained, as described
            // here:
            // <http://www.mozilla.org/projects/plugin/npruntime.html>
            if (pluginScriptObject)
                _NPN_RetainObject(pluginScriptObject);

            void** v = (void**)value;
            *v = pluginScriptObject;

            return NPERR_NO_ERROR;
        }

        case NPNVnetscapeWindow: {
            // Return the top level WebView Java object associated
            // with this instance.
            jobject *retObject = static_cast<jobject*>(value);
            *retObject = android::WebViewCore::getWebViewCore(parent())->getWebViewJavaObject();
            return NPERR_NO_ERROR;
        }

        case NPNVisOfflineBool: {
            if (value == NULL) {
              return NPERR_GENERIC_ERROR;
            }
            bool* retValue = static_cast<bool*>(value);
            *retValue = !networkStateNotifier().onLine();
            return NPERR_NO_ERROR;
        }
            
        case kSupportedDrawingModel_ANPGetValue: {
            uint32_t* bits = reinterpret_cast<uint32_t*>(value);
            *bits = (1 << kBitmap_ANPDrawingModel);
            return NPERR_NO_ERROR;
        }
            
        default: {
            NPError error = NPERR_GENERIC_ERROR;
            (void)anp_getInterface(variable, value, &error);
            return error;
        }
    }
}

NPError PluginView::platformSetValue(NPPVariable variable, void* value)
{
    NPError error = NPERR_GENERIC_ERROR;

    switch (variable) {
        case kRequestDrawingModel_ANPSetValue: {
            ANPDrawingModel model = reinterpret_cast<ANPDrawingModel>(value);
            switch (model) {
                case kBitmap_ANPDrawingModel:
                    m_window->setDrawingModel(model);
                    error = NPERR_NO_ERROR;
                    break;
                default:
                    break;
            }
        }
        default:
            break;
    }
    return error;
}

void PluginView::invalidateRect(const IntRect& r)
{
    m_window->inval(r, true);
}

void PluginView::invalidateRect(NPRect* rect)
{
    IntRect r;

    if (rect) {
        r = IntRect(rect->left, rect->top,
                    rect->right - rect->left, rect->bottom - rect->top);
    } else {
        r = IntRect(0, 0, m_npWindow.width, m_npWindow.height);
    }

    m_window->inval(r, true);
//    android::WebViewCore::getWebViewCore(parent())->contentInvalidate(r);
}

void PluginView::invalidateRegion(NPRegion region)
{
    // we don't support/define regions (yet), so do nothing
}

void PluginView::forceRedraw()
{
    this->invalidateRect(0);
}

void PluginView::setFocus()
{
    Widget::setFocus();
//    SkDebugf("------------- setFocus %p\n", this);
}

void PluginView::show()
{
    setSelfVisible(true);
    Widget::show();
}

void PluginView::hide()
{
    setSelfVisible(false);
    Widget::hide();
}

void PluginView::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!m_isStarted) {
        // Draw the "missing plugin" image
        paintMissingPluginIcon(context, rect);
        return;
    }
    
    IntRect frame = frameRect();
    if (!frame.width() || !frame.height()) {
        return;
    }
    
    m_window->inval(rect, false);
    m_window->draw(android_gc2canvas(context));
}

// new as of SVN 38068, Nov 5 2008
void PluginView::updatePluginWidget()
{
    // I bet/hope we can move all of setNPWindowRect() into here
    FrameView* frameView = static_cast<FrameView*>(parent());
    if (frameView) {
        m_windowRect = IntRect(frameView->contentsToWindow(frameRect().location()), frameRect().size());
    }
}

// new as of SVN 38068, Nov 5 2008
void PluginView::setParentVisible(bool) { 
    notImplemented(); 
}

} // namespace WebCore
