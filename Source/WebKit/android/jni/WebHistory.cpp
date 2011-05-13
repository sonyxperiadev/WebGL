/*
 * Copyright 2007, The Android Open Source Project
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "webhistory"

#include "config.h"
#include "WebHistory.h"

#include "BackForwardList.h"
#include "BackForwardListImpl.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClientAndroid.h"
#include "FrameTree.h"
#include "HistoryItem.h"
#include "IconDatabase.h"
#include "Page.h"
#include "TextEncoding.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreJni.h"
#include "WebIconDatabase.h"

#include <JNIHelp.h>
#include "JNIUtility.h"
#include <SkUtils.h>
#include <utils/misc.h>
#include <wtf/OwnPtr.h>
#include <wtf/Platform.h>
#include <wtf/text/CString.h>

namespace android {

// Forward declarations
static void write_item(WTF::Vector<char>& v, WebCore::HistoryItem* item);
static void write_children_recursive(WTF::Vector<char>& v, WebCore::HistoryItem* parent);
static bool read_item_recursive(WebCore::HistoryItem* child, const char** pData, int length);

// Field ids for WebHistoryItems
struct WebHistoryItemFields {
    jmethodID   mInit;
    jmethodID   mUpdate;
    jfieldID    mTitle;
    jfieldID    mUrl;
} gWebHistoryItem;

struct WebBackForwardListFields {
    jmethodID   mAddHistoryItem;
    jmethodID   mRemoveHistoryItem;
    jmethodID   mSetCurrentIndex;
} gWebBackForwardList;

//--------------------------------------------------------------------------
// WebBackForwardList native methods.
//--------------------------------------------------------------------------

static void WebHistoryClose(JNIEnv* env, jobject obj, jint frame)
{
    LOG_ASSERT(frame, "Close needs a valid Frame pointer!");
    WebCore::Frame* pFrame = (WebCore::Frame*)frame;

    WebCore::BackForwardListImpl* list = static_cast<WebCore::BackForwardListImpl*>(pFrame->page()->backForwardList());
    RefPtr<WebCore::HistoryItem> current = list->currentItem();
    // Remove each item instead of using close(). close() is intended to be used
    // right before the list is deleted.
    WebCore::HistoryItemVector& entries = list->entries();
    int size = entries.size();
    for (int i = size - 1; i >= 0; --i)
        list->removeItem(entries[i].get());
    // Add the current item back to the list.
    if (current) {
        current->setBridge(0);
        // addItem will update the children to match the newly created bridge
        list->addItem(current);

        /*
         * The Grand Prix site uses anchor navigations to change the display.
         * WebKit tries to be smart and not load child frames that have the
         * same history urls during an anchor navigation. This means that the
         * current history item stored in the child frame's loader does not
         * match the item found in the history tree. If we remove all the
         * entries in the back/foward list, we have to restore the entire tree
         * or else a HistoryItem might have a deleted parent.
         *
         * In order to restore the history tree correctly, we have to look up
         * all the frames first and then look up the history item. We do this
         * because the history item in the tree may be null at this point.
         * Unfortunately, a HistoryItem can only search its immediately
         * children so we do a breadth-first rebuild of the tree.
         */

        // Keep a small list of child frames to traverse.
        WTF::Vector<WebCore::Frame*> frameQueue;
        // Fix the top-level item.
        pFrame->loader()->history()->setCurrentItem(current.get());
        WebCore::Frame* child = pFrame->tree()->firstChild();
        // Remember the parent history item so we can search for a child item.
        RefPtr<WebCore::HistoryItem> parent = current;
        while (child) {
            // Use the old history item since the current one may have a
            // deleted parent.
            WebCore::HistoryItem* item = parent->childItemWithTarget(child->tree()->name());
            child->loader()->history()->setCurrentItem(item);
            // Append the first child to the queue if it exists. If there is no
            // item, then we do not need to traverse the children since there
            // will be no parent history item.
            WebCore::Frame* firstChild;
            if (item && (firstChild = child->tree()->firstChild()))
                frameQueue.append(firstChild);
            child = child->tree()->nextSibling();
            // If we don't have a sibling for this frame and the queue isn't
            // empty, use the next entry in the queue.
            if (!child && !frameQueue.isEmpty()) {
                child = frameQueue.at(0);
                frameQueue.remove(0);
                // Figure out the parent history item used when searching for
                // the history item to use.
                parent = child->tree()->parent()->loader()->history()->currentItem();
            }
        }
    }
}

static void WebHistoryRestoreIndex(JNIEnv* env, jobject obj, jint frame, jint index)
{
    LOG_ASSERT(frame, "RestoreState needs a valid Frame pointer!");
    WebCore::Frame* pFrame = (WebCore::Frame*)frame;
    WebCore::Page* page = pFrame->page();
    WebCore::HistoryItem* currentItem =
            static_cast<WebCore::BackForwardListImpl*>(page->backForwardList())->entries()[index].get();

    // load the current page with FrameLoadTypeIndexedBackForward so that it
    // will use cache when it is possible
    page->goToItem(currentItem, FrameLoadTypeIndexedBackForward);
}

static void WebHistoryInflate(JNIEnv* env, jobject obj, jint frame, jbyteArray data)
{
    LOG_ASSERT(frame, "Inflate needs a valid frame pointer!");
    LOG_ASSERT(data, "Inflate needs a valid data pointer!");

    // Get the actual bytes and the length from the java array.
    const jbyte* bytes = env->GetByteArrayElements(data, NULL);
    jsize size = env->GetArrayLength(data);

    // Inflate the history tree into one HistoryItem or null if the inflation
    // failed.
    RefPtr<WebCore::HistoryItem> newItem = WebCore::HistoryItem::create();
    WebHistoryItem* bridge = new WebHistoryItem(env, obj, newItem.get());
    newItem->setBridge(bridge);

    // Inflate the item recursively. If it fails, that is ok. We'll have an
    // incomplete HistoryItem but that is better than crashing due to a null
    // item.
    // We have a 2nd local variable since read_item_recursive may change the
    // ptr's value. We can't pass &bytes since we have to send bytes to
    // ReleaseByteArrayElements unchanged.
    const char* ptr = reinterpret_cast<const char*>(bytes);
    read_item_recursive(newItem.get(), &ptr, (int)size);
    env->ReleaseByteArrayElements(data, const_cast<jbyte*>(bytes), JNI_ABORT);
    bridge->setActive();

    // Add the new item to the back/forward list.
    WebCore::Frame* pFrame = (WebCore::Frame*)frame;
    pFrame->page()->backForwardList()->addItem(newItem);

    // Update the item.
    bridge->updateHistoryItem(newItem.get());
}

// 6 empty strings + no document state + children count + 2 scales = 10 unsigned values
// 1 char for isTargetItem.
#define HISTORY_MIN_SIZE ((int)(sizeof(unsigned) * 10 + sizeof(char)))

jbyteArray WebHistory::Flatten(JNIEnv* env, WTF::Vector<char>& v, WebCore::HistoryItem* item)
{
    if (!item)
        return NULL;

    // Reserve a vector of chars with an initial size of HISTORY_MIN_SIZE.
    v.reserveCapacity(HISTORY_MIN_SIZE);

    // Write the top-level history item and then write all the children
    // recursively.
    LOG_ASSERT(item->bridge(), "Why don't we have a bridge object here?");
    write_item(v, item);
    write_children_recursive(v, item);

    // Try to create a new java byte array.
    jbyteArray b = env->NewByteArray(v.size());
    if (!b)
        return NULL;

    // Write our flattened data to the java array.
    env->SetByteArrayRegion(b, 0, v.size(), (const jbyte*)v.data());
    return b;
}

WebHistoryItem::WebHistoryItem(JNIEnv* env, jobject obj,
        WebCore::HistoryItem* item) : WebCore::AndroidWebHistoryBridge(item) {
    m_object = env->NewWeakGlobalRef(obj);
    m_parent = 0;
}

WebHistoryItem::~WebHistoryItem() {
    if (m_object) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        if (!env)
            return;
        env->DeleteWeakGlobalRef(m_object);
    }
}

void WebHistoryItem::updateHistoryItem(WebCore::HistoryItem* item) {
    // Do not want to update during inflation.
    if (!m_active)
        return;
    WebHistoryItem* webItem = this;
    // Now we need to update the top-most WebHistoryItem based on the top-most
    // HistoryItem.
    if (m_parent) {
        webItem = m_parent.get();
        if (webItem->hasOneRef()) {
            // if the parent only has one ref, it is from this WebHistoryItem.
            // This means that the matching WebCore::HistoryItem has been freed.
            // This can happen during clear().
            LOGW("Can't updateHistoryItem as the top HistoryItem is gone");
            return;
        }
        while (webItem->parent())
            webItem = webItem->parent();
        item = webItem->historyItem();
        if (!item) {
            // If a HistoryItem only exists for page cache, it is possible that
            // the parent HistoryItem destroyed before the child HistoryItem. If
            // it happens, skip updating.
            LOGW("Can't updateHistoryItem as the top HistoryItem is gone");
            return;
        }
    }
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env)
        return;

    // Don't do anything if the item has been gc'd already
    AutoJObject realItem = getRealObject(env, webItem->m_object);
    if (!realItem.get())
        return;

    const WTF::String& urlString = item->urlString();
    jstring urlStr = NULL;
    if (!urlString.isNull())
        urlStr = wtfStringToJstring(env, urlString);
    const WTF::String& originalUrlString = item->originalURLString();
    jstring originalUrlStr = NULL;
    if (!originalUrlString.isNull())
        originalUrlStr = wtfStringToJstring(env, originalUrlString);
    const WTF::String& titleString = item->title();
    jstring titleStr = NULL;
    if (!titleString.isNull())
        titleStr = wtfStringToJstring(env, titleString);

    // Try to get the favicon from the history item. For some pages like Grand
    // Prix, there are history items with anchors. If the icon fails for the
    // item, try to get the icon using the url without the ref.
    jobject favicon = NULL;
    WTF::String url = item->urlString();
    if (item->url().hasFragmentIdentifier()) {
        int refIndex = url.reverseFind('#');
        url = url.substring(0, refIndex);
    }
    WebCore::Image* icon = WebCore::iconDatabase()->iconForPageURL(url,
            WebCore::IntSize(16, 16));

    if (icon)
        favicon = webcoreImageToJavaBitmap(env, icon);

    WTF::Vector<char> data;
    jbyteArray array = WebHistory::Flatten(env, data, item);
    env->CallVoidMethod(realItem.get(), gWebHistoryItem.mUpdate, urlStr,
            originalUrlStr, titleStr, favicon, array);
    env->DeleteLocalRef(urlStr);
    env->DeleteLocalRef(originalUrlStr);
    env->DeleteLocalRef(titleStr);
    if (favicon)
        env->DeleteLocalRef(favicon);
    env->DeleteLocalRef(array);
}

static void historyItemChanged(WebCore::HistoryItem* item) {
    LOG_ASSERT(item, "historyItemChanged called with a null item");

    if (item->bridge())
        item->bridge()->updateHistoryItem(item);
}

void WebHistory::AddItem(const AutoJObject& list, WebCore::HistoryItem* item)
{
    LOG_ASSERT(item, "newItem must take a valid HistoryItem!");
    // Item already added. Should only happen when we are inflating the list.
    if (item->bridge() || !list.get())
        return;

    JNIEnv* env = list.env();
    // Allocate a blank WebHistoryItem
    jclass clazz = env->FindClass("android/webkit/WebHistoryItem");
    jobject newItem = env->NewObject(clazz, gWebHistoryItem.mInit);
    env->DeleteLocalRef(clazz);

    // Create the bridge, make it active, and attach it to the item.
    WebHistoryItem* bridge = new WebHistoryItem(env, newItem, item);
    bridge->setActive();
    item->setBridge(bridge);

    // Update the history item which will flatten the data and call update on
    // the java item.
    bridge->updateHistoryItem(item);

    // Add it to the list.
    env->CallVoidMethod(list.get(), gWebBackForwardList.mAddHistoryItem, newItem);

    // Delete our local reference.
    env->DeleteLocalRef(newItem);
}

void WebHistory::RemoveItem(const AutoJObject& list, int index)
{
    if (list.get())
        list.env()->CallVoidMethod(list.get(), gWebBackForwardList.mRemoveHistoryItem, index);
}

void WebHistory::UpdateHistoryIndex(const AutoJObject& list, int newIndex)
{
    if (list.get())
        list.env()->CallVoidMethod(list.get(), gWebBackForwardList.mSetCurrentIndex, newIndex);
}

static void write_string(WTF::Vector<char>& v, const WTF::String& str)
{
    unsigned strLen = str.length();
    // Only do work if the string has data.
    if (strLen) {
        // Determine how much to grow the vector. Use the worst case for utf8 to
        // avoid reading the string twice. Add sizeof(unsigned) to hold the
        // string length in utf8.
        unsigned vectorLen = v.size() + sizeof(unsigned);
        unsigned length = (strLen << 2) + vectorLen;
        // Grow the vector. This will change the value of v.size() but we
        // remember the original size above.
        v.grow(length);
        // Grab the position to write to.
        char* data = v.begin() + vectorLen;
        // Write the actual string
        int l = SkUTF16_ToUTF8(str.characters(), strLen, data);
        LOGV("Writing string       %d %.*s", l, l, data);
        // Go back and write the utf8 length. Subtract sizeof(unsigned) from
        // data to get the position to write the length.
        memcpy(data - sizeof(unsigned), (char*)&l, sizeof(unsigned));
        // Shrink the internal state of the vector so we match what was
        // actually written.
        v.shrink(vectorLen + l);
    } else
        v.append((char*)&strLen, sizeof(unsigned));
}

static void write_item(WTF::Vector<char>& v, WebCore::HistoryItem* item)
{
    // Original url
    write_string(v, item->originalURLString());

    // Url
    write_string(v, item->urlString());

    // Title
    write_string(v, item->title());

    // Form content type
    write_string(v, item->formContentType());

    // Form data
    const WebCore::FormData* formData = item->formData();
    if (formData) {
        write_string(v, formData->flattenToString());
        // save the identifier as it is not included in the flatten data
        int64_t id = formData->identifier();
        v.append((char*)&id, sizeof(int64_t));
    } else
        write_string(v, WTF::String()); // Empty constructor does not allocate a buffer.

    // Target
    write_string(v, item->target());

    AndroidWebHistoryBridge* bridge = item->bridge();
    LOG_ASSERT(bridge, "We should have a bridge here!");
    // Screen scale
    const float scale = bridge->scale();
    LOGV("Writing scale %f", scale);
    v.append((char*)&scale, sizeof(float));
    const float textWrapScale = bridge->textWrapScale();
    LOGV("Writing text wrap scale %f", textWrapScale);
    v.append((char*)&textWrapScale, sizeof(float));

    // Scroll position.
    const int scrollX = item->scrollPoint().x();
    v.append((char*)&scrollX, sizeof(int));
    const int scrollY = item->scrollPoint().y();
    v.append((char*)&scrollY, sizeof(int));

    // Document state
    const WTF::Vector<WTF::String>& docState = item->documentState();
    WTF::Vector<WTF::String>::const_iterator end = docState.end();
    unsigned stateSize = docState.size();
    LOGV("Writing docState     %d", stateSize);
    v.append((char*)&stateSize, sizeof(unsigned));
    for (WTF::Vector<WTF::String>::const_iterator i = docState.begin(); i != end; ++i) {
        write_string(v, *i);
    }

    // Is target item
    LOGV("Writing isTargetItem %d", item->isTargetItem());
    v.append((char)item->isTargetItem());

    // Children count
    unsigned childCount = item->children().size();
    LOGV("Writing childCount   %d", childCount);
    v.append((char*)&childCount, sizeof(unsigned));
}

static void write_children_recursive(WTF::Vector<char>& v, WebCore::HistoryItem* parent)
{
    const WebCore::HistoryItemVector& children = parent->children();
    WebCore::HistoryItemVector::const_iterator end = children.end();
    for (WebCore::HistoryItemVector::const_iterator i = children.begin(); i != end; ++i) {
        WebCore::HistoryItem* item = (*i).get();
        LOG_ASSERT(parent->bridge(),
                "The parent item should have a bridge object!");
        if (!item->bridge()) {
            WebHistoryItem* bridge = new WebHistoryItem(static_cast<WebHistoryItem*>(parent->bridge()));
            item->setBridge(bridge);
            bridge->setActive();
        } else {
            // The only time this item's parent may not be the same as the
            // parent's bridge is during history close. In that case, the
            // parent must not have a parent bridge.
            WebHistoryItem* bridge = static_cast<WebHistoryItem*>(item->bridge());
            WebHistoryItem* parentBridge = static_cast<WebHistoryItem*>(parent->bridge());
            LOG_ASSERT(parentBridge->parent() == 0 ||
                    bridge->parent() == parentBridge,
                    "Somehow this item has an incorrect parent");
            bridge->setParent(parentBridge);
        }
        write_item(v, item);
        write_children_recursive(v, item);
    }
}

static bool read_item_recursive(WebCore::HistoryItem* newItem,
        const char** pData, int length)
{
    if (!pData || length < HISTORY_MIN_SIZE)
        return false;

    const WebCore::TextEncoding& e = WebCore::UTF8Encoding();
    const char* data = *pData;
    const char* end = data + length;
    int sizeofUnsigned = (int)sizeof(unsigned);

    // Read the original url
    // Read the expected length of the string.
    int l;
    memcpy(&l, data, sizeofUnsigned);
    // Increment data pointer by the size of an unsigned int.
    data += sizeofUnsigned;
    if (l) {
        LOGV("Original url    %d %.*s", l, l, data);
        // If we have a length, check if that length exceeds the data length
        // and return null if there is not enough data.
        if (data + l < end)
            newItem->setOriginalURLString(e.decode(data, l));
        else
            return false;
        // Increment the data pointer by the length of the string.
        data += l;
    }
    // Check if we have enough data left to continue.
    if (end - data < sizeofUnsigned)
        return false;

    // Read the url
    memcpy(&l, data, sizeofUnsigned);
    data += sizeofUnsigned;
    if (l) {
        LOGV("Url             %d %.*s", l, l, data);
        if (data + l < end)
            newItem->setURLString(e.decode(data, l));
        else
            return false;
        data += l;
    }
    if (end - data < sizeofUnsigned)
        return false;

    // Read the title
    memcpy(&l, data, sizeofUnsigned);
    data += sizeofUnsigned;
    if (l) {
        LOGV("Title           %d %.*s", l, l, data);
        if (data + l < end)
            newItem->setTitle(e.decode(data, l));
        else
            return false;
        data += l;
    }
    if (end - data < sizeofUnsigned)
        return false;

    // Generate a new ResourceRequest object for populating form information.
    WTF::String formContentType;
    WTF::PassRefPtr<WebCore::FormData> formData = NULL;

    // Read the form content type
    memcpy(&l, data, sizeofUnsigned);
    data += sizeofUnsigned;
    if (l) {
        LOGV("Content type    %d %.*s", l, l, data);
        if (data + l < end)
            formContentType = e.decode(data, l);
        else
            return false;
        data += l;
    }
    if (end - data < sizeofUnsigned)
        return false;

    // Read the form data
    memcpy(&l, data, sizeofUnsigned);
    data += sizeofUnsigned;
    if (l) {
        LOGV("Form data       %d %.*s", l, l, data);
        if (data + l < end)
            formData = WebCore::FormData::create(data, l);
        else
            return false;
        data += l;
        // Read the identifier
        {
            int64_t id;
            int size = (int)sizeof(int64_t);
            memcpy(&id, data, size);
            data += size;
            if (id)
                formData->setIdentifier(id);
        }
    }
    if (end - data < sizeofUnsigned)
        return false;

    // Set up the form info
    if (formData != NULL) {
        WebCore::ResourceRequest r;
        r.setHTTPMethod("POST");
        r.setHTTPContentType(formContentType);
        r.setHTTPBody(formData);
        newItem->setFormInfoFromRequest(r);
    }

    // Read the target
    memcpy(&l, data, sizeofUnsigned);
    data += sizeofUnsigned;
    if (l) {
        LOGV("Target          %d %.*s", l, l, data);
        if (data + l < end)
            newItem->setTarget(e.decode(data, l));
        else
            return false;
        data += l;
    }
    if (end - data < sizeofUnsigned)
        return false;

    AndroidWebHistoryBridge* bridge = newItem->bridge();
    LOG_ASSERT(bridge, "There should be a bridge object during inflate");
    float fValue;
    // Read the screen scale
    memcpy(&fValue, data, sizeof(float));
    LOGV("Screen scale    %f", fValue);
    bridge->setScale(fValue);
    data += sizeof(float);
    memcpy(&fValue, data, sizeofUnsigned);
    LOGV("Text wrap scale    %f", fValue);
    bridge->setTextWrapScale(fValue);
    data += sizeof(float);

    if (end - data < sizeofUnsigned)
        return false;

    // Read scroll position.
    int scrollX = 0;
    memcpy(&scrollX, data, sizeofUnsigned);
    data += sizeofUnsigned;
    int scrollY = 0;
    memcpy(&scrollY, data, sizeofUnsigned);
    data += sizeofUnsigned;
    newItem->setScrollPoint(IntPoint(scrollX, scrollY));

    if (end - data < sizeofUnsigned)
        return false;

    // Read the document state
    memcpy(&l, data, sizeofUnsigned);
    LOGV("Document state  %d", l);
    data += sizeofUnsigned;
    if (l) {
        // Check if we have enough data to at least parse the sizes of each
        // document state string.
        if (data + l * sizeofUnsigned >= end)
            return false;
        // Create a new vector and reserve enough space for the document state.
        WTF::Vector<WTF::String> docState;
        docState.reserveCapacity(l);
        while (l--) {
            // Check each time if we have enough to parse the length of the next
            // string.
            if (end - data < sizeofUnsigned)
                return false;
            int strLen;
            memcpy(&strLen, data, sizeofUnsigned);
            data += sizeofUnsigned;
            if (data + strLen < end)
                docState.append(e.decode(data, strLen));
            else
                return false;
            LOGV("\t\t%d %.*s", strLen, strLen, data);
            data += strLen;
        }
        newItem->setDocumentState(docState);
    }
    // Check if we have enough to read the next byte
    if (data >= end)
        return false;

    // Read is target item
    // Cast the value to unsigned char in order to make a negative value larger
    // than 1. A value that is not 0 or 1 is a failure.
    unsigned char c = (unsigned char)data[0];
    if (c > 1)
        return false;
    LOGV("Target item     %d", c);
    newItem->setIsTargetItem((bool)c);
    data++;
    if (end - data < sizeofUnsigned)
        return false;

    // Read the child count
    memcpy(&l, data, sizeofUnsigned);
    LOGV("Child count     %d", l);
    data += sizeofUnsigned;
    *pData = data;
    if (l) {
        // Check if we have the minimum amount need to parse l children.
        if (data + l * HISTORY_MIN_SIZE >= end)
            return false;
        while (l--) {
            // No need to check the length each time because read_item_recursive
            // will return null if there isn't enough data left to parse.
            WTF::PassRefPtr<WebCore::HistoryItem> child = WebCore::HistoryItem::create();
            // Set a bridge that will not call into java.
            child->setBridge(new WebHistoryItem(static_cast<WebHistoryItem*>(bridge)));
            // Read the child item.
            if (!read_item_recursive(child.get(), pData, end - data)) {
                child.clear();
                return false;
            }
            child->bridge()->setActive();
            newItem->addChildItem(child);
        }
    }
    return true;
}

// On arm, this test will cause memory corruption since converting char* will
// byte align the result and this test does not use memset (it probably
// should).
// On the simulator, using HistoryItem will invoke the IconDatabase which will
// initialize the main thread. Since this is invoked by the Zygote process, the
// main thread will be incorrect and an assert will fire later.
// In conclusion, define UNIT_TEST only if you know what you are doing.
#ifdef UNIT_TEST
static void unit_test()
{
    LOGD("Entering history unit test!");
    const char* test1 = new char[0];
    WTF::RefPtr<WebCore::HistoryItem> item = WebCore::HistoryItem::create();
    WebCore::HistoryItem* testItem = item.get();
    testItem->setBridge(new WebHistoryItem(0));
    LOG_ASSERT(!read_item_recursive(testItem, &test1, 0), "0 length array should fail!");
    delete[] test1;
    const char* test2 = new char[2];
    LOG_ASSERT(!read_item_recursive(testItem, &test2, 2), "Small array should fail!");
    delete[] test2;
    LOG_ASSERT(!read_item_recursive(testItem, NULL, HISTORY_MIN_SIZE), "Null data should fail!");
    // Original Url
    char* test3 = new char[HISTORY_MIN_SIZE];
    const char* ptr = (const char*)test3;
    memset(test3, 0, HISTORY_MIN_SIZE);
    *(int*)test3 = 4000;
    LOG_ASSERT(!read_item_recursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 length originalUrl should fail!");
    // Url
    int offset = 4;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 4000;
    LOG_ASSERT(!read_item_recursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 length url should fail!");
    // Title
    offset += 4;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 4000;
    LOG_ASSERT(!read_item_recursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 length title should fail!");
    // Form content type
    offset += 4;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 4000;
    LOG_ASSERT(!read_item_recursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 length contentType should fail!");
    // Form data
    offset += 4;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 4000;
    LOG_ASSERT(!read_item_recursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 length form data should fail!");
    // Target
    offset += 4;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 4000;
    LOG_ASSERT(!read_item_recursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 length target should fail!");
    offset += 4; // Scale
    // Document state 
    offset += 4;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 4000;
    LOG_ASSERT(!read_item_recursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 length document state should fail!");
    // Is target item
    offset += 1;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(char*)(test3 + offset) = '!';
    LOG_ASSERT(!read_item_recursive(testItem, &ptr, HISTORY_MIN_SIZE), "IsTargetItem should fail with ! as the value!");
    // Child count
    offset += 4;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 4000;
    LOG_ASSERT(!read_item_recursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 kids should fail!");
    offset = 36;
    // Test document state
    delete[] test3;
    test3 = new char[HISTORY_MIN_SIZE + sizeof(unsigned)];
    memset(test3, 0, HISTORY_MIN_SIZE + sizeof(unsigned));
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 1;
    *(int*)(test3 + offset + 4) = 20;
    LOG_ASSERT(!read_item_recursive(testItem, &ptr, HISTORY_MIN_SIZE + sizeof(unsigned)), "1 20 length document state string should fail!");
    delete[] test3;
    test3 = new char[HISTORY_MIN_SIZE + 2 * sizeof(unsigned)];
    memset(test3, 0, HISTORY_MIN_SIZE + 2 * sizeof(unsigned));
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 2;
    *(int*)(test3 + offset + 4) = 0;
    *(int*)(test3 + offset + 8) = 20;
    LOG_ASSERT(!read_item_recursive(testItem, &ptr, HISTORY_MIN_SIZE + 2 * sizeof(unsigned) ), "2 20 length document state string should fail!");
    delete[] test3;
}
#endif

//---------------------------------------------------------
// JNI registration
//---------------------------------------------------------
static JNINativeMethod gWebBackForwardListMethods[] = {
    { "nativeClose", "(I)V",
        (void*) WebHistoryClose },
    { "restoreIndex", "(II)V",
        (void*) WebHistoryRestoreIndex }
};

static JNINativeMethod gWebHistoryItemMethods[] = {
    { "inflate", "(I[B)V",
        (void*) WebHistoryInflate }
};

int registerWebHistory(JNIEnv* env)
{
    // Get notified of all changes to history items.
    WebCore::notifyHistoryItemChanged = historyItemChanged;
#ifdef UNIT_TEST
    unit_test();
#endif
    // Find WebHistoryItem, its constructor, and the update method.
    jclass clazz = env->FindClass("android/webkit/WebHistoryItem");
    LOG_ASSERT(clazz, "Unable to find class android/webkit/WebHistoryItem");
    gWebHistoryItem.mInit = env->GetMethodID(clazz, "<init>", "()V");
    LOG_ASSERT(gWebHistoryItem.mInit, "Could not find WebHistoryItem constructor");
    gWebHistoryItem.mUpdate = env->GetMethodID(clazz, "update",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Landroid/graphics/Bitmap;[B)V");
    LOG_ASSERT(gWebHistoryItem.mUpdate, "Could not find method update in WebHistoryItem");

    // Find the field ids for mTitle and mUrl.
    gWebHistoryItem.mTitle = env->GetFieldID(clazz, "mTitle", "Ljava/lang/String;");
    LOG_ASSERT(gWebHistoryItem.mTitle, "Could not find field mTitle in WebHistoryItem");
    gWebHistoryItem.mUrl = env->GetFieldID(clazz, "mUrl", "Ljava/lang/String;");
    LOG_ASSERT(gWebHistoryItem.mUrl, "Could not find field mUrl in WebHistoryItem");
    env->DeleteLocalRef(clazz);

    // Find the WebBackForwardList object and method.
    clazz = env->FindClass("android/webkit/WebBackForwardList");
    LOG_ASSERT(clazz, "Unable to find class android/webkit/WebBackForwardList");
    gWebBackForwardList.mAddHistoryItem = env->GetMethodID(clazz, "addHistoryItem",
            "(Landroid/webkit/WebHistoryItem;)V");
    LOG_ASSERT(gWebBackForwardList.mAddHistoryItem, "Could not find method addHistoryItem");
    gWebBackForwardList.mRemoveHistoryItem = env->GetMethodID(clazz, "removeHistoryItem",
            "(I)V");
    LOG_ASSERT(gWebBackForwardList.mRemoveHistoryItem, "Could not find method removeHistoryItem");
    gWebBackForwardList.mSetCurrentIndex = env->GetMethodID(clazz, "setCurrentIndex", "(I)V");
    LOG_ASSERT(gWebBackForwardList.mSetCurrentIndex, "Could not find method setCurrentIndex");
    env->DeleteLocalRef(clazz);

    int result = jniRegisterNativeMethods(env, "android/webkit/WebBackForwardList",
            gWebBackForwardListMethods, NELEM(gWebBackForwardListMethods));
    return (result < 0) ? result : jniRegisterNativeMethods(env, "android/webkit/WebHistoryItem",
            gWebHistoryItemMethods, NELEM(gWebHistoryItemMethods));
}

} /* namespace android */
