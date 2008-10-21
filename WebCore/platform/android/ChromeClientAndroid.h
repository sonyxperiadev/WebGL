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

#ifndef ChromeClientAndroid_h
#define ChromeClientAndroid_h

#include "ChromeClient.h"

namespace WebCore {

class FrameAndroid;
    class ChromeClientAndroid : public ChromeClient {
    public:
        ChromeClientAndroid() : m_frame(NULL) {}
        virtual void chromeDestroyed();
        
        virtual void setWindowRect(const FloatRect&);
        virtual FloatRect windowRect();
        
        virtual FloatRect pageRect();
        
        virtual float scaleFactor();
        
        virtual void focus();
        virtual void unfocus();
        
        virtual bool canTakeFocus(FocusDirection);
        virtual void takeFocus(FocusDirection);
        
        // The Frame pointer provides the ChromeClient with context about which
        // Frame wants to create the new Page.  Also, the newly created window
        // should not be shown to the user until the ChromeClient of the newly
        // created Page has its show method called.
        virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&);
        virtual Page* createModalDialog(Frame*, const FrameLoadRequest&);
        virtual void show();
        
        virtual bool canRunModal();
        virtual void runModal();
        
        virtual void setToolbarsVisible(bool);
        virtual bool toolbarsVisible();
        
        virtual void setStatusbarVisible(bool);
        virtual bool statusbarVisible();
        
        virtual void setScrollbarsVisible(bool);
        virtual bool scrollbarsVisible();
        
        virtual void setMenubarVisible(bool);
        virtual bool menubarVisible();
        
        virtual void setResizable(bool);
        
        virtual void addMessageToConsole(const String& message, unsigned int lineNumber, const String& sourceID);
        
        virtual bool canRunBeforeUnloadConfirmPanel();
        virtual bool runBeforeUnloadConfirmPanel(const String& message, Frame* frame);
        
        virtual void closeWindowSoon();
        
        virtual void runJavaScriptAlert(Frame*, const String&);
        virtual bool runJavaScriptConfirm(Frame*, const String&);
        virtual bool runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result);

        virtual void setStatusbarText(const String&);
        virtual bool shouldInterruptJavaScript();
        virtual bool tabsToLinks() const;

        virtual IntRect windowResizerRect() const;
        virtual void addToDirtyRegion(const IntRect&);
        virtual void scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect);
        virtual void updateBackingStore();
        virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned int);
        virtual void setToolTip(const String&);
        virtual void print(Frame*);
        virtual bool runDatabaseSizeLimitPrompt(Frame*, const String&);
        virtual void exceededDatabaseQuota(Frame*, const String&);
    // Android-specific
        void setFrame(FrameAndroid* frame) { m_frame = frame; }
    private:
        FrameAndroid* m_frame;
    };

}

#endif
