/* 
**
** Copyright 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "config.h"
#include "FrameAndroid.h"

#include <JavaScriptCore/bindings/runtime_root.h>
#include <JavaScriptCore/bindings/runtime_object.h>
#include <JavaScriptCore/bindings/jni/jni_utility.h>
#include "jni.h"
#include "kjs_proxy.h"
#include "kjs_window.h"

#include "CacheBuilder.h"
#include "CachedImage.h"
#include "Document.h"
#include "EventNames.h"
#include "FrameLoader.h"
#include "FramePrivate.h"
#include "FrameView.h"
#include "HTMLAreaElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "Image.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformString.h"
#ifdef ANDROID_PLUGINS
#include "Plugin.h"
#include "PluginViewAndroid.h"
#endif
#include "RenderImage.h"
#include "RenderTable.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SkScalar.h"
#include "Text.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreViewBridge.h"

#define LOG_TAG "WebCore"
#undef LOG
#include <utils/Log.h>

#ifdef ANDROID_INSTRUMENT
#include "CString.h"
#include "Cache.h"
#endif

using KJS::JSLock;
using KJS::JSValue;

namespace WebCore {
    
#ifdef ANDROID_INSTRUMENT
// The following code should be inside Frame.cpp. But android LOG is conflict 
// with webcore LOG
void Frame::resetTimeCounter() {
    KJS::JSGlobalObject::resetTimeCounter();
    resetLayoutTimeCounter();
    resetPaintTimeCounter();
    resetCSSTimeCounter();
    resetParsingTimeCounter();
    resetCalculateStyleTimeCounter();
    resetFramebridgeTimeCounter();
    resetSharedTimerTimeCounter();
    resetResourceLoadTimeCounter();
    resetWebViewCoreTimeCounter();
    LOG(LOG_DEBUG, "WebCore", "*-* Start browser instrument\n");
}

void Frame::reportTimeCounter(String url, int total, int totalThreadTime)
{
    LOG(LOG_DEBUG, "WebCore", 
            "*-* Total load time: %d ms, thread time: %d ms for %s\n", 
            total, totalThreadTime, url.utf8().data());
    KJS::JSGlobalObject::reportTimeCounter();
    reportLayoutTimeCounter();
    reportPaintTimeCounter();
    reportCSSTimeCounter();
    reportParsingTimeCounter();
    reportCalculateStyleTimeCounter();
    reportFramebridgeTimeCounter();
    reportSharedTimerTimeCounter();
    reportResourceLoadTimeCounter();
    reportWebViewCoreTimeCounter();
    LOG(LOG_DEBUG, "WebCore", "Current cache has %d bytes live and %d bytes dead", cache()->getLiveSize(), cache()->getDeadSize());
}
#endif    

#ifdef ANDROID_PLUGINS
KJS::Bindings::Instance* Frame::createScriptInstanceForWidget(Widget* widget)
{
    if (widget->isFrameView())
        return 0;

    return static_cast<PluginViewAndroid*>(widget)->bindingInstance();
}
#endif

FrameAndroid::FrameAndroid(Page* page, HTMLFrameOwnerElement* element, FrameLoaderClient* client)
    : Frame(page, element, client), 
    m_bindingRoot(NULL), m_bridge(NULL)
{
}

FrameAndroid::~FrameAndroid()
{
    if (view() != NULL)
        view()->getWebCoreViewBridge()->removeFrameGeneration(this);
    Release(m_bridge);
    setView(0);
    loader()->clearRecordedFormValues();
}

void FrameAndroid::select(int selectionStart, int selectionEnd) 
{
    if (selectionStart > selectionEnd) {
        int temp = selectionStart;
        selectionStart = selectionEnd;
        selectionEnd = temp;
    }
    Document* doc = document();
    if (!doc)
        return;
    Node* focus = doc->focusedNode();
    if (!focus)
        return;
    RenderObject* renderer = focus->renderer();
    if (renderer && (renderer->isTextField() || renderer->isTextArea())) {
        RenderTextControl* rtc = static_cast<RenderTextControl*>(renderer);
        rtc->setSelectionRange(selectionStart, selectionEnd);
        Frame::revealSelection();
    }
}

void FrameAndroid::cleanupForFullLayout(RenderObject* obj)
{
    recursiveCleanupForFullLayout(obj);
}

void FrameAndroid::recursiveCleanupForFullLayout(RenderObject* obj)
{
    obj->setNeedsLayout(true, false);
#ifdef ANDROID_LAYOUT
    if (obj->isTable())
        (static_cast<RenderTable *>(obj))->clearSingleColumn();
#endif
    for (RenderObject* n = obj->firstChild(); n; n = n->nextSibling())
        recursiveCleanupForFullLayout(n);
}

KJS::Bindings::RootObject* FrameAndroid::bindingRootObject() 
{
    if (!m_bindingRoot)
        m_bindingRoot = KJS::Bindings::RootObject::create(0, scriptProxy()->globalObject());    // The root gets deleted by JavaScriptCore.
    ASSERT(settings()->isJavaScriptEnabled()); 
    // The root gets deleted by JavaScriptCore.
    m_bindingRoot = KJS::Bindings::RootObject::create(0, scriptProxy()->globalObject());
    return m_bindingRoot.get();
}

/* 
* This function provides the ability to add a Java class to Javascript and 
* expose it through the Window object.
* The code to access the object would look something like: window.<objectName>.<class method>
*/
void FrameAndroid::addJavascriptInterface(void *javaVM, void* objectInstance, const char* objectNameStr)
{
    // Obtain the window object from KJS
    KJS::Bindings::RootObject *root = bindingRootObject();
    KJS::JSObject *rootObject = root->globalObject();
    KJS::ExecState *exec = root->globalObject()->globalExec();
    KJS::JSObject *window = rootObject->get(exec, KJS::Identifier("window"))->getObject();
    if (!window) {
        LOGE("Warning: couldn't get window object");
        return;
    }
    
    KJS::Bindings::setJavaVM((JavaVM*)javaVM);
  
    // Add the binding to JS environment
    KJS::JSObject *addedObject =
        KJS::Bindings::Instance::createRuntimeObject(KJS::Bindings::Instance::JavaLanguage,
                                                     (jobject)objectInstance, root);

    // Add the binding name to the window's table of child objects.
    window->put(exec, KJS::Identifier(objectNameStr), addedObject);
}

/*
* This function executes the provided javascript string in the context of
* the frame's document. The code is based on the implementation of the same
* function in WebCoreFrameBridge.mm
*/
String FrameAndroid::stringByEvaluatingJavaScriptFromString(const char* script)
{
    ASSERT(document());
    JSValue* result = loader()->executeScript(String(script), true);
    if (!result)
        return String();
    JSLock lock;
    // note: result->getString() returns a UString.
    return String(result->isString() ? result->getString() : 
            result->toString(scriptProxy()->globalObject()->globalExec()));
}

#if 0   // disabled for now by <reed>
// experimental function to allow portable code to query our bg-ness
bool android_should_draw_background(RenderObject* obj)
{
    Document* doc = obj->document();
    if (doc) {
        Frame* frame = doc->frame();
        if (frame) {
            Page* page = frame->page();
            if (page) {
                frame = page->mainFrame();
                if (frame) {
                    return AsAndroidFrame(frame)->shouldDrawBackgroundPhase();
                }
            }
        }
    }
    return false;
}
#endif

///////////////!!!!!!!!!!!! MUST COMPLETE THESE

#define verifiedOk() { } // not a problem that it's not implemented

// These two functions are called by JavaScript Window.focus() and Window.blur() methods. If
// a window has the window focus method called on it, it should be moved to the top of the
// browser window stack. If blur is called, it is moved to the bottom of the stack.
void FrameAndroid::focusWindow() { verifiedOk(); notImplemented(); }
void FrameAndroid::unfocusWindow() { verifiedOk(); notImplemented(); }

// This function is called by JavaScript when window.print() is called. It would normally map 
// straight to the menu print item.
// It is ok that we don't support this at this time.
void FrameAndroid::print() { verifiedOk(); }

// These functions are used to interact with the platform's clipboard. It is ok that we don't 
// support these at this time as we currently don't support sites that would use this 
// functionality.
void FrameAndroid::issueCutCommand() { verifiedOk(); }
void FrameAndroid::issueCopyCommand() { verifiedOk(); }
void FrameAndroid::issuePasteCommand() { verifiedOk(); }
void FrameAndroid::issuePasteAndMatchStyleCommand() { verifiedOk(); }

// This function seems to switch the two characters around the cursor.
// See WebHTMLView.mm:transpose.
// We are currently not implementing it.
void FrameAndroid::issueTransposeCommand() { verifiedOk(); }

// These functions are called when JavaScript tries to access the script interface for
// these objects. For Object and Embed, the interface expected from the 'plugin' is
// well defined here:
// http://www.mozilla.org/projects/plugins/npruntime.html
KJS::Bindings::Instance* FrameAndroid::getObjectInstanceForWidget(Widget *) { ASSERT(0); notImplemented(); return 0; }
KJS::Bindings::Instance* FrameAndroid::getEmbedInstanceForWidget(Widget *) { ASSERT(0); notImplemented(); return 0; }
KJS::Bindings::Instance* FrameAndroid::getAppletInstanceForWidget(Widget*) { ASSERT(0); notImplemented(); return 0; }

// These two functions are used to handle spell checking and context menus on selected text.
// It seems that if the text is selected, and the user tries to change the selection, the
// shouldChange function indicates if it is allowed or not. The second function is used for
// spell checking - check FrameMac.mm implementation for details.
bool FrameAndroid::shouldChangeSelection(Selection const&,Selection const&,EAffinity,bool) const { notImplemented(); return true; }
void FrameAndroid::respondToChangedSelection(Selection const&,bool) { notImplemented();  }

// This function is called when an textbox needs to find out if it's contents
// is marked. It is used in conjunction with the set method which uses
// NSArray, a Mac specific type. The set method is called whenever text is
// added (not removed) via a single key press (paste does not count)
Range* FrameAndroid::markedTextRange() const { return 0; }

// This function is called when determining the correct mimetype for a file that is going
// to be POSTed. That is when the form element <input type="file"> is used, and the file is
// being sent the server, the client should set the mimetype of the file.
String FrameAndroid::mimeTypeForFileName(String const&) const { ASSERT(0); notImplemented(); return String(); }

};  /* WebCore namespace */
