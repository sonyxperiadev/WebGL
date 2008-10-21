#ifdef ANDROID_PLUGINS

/*
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

#include "config.h"
#include "PluginViewAndroid.h"

#include "Document.h"
#include "Element.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "Image.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformGraphicsContext.h"
#include "PluginPackageAndroid.h"
#include "kjs_binding.h"
#include "kjs_proxy.h"
#include "kjs_window.h"
#include "android_graphics.h"
#include "SkCanvas.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include "Settings.h"
#include <kjs/JSLock.h>
#include <kjs/value.h>
#include <wtf/ASCIICType.h>
#include "runtime.h"
#include "WebViewCore.h"

#include "PluginDebug.h"

using KJS::ExecState;
using KJS::Interpreter;
using KJS::JSLock;
using KJS::JSObject;
using KJS::JSValue;
using KJS::UString;

using std::min;

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

PluginViewAndroid* PluginViewAndroid::s_currentPluginView = 0;

void PluginViewAndroid::setParent(ScrollView* parent)
{
    if (parent)
        init();
}

bool PluginViewAndroid::start()
{
    if (m_isStarted)
        return false;

    ASSERT(m_plugin);
    ASSERT(m_plugin->pluginFuncs()->newp);

    NPError npErr;
    PluginViewAndroid::setCurrentPluginView(this);
    {
        KJS::JSLock::DropAllLocks dropAllLocks;
        npErr = m_plugin->pluginFuncs()->newp((NPMIMEType)m_mimeType.data(),
                                              m_instance,
                                              m_mode,
                                              m_paramCount,
                                              m_paramNames,
                                              m_paramValues,
                                              NULL);
        if(npErr)
            PLUGIN_LOG("plugin->newp returned %d\n", static_cast<int>(npErr));
    }
    PluginViewAndroid::setCurrentPluginView(0);

    if (npErr != NPERR_NO_ERROR)
        return false;

    m_isStarted = true;

    return true;
}

void PluginViewAndroid::stop()
{
    if (!m_isStarted)
        return;

    m_isStarted = false;

    KJS::JSLock::DropAllLocks dropAllLocks;

    // Destroy the plugin
    NPError npErr = m_plugin->pluginFuncs()->destroy(m_instance, 0);
    if(npErr)
        PLUGIN_LOG("Plugin destroy returned %d\n", static_cast<int>(npErr));

    m_instance->pdata = 0;
}

void PluginViewAndroid::setCurrentPluginView(PluginViewAndroid* pluginView)
{
    s_currentPluginView = pluginView;
}

PluginViewAndroid* PluginViewAndroid::currentPluginView()
{
    return s_currentPluginView;
}

static char* createUTF8String(const String& str)
{
    CString cstr = str.utf8();
    char* result = reinterpret_cast<char*>(fastMalloc(cstr.length() + 1));

    strncpy(result, cstr.data(), cstr.length() + 1);

    return result;
}

static void freeStringArray(char** stringArray, int length)
{
    if (!stringArray)
        return;

    for (int i = 0; i < length; i++)
        fastFree(stringArray[i]);

    fastFree(stringArray);
}

NPError PluginViewAndroid::getURLNotify(const char* url,
                                        const char* target,
                                        void* notifyData)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

NPError PluginViewAndroid::getURL(const char* url, const char* target)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

NPError PluginViewAndroid::postURLNotify(const char* url,
                                         const char* target,
                                         uint32 len,
                                         const char* buf,
                                         NPBool file,
                                         void* notifyData)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

NPError PluginViewAndroid::postURL(const char* url,
                                   const char* target,
                                   uint32 len,
                                   const char* buf,
                                   NPBool file)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

NPError PluginViewAndroid::newStream(NPMIMEType type,
                                     const char* target,
                                     NPStream** stream)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

int32 PluginViewAndroid::write(NPStream* stream,
                               int32 len,
                               void* buffer)
{
    notImplemented();
    return -1;
}

NPError PluginViewAndroid::destroyStream(NPStream* stream, NPReason reason)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

const char* PluginViewAndroid::userAgent()
{
    if (m_userAgent.isNull())
        m_userAgent = m_parentFrame->loader()->userAgent(m_url).utf8();
    return m_userAgent.data();
}

void PluginViewAndroid::status(const char* message)
{
    String s = DeprecatedString::fromLatin1(message);

    if (Page* page = m_parentFrame->page())
        page->chrome()->setStatusbarText(m_parentFrame, s);
}

NPError PluginViewAndroid::getValue(NPNVariable variable, void* value)
{
    switch (variable) {
        case NPNVWindowNPObject: {
            NPObject* windowScriptObject =
                    m_parentFrame->windowScriptNPObject();

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
            // Dig down through to the parent frame's WebCoreViewBridge
            FrameView* frameView = m_parentFrame->view();
            WebCoreViewBridge* bridge = frameView->getWebCoreViewBridge();
            // Go up parents until we reach the top level.
            while (bridge->getParent() != NULL)
                bridge = bridge->getParent();
            // This is actually an instance of WebCoreView.
            android::WebViewCore* webViewCore =
                    static_cast<android::WebViewCore*>(bridge);
            // Finally, get hold of the Java WebView instance.
            *retObject = webViewCore->getWebViewJavaObject();
            return NPERR_NO_ERROR;
        }
        default:
            return NPERR_GENERIC_ERROR;
    }
}

NPError PluginViewAndroid::setValue(NPPVariable variable, void* value)
{
    switch (variable) {
        case NPPVpluginWindowBool:
            m_isWindowed = value;
            return NPERR_NO_ERROR;
        case NPPVpluginTransparentBool:
            m_isTransparent = value;
            return NPERR_NO_ERROR;
        default:
            notImplemented();
            return NPERR_GENERIC_ERROR;
    }
}

void PluginViewAndroid::invalidateRect(NPRect* rect)
{
    notImplemented();
}

void PluginViewAndroid::invalidateRegion(NPRegion region)
{
    notImplemented();
}

void PluginViewAndroid::forceRedraw()
{
    notImplemented();
}

KJS::Bindings::Instance* PluginViewAndroid::bindingInstance()
{
    NPObject* object = 0;

    if (!m_plugin || !m_plugin->pluginFuncs()->getvalue)
        return 0;

    NPError npErr;
    {
        KJS::JSLock::DropAllLocks dropAllLocks;
        npErr = m_plugin->pluginFuncs()->getvalue(m_instance,
                                                  NPPVpluginScriptableNPObject,
                                                  &object);
    }

    if (npErr != NPERR_NO_ERROR || !object)
        return 0;
    
    RefPtr<KJS::Bindings::RootObject> root = m_parentFrame->createRootObject(
            this,
            m_parentFrame->scriptProxy()->globalObject());
    KJS::Bindings::Instance *instance =
            KJS::Bindings::Instance::createBindingForLanguageInstance(
                    KJS::Bindings::Instance::CLanguage,
                    object,
                    root.release());

    _NPN_ReleaseObject(object);

    return instance;
}

PluginViewAndroid::~PluginViewAndroid()
{
    stop();

    freeStringArray(m_paramNames, m_paramCount);
    freeStringArray(m_paramValues, m_paramCount);

    m_parentFrame->cleanupScriptObjectsForPlugin(this);

    // Don't unload the plugin - let the reference count clean it up.

    // Can't use RefPtr<> because it's name clashing with SkRefCnt<>
    m_viewBridge->unref();
}

void PluginViewAndroid::setParameters(const Vector<String>& paramNames,
                                      const Vector<String>& paramValues)
{
    ASSERT(paramNames.size() == paramValues.size());

    // Also pass an extra parameter
    unsigned size = paramNames.size() + 1;
    unsigned paramCount = 0;

    m_paramNames = reinterpret_cast<char**>(fastMalloc(sizeof(char*) * size));
    m_paramValues = reinterpret_cast<char**>(fastMalloc(sizeof(char*) * size));

    for (unsigned i = 0; i < paramNames.size(); i++) {
        if (equalIgnoringCase(paramNames[i], "windowlessvideo"))
            continue;
        // Don't let the HTML element specify this value!
        if (equalIgnoringCase(paramNames[i], "android_plugins_dir"))
            continue;

        m_paramNames[paramCount] = createUTF8String(paramNames[i]);
        m_paramValues[paramCount] = createUTF8String(paramValues[i]);

        paramCount++;
    }

    // Pass the location of the plug-ins directory so the plug-in
    // knows where it can store any data it needs.
    WebCore::String path = m_parentFrame->settings()->pluginsPath();
    m_paramNames[paramCount] = createUTF8String("android_plugins_dir");
    m_paramValues[paramCount] = createUTF8String(path.utf8().data());
    paramCount++;

    m_paramCount = paramCount;
}

PluginViewAndroid::PluginViewAndroid(Frame* parentFrame,
                                     const IntSize& size,
                                     PluginPackageAndroid* plugin,
                                     Element* element,
                                     const KURL& url,
                                     const Vector<String>& paramNames,
                                     const Vector<String>& paramValues,
                                     const String& mimeType,
                                     bool loadManually)
    : m_plugin(plugin)
    , m_element(element)
    , m_parentFrame(parentFrame)
    , m_userAgent()
    , m_isStarted(false)
    , m_url(url)
    , m_status(PluginStatusLoadedSuccessfully)
    , m_mode(0)
    , m_paramCount(0)
    , m_paramNames(0)
    , m_paramValues(0)
    , m_mimeType()
    , m_instance()
    , m_instanceStruct()
    , m_isWindowed(true)
    , m_isTransparent(false)
    , m_haveInitialized(false)
    , m_lastMessage(0)
    , m_loadManually(loadManually)
    , m_viewBridge(new PluginViewBridgeAndroid())
{
    setWebCoreViewBridge(m_viewBridge);

    if (!m_plugin) {
        m_status = PluginStatusCanNotFindPlugin;
        return;
    }

    m_instance = &m_instanceStruct;
    m_instance->ndata = this;

    m_mimeType = mimeType.utf8();

    setParameters(paramNames, paramValues);

    m_mode = m_loadManually ? NP_FULL : NP_EMBED;

    resize(size);

    // Early initialisation until we can properly parent this
    init();
}

void PluginViewAndroid::init()
{
    if (m_haveInitialized)
        return;
    m_haveInitialized = true;

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

PluginViewBridgeAndroid::PluginViewBridgeAndroid()
{
    m_image = Image::loadPlatformResource("nullplugin");
    if(m_image)
        setSize(m_image->width(), m_image->height());
    else
        PLUGIN_LOG("Couldn't get nullplugin image\n");
}

PluginViewBridgeAndroid::~PluginViewBridgeAndroid()
{
    delete m_image;
}

void PluginViewBridgeAndroid::draw(GraphicsContext* gc,
                                   const IntRect& rect,
                                   bool)
{
    if (gc->paintingDisabled() || !m_image)
        return;

    // Clip the drawing rectangle to our bounds in case it is larger.
    IntRect transRect(rect);
    IntRect bounds = this->getBounds();
    transRect.intersect(bounds);

    // Move the drawing rectangle into our coordinate space.
    transRect.move(-bounds.x(), -bounds.y());

    // Translate the canvas by the view's location so that things will draw
    // in the right place. Clip the canvas to the drawing rectangle.
    SkRect r;
    android_setrect(&r, transRect);
    if (r.isEmpty())
        return;
    SkCanvas* canvas = gc->platformContext()->mCanvas;
    canvas->save();
    canvas->translate(SkIntToScalar(bounds.x()), SkIntToScalar(bounds.y()));
    canvas->clipRect(r);

    // Calculate where to place the image so it appears in the center of the
    // view.
    int imageWidth = m_image->width();
    int imageHeight = m_image->height();
    IntRect centerRect(0, 0, imageWidth, imageHeight);
    IntSize c1(bounds.width()/2, bounds.height()/2);
    IntSize c2(centerRect.width()/2, centerRect.height()/2);
    IntSize diff = c1 - c2;
    centerRect.move(diff);

    // Now move the top-left corner of the image to the top-left corner of
    // the view so that the tiling will hit the center image.
    while (diff.width() > 0)
        diff.setWidth(diff.width() - imageWidth);
    while (diff.height() > 0)
        diff.setHeight(diff.height() - imageHeight);

    // Draw the tiled transparent image adding one extra image width and
    // height so that we get a complete fill.
    gc->beginTransparencyLayer(0.1);
    gc->drawTiledImage(m_image,
                       IntRect(diff.width(), diff.height(),
                               bounds.width() + imageWidth,
                               bounds.height() + imageHeight),
                       IntPoint(0, 0), IntSize(imageWidth, imageHeight));
    gc->endTransparencyLayer();

    // Draw the image in the center.
    gc->drawImage(m_image, centerRect);

    // Restore our canvas
    canvas->restore();
}

} // namespace WebCore

#endif // defined(ANDROID_PLUGINS)
