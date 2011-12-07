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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "CachedPrefix.h"
#include "CachedNode.h"
#include "CachedRoot.h"
#include "ColumnInfo.h"
#include "Document.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClientAndroid.h"
#include "FrameTree.h"
#include "FrameView.h"
//#include "GraphicsContext.h"
#include "HTMLAreaElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "InlineTextBox.h"
#include "KURL.h"
#include "LayerAndroid.h"
#include "PluginView.h"
#include "RegisteredEventListener.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderLayerBacking.h"
#include "RenderListBox.h"
#include "RenderSkinCombo.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SkCanvas.h"
#include "SkPoint.h"
#include "Text.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreViewBridge.h"
#include "Widget.h"
#include <wtf/unicode/Unicode.h>

#ifdef DUMP_NAV_CACHE_USING_PRINTF
    FILE* gNavCacheLogFile = NULL;
    android::Mutex gWriteLogMutex;
#endif

#include "CacheBuilder.h"

#define MINIMUM_FOCUSABLE_WIDTH 3
#define MINIMUM_FOCUSABLE_HEIGHT 3
#define MAXIMUM_FOCUS_RING_COUNT 32

namespace android {

CacheBuilder* CacheBuilder::Builder(Frame* frame) { 
    return &((FrameLoaderClientAndroid*) frame->loader()->client())->getCacheBuilder(); 
}

Frame* CacheBuilder::FrameAnd(CacheBuilder* cacheBuilder) { 
    FrameLoaderClientAndroid* loader = (FrameLoaderClientAndroid*)
        ((char*) cacheBuilder - OFFSETOF(FrameLoaderClientAndroid, m_cacheBuilder));
    return loader->getFrame();
}

Frame* CacheBuilder::FrameAnd(const CacheBuilder* cacheBuilder) { 
    FrameLoaderClientAndroid* loader = (FrameLoaderClientAndroid*)
        ((char*) cacheBuilder - OFFSETOF(FrameLoaderClientAndroid, m_cacheBuilder)); 
    return loader->getFrame();
}

CacheBuilder::LayerTracker::~LayerTracker() {
    // Check for a stacking context to prevent a crash in layers without a
    // parent.
    if (mRenderLayer && mRenderLayer->stackingContext())
        // Restore the scroll position of the layer.  Does not affect layers
        // without overflow scroll as the layer will not be scrolled.
        mRenderLayer->scrollToOffset(mScroll.x(), mScroll.y());
}

#if DUMP_NAV_CACHE

static bool hasEventListener(Node* node, const AtomicString& eventType) {
    if (!node->isElementNode())
        return false;
    Element* element = static_cast<Element*>(node);
    EventListener* listener = element->getAttributeEventListener(eventType);
    return 0 != listener;
}

#define DEBUG_BUFFER_SIZE 256
#define DEBUG_WRAP_SIZE 150
#define DEBUG_WRAP_MAX 170

Frame* CacheBuilder::Debug::frameAnd() const {
    CacheBuilder* nav = (CacheBuilder*) ((char*) this - OFFSETOF(CacheBuilder, mDebug));
    return CacheBuilder::FrameAnd(nav); 
}

void CacheBuilder::Debug::attr(const AtomicString& name, const AtomicString& value) {
    if (name.isNull() || name.isEmpty() || value.isNull() || value.isEmpty())
        return;
    uChar(name.characters(), name.length(), false);
    print("=");
    wideString(value.characters(), value.length(), false);
    print(" ");
}

void CacheBuilder::Debug::comma(const char* str) {
    print(str);
    print(", ");
}

void CacheBuilder::Debug::flush() {
    int len;
    do {
        int limit = mIndex;
        if (limit < DEBUG_WRAP_SIZE)
            return;
        if (limit < DEBUG_WRAP_MAX)
            len = limit;
        else {
            limit = DEBUG_WRAP_MAX;
            len = DEBUG_WRAP_SIZE;
            while (len < limit) {
                char test = mBuffer[len];
                if (test < '/' || (test > '9' && test < 'A') || (test > 'Z' && test  < 'a') || test > 'z')
                    break;
                len++;
            }
            while (len > 0 && mBuffer[len - 1] == '\\')
                len--;
            while (mBuffer[len] == '"')
                len++;
        }
        const char* prefix = mPrefix;
        if (prefix[0] == '\"') {
            // see if we're inside a quote
            int quoteCount = 0;
            for (int index = 0; index < len; index++) {
                if (mBuffer[index] == '\\') {
                    index++;
                    continue;
                }
                if (mBuffer[index] == '\n') {
                    quoteCount = 0;
                    continue;
                }
                if (mBuffer[index] == '"')
                    quoteCount++;
            }
            if ((quoteCount & 1) == 0)
                prefix = "\n";
        }
        DUMP_NAV_LOGD("%.*s", len, mBuffer);
        int copy = mIndex - len;
        strcpy(mBuffer, prefix);
        memcpy(&mBuffer[strlen(prefix)], &mBuffer[len], copy);
        mIndex = strlen(prefix) + copy;
    } while (true);
}

void CacheBuilder::Debug::frameName(char*& namePtr, const char* max) const {
   if (namePtr >= max)
        return;
   Frame* frame = frameAnd();
   Frame* parent = frame->tree()->parent();
   if (parent)
        Builder(parent)->mDebug.frameName(namePtr, max);
    const AtomicString& name = frame->tree()->name();
    if (name.length() == 0)
        return;
    unsigned index = 0;
    if (name.startsWith(AtomicString("opener")))
        index = 6;
    for (; index < name.length(); index++) {
        char ch = name[index];
        if (ch <= ' ')
            ch = '_';
        if (WTF::isASCIIAlphanumeric(ch) || ch == '_')
            *namePtr++  = ch;
    }
}
    
void CacheBuilder::Debug::frames() {
    Frame* frame = frameAnd();
    Document* doc = frame->document();
    if (doc == NULL)
        return;
    bool top = frame->tree()->parent() == NULL;
    if (top) {
#ifdef DUMP_NAV_CACHE_USING_PRINTF
        gWriteLogMutex.lock();
        ASSERT(gNavCacheLogFile == NULL);
        gNavCacheLogFile = fopen(NAV_CACHE_LOG_FILE, "a");
#endif
        groups();
    }
    Frame* child = frame->tree()->firstChild();
    bool hasChild = child != NULL;   
    if (top && hasChild)
        DUMP_NAV_LOGD("\nnamespace TEST_SPACE {\n\n");
    while (child) {
        Builder(child)->mDebug.frames();
        child = child->tree()->nextSibling();
    }
    if (hasChild) {
        child = frame->tree()->firstChild();
        while (child) {
            char childName[256];
            char* childNamePtr = childName;
            Builder(child)->mDebug.frameName(childNamePtr, childNamePtr + sizeof(childName) - 1);
            *childNamePtr = '\0';
            if (child == frame->tree()->firstChild())
                DUMP_NAV_LOGD("DebugTestFrameGroup TEST%s_GROUP[] = {\n", childName);
            Frame* next = child->tree()->nextSibling();
            Document* doc = child->document();
            if (doc != NULL) {        
                RenderObject* renderer = doc->renderer();
                if (renderer != NULL) {
                    RenderLayer* layer = renderer->enclosingLayer();
                    if (layer != NULL) {
                        DUMP_NAV_LOGD("{ ");
                        DUMP_NAV_LOGD("TEST%s_RECTS, ", childName);
                        DUMP_NAV_LOGD("TEST%s_RECT_COUNT, ", childName);
                        DUMP_NAV_LOGD("TEST%s_RECTPARTS, ", childName);
                        DUMP_NAV_LOGD("TEST%s_BOUNDS,\n", childName); 
                        DUMP_NAV_LOGD("TEST%s_WIDTH, ", childName);
                        DUMP_NAV_LOGD("TEST%s_HEIGHT,\n", childName);
                        DUMP_NAV_LOGD("0, 0, 0, 0,\n");
                        DUMP_NAV_LOGD("TEST%s_FOCUS, ", childName);
                        Frame* grandChild = child->tree()->firstChild();
                         if (grandChild) {
                            char grandChildName[256];
                            char* grandChildNamePtr = grandChildName;
                            Builder(grandChild)->mDebug.frameName(grandChildNamePtr, 
                                grandChildNamePtr + sizeof(grandChildName) - 1);
                            *grandChildNamePtr = '\0';
                            DUMP_NAV_LOGD("TEST%s_GROUP, ", grandChildName);
                            DUMP_NAV_LOGD("sizeof(TEST%s_GROUP) / sizeof(DebugTestFrameGroup), ", grandChildName);
                        } else
                            DUMP_NAV_LOGD("NULL, 0, ");
                        DUMP_NAV_LOGD("\"%s\"\n", childName);
                        DUMP_NAV_LOGD("}%c\n", next ? ',' : ' ');
                    }
                }
            }
            child = next;
        }
        DUMP_NAV_LOGD("};\n");
    }
    if (top) {
        if (hasChild)
            DUMP_NAV_LOGD("\n}  // end of namespace\n\n");
        char name[256];
        char* frameNamePtr = name;
        frameName(frameNamePtr, frameNamePtr + sizeof(name) - 1);
        *frameNamePtr = '\0';
        DUMP_NAV_LOGD("DebugTestFrameGroup TEST%s_GROUP = {\n", name);
        DUMP_NAV_LOGD("TEST%s_RECTS, ", name);
        DUMP_NAV_LOGD("TEST%s_RECT_COUNT, ", name);
        DUMP_NAV_LOGD("TEST%s_RECTPARTS, ", name);
        DUMP_NAV_LOGD("TEST%s_BOUNDS,\n", name); 
        DUMP_NAV_LOGD("TEST%s_WIDTH, ", name);
        DUMP_NAV_LOGD("TEST%s_HEIGHT,\n", name);
        DUMP_NAV_LOGD("TEST%s_MAX_H, ", name);
        DUMP_NAV_LOGD("TEST%s_MIN_H, ", name);
        DUMP_NAV_LOGD("TEST%s_MAX_V, ", name);
        DUMP_NAV_LOGD("TEST%s_MIN_V,\n", name);
        DUMP_NAV_LOGD("TEST%s_FOCUS, ", name);
        if (hasChild) {
            child = frame->tree()->firstChild();
            char childName[256];
            char* childNamePtr = childName;
            Builder(child)->mDebug.frameName(childNamePtr, childNamePtr + sizeof(childName) - 1);
            *childNamePtr = '\0';
            DUMP_NAV_LOGD("TEST_SPACE::TEST%s_GROUP, ", childName);
            DUMP_NAV_LOGD("sizeof(TEST_SPACE::TEST%s_GROUP) / sizeof(DebugTestFrameGroup), \n" ,childName);
        } else
            DUMP_NAV_LOGD("NULL, 0, ");
        DUMP_NAV_LOGD("\"%s\"\n", name);
        DUMP_NAV_LOGD("};\n");
#ifdef DUMP_NAV_CACHE_USING_PRINTF
        if (gNavCacheLogFile)
            fclose(gNavCacheLogFile);
        gNavCacheLogFile = NULL;
        gWriteLogMutex.unlock();
#endif
    }
}

void CacheBuilder::Debug::init(char* buffer, size_t size) {
    mBuffer = buffer;
    mBufferSize = size;
    mIndex = 0;
    mPrefix = "";
}

void CacheBuilder::Debug::groups() {
    Frame* frame = frameAnd();
    Frame* child = frame->tree()->firstChild();
    bool hasChild = child != NULL;
    if (frame->tree()->parent() == NULL && hasChild)
        DUMP_NAV_LOGD("namespace TEST_SPACE {\n\n");
    while (child) {
        Builder(child)->mDebug.groups();
        child = child->tree()->nextSibling();
    }
    if (frame->tree()->parent() == NULL && hasChild)
        DUMP_NAV_LOGD("\n} // end of namespace\n\n"); 
    Document* doc = frame->document();
    char name[256];
    char* frameNamePtr = name;
    frameName(frameNamePtr, frameNamePtr + sizeof(name) - 1);
    *frameNamePtr = '\0';
    if (doc == NULL) {
        DUMP_NAV_LOGD("// %s has no document\n", name);
        return;
    }
    RenderObject* renderer = doc->renderer();
    if (renderer == NULL) {
        DUMP_NAV_LOGD("// %s has no renderer\n", name);
        return;
    }
    RenderLayer* layer = renderer->enclosingLayer();
    if (layer == NULL) {
        DUMP_NAV_LOGD("// %s has no enclosingLayer\n", name);
        return;
    }
    Node* node = doc;
    Node* focus = doc->focusedNode();
    bool atLeastOne = false;
    do {
        if ((atLeastOne |= isFocusable(node)) != false)
            break;
    } while ((node = node->traverseNextNode()) != NULL);
    int focusIndex = -1;
    if (atLeastOne == false) {
        DUMP_NAV_LOGD("static DebugTestNode TEST%s_RECTS[] = {\n"
            "{{0, 0, 0, 0}, \"\", 0, -1, \"\", {0, 0, 0, 0}, false, 0}\n"
            "};\n\n", name);
        DUMP_NAV_LOGD("static int TEST%s_RECT_COUNT = 1;"
            " // no focusable nodes\n", name);
        DUMP_NAV_LOGD("#define TEST%s_RECTPARTS NULL\n", name);
    } else {
        node = doc;
        int count = 1;
        DUMP_NAV_LOGD("static DebugTestNode TEST%s_RECTS[] = {\n", name);
        do {
            String properties;
            if (hasEventListener(node, eventNames().clickEvent))
                properties.append("ONCLICK | ");
            if (hasEventListener(node, eventNames().mousedownEvent))
                properties.append("MOUSEDOWN | ");
            if (hasEventListener(node, eventNames().mouseupEvent))
                properties.append("MOUSEUP | ");
            if (hasEventListener(node, eventNames().mouseoverEvent))
                properties.append("MOUSEOVER | ");
            if (hasEventListener(node, eventNames().mouseoutEvent))
                properties.append("MOUSEOUT | ");
            if (hasEventListener(node, eventNames().keydownEvent))
                properties.append("KEYDOWN | ");
            if (hasEventListener(node, eventNames().keyupEvent))
                properties.append("KEYUP | ");
            if (CacheBuilder::HasFrame(node))
                properties.append("FRAME | ");
            if (focus == node) {
                properties.append("FOCUS | ");
                focusIndex = count;
            }
            if (node->isKeyboardFocusable(NULL))
                properties.append("KEYBOARD_FOCUSABLE | ");
            if (node->isMouseFocusable())
                properties.append("MOUSE_FOCUSABLE | ");
            if (node->isFocusable())
                properties.append("SIMPLE_FOCUSABLE | ");
            if (properties.isEmpty())
                properties.append("0");
            else
                properties.truncate(properties.length() - 3);
            IntRect rect = node->getRect();
            if (node->hasTagName(HTMLNames::areaTag))
                rect = getAreaRect(static_cast<HTMLAreaElement*>(node));
            char buffer[DEBUG_BUFFER_SIZE];
            memset(buffer, 0, sizeof(buffer));
            mBuffer = buffer;
            mBufferSize = sizeof(buffer);
            mPrefix = "\"\n\"";
            mIndex = snprintf(buffer, sizeof(buffer), "{{%d, %d, %d, %d}, ", rect.x(), rect.y(), 
                rect.width(), rect.height());
            localName(node);
            uChar(properties.characters(), properties.length(), false);
            print(", ");
            int parentIndex = ParentIndex(node, count, node->parentNode());
            char scratch[256];
            snprintf(scratch, sizeof(scratch), "%d", parentIndex);
            comma(scratch);
            Element* element = static_cast<Element*>(node);
            if (node->isElementNode() && element->hasID())
                wideString(element->getIdAttribute());
            else if (node->isTextNode()) {
 #if 01 // set to one to abbreviate text that can be omitted from the address detection code
               if (rect.isEmpty() && node->textContent().length() > 100) {
                    wideString(node->textContent().characters(), 100, false);
                    snprintf(scratch, sizeof(scratch), "/* + %d bytes */", 
                        node->textContent().length() - 100);
                    print(scratch);
                } else
 #endif
                   wideString(node->textContent().characters(), node->textContent().length(), true);
            } else if (node->hasTagName(HTMLNames::aTag) || 
                node->hasTagName(HTMLNames::areaTag)) 
            {
                HTMLAnchorElement* anchor = static_cast<HTMLAnchorElement*>(node);
                wideString(anchor->href());
            } else if (node->hasTagName(HTMLNames::imgTag)) {
                HTMLImageElement* image = static_cast<HTMLImageElement*>(node);
                wideString(image->src());
            } else 
                print("\"\"");
            RenderObject* renderer = node->renderer();
            int tabindex = node->isElementNode() ? node->tabIndex() : 0;
            RenderLayer* layer = 0;
            if (renderer) {
                const IntRect& absB = renderer->absoluteBoundingBoxRect();
                bool hasLayer = renderer->hasLayer();
                layer = hasLayer ? toRenderBoxModelObject(renderer)->layer() : 0;
                snprintf(scratch, sizeof(scratch), ", {%d, %d, %d, %d}, %s"
                    ", %d, %s, %s},",
                    absB.x(), absB.y(), absB.width(), absB.height(),
                    renderer->hasOverflowClip() ? "true" : "false", tabindex,
                    hasLayer ? "true" : "false",
                    hasLayer && layer->isComposited() ? "true" : "false");
                // TODO: add renderer->style()->visibility()
                print(scratch);
            } else
                print(", {0, 0, 0, 0}, false, 0},");

            flush();
            snprintf(scratch, sizeof(scratch), "// %d: ", count);
            mPrefix = "\n// ";
            print(scratch);
            //print(renderer ? renderer->information().ascii() : "NO_RENDER_INFO");
            if (node->isElementNode()) {
                Element* element = static_cast<Element*>(node);
                NamedNodeMap* attrs = element->attributes();
                unsigned length = attrs->length();
                if (length > 0) {
                    newLine();
                    print("// attr: ");
                    for (unsigned i = 0; i < length; i++) {
                        Attribute* a = attrs->attributeItem(i);
                        attr(a->localName(), a->value());
                    }
                }
            }
            if (renderer) {
                RenderStyle* style = renderer->style();
                snprintf(scratch, sizeof(scratch), "// renderStyle:"
                    " visibility=%s hasBackGround=%d"
                    " tapHighlightColor().alpha()=0x%02x"
                    " isTransparent()=%s",
                    style->visibility() == HIDDEN ? "HIDDEN" : "VISIBLE",
                    renderer->hasBackground(), style->tapHighlightColor().alpha(),
                    renderer->isTransparent() ? "true" : "false");
                newLine();
                print(scratch);
                RenderBlock* renderBlock = static_cast<RenderBlock*>(renderer);
                if (renderer->isRenderBlock() && renderBlock->hasColumns()) {
                    const RenderBox* box = static_cast<RenderBox*>(renderer);
                    const IntRect& oRect = box->visibleOverflowRect();
                    snprintf(scratch, sizeof(scratch), "// renderBlock:"
                        " columnCount=%d columnGap=%d direction=%d"
                        " hasOverflowClip=%d overflow=(%d,%d,w=%d,h=%d)",
                        renderBlock->columnInfo()->columnCount(), renderBlock->columnGap(),
                        renderBlock->style()->direction(), renderer->hasOverflowClip(),
                        oRect.x(), oRect.y(), oRect.width(), oRect.height());
                    newLine();
                    print(scratch);
                }
            }
 #if USE(ACCELERATED_COMPOSITING)
            if (renderer && renderer->hasLayer()) {
                RenderLayer* layer = toRenderBoxModelObject(renderer)->layer();
                RenderLayerBacking* back = layer->backing();
                GraphicsLayer* grLayer = back ? back->graphicsLayer() : 0;
                LayerAndroid* aLayer = grLayer ? grLayer->platformLayer() : 0;
                const SkPicture* pict = aLayer ? aLayer->picture() : 0;
                const IntRect& r = renderer->absoluteBoundingBoxRect();
                snprintf(scratch, sizeof(scratch), "// layer:%p back:%p"
                    " gLayer:%p aLayer:%p pict:%p r:(%d,%d,w=%d,h=%d)",
                    layer, back, grLayer, aLayer, pict, r.x(), r.y(),
                    r.width(), r.height());
                newLine();
                print(scratch);
            }
 #endif
            count++;
            newLine();
        } while ((node = node->traverseNextNode()) != NULL);
        DUMP_NAV_LOGD("}; // focusables = %d\n", count - 1);
        DUMP_NAV_LOGD("\n");
        DUMP_NAV_LOGD("static int TEST%s_RECT_COUNT = %d;\n\n", name, count - 1);
        // look for rects with multiple parts
        node = doc;
        count = 1;
        bool hasRectParts = false;
        int globalOffsetX, globalOffsetY;
        GetGlobalOffset(frame, &globalOffsetX, &globalOffsetY);
        do {
            IntRect rect;
            bool _isFocusable = isFocusable(node) || (node->isTextNode() 
              && node->getRect().isEmpty() == false
                );
            int nodeIndex = count++;
            if (_isFocusable == false)
                continue;
            RenderObject* renderer = node->renderer();
            if (renderer == NULL)
                continue;
            WTF::Vector<IntRect> rects;
            IntRect clipBounds = IntRect(0, 0, INT_MAX, INT_MAX);
            IntRect focusBounds = IntRect(0, 0, INT_MAX, INT_MAX);
            IntRect* rectPtr = &focusBounds;
            int imageCount = 0;
            if (node->isTextNode()) {
                Text* textNode = (Text*) node;
                if (CacheBuilder::ConstructTextRects(textNode, 0, textNode, 
                        INT_MAX, globalOffsetX, globalOffsetY, rectPtr, 
                        clipBounds, &rects) == false)
                    continue;
            } else {
                IntRect nodeBounds = node->getRect();
                if (CacheBuilder::ConstructPartRects(node, nodeBounds, rectPtr, 
                        globalOffsetX, globalOffsetY, &rects, &imageCount) == false)
                    continue;
            }
            unsigned arraySize = rects.size();
            if (arraySize > 1 || (arraySize == 1 && (rectPtr->width() != rect.width())) || 
                    rectPtr->height() != rect.height()) {
                if (hasRectParts == false) {
                    DUMP_NAV_LOGD("static DebugTestRectPart TEST%s_RECTPARTS[] = {\n", name);
                    hasRectParts = true;
                }
                if (node->isTextNode() == false) {
                    unsigned rectIndex = 0;
                    for (; rectIndex < arraySize; rectIndex++) {
                        rectPtr = &rects.at(rectIndex);
                        DUMP_NAV_LOGD("{ %d, %d, %d, %d, %d }, // %d\n", nodeIndex, 
                            rectPtr->x(), rectPtr->y(), rectPtr->width(), 
                            rectPtr->height(), rectIndex + 1);
                    }
                } else {
                    RenderText* renderText = (RenderText*) node->renderer();
                    InlineTextBox* textBox = renderText->firstTextBox();
                    unsigned rectIndex = 0;
                    while (textBox) {
                        FloatPoint pt = renderText->localToAbsolute();
                        IntRect rect = textBox->selectionRect((int) pt.x(), (int) pt.y(), 0, INT_MAX);
                        mIndex = 0;
                        mIndex += snprintf(&mBuffer[mIndex], mBufferSize - mIndex, "{ %d, %d, %d, %d, %d", 
                            nodeIndex, rect.x(), rect.y(), rect.width(), rect.height());
                        mIndex += snprintf(&mBuffer[mIndex], mBufferSize - mIndex, ", %d, %d, %d", 
                            textBox->len(), 0 /*textBox->selectionHeight()*/, 
                            0 /*textBox->selectionTop()*/);
                        mIndex += snprintf(&mBuffer[mIndex], mBufferSize - mIndex, ", %d, %d, %d", 
                            0 /*textBox->spaceAdd()*/, textBox->start(), 0 /*textBox->textPos()*/);
                        mIndex += snprintf(&mBuffer[mIndex], mBufferSize - mIndex, ", %d, %d, %d, %d", 
                            textBox->x(), textBox->y(), textBox->logicalWidth(), textBox->logicalHeight());
                        int baseline = textBox->renderer()->style(textBox->isFirstLineStyle())->font().ascent();
                        mIndex += snprintf(&mBuffer[mIndex], mBufferSize - mIndex, ", %d, %d }, // %d ",
                            baseline, imageCount, ++rectIndex);
                        wideString(node->textContent().characters() + textBox->start(), textBox->len(), true);
                        DUMP_NAV_LOGD("%.*s\n", mIndex, mBuffer);
                        textBox = textBox->nextTextBox();
                    }
                }
            }
        } while ((node = node->traverseNextNode()) != NULL);
        if (hasRectParts)
            DUMP_NAV_LOGD("{0}\n};\n\n");
        else
            DUMP_NAV_LOGD("static DebugTestRectPart* TEST%s_RECTPARTS = NULL;\n", name);
    }
    int contentsWidth = layer->width();
    int contentsHeight = layer->height();
    DUMP_NAV_LOGD("static int TEST%s_FOCUS = %d;\n", name, focusIndex);        
    DUMP_NAV_LOGD("static int TEST%s_WIDTH = %d;\n", name, contentsWidth);
    DUMP_NAV_LOGD("static int TEST%s_HEIGHT = %d;\n\n", name, contentsHeight);
}

bool CacheBuilder::Debug::isFocusable(Node* node) {
    if (node->hasTagName(HTMLNames::areaTag))
        return true;
    if (node->renderer() == false)
        return false;
    if (node->isKeyboardFocusable(NULL))
        return true;
    if (node->isMouseFocusable())
        return true;
    if (node->isFocusable())
        return true;
    if (CacheBuilder::AnyIsClick(node))
        return false;
    if (CacheBuilder::HasTriggerEvent(node))
        return true;
    return false;
}

void CacheBuilder::Debug::localName(Node* node) {
    const AtomicString& local = node->localName();
    if (node->isTextNode())
        print("\"#text\"");
    else
        wideString(local.characters(), local.length(), false);
    print(", ");
}

void CacheBuilder::Debug::newLine(int indent) {
    if (mPrefix[0] != '\n')
        print(&mPrefix[0], 1);
    flush();
    int lastnewline = mIndex - 1;
    while (lastnewline >= 0 && mBuffer[lastnewline] != '\n')
        lastnewline--;
    lastnewline++;
    char* buffer = mBuffer;
    if (lastnewline > 0) {
        DUMP_NAV_LOGD("%.*s", lastnewline, buffer);
        mIndex -= lastnewline;
        buffer += lastnewline;
    }
    size_t prefixLen = strlen(mPrefix);
    int minPrefix = prefixLen - 1;
    while (minPrefix >= 0 && mPrefix[minPrefix] != '\n')
        minPrefix--;
    minPrefix = prefixLen - minPrefix - 1;
    if (mIndex > minPrefix)
        DUMP_NAV_LOGD("%.*s\n", mIndex, buffer);
    mIndex = 0;
    setIndent(indent);
}

int CacheBuilder::Debug::ParentIndex(Node* node, int count, Node* parent) 
{
    if (parent == NULL)
        return -1;
    ASSERT(node != parent);
    int result = count;
    Node* previous = node;
    do {
        result--;
        previous = previous->traversePreviousNode();
    } while (previous && previous != parent);
    if (previous != NULL) 
        return result;
    result = count;
    do {
        result++;
    } while ((node = node->traverseNextNode()) != NULL && node != parent);
    if (node != NULL)
        return result;
    ASSERT(0);
    return -1;
}

void CacheBuilder::Debug::print(const char* name) {
    print(name, strlen(name));
}

void CacheBuilder::Debug::print(const char* name, unsigned len) {
    do {
        if (mIndex + len >= DEBUG_BUFFER_SIZE)
            flush();
        int copyLen = mIndex + len < DEBUG_BUFFER_SIZE ?
             len : DEBUG_BUFFER_SIZE - mIndex;
        memcpy(&mBuffer[mIndex], name, copyLen);
        mIndex += copyLen;
        name += copyLen;
        len -= copyLen;
    } while (len > 0);
    mBuffer[mIndex] = '\0';
}

void CacheBuilder::Debug::setIndent(int indent)
{
    char scratch[64];
    snprintf(scratch, sizeof(scratch), "%.*s", indent, 
        "                                                                    ");
    print(scratch);
}

void CacheBuilder::Debug::uChar(const UChar* name, unsigned len, bool hex) {
    const UChar* end = name + len;
    bool wroteHex = false;
    while (name < end) {
        unsigned ch = *name++;
        if (ch == '\t' || ch == '\n' || ch == '\r' || ch == 0xa0)
            ch = ' ';
        if (ch < ' ' || ch == 0x7f) {
            if (hex) {
                mIndex += snprintf(&mBuffer[mIndex], mBufferSize - mIndex, "\\x%02x", ch);
                wroteHex = true;
            } else
                mBuffer[mIndex++] = '?';
        } else if (ch >= 0x80) {
            if (hex) {
                if (ch < 0x800)
                    mIndex += snprintf(&mBuffer[mIndex], mBufferSize - mIndex,
                        "\\x%02x\\x%02x", ch >> 6 | 0xc0, (ch & 0x3f) | 0x80);
                else
                    mIndex += snprintf(&mBuffer[mIndex], mBufferSize - mIndex,
                        "\\x%02x\\x%02x\\x%02x", ch >> 12 | 0xe0,
                        (ch >> 6 & 0x3f) | 0x80, (ch & 0x3f) | 0x80);
                wroteHex = true;
            } else
                mBuffer[mIndex++] = '?';
        } else {
            if (wroteHex && WTF::isASCIIHexDigit((UChar) ch))
                mIndex += snprintf(&mBuffer[mIndex], mBufferSize - mIndex,
                    "\" \"");
            else if (ch == '"' || ch == '\\')
                mBuffer[mIndex++] = '\\';
            mBuffer[mIndex++] = ch;
            wroteHex = false;
        }
        if (mIndex + 1 >= DEBUG_BUFFER_SIZE)
            flush();
    }
    flush();
}

void CacheBuilder::Debug::validateFrame() {
    Frame* frame = frameAnd();
    Page* page = frame->page();
    ASSERT(page);
    ASSERT((int) page > 0x10000);
    Frame* child = frame->tree()->firstChild();
    while (child) {
        Builder(child)->mDebug.validateFrame();
        child = child->tree()->nextSibling();
    }
}

void CacheBuilder::Debug::wideString(const UChar* chars, int length, bool hex) {
    if (length == 0)
        print("\"\"");
    else {
        print("\"");
        uChar(chars, length, hex);
        print("\"");
    }
}

void CacheBuilder::Debug::wideString(const String& str) {
    wideString(str.characters(), str.length(), false);
}

#endif // DUMP_NAV_CACHE

CacheBuilder::CacheBuilder()
{
    mAllowableTypes = ALL_CACHEDNODE_BITS;
#ifdef DUMP_NAV_CACHE_USING_PRINTF
    gNavCacheLogFile = NULL;
#endif
}

void CacheBuilder::adjustForColumns(const ClipColumnTracker& track, 
    CachedNode* node, IntRect* bounds, RenderBlock* renderer)
{
    if (!renderer->hasColumns())
        return;
    int x = 0;
    int y = 0;
    int tx = track.mBounds.x();
    int ty = track.mBounds.y();
    int columnGap = track.mColumnGap;
    size_t limit = track.mColumnInfo->columnCount();
    for (size_t index = 0; index < limit; index++) {
        IntRect column = renderer->columnRectAt(track.mColumnInfo, index);
        column.move(tx, ty);
        IntRect test = *bounds;
        test.move(x, y);
        if (column.contains(test)) {
            if ((x | y) == 0)
                return;
            *bounds = test;
            node->move(x, y);
            return;
        }
        int xOffset = column.width() + columnGap;
        x += track.mDirection == LTR ? xOffset : -xOffset;
        y -= column.height();
    }    
}

// Checks if a node has one of event listener types.
bool CacheBuilder::NodeHasEventListeners(Node* node, AtomicString* eventTypes, int length) {
    for (int i = 0; i < length; ++i) {
        if (!node->getEventListeners(eventTypes[i]).isEmpty())
            return true;
    }
    return false;
}

bool CacheBuilder::AnyChildIsClick(Node* node)
{
    AtomicString eventTypes[5] = {
        eventNames().clickEvent,
        eventNames().mousedownEvent,
        eventNames().mouseupEvent,
        eventNames().keydownEvent,
        eventNames().keyupEvent
    };

    Node* child = node->firstChild();
    while (child != NULL) {
        if (child->isFocusable() ||
            NodeHasEventListeners(child, eventTypes, 5))
                return true;
        if (AnyChildIsClick(child))
            return true;
        child = child->nextSibling();
    }
    return false;
}

bool CacheBuilder::AnyIsClick(Node* node)
{
    if (node->hasTagName(HTMLNames::bodyTag))
        return AnyChildIsClick(node);

    AtomicString eventTypeSetOne[4] = {
        eventNames().mouseoverEvent,
        eventNames().mouseoutEvent,
        eventNames().keydownEvent,
        eventNames().keyupEvent
    };

    if (!NodeHasEventListeners(node, eventTypeSetOne, 4))
        return false;

    AtomicString eventTypeSetTwo[3] = {
        eventNames().clickEvent,
        eventNames().mousedownEvent,
        eventNames().mouseupEvent
    };

    if (NodeHasEventListeners(node, eventTypeSetTwo, 3))
        return false;

    return AnyChildIsClick(node);
}

void CacheBuilder::buildCache(CachedRoot* root)
{
    Frame* frame = FrameAnd(this);
    mPictureSetDisabled = false;
    BuildFrame(frame, frame, root, (CachedFrame*) root);
    root->finishInit(); // set up frame parent pointers, child pointers
    setData((CachedFrame*) root);
}

static Node* ParentWithChildren(Node* node)
{
    Node* parent = node;
    while ((parent = parent->parentNode())) {
        if (parent->childNodeCount() > 1)
            return parent;
    }
    return 0;
}

// FIXME
// Probably this should check for null instead of the caller. If the
// Tracker object is the last thing in the dom, checking for null in the
// caller in some cases fails to set up Tracker state which may be useful
// to the nodes parsed immediately after the tracked noe.
static Node* OneAfter(Node* node) 
{
    Node* parent = node;
    Node* sibling = NULL;
    while ((parent = parent->parentNode()) != NULL) {
        sibling = parent->nextSibling();
        if (sibling != NULL)
            break;
    }
    return sibling;
}

// return true if this renderer is really a pluinview, and it wants
// key-events (i.e. focus)
static bool checkForPluginViewThatWantsFocus(RenderObject* renderer) {
    if (renderer->isWidget()) {
        Widget* widget = static_cast<RenderWidget*>(renderer)->widget();
        if (widget && (widget->isPluginView() || widget->isPluginViewBase())) {
            // check if this plugin really wants key events (TODO)
            return true;
        }
    }
    return false;
}

#if USE(ACCELERATED_COMPOSITING)
static void AddLayer(CachedFrame* frame, size_t index, const IntPoint& location, int id)
{
    DBG_NAV_LOGD("frame=%p index=%d loc=(%d,%d) id=%d", frame, index,
        location.x(), location.y(), id);
    CachedLayer cachedLayer;
    cachedLayer.setCachedNodeIndex(index);
    cachedLayer.setOffset(location);
    cachedLayer.setUniqueId(id);
    frame->add(cachedLayer);
}
#endif

static int FindColorIndex(WTF::Vector<CachedColor>& colorTracker,
    const CachedColor& cachedColor)
{
    CachedColor* work = colorTracker.begin() - 1;
    CachedColor* end = colorTracker.end();
    while (++work < end) {
        if (*work == cachedColor)
            return work - colorTracker.begin();
    }
    int result = colorTracker.size();
    colorTracker.grow(result + 1);
    CachedColor& newColor = colorTracker.last();
    newColor = cachedColor;
    return result;
}

static void InitColor(CachedColor* color)
{
    color->setFillColor(RenderStyle::initialRingFillColor());
    color->setInnerWidth(RenderStyle::initialRingInnerWidth());
    color->setOuterWidth(RenderStyle::initialRingOuterWidth());
    color->setOutset(RenderStyle::initialRingOutset());
    color->setPressedInnerColor(RenderStyle::initialRingPressedInnerColor());
    color->setPressedOuterColor(RenderStyle::initialRingPressedOuterColor());
    color->setRadius(RenderStyle::initialRingRadius());
    color->setSelectedInnerColor(RenderStyle::initialRingSelectedInnerColor());
    color->setSelectedOuterColor(RenderStyle::initialRingSelectedOuterColor());
}

// when new focus is found, push it's parent on a stack
    // as long as more focii are found with the same (grand) parent, note it
    // (which only requires retrieving the last parent on the stack)
// when the parent's last child is found, pop the stack
// different from Tracker in that Tracker only pushes focii with children

// making this work with focus - child focus - grandchild focus is tricky
// if I keep the generation number, I may be able to more quickly determine that
// a node is a grandchild of the focus's parent
// this additionally requires being able to find the grandchild's parent

// keep nodes that are focusable
void CacheBuilder::BuildFrame(Frame* root, Frame* frame,
    CachedRoot* cachedRoot, CachedFrame* cachedFrame)
{
    WTF::Vector<FocusTracker> tracker(1); // sentinel
    {
        FocusTracker* baseTracker = tracker.data();
        bzero(baseTracker, sizeof(FocusTracker));
        baseTracker->mCachedNodeIndex = -1;
    }
    WTF::Vector<LayerTracker> layerTracker(1); // sentinel
    bzero(layerTracker.data(), sizeof(LayerTracker));
    WTF::Vector<ClipColumnTracker> clipTracker(1); // sentinel
    bzero(clipTracker.data(), sizeof(ClipColumnTracker));
    WTF::Vector<TabIndexTracker> tabIndexTracker(1); // sentinel
    bzero(tabIndexTracker.data(), sizeof(TabIndexTracker));
    WTF::Vector<CachedColor> colorTracker(1);
    InitColor(colorTracker.data());
#if DUMP_NAV_CACHE
    char* frameNamePtr = cachedFrame->mDebug.mFrameName;
    Builder(frame)->mDebug.frameName(frameNamePtr, frameNamePtr + 
        sizeof(cachedFrame->mDebug.mFrameName) - 1);
    *frameNamePtr = '\0';
    int nodeIndex = 1;
#endif
    NodeWalk walk;
    Document* doc = frame->document();
    Node* parent = doc;
    CachedNode cachedParentNode;
    cachedParentNode.init(parent);
#if DUMP_NAV_CACHE
    cachedParentNode.mDebug.mNodeIndex = nodeIndex;
#endif
    cachedFrame->add(colorTracker[0]);
    cachedFrame->add(cachedParentNode);
    Node* node = parent;
    int cacheIndex = 1;
    int colorIndex = 0; // assume no special css ring colors
    const void* lastStyleDataPtr = 0;
    int textInputIndex = 0;
    Node* focused = doc->focusedNode();
    if (focused)
        cachedRoot->setFocusBounds(focused->getRect());
    int globalOffsetX, globalOffsetY;
    GetGlobalOffset(frame, &globalOffsetX, &globalOffsetY);
#if USE(ACCELERATED_COMPOSITING)
    // The frame itself might be composited so we need to track the layer.  Do
    // not track the base frame's layer as the main content is draw as part of
    // BaseLayerAndroid's picture.
    if (frame != root && frame->contentRenderer()
        && frame->contentRenderer()->usesCompositing() && node->lastChild())
        TrackLayer(layerTracker, frame->contentRenderer(), node->lastChild(),
            globalOffsetX, globalOffsetY);
#endif
    while (walk.mMore || (node = node->traverseNextNode()) != NULL) {
#if DUMP_NAV_CACHE
        nodeIndex++;
#endif
        FocusTracker* last = &tracker.last();
        int lastChildIndex = cachedFrame->size() - 1;
        while (node == last->mLastChild) {
            if (CleanUpContainedNodes(cachedRoot, cachedFrame, last, lastChildIndex))
                cacheIndex--;
            tracker.removeLast();
            lastChildIndex = last->mCachedNodeIndex;
            last = &tracker.last();
        }
        do {
            const ClipColumnTracker* lastClip = &clipTracker.last();
            if (node != lastClip->mLastChild)
                break;
            clipTracker.removeLast();
        } while (true);
        do {
            const LayerTracker* lastLayer = &layerTracker.last();
            if (node != lastLayer->mLastChild)
                break;
            layerTracker.removeLast();
        } while (true);
        do {
            const TabIndexTracker* lastTabIndex = &tabIndexTracker.last();
            if (node != lastTabIndex->mLastChild)
                break;
            tabIndexTracker.removeLast();
        } while (true);
        Frame* child = HasFrame(node);
        CachedNode cachedNode;
        if (child != NULL) {
            if (child->document() == NULL)
                continue;
            RenderObject* nodeRenderer = node->renderer();
            if (nodeRenderer != NULL && nodeRenderer->style()->visibility() == HIDDEN)
                continue;
            CachedFrame cachedChild;
            cachedChild.init(cachedRoot, cacheIndex, child);
            int childFrameIndex = cachedFrame->childCount();
            cachedFrame->addFrame(cachedChild);
            cachedNode.init(node);
            cachedNode.setIndex(cacheIndex++);
            cachedNode.setDataIndex(childFrameIndex);
            cachedNode.setType(FRAME_CACHEDNODETYPE);
#if DUMP_NAV_CACHE
            cachedNode.mDebug.mNodeIndex = nodeIndex;
            cachedNode.mDebug.mParentGroupIndex = Debug::ParentIndex(
                node, nodeIndex, NULL);
#endif
            cachedFrame->add(cachedNode);
            CachedFrame* childPtr = cachedFrame->lastChild();
            BuildFrame(root, child, cachedRoot, childPtr);
            continue;
        }
        int tabIndex = node->tabIndex();
        Node* lastChild = node->lastChild();
        if (tabIndex <= 0)
            tabIndex = tabIndexTracker.last().mTabIndex;
        else if (tabIndex > 0 && lastChild) {
            DBG_NAV_LOGD("tabIndex=%d node=%p", tabIndex, node);
            tabIndexTracker.grow(tabIndexTracker.size() + 1);
            TabIndexTracker& indexTracker = tabIndexTracker.last();
            indexTracker.mTabIndex = tabIndex;
            indexTracker.mLastChild = OneAfter(lastChild);
        }
        RenderObject* nodeRenderer = node->renderer();
        bool isTransparent = false;
        bool hasCursorRing = true;
        if (nodeRenderer != NULL) {
            RenderStyle* style = nodeRenderer->style();
            if (style->visibility() == HIDDEN)
                continue;
            isTransparent = nodeRenderer->hasBackground() == false;
#ifdef ANDROID_CSS_TAP_HIGHLIGHT_COLOR
            hasCursorRing = style->tapHighlightColor().alpha() > 0;
#endif
#if USE(ACCELERATED_COMPOSITING)
            // If this renderer has its own layer and the layer is composited,
            // start tracking it.
            if (lastChild && nodeRenderer->hasLayer() && toRenderBoxModelObject(nodeRenderer)->layer()->backing())
                TrackLayer(layerTracker, nodeRenderer, lastChild, globalOffsetX, globalOffsetY);
#endif
        }
        bool more = walk.mMore;
        walk.reset();
     //   GetGlobalBounds(node, &bounds, false);
        bool computeCursorRings = false;
        bool hasClip = false;
        bool hasMouseOver = false;
        bool isUnclipped = false;
        bool isFocus = node == focused;
        bool takesFocus = false;
        int columnGap = 0;
        int imageCount = 0;
        TextDirection direction = LTR;
        String exported;
        CachedNodeType type = NORMAL_CACHEDNODETYPE;
        CachedColor cachedColor;
        CachedInput cachedInput;
        IntRect bounds;
        IntRect absBounds;
        IntRect originalAbsBounds;
        ColumnInfo* columnInfo = NULL;
        if (node->hasTagName(HTMLNames::areaTag)) {
            type = AREA_CACHEDNODETYPE;
            HTMLAreaElement* area = static_cast<HTMLAreaElement*>(node);
            bounds = getAreaRect(area);
            originalAbsBounds = bounds;
            bounds.move(globalOffsetX, globalOffsetY);
            absBounds = bounds;
            isUnclipped = true;  // FIXME: areamaps require more effort to detect
             // assume areamaps are always visible for now
            takesFocus = true;
            goto keepNode;
        }
        if (nodeRenderer == NULL)
            continue;

        // some common setup
        absBounds = nodeRenderer->absoluteBoundingBoxRect();
        originalAbsBounds = absBounds;
        absBounds.move(globalOffsetX, globalOffsetY);
        hasClip = nodeRenderer->hasOverflowClip();

        if (checkForPluginViewThatWantsFocus(nodeRenderer)) {
            bounds = absBounds;
            isUnclipped = true;
            takesFocus = true;
            type = PLUGIN_CACHEDNODETYPE;
            goto keepNode;
        }
        // Only use the root contentEditable element
        if (node->rendererIsEditable() && !node->parentOrHostNode()->rendererIsEditable()) {
            bounds = absBounds;
            takesFocus = true;
            type = CONTENT_EDITABLE_CACHEDNODETYPE;
            goto keepNode;
        }
        if (nodeRenderer->isRenderBlock()) {
            RenderBlock* renderBlock = (RenderBlock*) nodeRenderer;
            if (renderBlock->hasColumns()) {
                columnInfo = renderBlock->columnInfo();
                columnGap = renderBlock->columnGap();
                direction = renderBlock->style()->direction();
            }
        }
        if ((hasClip != false || columnInfo != NULL) && lastChild) {
            clipTracker.grow(clipTracker.size() + 1);
            ClipColumnTracker& clip = clipTracker.last();
            clip.mBounds = absBounds;
            clip.mLastChild = OneAfter(lastChild);
            clip.mNode = node;
            clip.mColumnInfo = columnInfo;
            clip.mColumnGap = columnGap;
            clip.mHasClip = hasClip;
            clip.mDirection = direction;
            if (columnInfo != NULL) {
                const IntRect& oRect = ((RenderBox*)nodeRenderer)->visualOverflowRect();
                clip.mBounds.move(oRect.x(), oRect.y());
            }
        }
        if (node->isTextNode() && mAllowableTypes != NORMAL_CACHEDNODE_BITS) {
            if (last->mSomeParentTakesFocus) // don't look at text inside focusable node
                continue;
            CachedNodeType checkType;
            if (isFocusableText(&walk, more, node, &checkType, 
                    &exported) == false)
                continue;
        #if DUMP_NAV_CACHE
            { 
                char buffer[DEBUG_BUFFER_SIZE];
                mDebug.init(buffer, sizeof(buffer));
                mDebug.print("text link found: ");
                mDebug.wideString(exported);
                DUMP_NAV_LOGD("%s\n", buffer);
            }
        #endif
            type = checkType;
            // !!! test ! is the following line correctly needed for frames to work?
            cachedNode.init(node);
            const ClipColumnTracker& clipTrack = clipTracker.last();
            const IntRect& clip = clipTrack.mHasClip ? clipTrack.mBounds :
                IntRect(0, 0, INT_MAX, INT_MAX);
            if (ConstructTextRects((WebCore::Text*) node, walk.mStart, 
                    (WebCore::Text*) walk.mFinalNode, walk.mEnd, globalOffsetX,
                    globalOffsetY, &bounds, clip, &cachedNode.mCursorRing) == false)
                continue;
            absBounds = bounds;
            cachedNode.setBounds(bounds);
            if (bounds.width() < MINIMUM_FOCUSABLE_WIDTH)
                continue;
            if (bounds.height() < MINIMUM_FOCUSABLE_HEIGHT)
                continue;
            computeCursorRings = true;
            isUnclipped = true;  // FIXME: to hide or partially occlude synthesized links, each
                                 // focus ring will also need the offset and length of characters
                                 // used to produce it
            goto keepTextNode;
        }
        if (node->hasTagName(WebCore::HTMLNames::inputTag)) {
            HTMLInputElement* input = static_cast<HTMLInputElement*>(node);
            if (input->isTextField()) {
                if (input->readOnly())
                    continue;
                type = TEXT_INPUT_CACHEDNODETYPE;
                cachedInput.init();
                cachedInput.setAutoComplete(input->autoComplete());
                cachedInput.setSpellcheck(input->spellcheck());
                cachedInput.setFormPointer(input->form());
                cachedInput.setIsTextField(true);
                exported = input->value().threadsafeCopy();
                cachedInput.setMaxLength(input->maxLength());
                cachedInput.setTypeFromElement(input);
    // If this does not need to be threadsafe, we can use crossThreadString().
    // See http://trac.webkit.org/changeset/49160.
                cachedInput.setName(input->name().string().threadsafeCopy());
    // can't detect if this is drawn on top (example: deviant.com login parts)
                isUnclipped = isTransparent;
            } else if (input->isInputTypeHidden())
                continue;
            else if (input->isRadioButton() || input->isCheckbox())
                isTransparent = false;
        } else if (node->hasTagName(HTMLNames::textareaTag)) {
            HTMLTextAreaElement* area = static_cast<HTMLTextAreaElement*>(node);
            if (area->readOnly())
                continue;
            cachedInput.init();
            type = TEXT_INPUT_CACHEDNODETYPE;
            cachedInput.setFormPointer(area->form());
            cachedInput.setIsTextArea(true);
            cachedInput.setSpellcheck(area->spellcheck());
            exported = area->value().threadsafeCopy();
        } else if (node->hasTagName(HTMLNames::aTag)) {
            const HTMLAnchorElement* anchorNode = 
                (const HTMLAnchorElement*) node;
            if (!anchorNode->isFocusable() && !HasTriggerEvent(node))
                continue;
            if (node->disabled())
                continue;
            hasMouseOver = NodeHasEventListeners(node, &eventNames().mouseoverEvent, 1);
            type = ANCHOR_CACHEDNODETYPE;
            KURL href = anchorNode->href();
            if (!href.isEmpty() && !WebCore::protocolIsJavaScript(href.string()))
                // Set the exported string for all non-javascript anchors.
                exported = href.string().threadsafeCopy();
        } else if (node->hasTagName(HTMLNames::selectTag)) {
            type = SELECT_CACHEDNODETYPE;
        }
        if (type == TEXT_INPUT_CACHEDNODETYPE) {
            RenderTextControl* renderText = 
                static_cast<RenderTextControl*>(nodeRenderer);
            if (isFocus)
                cachedRoot->setSelection(renderText->selectionStart(), renderText->selectionEnd());
            // FIXME: Are we sure there will always be a style and font, and it's correct?
            RenderStyle* style = nodeRenderer->style();
            if (style) {
                isUnclipped |= !style->hasAppearance();
                int lineHeight = -1;
                Length lineHeightLength = style->lineHeight();
                // If the lineHeight is negative, WebTextView will calculate it
                // based on the text size, using the Paint.
                // See RenderStyle.computedLineHeight.
                if (lineHeightLength.isPositive())
                    lineHeight = style->computedLineHeight();
                cachedInput.setLineHeight(lineHeight);
                cachedInput.setTextSize(style->font().size());
                cachedInput.setIsRtlText(style->direction() == RTL
                        || style->textAlign() == WebCore::RIGHT
                        || style->textAlign() == WebCore::WEBKIT_RIGHT);
            }
            cachedInput.setPaddingLeft(renderText->paddingLeft() + renderText->borderLeft());
            cachedInput.setPaddingTop(renderText->paddingTop() + renderText->borderTop());
            cachedInput.setPaddingRight(renderText->paddingRight() + renderText->borderRight());
            cachedInput.setPaddingBottom(renderText->paddingBottom() + renderText->borderBottom());
        }
        takesFocus = true;
        bounds = absBounds;
        if (type != ANCHOR_CACHEDNODETYPE) {
            bool isFocusable = node->isKeyboardFocusable(NULL) || 
                node->isMouseFocusable() || node->isFocusable();
            if (isFocusable == false) {
                if (node->disabled())
                    continue;
                bool overOrOut = HasOverOrOut(node);
                bool hasTrigger = HasTriggerEvent(node);
                if (overOrOut == false && hasTrigger == false)
                    continue;
                takesFocus = hasTrigger;
            }
        }
        computeCursorRings = true;
    keepNode:
        cachedNode.init(node);
        if (computeCursorRings == false) {
            cachedNode.setBounds(bounds);
            cachedNode.mCursorRing.append(bounds);
        } else if (ConstructPartRects(node, bounds, &cachedNode.mBounds,
                globalOffsetX, globalOffsetY, &cachedNode.mCursorRing,
                &imageCount) == false)
            continue;
    keepTextNode:
        if (nodeRenderer) { // area tags' node->renderer() == 0
            RenderStyle* style = nodeRenderer->style();
            const void* styleDataPtr = style->ringData();
            // to save time, see if we're pointing to the same style data as before
            if (lastStyleDataPtr != styleDataPtr) {
                lastStyleDataPtr = styleDataPtr;
                cachedColor.setFillColor(style->ringFillColor());
                cachedColor.setInnerWidth(style->ringInnerWidth());
                cachedColor.setOuterWidth(style->ringOuterWidth());
                cachedColor.setOutset(style->ringOutset());
                cachedColor.setPressedInnerColor(style->ringPressedInnerColor());
                cachedColor.setPressedOuterColor(style->ringPressedOuterColor());
                cachedColor.setRadius(style->ringRadius());
                cachedColor.setSelectedInnerColor(style->ringSelectedInnerColor());
                cachedColor.setSelectedOuterColor(style->ringSelectedOuterColor());
                int oldSize = colorTracker.size();
                colorIndex = FindColorIndex(colorTracker, cachedColor);
                if (colorIndex == oldSize)
                    cachedFrame->add(cachedColor);
            }
        } else
            colorIndex = 0;
        IntRect clip = hasClip ? bounds : absBounds;
        size_t clipIndex = clipTracker.size();
        if (clipTracker.last().mNode == node)
            clipIndex -= 1;
        while (--clipIndex > 0) {
            const ClipColumnTracker& clipTrack = clipTracker.at(clipIndex);
            if (clipTrack.mHasClip == false) {
                adjustForColumns(clipTrack, &cachedNode, &absBounds, static_cast<RenderBlock*>(nodeRenderer));
                continue;
            }
            const IntRect& parentClip = clipTrack.mBounds;
            if (hasClip == false && type == ANCHOR_CACHEDNODETYPE)
                clip = parentClip;
            else
                clip.intersect(parentClip);
            hasClip = true;
        }
        bool isInLayer = false;
#if USE(ACCELERATED_COMPOSITING)
        // If this renderer has a composited parent layer (including itself),
        // add the node to the cached layer.
        // FIXME: does not work for area rects
        RenderLayer* enclosingLayer = nodeRenderer->enclosingLayer();
        if (enclosingLayer && enclosingLayer->enclosingCompositingLayer()) {
            LayerAndroid* layer = layerTracker.last().mLayer;
            if (layer) {
                const IntRect& layerClip = layerTracker.last().mBounds;
                if (!layerClip.isEmpty() && !cachedNode.clip(layerClip)) {
                    DBG_NAV_LOGD("skipped on layer clip %d", cacheIndex);
                    continue; // skip this node if outside of the clip
                }
                isInLayer = true;
                isUnclipped = true; // assume that layers do not have occluded nodes
                hasClip = false;
                AddLayer(cachedFrame, cachedFrame->size(), layerClip.location(),
                         layer->uniqueId());
            }
        }
#endif
        if (hasClip) {
            if (clip.isEmpty())
                continue; // skip this node if clip prevents all drawing
            else if (cachedNode.clip(clip) == false)
                continue; // skip this node if outside of the clip
        }
        cachedNode.setColorIndex(colorIndex);
        cachedNode.setExport(exported);
        cachedNode.setHasCursorRing(hasCursorRing);
        cachedNode.setHasMouseOver(hasMouseOver);
        cachedNode.setHitBounds(absBounds);
        cachedNode.setIndex(cacheIndex);
        cachedNode.setIsFocus(isFocus);
        cachedNode.setIsInLayer(isInLayer);
        cachedNode.setIsTransparent(isTransparent);
        cachedNode.setIsUnclipped(isUnclipped);
        cachedNode.setOriginalAbsoluteBounds(originalAbsBounds);
        cachedNode.setParentIndex(last->mCachedNodeIndex);
        cachedNode.setParentGroup(ParentWithChildren(node));
        cachedNode.setSingleImage(imageCount == 1);
        cachedNode.setTabIndex(tabIndex);
        cachedNode.setType(type);
        if (type == TEXT_INPUT_CACHEDNODETYPE) {
            cachedFrame->add(cachedInput);
            cachedNode.setDataIndex(textInputIndex);
            textInputIndex++;
        } else
            cachedNode.setDataIndex(-1);
#if DUMP_NAV_CACHE
        cachedNode.mDebug.mNodeIndex = nodeIndex;
        cachedNode.mDebug.mParentGroupIndex = Debug::ParentIndex(
            node, nodeIndex, (Node*) cachedNode.parentGroup());
#endif
        cachedFrame->add(cachedNode);
        {
            int lastIndex = cachedFrame->size() - 1;
            if (node == focused) {
                CachedNode* cachedNodePtr = cachedFrame->getIndex(lastIndex);
                cachedRoot->setCachedFocus(cachedFrame, cachedNodePtr);
            }
            if (lastChild != NULL) {
                tracker.grow(tracker.size() + 1);
                FocusTracker& working = tracker.last();
                working.mCachedNodeIndex = lastIndex;
                working.mLastChild = OneAfter(lastChild);
                last = &tracker.at(tracker.size() - 2);
                working.mSomeParentTakesFocus = last->mSomeParentTakesFocus | takesFocus;
            } 
        }
        cacheIndex++;
    }
    while (tracker.size() > 1) {
        FocusTracker* last = &tracker.last();
        int lastChildIndex = cachedFrame->size() - 1;
        if (CleanUpContainedNodes(cachedRoot, cachedFrame, last, lastChildIndex))
            cacheIndex--;
        tracker.removeLast();
    }
}

bool CacheBuilder::CleanUpContainedNodes(CachedRoot* cachedRoot,
    CachedFrame* cachedFrame, const FocusTracker* last, int lastChildIndex)
{
    // if outer is body, disable outer
    // or if there's more than one inner, disable outer
    // or if inner is keyboard focusable, disable outer
    // else disable inner by removing it
    int childCount = lastChildIndex - last->mCachedNodeIndex;
    if (childCount == 0)
        return false;
    CachedNode* lastCached = cachedFrame->getIndex(last->mCachedNodeIndex);
    Node* lastNode = (Node*) lastCached->nodePointer();
    if ((childCount > 1 && lastNode->hasTagName(HTMLNames::selectTag) == false) ||
            lastNode->hasTagName(HTMLNames::bodyTag) ||
            lastNode->hasTagName(HTMLNames::formTag)) {
        lastCached->setBounds(IntRect(0, 0, 0, 0));
        lastCached->mCursorRing.clear();
        return false;
    }
    CachedNode* onlyChildCached = cachedFrame->lastNode();
    Node* onlyChild = (Node*) onlyChildCached->nodePointer();
    bool outerIsMouseMoveOnly = 
        lastNode->isKeyboardFocusable(NULL) == false && 
        lastNode->isMouseFocusable() == false &&
        lastNode->isFocusable() == false &&
        HasOverOrOut(lastNode) == true && 
        HasTriggerEvent(lastNode) == false;
    if (onlyChildCached->parent() == lastCached)
        onlyChildCached->setParentIndex(lastCached->parentIndex());
    bool hasFocus = lastCached->isFocus() || onlyChildCached->isFocus();
    if (outerIsMouseMoveOnly || onlyChild->isKeyboardFocusable(NULL)
            || onlyChildCached->isPlugin()) {
        int index = lastCached->index();
        *lastCached = *onlyChildCached;
        lastCached->setIndex(index);
        CachedFrame* frame = cachedFrame->hasFrame(lastCached);
        if (frame)
            frame->setIndexInParent(index);
    }
    cachedFrame->removeLast();
    if (hasFocus)
        cachedRoot->setCachedFocus(cachedFrame, cachedFrame->lastNode());
    return true;
}

Node* CacheBuilder::currentFocus() const
{
    Frame* frame = FrameAnd(this);
    Document* doc = frame->document();
    if (doc != NULL) {
        Node* focus = doc->focusedNode();
        if (focus != NULL)
            return focus;
    }
    Frame* child = frame->tree()->firstChild();
    while (child) {
        CacheBuilder* cacheBuilder = Builder(child);
        Node* focus = cacheBuilder->currentFocus();
        if (focus)
            return focus;
        child = child->tree()->nextSibling();
    }
    return NULL;
}

static bool strCharCmp(const char* matches, const UChar* test, int wordLength, 
    int wordCount)
{
    for (int index = 0; index < wordCount; index++) {
        for (int inner = 0; inner < wordLength; inner++) {
            if (matches[inner] != test[inner]) {
                matches += wordLength;
                goto next;
            }
        }
        return true;
next:
        ;
    }
    return false;
}

static const int stateTwoLetter[] = {
    0x02060c00,  // A followed by: [KLRSZ]
    0x00000000,  // B
    0x00084001,  // C followed by: [AOT]
    0x00000014,  // D followed by: [CE]
    0x00000000,  // E
    0x00001800,  // F followed by: [LM]
    0x00100001,  // G followed by: [AU]
    0x00000100,  // H followed by: [I]
    0x00002809,  // I followed by: [ADLN]
    0x00000000,  // J
    0x01040000,  // K followed by: [SY]
    0x00000001,  // L followed by: [A]
    0x000ce199,  // M followed by: [ADEHINOPST]
    0x0120129c,  // N followed by: [CDEHJMVY]
    0x00020480,  // O followed by: [HKR]
    0x00420001,  // P followed by: [ARW]
    0x00000000,  // Q
    0x00000100,  // R followed by: [I]
    0x0000000c,  // S followed by: [CD]
    0x00802000,  // T followed by: [NX]
    0x00080000,  // U followed by: [T]
    0x00080101,  // V followed by: [AIT]
    0x01200101   // W followed by: [AIVY]
};

static const char firstIndex[] = {
     0,  5,  5,  8, 10, 10, 12, 14, 
    15, 19, 19, 21, 22, 32, 40, 43, 
    46, 46, 47, 49, 51, 52, 55, 59  
};

// from http://infolab.stanford.edu/~manku/bitcount/bitcount.html
#define TWO(c)     (0x1u << (c))
#define MASK(c)    (((unsigned int)(-1)) / (TWO(TWO(c)) + 1u))
#define COUNT(x,c) ((x) & MASK(c)) + (((x) >> (TWO(c))) & MASK(c))
 
int bitcount (unsigned int n)
{
    n = COUNT(n, 0);
    n = COUNT(n, 1);
    n = COUNT(n, 2);
    n = COUNT(n, 3);
    return COUNT(n, 4);
}

#undef TWO
#undef MASK
#undef COUNT

static bool isUnicodeSpace(UChar ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == 0xa0;
}

static bool validZip(int stateIndex, const UChar* zipPtr) 
{
    static const struct {
        char mLow;
        char mHigh;
        char mException1;
        char mException2;
    } zipRange[] = { 
        { 99, 99, -1, -1 }, // AK Alaska
        { 35, 36, -1, -1 }, // AL Alabama
        { 71, 72, -1, -1 }, // AR Arkansas
        { 96, 96, -1, -1 }, // AS American Samoa
        { 85, 86, -1, -1 }, // AZ Arizona
        { 90, 96, -1, -1 }, // CA California
        { 80, 81, -1, -1 }, // CO Colorado
        {  6,  6, -1, -1 }, // CT Connecticut
        { 20, 20, -1, -1 }, // DC District of Columbia
        { 19, 19, -1, -1 }, // DE Delaware
        { 32, 34, -1, -1 }, // FL Florida
        { 96, 96, -1, -1 }, // FM Federated States of Micronesia
        { 30, 31, -1, -1 }, // GA Georgia
        { 96, 96, -1, -1 }, // GU Guam
        { 96, 96, -1, -1 }, // HI Hawaii
        { 50, 52, -1, -1 }, // IA Iowa
        { 83, 83, -1, -1 }, // ID Idaho
        { 60, 62, -1, -1 }, // IL Illinois
        { 46, 47, -1, -1 }, // IN Indiana
        { 66, 67, 73, -1 }, // KS Kansas
        { 40, 42, -1, -1 }, // KY Kentucky
        { 70, 71, -1, -1 }, // LA Louisiana
        {  1,  2, -1, -1 }, // MA Massachusetts
        { 20, 21, -1, -1 }, // MD Maryland
        {  3,  4, -1, -1 }, // ME Maine
        { 96, 96, -1, -1 }, // MH Marshall Islands
        { 48, 49, -1, -1 }, // MI Michigan
        { 55, 56, -1, -1 }, // MN Minnesota
        { 63, 65, -1, -1 }, // MO Missouri
        { 96, 96, -1, -1 }, // MP Northern Mariana Islands
        { 38, 39, -1, -1 }, // MS Mississippi
        { 55, 56, -1, -1 }, // MT Montana
        { 27, 28, -1, -1 }, // NC North Carolina
        { 58, 58, -1, -1 }, // ND North Dakota
        { 68, 69, -1, -1 }, // NE Nebraska
        {  3,  4, -1, -1 }, // NH New Hampshire
        {  7,  8, -1, -1 }, // NJ New Jersey
        { 87, 88, 86, -1 }, // NM New Mexico
        { 88, 89, 96, -1 }, // NV Nevada
        { 10, 14,  0,  6 }, // NY New York
        { 43, 45, -1, -1 }, // OH Ohio
        { 73, 74, -1, -1 }, // OK Oklahoma
        { 97, 97, -1, -1 }, // OR Oregon
        { 15, 19, -1, -1 }, // PA Pennsylvania
        {  6,  6,  0,  9 }, // PR Puerto Rico
        { 96, 96, -1, -1 }, // PW Palau
        {  2,  2, -1, -1 }, // RI Rhode Island
        { 29, 29, -1, -1 }, // SC South Carolina
        { 57, 57, -1, -1 }, // SD South Dakota
        { 37, 38, -1, -1 }, // TN Tennessee
        { 75, 79, 87, 88 }, // TX Texas
        { 84, 84, -1, -1 }, // UT Utah
        { 22, 24, 20, -1 }, // VA Virginia
        {  6,  9, -1, -1 }, // VI Virgin Islands
        {  5,  5, -1, -1 }, // VT Vermont
        { 98, 99, -1, -1 }, // WA Washington
        { 53, 54, -1, -1 }, // WI Wisconsin
        { 24, 26, -1, -1 }, // WV West Virginia
        { 82, 83, -1, -1 }  // WY Wyoming
    };

    int zip = zipPtr[0] - '0';
    zip *= 10;
    zip += zipPtr[1] - '0';
    int low = zipRange[stateIndex].mLow;
    int high = zipRange[stateIndex].mHigh;
    if (zip >= low && zip <= high)
        return true;
    if (zip == zipRange[stateIndex].mException1)
        return true;
    if (zip == zipRange[stateIndex].mException2)
        return true;
    return false;
}

#define MAX_PLACE_NAME_LENGTH 25 // the longest allowable one word place name

CacheBuilder::FoundState CacheBuilder::FindAddress(const UChar* chars,
    unsigned length, int* start, int* end, bool caseInsensitive)
{
    FindState addressState;
    FindReset(&addressState);
    addressState.mWords[0] = addressState.mStarts[0] = chars;
    addressState.mCaseInsensitive = caseInsensitive;
    FoundState state = FindPartialAddress(chars, chars, length, &addressState);
    if (state == FOUND_PARTIAL && addressState.mProgress == ZIP_CODE &&
            addressState.mNumberCount == 0) {
        addressState.mProgress = FIND_STREET;
        state = FindPartialAddress(NULL, NULL, 0, &addressState);
    }
    *start = addressState.mStartResult;
    *end = addressState.mEndResult;
    return state;
}

CacheBuilder::FoundState CacheBuilder::FindPartialAddress(const UChar* baseChars,
    const UChar* chars, unsigned length, FindState* s)
{
    // lower case letters are optional; trailing asterisk is optional 's'
    static char const* const longStreetNames[] = {
        "\x04" "LleY" "\x04" "NneX" "\x05" "RCade" "\x05" "VEnue" "\x06" "LAMEDA", // A
        "\x04" "aYoU" "\x04" "eaCH" "\x03" "eND" "\x05" "LuFf*" "\x05" "oTtoM"
            "\x08" "ouLeVarD" "\x05" "Ranch" "\x05" "RidGe" "\x05" "RooK*"
            "\x04" "urG*" "\x05" "YPass" "\x07" "roadWAY", // B
        "\x05" "AMINO"
        "\x03" "amP" "\x05" "anYoN" "\x03" "aPE" "\x07" "auSeWaY" "\x06" "enTeR*"
            "\x06" "IRcle*" "\x05" "LiFf*" "\x03" "LuB" "\x05" "oMmoN" "\x06" "ORner*"
            "\x05" "ouRSE" "\x05" "ourT*" "\x04" "oVe*" "\x04" "ReeK" "\x07" "REScent"
            "\x04" "ReST" "\x07" "ROSSING" "\x08" "ROSSROAD" "\x04" "URVe"
            "\x05" "AMINO" "\x06" "IRCULO" "\x07" "REScent", // C
        "\x03" "aLe" "\x02" "aM" "\x05" "iVide" "\x05" "Rive*", // D
        "\x06" "STate*" "\x09" "XPresswaY" "\x09" "XTension*", // E
        "\x04" "ALL*" "\x04" "eRrY" "\x05" "ieLD*" "\x04" "LaT*" "\x04" "oRD*"
            "\x05" "oReST" "\x05" "oRGe*" "\x04" "oRK*" "\x03" "orT" "\x06" "reeWaY", // F
        "\x06" "arDeN*" "\x06" "aTeWaY" "\x04" "LeN*" "\x05" "ReeN*" "\x05" "RoVe*", // G
        "\x06" "arBoR*" "\x04" "aVeN" "\x06" "eighTS" "\x06" "ighWaY" "\x04" "iLl*"
            "\x05" "OLloW", // H
        "\x04" "NLeT" "\x06" "Sland*" "\x03" "SLE", // I
        "\x08" "unCTion*", // J
        "\x03" "eY*" "\x05" "NoLl*", // K
        "\x04" "aKe*" "\x03" "AND" "\x06" "aNDinG" "\x03" "aNe" "\x05" "iGhT*"
            "\x03" "oaF" "\x04" "oCK*" "\x04" "oDGe" "\x03" "OOP", // L
        "\x03" "ALL" "\x05" "aNoR*" "\x06" "eaDoW*" "\x03" "EWS" "\x04" "iLl*"
            "\x06" "iSsioN" "\x07" "oTorWaY" "\x04" "ounT" "\x08" "ounTaiN*", // M
        "\x03" "eCK", // N
        "\x06" "RCHard" "\x03" "VAL" "\x07" "verPASs", // O
        "\x04" "ARK*" "\x07" "arKWaY*" "\x03" "ASS" "\x06" "aSsaGE" "\x03" "ATH"
            "\x03" "IKE" "\x04" "iNE*" "\x04" "Lace" "\x05" "LaiN*" "\x04" "LaZa"
            "\x05" "oinT*" "\x04" "oRT*" "\x06" "Rairie" "\x06" "RIVADA", // P
        NULL,
        "\x05" "ADiaL" "\x03" "AMP" "\x04" "aNCH" "\x05" "aPiD*"
            "\x03" "eST"
            "\x05" "iDGe*" "\x04" "IVer" "\x04" "oaD*" "\x04" "ouTE" "\x02" "OW"
            "\x02" "UE" "\x02" "UN", // R
        "\x05" "HoaL*" "\x05" "HoRe*" "\x05" "KyWaY" "\x06" "PrinG*" "\x04" "PUR*"
            "\x06" "Quare*" "\x06" "TAtion" "\x08" "TRAvenue" "\x05" "TReaM"
            "\x06" "Treet*" "\x05" "uMmiT" "\x07" "PeeDWaY", // S
        "\x06" "ERrace" "\x09" "hRoughWaY" "\x04" "RaCE" "\x04" "RAcK" "\x09" "RaFficwaY"
            "\x04" "RaiL" "\x05" "UNneL" "\x07" "urnPiKE", // T
        "\x08" "nderPASs" "\x05" "Nion*", // U
        "\x06" "aLleY*" "\x06" "IAduct" "\x04" "ieW*" "\x07" "iLlaGe*" "\x04" "iLle"
            "\x04" "ISta", // V
        "\x04" "ALK*" "\x03" "ALL" "\x03" "AY*" "\x04" "eLl*", // W
        "\x03" "ING" "\x02" "RD", // X
    };
    
    static char const* const longStateNames[] = {
        "\x8e" "la" "\x85" "bama" "\x02" "\x84" "ska" "\x01" "\x8f" "merican Samoa" "\x04"
             "\x91" "r" "\x86" "izona" "\x05" "\x87" "kansas" "\x03",
        NULL,
        "\x8b" "alifornia" "\x06" "\x95" "o" "\x87" "lorado" "\x07" "\x8a" "nnecticut" "\x08",
        "\x89" "elaware" "\x0a" "\x95" "istrict of Columbia" "\x09",
        NULL,
        "\x9f" "ederated States of Micronesia" "\x0c" "\x88" "lorida" "\x0b",
        "\x85" "uam" "\x0e" "\x88" "eorgia" "\x0d",
        "\x87" "awaii" "\x0f",
        "\x86" "daho" "\x11" "\x89" "llinois" "\x12" "\x88" "ndiana" "\x13" "\x85"
             "owa" "\x10",
        NULL,
        "\x87" "ansas" "\x14" "\x89" "entucky" "\x15",
        "\x8a" "ouisiana" "\x16",
        "\x86" "aine" "\x19" "\x99" "ar" "\x8e" "shall Islands" "\x1a" "\x86" "yland" "\x18"
             "\x8e" "assachusetts" "\x17" "\x93" "i" "\x87" "chigan" "\x1b"
             "\x88" "nnesota" "\x1c" "\x93" "iss" "\x88" "issippi" "\x1f" "\x85"
             "ouri" "\x1d" "\x88" "ontana" "\x20",
        "\x90" "e" "\x87" "braska" "\x23" "\x85" "vada" "\x27" "\xa5" "ew " "\x8a"
             "Hampshire" "\x24" "\x87" "Jersey" "\x25" "\x87" "Mexico" "\x26"
             "\x85" "York" "\x28" "\x98" "orth " "\x89" "Carolina" "\x21" "\x87"
             "Dakota" "\x22" "\x99" "orthern Mariana Islands" "\x1e",
        "\x85" "hio" "\x29" "\x89" "klahoma" "\x2a" "\x87" "regon" "\x2b",
        "\x86" "alau" "\x2e" "\x8d" "ennsylvania" "\x2c" "\x8c" "uerto Rico" "\x2d",
        NULL,
        "\x8d" "hode Island" "\x2f",
        "\x98" "outh " "\x89" "Carolina" "\x30" "\x87" "Dakota" "\x31",
        "\x90" "e" "\x88" "nnessee" "\x32" "\x84" "xas" "\x33",
        "\x85" "tah" "\x34",
        "\x88" "ermont" "\x37" "\x94" "irgin" "\x89" " Islands" "\x36" "\x83" "ia" "\x35",
        "\x8b" "ashington" "\x38" "\x8e" "est Virginia" "\x3a" "\x8a" "isconsin" "\x39"
             "\x88" "yoming" "\x3b"
    };
     
#if 0 // DEBUG_NAV_UI
    static char const* const progressNames[] = {
        "NO_ADDRESS",
        "SKIP_TO_SPACE",
        "HOUSE_NUMBER",
        "NUMBER_TRAILING_SPACE",
        "ADDRESS_LINE",
        "STATE_NAME",
        "SECOND_HALF",
        "ZIP_CODE",
        "PLUS_4",
        "FIND_STREET"
    };
#endif
    // strategy: US only support at first
    // look for a 1 - 5 digit number for a street number (no support for 'One Microsoft Way')
        // ignore if preceded by '#', Suite, Ste, Rm
    // look for two or more words (up to 5? North Frank Lloyd Wright Blvd)
            // note: "The Circle at North Hills St." has six words, and a lower 'at' -- allow at, by, of, in, the, and, ... ?
        // if a word starts with a lowercase letter, no match
        // allow: , . - # / (for 1/2) ' " 
        // don't look for street name type yet
    // look for one or two delimiters to represent possible 2nd addr line and city name
    // look for either full state name, or state two letters, and/or zip code (5 or 9 digits)
    // now look for street suffix, either in full or abbreviated form, with optional 's' if there's an asterisk 

    s->mCurrentStart = chars;
    s->mEnd = chars + length;
    int candIndex = 0;
    bool retryState;
    bool mustBeAllUpper = false;
    bool secondHalf = false;
    chars -= 1;
    UChar ch = s->mCurrent;
    while (++chars <= s->mEnd) {
        UChar prior = ch;
        ch = chars < s->mEnd ? *chars : ' ';
        switch (s->mProgress) {
            case NO_ADDRESS:
                if (WTF::isASCIIDigit(ch) == false) {
                    if (ch != 'O') // letter 'O', not zero
                        continue;
                    if (s->mEnd - chars < 3)
                        continue;
                    prior = *++chars;
                    ch = *++chars;
                    if ((prior != 'n' || ch != 'e') && (prior != 'N' || ch != 'E'))
                        continue;
                    if (isUnicodeSpace(*++chars) == false)
                        continue;
                    s->mProgress = ADDRESS_LINE;
                    s->mStartResult = chars - 3 - s->mCurrentStart;
                    continue;
                }
                if (isUnicodeSpace(prior) == false) {
                    s->mProgress = SKIP_TO_SPACE;
                    continue;
                }
                s->mNumberCount = 1;
                s->mProgress = HOUSE_NUMBER;
                s->mStartResult = chars - s->mCurrentStart;
                continue;
            case SKIP_TO_SPACE:
                if (isUnicodeSpace(ch) == false)
                    continue;
                break;
            case HOUSE_NUMBER:
                if (WTF::isASCIIDigit(ch)) {
                    if (++s->mNumberCount >= 6)
                        s->mProgress = SKIP_TO_SPACE;
                    continue;
                }
                if (WTF::isASCIIUpper(ch)) { // allow one letter after house number, e.g. 12A SKOLFIELD PL, HARPSWELL, ME 04079
                    if (WTF::isASCIIDigit(prior) == false)
                        s->mProgress = SKIP_TO_SPACE;
                    continue;
                }
                if (ch == '-') {
                    if (s->mNumberCount > 0) { // permit 21-23 ELM ST
                        ++s->mNumberCount;
                        continue;
                    }
                }
                s->mNumberCount = 0;
                s->mProgress = NUMBER_TRAILING_SPACE;
            case NUMBER_TRAILING_SPACE:
                if (isUnicodeSpace(ch))
                    continue;
                if (0 && WTF::isASCIIDigit(ch)) {
                    s->mNumberCount = 1;
                    s->mProgress = HOUSE_NUMBER;
                    s->mStartResult = chars - s->mCurrentStart;
                    continue;
                }
                if (WTF::isASCIIDigit(ch) == false && WTF::isASCIIUpper(ch) == false)
                    break;
                s->mProgress = ADDRESS_LINE;
            case ADDRESS_LINE:
                if (WTF::isASCIIAlpha(ch) || ch == '\'' || ch == '-' || ch == '&' || ch == '(' || ch == ')') {
                    if (++s->mLetterCount > 1) {
                        s->mNumberWords &= ~(1 << s->mWordCount);
                        continue;
                    }
                    if (s->mNumberCount >= 5)
                        break;
// FIXME: the test below was added to give up on a non-address, but it
// incorrectly discards addresses where the house number is in one node
// and the street name is in the next; I don't recall what the failing case
// is that suggested this fix.
//                    if (s->mWordCount == 0 && s->mContinuationNode)
//                        return FOUND_NONE;
                    s->newWord(baseChars, chars);
                    if (WTF::isASCIILower(ch) && s->mNumberCount == 0)
                        s->mFirstLower = chars;
                    s->mNumberCount = 0;
                    if (WTF::isASCIILower(ch) || (WTF::isASCIIAlpha(ch) == false && ch != '-'))
                        s->mNumberWords &= ~(1 << s->mWordCount);
                    s->mUnparsed = true;
                    continue;
                } else if (s->mLetterCount >= MAX_PLACE_NAME_LENGTH) {
                    break;
                } else if (s->mFirstLower != NULL) {
                    if (s->mCaseInsensitive)
                        goto resetWord;
                    size_t length = chars - s->mFirstLower;
                    if (length > 3)
                        break;
                    if (length == 3 && strCharCmp("and" "the", s->mFirstLower, 3, 2) == false)
                        break;
                    if (length == 2 && strCharCmp("at" "by" "el" "in" "of", s->mFirstLower, 2, 5) == false)
                        break;
                    goto resetWord;
                }
                if (ch == ',' || ch == '*') { // delimits lines
                    // asterisk as delimiter: http://www.sa.sc.edu/wellness/members.html
                    if (++s->mLineCount > 5)
                        break;
                    goto lookForState;
                }
                if (isUnicodeSpace(ch) || prior == '-') {
            lookForState:
                    if (s->mUnparsed == false)
                        continue;
                    const UChar* candidate = s->mWords[s->mWordCount];
                    UChar firstLetter = candidate[0];
                    if (WTF::isASCIIUpper(firstLetter) == false && WTF::isASCIIDigit(firstLetter) == false)
                        goto resetWord;
                    s->mWordCount++;
                    if ((s->mWordCount == 2 && s->mNumberWords == 3 && WTF::isASCIIDigit(s->mWords[1][1])) || // choose second of 8888 333 Main
                        (s->mWordCount >= sizeof(s->mWords) / sizeof(s->mWords[0]) - 1)) { // subtract 1 since state names may have two parts
                        // search for simple number already stored since first potential house # didn't pan out 
                        if (s->mNumberWords == 0)
                            break;
                        int shift = 0;
                        while ((s->mNumberWords & (1 << shift)) == 0)
                            shift++;
                        s->mNumberWords >>= ++shift;
                        if (s->mBases[0] != s->mBases[shift]) // if we're past the original node, bail
                            break;
                        s->shiftWords(shift);
                        s->mStartResult = s->mWords[0] - s->mStarts[0];
                        s->mWordCount -= shift;
                        // FIXME: need to adjust lineCount to account for discarded delimiters
                    }
                    if (s->mWordCount < 4) 
                        goto resetWord;
                    firstLetter -= 'A';
                    if (firstLetter > 'W' - 'A')
                        goto resetWord;
                    UChar secondLetter = candidate[1];
                    if (prior == '-')
                        s->mLetterCount--; // trim trailing dashes, to accept CA-94043
                    if (s->mLetterCount == 2) {
                        secondLetter -= 'A';
                        if (secondLetter > 'Z' - 'A')
                            goto resetWord;
                        if ((stateTwoLetter[firstLetter] & 1 << secondLetter) != 0) {
                            // special case to ignore 'et al'
                            if (strCharCmp("ET", s->mWords[s->mWordCount - 2], 2, 1) == false) {                   
                                s->mStateWord = s->mWordCount - 1;
                                s->mZipHint = firstIndex[firstLetter] + 
                                    bitcount(stateTwoLetter[firstLetter] & ((1 << secondLetter) - 1));
                                goto foundStateName;
                            }
                        }
                        goto resetWord;
                    } 
                    s->mStates = longStateNames[firstLetter];
                    if (s->mStates == NULL)
                        goto resetWord;
                    mustBeAllUpper = false;
                    s->mProgress = STATE_NAME;
                    unsigned char section = s->mStates[0];
                    ASSERT(section > 0x80);
                    s->mSectionLength = section & 0x7f;
                    candIndex = 1;
                    secondHalf = false;
                    s->mStateWord = s->mWordCount - 1;
                    goto stateName;
                }
                if (WTF::isASCIIDigit(ch)) {
                    if (s->mLetterCount == 0) {
                        if (++s->mNumberCount > 1)
                            continue;
                        if (s->mWordCount == 0 && s->mContinuationNode)
                            return FOUND_NONE;
                        s->newWord(baseChars, chars);
                        s->mNumberWords |= 1 << s->mWordCount;
                        s->mUnparsed = true;
                    }
                    continue;
                }
                if (ch == '.') { // optionally can follow letters
                    if (s->mLetterCount == 0)
                        break;
                    if (s->mNumberCount > 0)
                        break;
                    continue;
                }
                if (ch == '/') // between numbers (1/2) between words (12 Main / Ste 4d)
                    goto resetWord;
                if (ch == '#') // can precede numbers, allow it to appear randomly
                    goto resetWord;
                if (ch == '"') // sometimes parts of addresses are quoted (FIXME: cite an example here)
                    continue;
                break;
            case SECOND_HALF:
                if (WTF::isASCIIAlpha(ch)) {
                    if (s->mLetterCount == 0) {
                        s->newWord(baseChars, chars);
                        s->mWordCount++;
                    }
                    s->mLetterCount++;
                    continue;
                }
                if (WTF::isASCIIDigit(ch) == false) {
                    if (s->mLetterCount > 0) {
                        s->mProgress = STATE_NAME;
                        candIndex = 0;
                        secondHalf = true;
                        goto stateName;
                    }
                    continue;
                }
                s->mProgress = ADDRESS_LINE;
                goto resetState;
            case STATE_NAME:
            stateName:
                // pick up length of first section
                do {
                    int stateIndex = 1;
                    int skip = 0;
                    int prefix = 0;
                    bool subStr = false;
                    do {
                        unsigned char match = s->mStates[stateIndex];
                        if (match >= 0x80) {
                            if (stateIndex == s->mSectionLength)
                                break;
                            subStr = true;
                  //          if (skip > 0)
                  //              goto foundStateName;
                            prefix = candIndex;
                            skip = match & 0x7f;
                            match = s->mStates[++stateIndex];
                        }
                        UChar candChar = s->mWords[s->mWordCount - 1][candIndex];
                        if (mustBeAllUpper && WTF::isASCIILower(candChar))
                            goto skipToNext;
                        if (match != candChar) {
                            if (match != WTF::toASCIILower(candChar)) {
                       skipToNext:
                                if (subStr == false)
                                    break;
                                if (stateIndex == s->mSectionLength) {
                                    if (secondHalf) {
                                        s->mProgress = ADDRESS_LINE;
                                        goto resetState;
                                    }
                                    break;
                                }
                                stateIndex += skip;
                                skip = 0;
                                candIndex = prefix;
                                continue; // try next substring
                            }
                            mustBeAllUpper = true;
                        }
                        int nextindex = stateIndex + 1;
                        if (++candIndex >= s->mLetterCount && s->mStates[nextindex] == ' ') {
                            s->mProgress = SECOND_HALF;
                            s->mStates += nextindex;
                            s->mSectionLength -= nextindex;
                            goto resetWord;
                        }
                        if (nextindex + 1 == s->mSectionLength || skip == 2) {
                            s->mZipHint = s->mStates[nextindex] - 1;
                            goto foundStateName;
                        }
                        stateIndex += 1;
                        skip -= 1;
                    } while (true);
                    s->mStates += s->mSectionLength;
                    ASSERT(s->mStates[0] == 0 || (unsigned) s->mStates[0] > 0x80);
                    s->mSectionLength = s->mStates[0] & 0x7f;
                    candIndex = 1;
                    subStr = false;
                } while (s->mSectionLength != 0);
                s->mProgress = ADDRESS_LINE;
                goto resetState;
            foundStateName:
                s->mEndResult = chars - s->mCurrentStart;
                s->mEndWord = s->mWordCount - 1;
                s->mProgress = ZIP_CODE;
                // a couple of delimiters is an indication that the state name is good
                // or, a non-space / non-alpha-digit is also good
                s->mZipDelimiter = s->mLineCount > 2
                    || isUnicodeSpace(ch) == false
                    || chars == s->mEnd;
                if (WTF::isASCIIDigit(ch))
                    s->mZipStart = chars;
                goto resetState;
            case ZIP_CODE:
                if (WTF::isASCIIDigit(ch)) {
                    int count = ++s->mNumberCount;
                    if (count == 1) {
                        if (WTF::isASCIIDigit(prior))
                            ++s->mNumberCount;
                        else
                            s->mZipStart = chars;
                    }
                    if (count <= 9)
                        continue;
                } else if (isUnicodeSpace(ch)) {
                    if (s->mNumberCount == 0) {
                        s->mZipDelimiter = true; // two spaces delimit state name
                        continue;
                    }
                } else if (ch == '-') {
                    if (s->mNumberCount == 5 && validZip(s->mZipHint, s->mZipStart)) {
                        s->mNumberCount = 0;
                        s->mProgress = PLUS_4;
                        continue;
                    }
                    if (s->mNumberCount == 0)
                        s->mZipDelimiter = true;
                } else if (WTF::isASCIIAlpha(ch) == false)
                    s->mZipDelimiter = true;
                else {
                    if (s->mLetterCount == 0) {
                        s->newWord(baseChars, chars);
                        s->mUnparsed = true;
                    }
                    ++s->mLetterCount;
                }
                if (s->mNumberCount == 5 || s->mNumberCount == 9) {
                    if (validZip(s->mZipHint, s->mZipStart) == false)
                        goto noZipMatch;
                    s->mEndResult = chars - s->mCurrentStart;
                    s->mEndWord = s->mWordCount - 1;
                } else if (s->mZipDelimiter == false) {
            noZipMatch:
                    --chars;
                    s->mProgress = ADDRESS_LINE;
                    continue;
                }
                s->mProgress = FIND_STREET;
                goto findStreet;
            case PLUS_4:
                if (WTF::isASCIIDigit(ch)) {
                    if (++s->mNumberCount <= 4)
                        continue;
                }
                if (isUnicodeSpace(ch)) {
                    if (s->mNumberCount == 0)
                        continue;
                }
                if (s->mNumberCount == 4) {
                    if (WTF::isASCIIAlpha(ch) == false) {
                        s->mEndResult = chars - s->mCurrentStart;
                        s->mEndWord = s->mWordCount - 1;
                    }
                } else if (s->mNumberCount != 0)
                    break;
                s->mProgress = FIND_STREET;
            case FIND_STREET:
            findStreet:
                retryState = false;
                for (int wordsIndex = s->mStateWord - 1; wordsIndex >= 0; --wordsIndex) {
                    const UChar* test = s->mWords[wordsIndex];
                    UChar letter = test[0];
                    letter -= 'A';
                    if (letter > 'X' - 'A')
                        continue;
                    const char* names = longStreetNames[letter];
                    if (names == NULL)
                        continue;
                    int offset;
                    while ((offset = *names++) != 0) {
                        int testIndex = 1;
                        bool abbr = false;
                        for (int idx = 0; idx < offset; idx++) {
                            char nameLetter = names[idx];
                            char testUpper = WTF::toASCIIUpper(test[testIndex]);
                            if (nameLetter == '*') {
                                if (testUpper == 'S')
                                    testIndex++;
                                break;
                            }
                            bool fullOnly = WTF::isASCIILower(nameLetter);
                            nameLetter = WTF::toASCIIUpper(nameLetter);
                            if (testUpper == nameLetter) {
                                if (abbr && fullOnly)
                                    goto nextTest;
                                testIndex++;
                                continue;
                            }
                            if (fullOnly == false)
                                goto nextTest;
                            abbr = true;
                        }
                        letter = &test[testIndex] < s->mEnds[wordsIndex] ?
                            test[testIndex] : ' ';
                        if (WTF::isASCIIAlpha(letter) == false && WTF::isASCIIDigit(letter) == false) {
                            if (s->mNumberWords != 0) {
                                int shift = 0;
                                int wordReduction = -1;
                                do {
                                    while ((s->mNumberWords & (1 << shift)) == 0)
                                        shift++;
                                    if (shift > wordsIndex)
                                        break;
                                    wordReduction = shift;
                                } while (s->mNumberWords >> ++shift != 0);
                                if (wordReduction >= 0) {
                                    if (s->mContinuationNode) {
                                        if (retryState)
                                            break;
                                        return FOUND_NONE;
                                    }
                                    s->mStartResult = s->mWords[wordReduction] - s->mStarts[wordReduction];
                                }
                            }
                            if (wordsIndex != s->mStateWord - 1)
                                return FOUND_COMPLETE;
                            retryState = true;
                        }
                    nextTest:
                        names += offset;
                    }
                }
                if (retryState) {
                    s->mProgress = ADDRESS_LINE;
                    s->mStates = NULL;
                    continue;
                }
                if (s->mNumberWords != 0) {
                    unsigned shift = 0;
                    while ((s->mNumberWords & (1 << shift)) == 0)
                        shift++;
                    s->mNumberWords >>= ++shift;
                    if (s->mBases[0] != s->mBases[shift])
                        return FOUND_NONE;
                    s->shiftWords(shift);
                    s->mStartResult = s->mWords[0] - s->mStarts[0];
                    s->mWordCount -= shift;
                    s->mProgress = ADDRESS_LINE;
                    --chars;
                    continue;
                }
                break;
        }
        if (s->mContinuationNode)
            return FOUND_NONE;
        s->mProgress = NO_ADDRESS;
        s->mWordCount = s->mLineCount = 0;
        s->mNumberWords = 0;
    resetState:
        s->mStates = NULL;
    resetWord:
        s->mNumberCount = s->mLetterCount = 0;
        s->mFirstLower = NULL;
        s->mUnparsed = false;
    }
    s->mCurrent = ch;
    return s->mProgress == NO_ADDRESS ? FOUND_NONE : FOUND_PARTIAL;
}

// Recogize common email patterns only. Currently has lots of state, walks text forwards and backwards -- will be
// a real challenge to adapt to walk text across multiple nodes, I imagine
// FIXME: it's too hard for the caller to call these incrementally -- it's probably best for this to
// either walk the node tree directly or make a callout to get the next or previous node, if there is one
// walking directly will avoid adding logic in caller to track the multiple partial or full nodes that compose this 
// text pattern.
CacheBuilder::FoundState CacheBuilder::FindPartialEMail(const UChar* chars, unsigned length, 
    FindState* s)
{
    // the following tables were generated by tests/browser/focusNavigation/BrowserDebug.cpp
    // hand-edit at your own risk
    static const int domainTwoLetter[] = {
        0x02df797c,  // a followed by: [cdefgilmnoqrstuwxz]
        0x036e73fb,  // b followed by: [abdefghijmnorstvwyz]
        0x03b67ded,  // c followed by: [acdfghiklmnorsuvxyz]
        0x02005610,  // d followed by: [ejkmoz]
        0x001e00d4,  // e followed by: [ceghrstu]
        0x00025700,  // f followed by: [ijkmor]
        0x015fb9fb,  // g followed by: [abdefghilmnpqrstuwy]
        0x001a3400,  // h followed by: [kmnrtu]
        0x000f7818,  // i followed by: [delmnoqrst]
        0x0000d010,  // j followed by: [emop]
        0x0342b1d0,  // k followed by: [eghimnprwyz]
        0x013e0507,  // l followed by: [abcikrstuvy]
        0x03fffccd,  // m followed by: [acdghklmnopqrstuvwxyz]
        0x0212c975,  // n followed by: [acefgilopruz]
        0x00001000,  // o followed by: [m]
        0x014e3cf1,  // p followed by: [aefghklmnrstwy]
        0x00000001,  // q followed by: [a]
        0x00504010,  // r followed by: [eouw]
        0x032a7fdf,  // s followed by: [abcdeghijklmnortvyz]
        0x026afeec,  // t followed by: [cdfghjklmnoprtvwz]
        0x03041441,  // u followed by: [agkmsyz]
        0x00102155,  // v followed by: [aceginu]
        0x00040020,  // w followed by: [fs]
        0x00000000,  // x
        0x00180010,  // y followed by: [etu]
        0x00401001,  // z followed by: [amw]
    };

    static char const* const longDomainNames[] = {
        "\x03" "ero" "\x03" "rpa",  // aero, arpa  
        "\x02" "iz",  // biz  
        "\x02" "at" "\x02" "om" "\x03" "oop",  // cat, com, coop  
        NULL,  // d
        "\x02" "du",  // edu  
        NULL,  // f
        "\x02" "ov",  // gov  
        NULL,  // h
        "\x03" "nfo" "\x02" "nt",  // info, int  
        "\x03" "obs",  // jobs  
        NULL,  // k
        NULL,  // l
        "\x02" "il" "\x03" "obi" "\x05" "useum",  // mil, mobi, museum  
        "\x03" "ame" "\x02" "et",  // name, net  
        "\x02" "rg",  // , org  
        "\x02" "ro",  // pro  
        NULL,  // q  
        NULL,  // r
        NULL,  // s
        "\x05" "ravel",  // travel  
        NULL,  // u
        NULL,  // v
        NULL,  // w
        NULL,  // x
        NULL,  // y
        NULL,  // z
    };
    
    const UChar* start = chars;
    const UChar* end = chars + length;
    while (chars < end) {
        UChar ch = *chars++;
        if (ch != '@')
            continue;
        const UChar* atLocation = chars - 1;
        // search for domain
        ch = *chars++ | 0x20; // convert uppercase to lower
        if (ch < 'a' || ch > 'z')
            continue;
        while (chars < end) {
            ch = *chars++;
            if (IsDomainChar(ch) == false)
                goto nextAt;
            if (ch != '.')
                continue;
            UChar firstLetter = *chars++ | 0x20; // first letter of the domain
            if (chars >= end)
                return FOUND_NONE; // only one letter; must be at least two
            firstLetter -= 'a';
            if (firstLetter > 'z' - 'a')
                continue; // non-letter followed '.'
            int secondLetterMask = domainTwoLetter[firstLetter];
            ch = *chars | 0x20; // second letter of the domain
            ch -= 'a';
            if (ch >= 'z' - 'a')
                continue;
            bool secondMatch = (secondLetterMask & 1 << ch) != 0;
            const char* wordMatch = longDomainNames[firstLetter];
            int wordIndex = 0;
            while (wordMatch != NULL) {
                int len = *wordMatch++;
                char match;
                do {
                    match = wordMatch[wordIndex];
                    if (match < 0x20)
                        goto foundDomainStart;
                    if (chars[wordIndex] != match)
                        break;
                    wordIndex++;
                } while (true);
                wordMatch += len;
                if (*wordMatch == '\0')
                    break;
                wordIndex = 0;
            }
            if (secondMatch) {
                wordIndex = 1;
        foundDomainStart:
                chars += wordIndex;
                if (chars < end) {
                    ch = *chars;
                    if (ch != '.') {
                        if (IsDomainChar(ch))
                            goto nextDot;
                    } else if (chars + 1 < end && IsDomainChar(chars[1]))
                        goto nextDot;
                }
                // found domain. Search backwards from '@' for beginning of email address
                s->mEndResult = chars - start;
                chars = atLocation;
                if (chars <= start)
                    goto nextAt;
                ch = *--chars;
                if (ch == '.')
                    goto nextAt; // mailbox can't end in period
                do {
                    if (IsMailboxChar(ch) == false) {
                        chars++;
                        break;
                    }
                    if (chars == start)
                        break;
                    ch = *--chars;
                } while (true);
                UChar firstChar = *chars;
                if (firstChar == '.' || firstChar == '@') // mailbox can't start with period or be empty
                    goto nextAt;
                s->mStartResult = chars - start;
                return FOUND_COMPLETE;
            }
    nextDot:
            ;
        }
nextAt:
        chars = atLocation + 1;
    }
    return FOUND_NONE;
}

#define PHONE_PATTERN "(200) /-.\\ 100 -. 0000" // poor man's regex: parens optional, any one of punct, digit smallest allowed

CacheBuilder::FoundState CacheBuilder::FindPartialNumber(const UChar* chars, unsigned length, 
    FindState* s)
{
    char* pattern = s->mPattern;
    UChar* store = s->mStorePtr;
    const UChar* start = chars;
    const UChar* end = chars + length;
    const UChar* lastDigit = NULL;
    do {
        bool initialized = s->mInitialized;
        while (chars < end) {
            if (initialized == false) {
                s->mBackTwo = s->mBackOne;
                s->mBackOne = s->mCurrent;
            }
            UChar ch = s->mCurrent = *chars;
            do {
                char patternChar = *pattern;
                switch (patternChar) {
                    case '2':
                            if (initialized == false) {
                                s->mStartResult = chars - start;
                                initialized = true;
                            }
                    case '0':
                    case '1':
                        if (ch < patternChar || ch > '9')
                            goto resetPattern;
                        *store++ = ch;
                        pattern++;
                        lastDigit = chars;
                        goto nextChar;
                    case '\0':
                        if (WTF::isASCIIDigit(ch) == false) {
                            *store = '\0';
                            goto checkMatch;
                        }
                        goto resetPattern;
                    case ' ':
                        if (ch == patternChar)
                            goto nextChar;
                        break;
                    case '(':
                        if (ch == patternChar) {
                            s->mStartResult = chars - start;
                            initialized = true;
                            s->mOpenParen = true;
                        }
                        goto commonPunctuation;
                    case ')':
                        if ((ch == patternChar) ^ s->mOpenParen)
                            goto resetPattern;
                    default:
                    commonPunctuation:
                        if (ch == patternChar) {
                            pattern++;
                            goto nextChar;
                        }
                }
            } while (++pattern); // never false
    nextChar:
            chars++;
        }
        break;
resetPattern:
        if (s->mContinuationNode)
            return FOUND_NONE;
        FindResetNumber(s);
        pattern = s->mPattern;
        store = s->mStorePtr;
    } while (++chars < end);
checkMatch:
    if (WTF::isASCIIDigit(s->mBackOne != '1' ? s->mBackOne : s->mBackTwo))
        return FOUND_NONE;
    *store = '\0';
    s->mStorePtr = store;
    s->mPattern = pattern;
    s->mEndResult = lastDigit - start + 1;
    char pState = pattern[0];
    return pState == '\0' ? FOUND_COMPLETE : pState == '(' || (WTF::isASCIIDigit(pState) && WTF::isASCIIDigit(pattern[-1])) ? 
        FOUND_NONE : FOUND_PARTIAL;
}

CacheBuilder::FoundState CacheBuilder::FindPhoneNumber(const UChar* chars, unsigned length, 
    int* start, int* end)
{
    FindState state;
    FindReset(&state);
    FoundState result = FindPartialNumber(chars, length, &state);
    *start = state.mStartResult;
    *end = state.mEndResult;
    return result;
}

void CacheBuilder::FindReset(FindState* state)
{
    memset(state, 0, sizeof(FindState));
    state->mCurrent = ' ';
    FindResetNumber(state);
}

void CacheBuilder::FindResetNumber(FindState* state)
{
    state->mOpenParen = false;
    state->mPattern = (char*) PHONE_PATTERN;
    state->mStorePtr = state->mStore;
}

IntRect CacheBuilder::getAreaRect(const HTMLAreaElement* area)
{
    Node* node = area->document();
    while ((node = node->traverseNextNode()) != NULL) {
        RenderObject* renderer = node->renderer();
        if (renderer && renderer->isRenderImage()) {
            RenderImage* image = static_cast<RenderImage*>(renderer);
            HTMLMapElement* map = image->imageMap();
            if (map) {
                Node* n;
                for (n = map->firstChild(); n;
                        n = n->traverseNextNode(map)) {
                    if (n == area) {
                        if (area->isDefault())
                            return image->absoluteBoundingBoxRect();
                        return area->computeRect(image);
                    }
                }
            }
        }
    }
    return IntRect();
}

void CacheBuilder::GetGlobalOffset(Node* node, int* x, int * y) 
{ 
    GetGlobalOffset(node->document()->frame(), x, y); 
}

void CacheBuilder::GetGlobalOffset(Frame* frame, int* x, int* y)
{
//    TIMER_PROBE(__FUNCTION__);
    ASSERT(x);
    ASSERT(y);
    *x = 0;
    *y = 0;
    if (!frame->view())
        return;
    Frame* parent;
    while ((parent = frame->tree()->parent()) != NULL) {
        const WebCore::IntRect& rect = frame->view()->platformWidget()->getBounds();
        *x += rect.x();
        *y += rect.y();
        frame = parent;
    }
 //   TIMER_PROBE_END();
}

Frame* CacheBuilder::HasFrame(Node* node)
{
    RenderObject* renderer = node->renderer();
    if (renderer == NULL)
        return NULL;
    if (renderer->isWidget() == false)
        return NULL;
    Widget* widget = static_cast<RenderWidget*>(renderer)->widget();
    if (widget == NULL)
        return NULL;
    if (widget->isFrameView() == false)
        return NULL;
    return static_cast<FrameView*>(widget)->frame();
}

bool CacheBuilder::HasOverOrOut(Node* node)
{
    // eventNames are thread-local data, I avoid using 'static' variable here.
    AtomicString eventTypes[2] = {
        eventNames().mouseoverEvent,
        eventNames().mouseoutEvent
    };

    return NodeHasEventListeners(node, eventTypes, 2);
}

bool CacheBuilder::HasTriggerEvent(Node* node)
{
    AtomicString eventTypes[5] = {
        eventNames().clickEvent,
        eventNames().mousedownEvent,
        eventNames().mouseupEvent,
        eventNames().keydownEvent,
        eventNames().keyupEvent
    };

    return NodeHasEventListeners(node, eventTypes, 5);
}

// #define EMAIL_PATTERN "x@y.d" // where 'x' is letters, numbers, and '-', '.', '_' ; 'y' is 'x' without the underscore, and 'd' is a valid domain
//  - 0x2D . 0x2E 0-9 0x30-39 A-Z 0x41-5A  _ 0x5F a-z 0x61-7A 

bool CacheBuilder::IsDomainChar(UChar ch)
{
    static const unsigned body[] = {0x03ff6000, 0x07fffffe, 0x07fffffe}; // 0-9 . - A-Z a-z
    ch -= 0x20;
    if (ch > 'z' - 0x20)
        return false;
    return (body[ch >> 5] & 1 << (ch & 0x1f)) != 0;
}

bool CacheBuilder::isFocusableText(NodeWalk* walk, bool more, Node* node, 
    CachedNodeType* type, String* exported) const
{
    Text* textNode = static_cast<Text*>(node);
    StringImpl* string = textNode->dataImpl();
    const UChar* baseChars = string->characters();
//    const UChar* originalBase = baseChars;
    int length = string->length();
    int index = 0;
    while (index < length && isUnicodeSpace(baseChars[index]))
        index++;
    if (index >= length)
        return false;
    if (more == false) {
        walk->mStart = 0;
        walk->mEnd = 0;
        walk->mFinalNode = node;
        walk->mLastInline = NULL;
    }
    // starting with this node, search forward for email, phone number, and address
    // if any of the three is found, track it so that the remaining can be looked for later
    FoundState state = FOUND_NONE;
    RenderText* renderer = (RenderText*) node->renderer();
    bool foundBetter = false;
    InlineTextBox* baseInline = walk->mLastInline != NULL ? walk->mLastInline :
        renderer->firstTextBox();
    if (baseInline == NULL)
        return false;
    int start = walk->mEnd;
    InlineTextBox* saveInline;
    int baseStart, firstStart = start;
    saveInline = baseInline;
    baseStart = start;
    for (CachedNodeType checkType = ADDRESS_CACHEDNODETYPE;
        checkType <= PHONE_CACHEDNODETYPE; 
        checkType = static_cast<CachedNodeType>(checkType + 1))
    {
        if ((1 << (checkType - 1) & mAllowableTypes) == 0)
            continue;
        InlineTextBox* inlineTextBox = baseInline;
        FindState findState;
        FindReset(&findState);
        start = baseStart;
        if (checkType == ADDRESS_CACHEDNODETYPE) {
            findState.mBases[0] = baseChars;
            findState.mWords[0] = baseChars + start;
            findState.mStarts[0] = baseChars + start;
        }
        Node* lastPartialNode = NULL;
        int lastPartialEnd = -1;
        bool lastPartialMore = false;
        bool firstPartial = true;
        InlineTextBox* lastPartialInline = NULL;
        do {
            do {
                const UChar* chars = baseChars + start;
                length = inlineTextBox == NULL ? 0 :
                    inlineTextBox->end() - start + 1;
                bool wasInitialized = findState.mInitialized;
                switch (checkType) {
                    case ADDRESS_CACHEDNODETYPE:
                        state = FindPartialAddress(baseChars, chars, length, &findState);
                    break;
                    case EMAIL_CACHEDNODETYPE:
                        state = FindPartialEMail(chars, length, &findState);
                    break;
                    case PHONE_CACHEDNODETYPE:
                        state = FindPartialNumber(chars, length, &findState);
                    break;
                    default:
                        ASSERT(0);
                }
                findState.mInitialized = state != FOUND_NONE;
                if (wasInitialized != findState.mInitialized)
                    firstStart = start;
                if (state == FOUND_PARTIAL) {
                    lastPartialNode = node;
                    lastPartialEnd = findState.mEndResult + start;
                    lastPartialMore = firstPartial && 
                        lastPartialEnd < (int) string->length();
                    firstPartial = false;
                    lastPartialInline = inlineTextBox;
                    findState.mContinuationNode = true;
                } else if (state == FOUND_COMPLETE) {
                    if (foundBetter == false || walk->mStart > findState.mStartResult) {
                        walk->mStart = findState.mStartResult + firstStart; 
                        if (findState.mEndResult > 0) {
                            walk->mFinalNode = node;
                            walk->mEnd = findState.mEndResult + start;
                            walk->mMore = node == textNode &&
                                walk->mEnd < (int) string->length();
                            walk->mLastInline = inlineTextBox;
                        } else {
                            walk->mFinalNode = lastPartialNode;
                            walk->mEnd = lastPartialEnd;
                            walk->mMore = lastPartialMore;
                            walk->mLastInline = lastPartialInline;
                        }
                        *type = checkType;
                        if (checkType == PHONE_CACHEDNODETYPE) {
                            const UChar* store = findState.mStore;
                            *exported = String(store);
                        } else {
                            Node* temp = textNode;
                            length = 1;
                            start = walk->mStart;
                            exported->truncate(0);
                            do {
                                Text* tempText = static_cast<Text*>(temp);
                                StringImpl* string = tempText->dataImpl();
                                int end = tempText == walk->mFinalNode ? 
                                    walk->mEnd : string->length();
                                exported->append(String(string->substring(
                                    start, end - start)));
                                ASSERT(end > start);
                                length += end - start + 1;
                                if (temp == walk->mFinalNode)
                                    break;
                                start = 0;
                                do {
                                    temp = temp->traverseNextNode();
                                    ASSERT(temp);
                                } while (temp->isTextNode() == false);
                                // add a space in between text nodes to avoid 
                                // words collapsing together
                                exported->append(" ");
                            } while (true);
                        }
                        foundBetter = true;
                    }
                    goto tryNextCheckType;
                } else if (findState.mContinuationNode)
                    break;
                if (inlineTextBox == NULL)
                    break;
                inlineTextBox = inlineTextBox->nextTextBox();
                if (inlineTextBox == NULL)
                    break;
                start = inlineTextBox->start();
                if (state == FOUND_PARTIAL && node == textNode)
                    findState.mContinuationNode = false;
            } while (true);
            if (state == FOUND_NONE)
                break;
            // search for next text node, if any
            Text* nextNode;
            do {
                do {
                    do {
                        if (node)
                            node = node->traverseNextNode();
                        if (node == NULL || node->hasTagName(HTMLNames::aTag)
                                || node->hasTagName(HTMLNames::inputTag)
                                || node->hasTagName(HTMLNames::textareaTag)) {
                            if (state == FOUND_PARTIAL && 
                                    checkType == ADDRESS_CACHEDNODETYPE && 
                                    findState.mProgress == ZIP_CODE && 
                                    findState.mNumberCount == 0) {
                                baseChars = NULL;
                                inlineTextBox = NULL;
                                start = 0;
                                findState.mProgress = FIND_STREET;
                                goto finalNode;
                            }
                            goto tryNextCheckType;
                        }
                    } while (node->isTextNode() == false);
                    nextNode = static_cast<Text*>(node);
                    renderer = (RenderText*) nextNode->renderer();
                } while (renderer == NULL);
                baseInline = renderer->firstTextBox();
            } while (baseInline == NULL);
            string = nextNode->dataImpl();
            baseChars = string->characters();
            inlineTextBox = baseInline;
            start = inlineTextBox->start();
        finalNode:
            findState.mEndResult = 0;
        } while (true);
tryNextCheckType:
        node = textNode;
        baseInline = saveInline;
        string = textNode->dataImpl();
        baseChars = string->characters();
    }
    if (foundBetter) {
        CachedNodeType temp = *type;
        switch (temp) {
            case ADDRESS_CACHEDNODETYPE: {
                static const char geoString[] = "geo:0,0?q=";
                exported->insert(String(geoString), 0);
                int index = sizeof(geoString) - 1;
                String escapedComma("%2C");
                while ((index = exported->find(',', index)) >= 0)
                    exported->replace(index, 1, escapedComma);
                } break;
            case EMAIL_CACHEDNODETYPE: {
                String encoded = WebCore::encodeWithURLEscapeSequences(*exported);
                exported->swap(encoded);
                exported->insert(WTF::String("mailto:"), 0);
                } break;
            case PHONE_CACHEDNODETYPE:
                exported->insert(WTF::String("tel:"), 0);
                break;
            default:
                break;
        }
        return true;
    }
noTextMatch:
    walk->reset();
    return false;
}

bool CacheBuilder::IsMailboxChar(UChar ch)
{
    // According to http://en.wikipedia.org/wiki/Email_address
    // ! # $ % & ' * + - . / 0-9 = ?
    // A-Z ^ _
    // ` a-z { | } ~
    static const unsigned body[] = {0xa3ffecfa, 0xc7fffffe, 0x7fffffff};
    ch -= 0x20;
    if (ch > '~' - 0x20)
        return false;
    return (body[ch >> 5] & 1 << (ch & 0x1f)) != 0;
}

bool CacheBuilder::setData(CachedFrame* cachedFrame) 
{
    Frame* frame = FrameAnd(this);
    Document* doc = frame->document();
    if (doc == NULL)
        return false;
    RenderObject* renderer = doc->renderer();
    if (renderer == NULL)
        return false;
    RenderLayer* layer = renderer->enclosingLayer();
    if (layer == NULL)
        return false;
    if (!frame->view())
        return false;
    int x, y;
    GetGlobalOffset(frame, &x, &y);
    WebCore::IntRect viewBounds = frame->view()->platformWidget()->getBounds();
    if ((x | y) != 0)
        viewBounds.setLocation(WebCore::IntPoint(x, y));
    cachedFrame->setLocalViewBounds(viewBounds);
    cachedFrame->setContentsSize(layer->scrollWidth(), layer->scrollHeight());
    if (cachedFrame->childCount() == 0)
        return true;
    CachedFrame* lastCachedFrame = cachedFrame->lastChild();
    cachedFrame = cachedFrame->firstChild();
    do {
        CacheBuilder* cacheBuilder = Builder((Frame* )cachedFrame->framePointer());
        cacheBuilder->setData(cachedFrame);
    } while (cachedFrame++ != lastCachedFrame);
    return true;
}

#if USE(ACCELERATED_COMPOSITING)
void CacheBuilder::TrackLayer(WTF::Vector<LayerTracker>& layerTracker,
    RenderObject* nodeRenderer, Node* lastChild, int offsetX, int offsetY)
{
    RenderLayer* layer = nodeRenderer->enclosingLayer();
    RenderLayerBacking* back = layer->backing();
    if (!back)
        return;
    GraphicsLayer* grLayer = back->graphicsLayer();
    if (back->hasContentsLayer())
        grLayer = back->foregroundLayer();
    if (!grLayer)
        return;
    LayerAndroid* aLayer = grLayer->platformLayer();
    if (!aLayer)
        return;
    IntPoint scroll(layer->scrollXOffset(), layer->scrollYOffset());
#if ENABLE(ANDROID_OVERFLOW_SCROLL)
    // If this is an overflow element, track the content layer.
    if (layer->hasOverflowScroll() && aLayer->getChild(0))
        aLayer = aLayer->getChild(0)->getChild(0);
    if (!aLayer)
        return;
    // Prevent a crash when scrolling a layer that does not have a parent.
    if (layer->stackingContext())
        layer->scrollToOffset(0, 0);
#endif
    layerTracker.grow(layerTracker.size() + 1);
    LayerTracker& indexTracker = layerTracker.last();
    indexTracker.mLayer = aLayer;
    indexTracker.mRenderLayer = layer;
    indexTracker.mBounds = enclosingIntRect(aLayer->bounds());
    // Use the absolute location of the layer as the bounds location.  This
    // provides the original offset of nodes in the layer so that we can
    // translate nodes between their original location and the layer's new
    // location.
    indexTracker.mBounds.setLocation(layer->absoluteBoundingBox().location());
    indexTracker.mBounds.move(offsetX, offsetY);
    indexTracker.mScroll = scroll;
    indexTracker.mLastChild = OneAfter(lastChild);
    DBG_NAV_LOGD("layer=%p [%d] bounds=(%d,%d,w=%d,h=%d)", aLayer,
        aLayer->uniqueId(), indexTracker.mBounds.x(), indexTracker.mBounds.y(),
        indexTracker.mBounds.width(), indexTracker.mBounds.height());
}
#endif

bool CacheBuilder::validNode(Frame* startFrame, void* matchFrame,
        void* matchNode)
{
    if (matchFrame == startFrame) {
        if (matchNode == NULL)
            return true;
        Node* node = startFrame->document();
        while (node != NULL) {
            if (node == matchNode) {
                const IntRect& rect = node->hasTagName(HTMLNames::areaTag) ? 
                    getAreaRect(static_cast<HTMLAreaElement*>(node)) : node->getRect();
                // Consider nodes with empty rects that are not at the origin
                // to be valid, since news.google.com has valid nodes like this
                if (rect.x() == 0 && rect.y() == 0 && rect.isEmpty())
                    return false;
                return true;
            }
            node = node->traverseNextNode();
        }
        DBG_NAV_LOGD("frame=%p valid node=%p invalid\n", matchFrame, matchNode);
        return false;
    }
    Frame* child = startFrame->tree()->firstChild();
    while (child) {
        bool result = validNode(child, matchFrame, matchNode);
        if (result)
            return result;
        child = child->tree()->nextSibling();
    }
#if DEBUG_NAV_UI
    if (startFrame->tree()->parent() == NULL)
        DBG_NAV_LOGD("frame=%p node=%p false\n", matchFrame, matchNode);
#endif
    return false;
}

static int Area(const IntRect& rect)
{
    return rect.width() * rect.height();
}

bool CacheBuilder::AddPartRect(IntRect& bounds, int x, int y,
    WTF::Vector<IntRect>* result, IntRect* focusBounds)
{
    if (bounds.isEmpty())
        return true;
    bounds.move(x, y);
    if (bounds.maxX() <= 0 || bounds.maxY() <= 0)
        return true;
    IntRect* work = result->begin() - 1; 
    IntRect* end = result->end();
    while (++work < end) {
        if (work->contains(bounds))
            return true;
        if (bounds.contains(*work)) {
            *work = bounds;
            focusBounds->unite(bounds);
            return true;
        }
        if ((bounds.x() != work->x() || bounds.width() != work->width()) &&
               (bounds.y() != work->y() || bounds.height() != work->height()))
            continue;
        IntRect test = *work;
        test.unite(bounds);
        if (Area(test) > Area(*work) + Area(bounds))
            continue;
        *work = test;
        focusBounds->unite(bounds);
        return true;
    }
    if (result->size() >= MAXIMUM_FOCUS_RING_COUNT)
        return false;
    result->append(bounds);
    if (focusBounds->isEmpty())
        *focusBounds = bounds;
    else
        focusBounds->unite(bounds);
    return true;
}

bool CacheBuilder::ConstructPartRects(Node* node, const IntRect& bounds, 
    IntRect* focusBounds, int x, int y, WTF::Vector<IntRect>* result,
    int* imageCountPtr)
{
    WTF::Vector<ClipColumnTracker> clipTracker(1);
    ClipColumnTracker* baseTracker = clipTracker.data(); // sentinel
    bzero(baseTracker, sizeof(ClipColumnTracker));
    if (node->hasChildNodes() && node->hasTagName(HTMLNames::buttonTag) == false
            && node->hasTagName(HTMLNames::selectTag) == false) {
        // collect all text rects from first to last child
        Node* test = node->firstChild();
        Node* last = NULL;
        Node* prior = node;
        while ((prior = prior->lastChild()) != NULL)
            last = prior;
        ASSERT(last != NULL);
        bool nodeIsAnchor = node->hasTagName(HTMLNames::aTag);
        do {
            do {
                const ClipColumnTracker* lastClip = &clipTracker.last();
                if (test != lastClip->mLastChild)
                    break;
                clipTracker.removeLast();
            } while (true);
            RenderObject* renderer = test->renderer();
            if (renderer == NULL)
                continue;
            EVisibility vis = renderer->style()->visibility();
            if (vis == HIDDEN)
                continue;
            bool hasClip = renderer->hasOverflowClip();
            size_t clipIndex = clipTracker.size();
            IntRect clipBounds = IntRect(0, 0, INT_MAX, INT_MAX);
            if (hasClip || --clipIndex > 0) {
                clipBounds = hasClip ? renderer->absoluteBoundingBoxRect() :
                    clipTracker.at(clipIndex).mBounds; // x, y fixup done by ConstructTextRect
            }
            if (test->isTextNode()) {
                RenderText* renderText = (RenderText*) renderer;
                InlineTextBox *textBox = renderText->firstTextBox();
                if (textBox == NULL)
                    continue;
                if (ConstructTextRect((Text*) test, textBox, 0, INT_MAX, 
                        x, y, focusBounds, clipBounds, result) == false) {
                    return false;
                }
                continue;
            }
            if (test->hasTagName(HTMLNames::imgTag)) {
                IntRect bounds = test->getRect();
                bounds.intersect(clipBounds);
                if (AddPartRect(bounds, x, y, result, focusBounds) == false)
                    return false;
                *imageCountPtr += 1;
                continue;
            } 
            if (hasClip == false) {
                if (nodeIsAnchor && test->hasTagName(HTMLNames::divTag)) {
                    IntRect bounds = renderer->absoluteBoundingBoxRect();  // x, y fixup done by AddPartRect
                    RenderBox* renderBox = static_cast<RenderBox*>(renderer);
                    int left = bounds.x() + renderBox->paddingLeft() + renderBox->borderLeft();
                    int top = bounds.y() + renderBox->paddingTop() + renderBox->borderTop();
                    int right = bounds.maxX() - renderBox->paddingRight() - renderBox->borderRight();
                    int bottom = bounds.maxY() - renderBox->paddingBottom() - renderBox->borderBottom();
                    if (left >= right || top >= bottom)
                        continue;
                    bounds = IntRect(left, top, right - left, bottom - top);
                    if (AddPartRect(bounds, x, y, result, focusBounds) == false)
                        return false;
                }
                continue;
            }
            Node* lastChild = test->lastChild();
            if (lastChild == NULL)
                continue;
            clipTracker.grow(clipTracker.size() + 1);
            ClipColumnTracker& clip = clipTracker.last();
            clip.mBounds = renderer->absoluteBoundingBoxRect(); // x, y fixup done by ConstructTextRect
            clip.mLastChild = OneAfter(lastChild);
            clip.mNode = test;
        } while (test != last && (test = test->traverseNextNode()) != NULL);
    }
    if (result->size() == 0 || focusBounds->width() < MINIMUM_FOCUSABLE_WIDTH
            || focusBounds->height() < MINIMUM_FOCUSABLE_HEIGHT) {
        if (bounds.width() < MINIMUM_FOCUSABLE_WIDTH)
            return false;
        if (bounds.height() < MINIMUM_FOCUSABLE_HEIGHT)
            return false;
        result->append(bounds);
        *focusBounds = bounds;
    }
    return true;
}

static inline bool isNotSpace(UChar c)
{
    return c <= 0xA0 ? isUnicodeSpace(c) == false : 
        WTF::Unicode::direction(c) != WTF::Unicode::WhiteSpaceNeutral;
}

bool CacheBuilder::ConstructTextRect(Text* textNode,
    InlineTextBox* textBox, int start, int relEnd, int x, int y, 
    IntRect* focusBounds, const IntRect& clipBounds, WTF::Vector<IntRect>* result)
{
    RenderText* renderText = (RenderText*) textNode->renderer();
    EVisibility vis = renderText->style()->visibility();
    StringImpl* string = textNode->dataImpl();
    const UChar* chars = string->characters();
    FloatPoint pt = renderText->localToAbsolute();
    do {
        int textBoxStart = textBox->start();
        int textBoxEnd = textBoxStart + textBox->len();
        if (textBoxEnd <= start)
            continue;
        if (textBoxEnd > relEnd)
            textBoxEnd = relEnd;
        IntRect bounds = textBox->selectionRect((int) pt.x(), (int) pt.y(), 
            start, textBoxEnd);
        bounds.intersect(clipBounds);
        if (bounds.isEmpty())
            continue;
        bool drawable = false;
        for (int index = start; index < textBoxEnd; index++)
            if ((drawable |= isNotSpace(chars[index])) != false)
                break;
        if (drawable && vis != HIDDEN) {
            if (AddPartRect(bounds, x, y, result, focusBounds) == false)
                return false;
        }
        if (textBoxEnd == relEnd)
            break;
    } while ((textBox = textBox->nextTextBox()) != NULL);
    return true;
}

bool CacheBuilder::ConstructTextRects(Text* node, int start, 
    Text* last, int end, int x, int y, IntRect* focusBounds, 
    const IntRect& clipBounds, WTF::Vector<IntRect>* result)
{
    result->clear();
    *focusBounds = IntRect(0, 0, 0, 0);
    do {
        RenderText* renderText = (RenderText*) node->renderer();
        int relEnd = node == last ? end : renderText->textLength();
        InlineTextBox *textBox = renderText->firstTextBox();
        if (textBox != NULL) {
            do {
                if ((int) textBox->end() >= start)
                    break;
            } while ((textBox = textBox->nextTextBox()) != NULL);
            if (textBox && ConstructTextRect(node, textBox, start, relEnd,
                    x, y, focusBounds, clipBounds, result) == false)
                return false;
        }
        start = 0;
        do {
            if (node == last)
                return true;
            node = (Text*) node->traverseNextNode();
            ASSERT(node != NULL);
        } while (node->isTextNode() == false || node->renderer() == NULL);
    } while (true);
}

}
