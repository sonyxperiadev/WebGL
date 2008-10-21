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

#ifndef FrameAndroid_H
#define FrameAndroid_H

#include "CacheBuilder.h"
#include "Frame.h"
#include "FrameLoaderTypes.h"
#include "PlatformGraphicsContext.h"
#include "Plugin.h"
#include "StringHash.h"
#include "WebCoreFrameBridge.h"

class WebCoreResourceHandleClientBridge;

namespace KJS {
  namespace Bindings {
    class RootObject;
  }
}

namespace WebCore {

class EditCommand;
class MouseEventWithHitTestResults;
class Node;
class ResourceHandle;
class ResourceRequest;
struct LinkArray;
struct LinkArray;

enum KWQSelectionDirection {
    KWQSelectingNext,
    KWQSelectingPrevious
};

class FrameAndroid : public Frame
{
public:
    FrameAndroid(Page*, HTMLFrameOwnerElement*, FrameLoaderClient*);
    virtual ~FrameAndroid();

    /* WebCoreFrameBridge setter and getter */
    inline void setBridge(android::WebCoreFrameBridge* bridge)   
    {   
        Release(m_bridge);
        m_bridge = bridge; 
        Retain(m_bridge); 
    }
    inline android::WebCoreFrameBridge* bridge() const { return m_bridge; }
    
    virtual void focusWindow();
    virtual void unfocusWindow();
    
    virtual Range* markedTextRange() const;

    virtual String mimeTypeForFileName(const String&) const;

    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*);
    virtual KJS::Bindings::RootObject* bindingRootObject();
    
    virtual void issueCutCommand();
    virtual void issueCopyCommand();
    virtual void issuePasteCommand();
    virtual void issuePasteAndMatchStyleCommand();
    virtual void issueTransposeCommand();
    virtual void respondToChangedSelection(const Selection& oldSelection, bool closeTyping);
    virtual bool shouldChangeSelection(const Selection& oldSelection, 
                                       const Selection& newSelection, 
                                       EAffinity affinity, 
                                       bool stillSelecting) const;

    virtual void print();

    void addJavascriptInterface(void* javaVM, void* objectInstance, const char* objectNameStr);
    String stringByEvaluatingJavaScriptFromString(const char* script);

    /* FrameAndroid specific */
    CacheBuilder& getCacheBuilder() { return m_cacheBuilder; }
    void select(int, int);
    
    void cleanupForFullLayout(RenderObject *);
    
private:
    static void recursiveCleanupForFullLayout(RenderObject *);
    
    RefPtr<KJS::Bindings::RootObject> m_bindingRoot; // The root object used for objects
                                               // bound outside the context of a plugin.
    /* End FrameAndroid specific */
    
    android::WebCoreFrameBridge* m_bridge;
    CacheBuilder m_cacheBuilder;
    friend class CacheBuilder;
};

inline FrameAndroid* Android(Frame* frame) { return static_cast<FrameAndroid*>(frame); }
inline const FrameAndroid* Android(const Frame* frame) { return static_cast<const FrameAndroid*>(frame); }

inline FrameAndroid* AsAndroidFrame(Frame* frame) { return static_cast<FrameAndroid*>(frame); }

}

#endif
