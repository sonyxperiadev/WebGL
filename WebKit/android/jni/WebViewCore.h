/*
 * Copyright 2006, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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

#ifndef WEBVIEWCORE_H
#define WEBVIEWCORE_H

#include "CacheBuilder.h"
#include "CachedHistory.h"
#include "PictureSet.h"
#include "PlatformGraphicsContext.h"
#include "SkColor.h"
#include "SkTDArray.h"
#include "SkRegion.h"
#include "Timer.h"
#include "WebCoreRefObject.h"
#include "WebCoreJni.h"
#include <jni.h>
#include <ui/KeycodeLabels.h>

namespace WebCore {
    class AtomicString;
    class Color;
    class FrameView;
    class HTMLSelectElement;
    class RenderPart;
    class RenderText;
    class Node;
    class RenderTextControl;
    class ScrollView;
    class TimerBase;
}

struct PluginWidgetAndroid;
class SkPicture;
class SkIRect;

namespace android {
    
    class CachedRoot;
    class ListBoxReply;

    class WebCoreReply : public WebCoreRefObject {
    public:
        virtual ~WebCoreReply() {}

        virtual void replyInt(int value) {
            SkDEBUGF(("WebCoreReply::replyInt(%d) not handled\n", value));
        }

        virtual void replyIntArray(const int* array, int count) {
            SkDEBUGF(("WebCoreReply::replyIntArray() not handled\n"));
        }
            // add more replyFoo signatures as needed
    };

    // one instance of WebViewCore per page for calling into Java's WebViewCore
    class WebViewCore : public WebCoreRefObject {
    public:
        /**
         * Initialize the native WebViewCore with a JNI environment, a Java
         * WebViewCore object and the main frame.
         */
        WebViewCore(JNIEnv* env, jobject javaView, WebCore::Frame* mainframe);
        ~WebViewCore();

        // helper function
        static WebViewCore* getWebViewCore(const WebCore::FrameView* view);
        static WebViewCore* getWebViewCore(const WebCore::ScrollView* view);

        // Followings are called from native WebCore to Java

        /**
         * Scroll to an absolute position.
         * @param x The x coordinate.
         * @param y The y coordinate.
         * @param animate If it is true, animate to the new scroll position
         *
         * This method calls Java to trigger a gradual scroll event.
         */
        void scrollTo(int x, int y, bool animate = false);

        /**
         * Scroll to the point x,y relative to the current position.
         * @param x The relative x position.
         * @param y The relative y position.
         * @param animate If it is true, animate to the new scroll position
         */
        void scrollBy(int x, int y, bool animate);

        /**
         * Record the invalid rectangle
         */
        void contentInvalidate(const WebCore::IntRect &rect);

        /**
         * Satisfy any outstanding invalidates, so that the current state
         * of the DOM is drawn.
         */
        void contentDraw();

        /** Invalidate the view/screen, NOT the content/DOM, but expressed in
         *  content/DOM coordinates (i.e. they need to eventually be scaled,
         *  by webview into view.java coordinates
         */
        void viewInvalidate(const WebCore::IntRect& rect);

        /**
         * Invalidate part of the content that may be offscreen at the moment
         */
        void offInvalidate(const WebCore::IntRect &rect);

        /**
         * Called by webcore when the focus was set after returning to prior page
         * used to rebuild and display any changes in focus
         */
        void notifyFocusSet();

        /**
         * Called by webcore when the progress indicator is done
         * used to rebuild and display any changes in focus
         */
        void notifyProgressFinished();

        /**
         * Notify the view that WebCore did its first layout.
         */
        void didFirstLayout();

        /**
         * Notify the view to restore the screen width, which in turn restores 
         * the scale.
         */
        void restoreScale(int);

        /**
         * Tell the java side to update the focused textfield
         * @param pointer   Pointer to the node for the input field.
         * @param   changeToPassword  If true, we are changing the textfield to
         *          a password field, and ignore the String
         * @param text  If changeToPassword is false, this is the new text that
         *              should go into the textfield.
         */
        void updateTextfield(WebCore::Node* pointer,
                bool changeToPassword, const WebCore::String& text);

        // JavaScript support
        void jsAlert(const WebCore::String& url, const WebCore::String& text);
        bool jsConfirm(const WebCore::String& url, const WebCore::String& text);
        bool jsPrompt(const WebCore::String& url, const WebCore::String& message,
                const WebCore::String& defaultValue, WebCore::String& result);
        bool jsUnload(const WebCore::String& url, const WebCore::String& message);

        //
        // Followings support calls from Java to native WebCore
        //

        WebCore::String retrieveHref(WebCore::Frame* frame, WebCore::Node* node);

        WebCore::String getSelection(SkRegion* );
        
        // Create a single picture to represent the drawn DOM (used by navcache)
        void recordPicture(SkPicture* picture);
        
        // Create a set of pictures to represent the drawn DOM, driven by
        // the invalidated region and the time required to draw (used to draw)
        void recordPictureSet(PictureSet* master);
        void setFinalFocus(WebCore::Frame* frame, WebCore::Node* node, 
            int x, int y, bool block);
        void setKitFocus(int moveGeneration, int buildGeneration,
            WebCore::Frame* frame, WebCore::Node* node, int x, int y,
            bool ignoreNullFocus);

        // set the scroll amount that webview.java is currently showing
        void setScrollOffset(int dx, int dy);

        void setGlobalBounds(int x, int y, int h, int v);

        void setSizeScreenWidthAndScale(int width, int height, int screenWidth, 
            int scale, int realScreenWidth, int screenHeight);

        /**
         * Handle key events from Java.
         * @return Whether keyCode was handled by this class.
         */
        bool key(int keyCode, UChar32 unichar, int repeatCount, bool isShift, bool isAlt, bool isDown);

        /**
         * Handle (mouse) click event from Java
         */
        bool click();

        /**
         * Handle touch event
         */
        bool handleTouchEvent(int action, int x, int y);

        /**
         * Handle motionUp event from the UI thread (called touchUp in the
         * WebCore thread).
         */
        void touchUp(int touchGeneration, int buildGeneration, 
            WebCore::Frame* frame, WebCore::Node* node, int x, int y, 
            int size, bool isClick, bool retry);
        
        /**
         * Sets the index of the label from a popup
         */
        void popupReply(int index);
        void popupReply(const int* array, int count);

        /**
         *  Delete text from start to end in the focused textfield. If there is no
         *  focus, or if start == end, silently fail, but set selection to that value.  
         *  If start and end are out of order, swap them.
         *  Use the frame, node, x, and y to ensure that the correct node is focused.
         *  Return a frame. Convenience so replaceTextfieldText can use this function.
         */
        WebCore::Frame* deleteSelection(WebCore::Frame* frame, WebCore::Node* node, int x,
            int y,int start, int end);

        /**
         *  Set the selection of the currently focused textfield to (start, end).
         *  If start and end are out of order, swap them.
         *  Use the frame, node, x, and y to ensure that the correct node is focused.
         *  Return a frame. Convenience so deleteSelection can use this function.
         */
        WebCore::Frame* setSelection(WebCore::Frame* frame, WebCore::Node* node, int x,
            int y,int start, int end);
        /**
         *  In the currenlty focused textfield, represented by frame, node, x, and y (which
         *  are used to ensure it has focus), replace the characters from oldStart to oldEnd
         *  (if oldStart == oldEnd, this will be an insert at that position) with replace,
         *  and set the selection to (start, end).
         */
        void replaceTextfieldText(WebCore::Frame* frame, WebCore::Node* node, int x, int y, 
                int oldStart, int oldEnd, jstring replace, int start, int end);
        void passToJs(WebCore::Frame* frame, WebCore::Node* node, int x, int y, int generation,
            jstring currentText, int jKeyCode, int keyVal, bool down, bool cap, bool fn, bool sym);

        void saveDocumentState(WebCore::Frame* frame);

        // TODO: I don't like this hack but I need to access the java object in
        // order to send it as a parameter to java
        AutoJObject getJavaObject();

        // Return the parent WebView Java object associated with this
        // WebViewCore.
        jobject getWebViewJavaObject();

        void setBackgroundColor(SkColor c);
        void setSnapAnchor(int x, int y);
        void snapToAnchor();
        void unblockFocus() { m_blockFocusChange = false; }
        void updateFrameCache();
        void dumpDomTree(bool);
        void dumpRenderTree(bool);
        void dumpNavTree();

        /*  We maintain a list of active plugins. The list is edited by the
            pluginview itself. The list is used to service invals to the plugin
            pageflipping bitmap.
         */
        void addPlugin(PluginWidgetAndroid*);
        void removePlugin(PluginWidgetAndroid*);
        void invalPlugin(PluginWidgetAndroid*);
        void drawPlugins();

        // Notify the Java side whether it needs to pass down the touch events
        void needTouchEvents(bool);

        // other public functions
    public:
        void removeFrameGeneration(WebCore::Frame* );
        void updateFrameGeneration(WebCore::Frame* );

        // reset the picture set to empty
        void clearContent();
        
        // flatten the picture set to a picture
        void copyContentToPicture(SkPicture* );
        
        // draw the picture set with the specified background color
        bool drawContent(SkCanvas* , SkColor );
        
        // record the inval area, and the picture size
        bool recordContent(SkRegion* , SkIPoint* );
        int screenWidth() const { return m_screenWidth; }
        int scale() const { return m_scale; }
        WebCore::Frame* mainFrame() const { return m_mainFrame; }
        
        // utility to split slow parts of the picture set
        void splitContent();
        
        // these members are shared with webview.cpp
        int retrieveFrameGeneration(WebCore::Frame* );
        static Mutex gFrameCacheMutex;
        CachedRoot* m_frameCacheKit; // nav data being built by webcore
        SkPicture* m_navPictureKit;
        int m_generation; // copy of the number bumped by WebViewNative
        int m_moveGeneration; // copy of state in WebViewNative triggered by move
        int m_touchGeneration; // copy of state in WebViewNative triggered by touch
        int m_lastGeneration; // last action using up to date cache
        bool m_updatedFrameCache;
        bool m_useReplay;
        bool m_findIsUp;
        static Mutex gRecomputeFocusMutex;
        WTF::Vector<int> m_recomputeEvents;
        // These two fields go together: we use the mutex to protect access to
        // m_buttons, so that we, and webview.cpp can look/modify the m_buttons
        // field safely from our respective threads
        static Mutex gButtonMutex;
        WTF::Vector<Container> m_buttons;
        // end of shared members

        // internal functions
    private:
        // Compare the new set of buttons to the old one.  All of the new
        // buttons either replace our old ones or should be added to our list.
        // Then check the old buttons to see if any are no longer needed.
        void updateButtonList(WTF::Vector<Container>* buttons);
        void reset(bool fromConstructor);

        void listBoxRequest(WebCoreReply* reply, const uint16_t** labels,
                size_t count, const int enabled[], size_t enabledCount,
                bool multiple, const int selected[], size_t selectedCountOrSelection);

        friend class ListBoxReply;
        struct FrameGen {
            const WebCore::Frame* m_frame;
            int m_generation;
        };
        WTF::Vector<FrameGen> m_frameGenerations;
        static Mutex gFrameGenerationMutex;
        struct JavaGlue;
        struct JavaGlue*       m_javaGlue;
        WebCore::Frame*        m_mainFrame;
        WebCoreReply*          m_popupReply;
        WebCore::Node* m_lastFocused;
        WebCore::IntRect m_lastFocusedBounds;
        static Mutex m_contentMutex; // protects ui/core thread pictureset access
        PictureSet m_content; // the set of pictures to draw (accessed by UI too)
        SkRegion m_addInval; // the accumulated inval region (not yet drawn)
        SkRegion m_rebuildInval; // the accumulated region for rebuilt pictures
        // Used in passToJS to avoid updating the UI text field until after the
        // key event has been processed.
        bool m_blockTextfieldUpdates;
        bool m_skipContentDraw;
        // Passed in with key events to know when they were generated.  Store it
        // with the cache so that we can ignore stale text changes.
        int m_textGeneration;
        CachedRoot* m_temp;
        SkPicture* m_tempPict;
        int m_buildGeneration;
        int m_maxXScroll;
        int m_maxYScroll;
        int m_scrollOffsetX; // webview.java's current scroll in X
        int m_scrollOffsetY; // webview.java's current scroll in Y
        WebCore::IntPoint m_mousePos;
        bool m_frameCacheOutOfDate;
        bool m_blockFocusChange;
        int m_lastPassed;
        int m_lastVelocity;
        CachedHistory m_history;
        WebCore::Node* m_snapAnchorNode;
        int m_screenWidth;
        int m_scale;
        unsigned m_domtree_version;
        bool m_check_domtree_version;
        
        SkTDArray<PluginWidgetAndroid*> m_plugins;
        WebCore::Timer<WebViewCore> m_pluginInvalTimer;
        void pluginInvalTimerFired(WebCore::Timer<WebViewCore>*) {
            this->drawPlugins();
        }

        WebCore::Frame* changedKitFocus(WebCore::Frame* frame,
            WebCore::Node* node, int x, int y);
        bool commonKitFocus(int generation, int buildGeneration, 
            WebCore::Frame* frame, WebCore::Node* node, int x, int y,
            bool ignoreNullFocus);
        bool finalKitFocus(WebCore::Frame* frame, WebCore::Node* node, int x, int y, bool donotChangeDOMFocus);
        void doMaxScroll(CacheBuilder::Direction dir);
        SkPicture* rebuildPicture(const SkIRect& inval);
        void rebuildPictureSet(PictureSet* );
        void sendMarkNodeInvalid(WebCore::Node* );
        void sendNotifyFocusSet();
        void sendNotifyProgressFinished();
        void sendRecomputeFocus();
        bool handleMouseClick(WebCore::Frame* framePtr, WebCore::Node* nodePtr);
        bool prepareFrameCache();
        void releaseFrameCache(bool newCache);
#if DEBUG_NAV_UI
        uint32_t m_now;
#endif
    };

}   // namespace android

#endif // WEBVIEWCORE_H
