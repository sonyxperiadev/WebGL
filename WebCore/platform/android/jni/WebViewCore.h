/*
 *
 * Copyright 2006, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *
 *     http://www.apache.org/licenses/LICENSE-2.0 
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License.
 */

#ifndef ANDROID_WIDGET_HTMLWIDGET_H
#define ANDROID_WIDGET_HTMLWIDGET_H

#include "WebCoreViewBridge.h"
#include "CacheBuilder.h"
#include "CachedHistory.h"
#include "FrameView.h"
#include "SkColor.h"
#include "SkScalar.h"
#include "SkRegion.h"
#include <ui/Rect.h>
#include <jni.h>

namespace WebCore {
    class AtomicString;
    class Color;
    class GraphicsContext;
    class HTMLSelectElement;
    class RenderPart;
    class RenderText;
    class FrameAndroid;
    class Node;
    class RenderTextControl;
}

class SkPicture;

namespace android {
    
    class CachedRoot;
    
    class ListBoxReply;

    class WebViewCore : public WebCoreViewBridge 
    {
    public:
        /**
         * Initialize the ViewBridge with a JNI environment, a native HTMLWidget object
         * and an associated frame to perform actions on.
         */
        WebViewCore(JNIEnv* env, jobject javaView, WebCore::FrameView* view);
        virtual ~WebViewCore();

        /**
         * Set the scroll offset.
         * @param x The x position of the new scroll offset.
         * @param y The y position of the new scroll offset.
         */
//        void setScrollOffset(int x, int y);

        // Inherited from WebCoreViewBridge
        virtual void draw(WebCore::GraphicsContext* ctx, 
            const WebCore::IntRect& rect, bool invalCache);

        /**
         * Layout our Frame if needed and recursively layout all child frames.
         */
        virtual void layout();

        /**
         * Scroll to an absolute position.
         * @param x The x coordinate.
         * @param y The y coordinate.
         * @param animate If it is true, animate to the new scroll position
         *
         * This method calls Java to trigger a gradual scroll event.
         */
        virtual void scrollTo(int x, int y, bool animate = false);
                 
        /**
         * Scroll to the point x,y relative to the current position.
         * @param x The relative x position.
         * @param y The relative y position.
         */
        virtual void scrollBy(int x, int y);
        
        /**
         * Mark the display list as invalid, and post an event (once) to
         * rebuild the display list by calling webcore to draw the dom
         */
        virtual void contentInvalidate();
        virtual void contentInvalidate(const WebCore::IntRect &rect);
        
        // invalidate the view/display, NOT the content/DOM
        virtual void viewInvalidate() { sendViewInvalidate(); }
        
        /**
         * Called by webcore when the focus was set after returning to prior page
         * used to rebuild and display any changes in focus
         */
        virtual void notifyFocusSet();
        /**
         * Called by webcore when the progress indicator is done
         * used to rebuild and display any changes in focus
         */
        virtual void notifyProgressFinished();
        
        /**
         * On resize is called after a setSize event on WebCoreViewBridge. onResize
         * then tells the frame to relayout the contents due to the size change
         */
        virtual void onResize();

        /**
         * Notify the view that WebCore did its first layout.
         */
        virtual void didFirstLayout();

        /**
         * Notify the view to restore the screen width, which in turn restores 
         * the scale.
         */
        virtual void restoreScale(int);

        /* Set the view and frame */
        virtual void setView(WebCore::FrameView* view) {
            if (mView)
                mView->deref();
            mView = view;
            if (mView) {
                mView->ref();
                mFrame = (WebCore::FrameAndroid*)mView->frame();
                reset(false);
            } else
                mFrame = NULL;
        }

        // new methods for this subclass

        void reset(bool fromConstructor);

        WebCore::FrameAndroid* frame() const { return mFrame; }
        WebCore::String retrieveHref(WebCore::Frame* frame, WebCore::Node* node);

        /**
         *  Return the url of the image located at (x,y) in content coordinates, or
         *  null if there is no image at that point.
         *
         *  @param x    x content ordinate
         *  @param y    y content ordinate
         *  @return WebCore::String  url of the image located at (x,y), or null if there is
         *                  no image there.
         */
        WebCore::String retrieveImageRef(int x, int y);

        WebCore::String getSelection(SkRegion* );
        void recordPicture(SkPicture* picture, bool invalCache);
        void setFrameCacheOutOfDate();
        void setFinalFocus(WebCore::Frame* frame, WebCore::Node* node, 
            int x, int y, bool block);
        void setKitFocus(int moveGeneration, int buildGeneration,
            WebCore::Frame* frame, WebCore::Node* node, int x, int y,
            bool ignoreNullFocus);
        int getMaxXScroll() const { return mMaxXScroll; }
        int getMaxYScroll() const { return mMaxYScroll; }
        void setMaxXScroll(int maxx) { mMaxXScroll = maxx; }
        void setMaxYScroll(int maxy) { mMaxYScroll = maxy; }

        int contentWidth() const { return mView->contentsWidth(); }
        int contentHeight() const { return mView->contentsHeight(); }
    
        // the visible rect is in document coordinates, and describes the
        // intersection of the document with the "window" in the UI.
        void getVisibleRect(WebCore::IntRect* rect) const;
        void setVisibleRect(const WebCore::IntRect& rect);

        WebCore::FrameView* getFrameView()  { return mView; }
        void listBoxRequest(WebCoreReply* reply, const uint16_t** labels, size_t count, const int enabled[], size_t enabledCount, 
                bool multiple, const int selected[], size_t selectedCountOrSelection);

        /**
         * Handle keyDown events from Java.
         * @param keyCode The key pressed.
         * @return Whether keyCode was handled by this class.
         */
        bool keyUp(KeyCode keyCode, int keyValue);

        /**
         * Handle motionUp event from the UI thread (called touchUp in the
         * WebCore thread).
         */
        void touchUp(int touchGeneration, int buildGeneration, 
            WebCore::Frame* frame, WebCore::Node* node, int x, int y, 
            int size, bool isClick, bool retry);
        
        /**
         * Return a new WebCoreViewBridge to interface with the passed in view.
         */
        virtual WebCoreViewBridge* createBridgeForView(WebCore::FrameView* view);
        
        /**
         * Sets the index of the label from a popup
         */
        void popupReply(int index);
        void popupReply(SkTDArray<int>array);
        
        virtual void jsAlert(const WebCore::String& url, const WebCore::String& text);
        virtual bool jsConfirm(const WebCore::String& url, const WebCore::String& text);
        virtual bool jsPrompt(const WebCore::String& url, const WebCore::String& message, const WebCore::String& defaultValue, WebCore::String& result);
        virtual bool jsUnload(const WebCore::String& url, const WebCore::String& message);
        
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

        void saveDocumentState(WebCore::Frame* frame, WebCore::Node* node, int x, int y);

        // TODO: I don't like this hack but I need to access the java object in
        // order to send it as a parameter to java
        jobject getJavaObject();

        // Return the parent WebView Java object associated with this
        // WebViewCore.
        jobject getWebViewJavaObject();
        
        bool pinXToDocument(int* yPtr);
        bool pinYToDocument(int* yPtr);

        WebCore::RenderLayer* getRenderLayer();
        
        /**
         * Tell the java side to update the focused textfield
         * @param pointer   Pointer to the node for the input field.
         * @param   changeToPassword  If true, we are changing the textfield to
         *          a password field, and ignore the String
         * @param text  If changeToPassword is false, this is the new text that 
         *              should go into the textfield.
         */
        virtual void updateTextfield(WebCore::Node* pointer, 
                bool changeToPassword, const WebCore::String& text);

        virtual void removeFrameGeneration(WebCore::Frame* );
        virtual void updateFrameGeneration(WebCore::Frame* );
        
        void setBackgroundColor(SkColor c);
        void setSnapAnchor(int x, int y);
        void snapToAnchor();
        void unblockFocus() { mBlockFocusChange = false; }
        void updateFrameCache();
        void dump();
        
        // jni methods
        static void Destroy(JNIEnv*, jobject);
        static void Dump(JNIEnv*, jobject);
        static void RefreshPlugins(JNIEnv*, jobject, jboolean);
        static void SetSize(JNIEnv*, jobject, jint, jint, jint, jfloat);
        static void SetVisibleRect(JNIEnv*, jobject, jint, jint, jint, jint);
        static jboolean KeyUp(JNIEnv*, jobject, jint, jint);
        static void DeleteSelection(JNIEnv*, jobject, jint, jint, jint, jint,
                jint, jint);
        static void SetSelection(JNIEnv*, jobject, jint, jint, jint, jint,
                jint, jint);
        static void ReplaceTextfieldText(JNIEnv*, jobject, jint, jint, jint,
                jint, jint, jint, jstring, jint, jint);
        static void PassToJs(JNIEnv*, jobject, jint, jint, jint, jint, jint,
                jstring, jint, jint, jboolean, jboolean, jboolean, jboolean);
        static void SaveDocumentState(JNIEnv*, jobject, jint, jint, jint, jint);
        static void Draw(JNIEnv*, jobject, jobject);
        static void SendListBoxChoices(JNIEnv*, jobject, jbooleanArray, jint);
        static void SendListBoxChoice(JNIEnv* env, jobject obj, jint choice);
        static void ClearMatches(JNIEnv*, jobject);
        static jboolean Find(JNIEnv*, jobject, jstring, jboolean, jboolean);
        static jstring FindAddress(JNIEnv*, jobject, jstring);
        static jint FindAll(JNIEnv*, jobject, jstring);
        static void TouchUp(JNIEnv*, jobject, jint, jint, jint, jint, jint,
                jint, jint, jboolean, jboolean);
        static jstring RetrieveHref(JNIEnv*, jobject, jint, jint);
        static jstring RetrieveImageRef(JNIEnv*, jobject, jint, jint);
        static void SetFinalFocus(JNIEnv*, jobject, jint, jint, jint, jint,
                jboolean);
        static void SetKitFocus(JNIEnv*, jobject, jint, jint, jint, jint, jint,
                jint, jboolean);
        static void UnblockFocus(JNIEnv*, jobject);
        static void UpdateFrameCache(JNIEnv*, jobject);
        static jint GetContentMinPrefWidth(JNIEnv*, jobject);
        static void SetViewportSettingsFromNative(JNIEnv*, jobject);
        static void SetBackgroundColor(JNIEnv *env, jobject obj, jint color);
        static void SetSnapAnchor(JNIEnv*, jobject, jint, jint);
        static void SnapToAnchor(JNIEnv*, jobject);
        static jstring GetSelection(JNIEnv*, jobject, jobject);
        // end jni methods

        // these members are shared with webview.cpp
        int retrieveFrameGeneration(WebCore::Frame* );
        static Mutex gFrameCacheMutex;
        CachedRoot* mFrameCacheKit; // nav data being built by webcore
        SkPicture* mNavPictureKit;
        int mGeneration; // copy of the number bumped by WebViewNative
        int mMoveGeneration; // copy of state in WebViewNative triggered by move
        int mTouchGeneration; // copy of state in WebViewNative triggered by touch
        int mLastGeneration; // last action using up to date cache
        bool mUpdatedFrameCache;
        bool mUseReplay;
        static Mutex gRecomputeFocusMutex;
        WTF::Vector<int> mRecomputeEvents;
        // These two fields go together: we use the mutex to protect access to 
        // mButtons, so that we, and webview.cpp can look/modify the mButtons 
        // field safely from our respective threads
        static Mutex gButtonMutex;
        SkTDArray<Container*>* mButtons;
        // end of shared members
    private:
        friend class ListBoxReply;
        struct FrameGen {
            const WebCore::Frame* mFrame;
            int mGeneration;
        };
        WTF::Vector<FrameGen> mFrameGenerations;
        static Mutex gFrameGenerationMutex;
        struct JavaGlue;
        struct JavaGlue*       mJavaGlue;
        WebCore::FrameView*    mView;
        WebCore::FrameAndroid* mFrame;
        WebCoreReply*          mPopupReply;
        WebCore::Node* mLastFocused;
        WebCore::IntRect mLastFocusedBounds;
        // Used in passToJS to avoid updating the UI text field until after the
        // key event has been processed.
        bool mBlockTextfieldUpdates;
        // Passed in with key events to know when they were generated.  Store it
        // with the cache so that we can ignore stale text changes.
        int mTextGeneration;
        CachedRoot* mTemp;
        SkPicture* mTempPict;
        int mBuildGeneration;
        int mMaxXScroll;
        int mMaxYScroll;
        WebCore::IntRect mVisibleRect;
        WebCore::IntPoint mMousePos;
        bool mFrameCacheOutOfDate;
        bool mBlockFocusChange;
        int mLastPassed;
        int mLastVelocity;
        CachedHistory mHistory;
        WebCore::Node* mSnapAnchorNode;
        WebCore::Frame* changedKitFocus(WebCore::Frame* frame,
            WebCore::Node* node, int x, int y);
        bool commonKitFocus(int generation, int buildGeneration, 
            WebCore::Frame* frame, WebCore::Node* node, int x, int y,
            bool ignoreNullFocus);
        bool finalKitFocus(WebCore::Frame* frame, WebCore::Node* node, int x, int y);
        void doMaxScroll(WebCore::CacheBuilder::Direction dir);
        void sendMarkNodeInvalid(WebCore::Node* );
        void sendNotifyFocusSet();
        void sendNotifyProgressFinished();
        void sendRecomputeFocus();
        void sendViewInvalidate();
        bool handleMouseClick(WebCore::Frame* framePtr, WebCore::Node* nodePtr);
        bool prepareFrameCache();
        void releaseFrameCache(bool newCache);
#if DEBUG_NAV_UI
        uint32_t mNow;
#endif
    };

}   // namespace android

#endif // ANDROID_WIDGET_HTMLWIDGET_H
