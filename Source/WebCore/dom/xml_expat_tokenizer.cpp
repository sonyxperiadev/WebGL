/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2007 The Android Open Source Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "XMLTokenizer.h"

#include "CDATASection.h"
#include "CachedScript.h"
#include "Comment.h"
#include "CString.h"
#include "DocLoader.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#include "HTMLTableSectionElement.h"
#include "HTMLTokenizer.h"
#include "ProcessingInstruction.h"
#include "EventNames.h"

// strndup is not available everywhere, so here is a portable version <reed>
static char* portable_strndup(const char src[], size_t len)
{
    char* origDst = (char*)malloc(len + 1);
    if (NULL == origDst)
        return NULL;

    char* dst = origDst;
    while (len-- > 0) {
        if ((*dst++ = *src++) == 0)
            return origDst;
    }
    *dst = 0;
    return origDst;
}

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

const int maxErrors = 25;

class PendingCallbacks {
public:
    PendingCallbacks() {
        m_callbacks.setAutoDelete(true);
    }
    
    void appendStartElementNSCallback(const XML_Char* name, const XML_Char** atts) {
        PendingStartElementNSCallback* callback = new PendingStartElementNSCallback;
        
        callback->name = strdup(name);
        callback->count = 0;
        while (atts[callback->count])
            callback->count++;
        callback->atts = (XML_Char**)malloc(sizeof(XML_Char*) * (callback->count+1));
        for (int i=0; i<callback->count; i++)
            callback->atts[i] = strdup(atts[i]);
        callback->atts[callback->count] = NULL;              
        
        m_callbacks.append(callback);
    }

    void appendEndElementNSCallback() {
        PendingEndElementNSCallback* callback = new PendingEndElementNSCallback;
        
        m_callbacks.append(callback);
    }
    
    void appendCharactersCallback(const XML_Char* s, int len) {
        PendingCharactersCallback* callback = new PendingCharactersCallback;
        
        callback->s = portable_strndup(s, len);
        callback->len = len;
        
        m_callbacks.append(callback);        
    }
    
    void appendProcessingInstructionCallback(const XML_Char* target, const XML_Char* data) {
        PendingProcessingInstructionCallback* callback = new PendingProcessingInstructionCallback;
        
        callback->target = strdup(target);
        callback->data = strdup(data);
        
        m_callbacks.append(callback);
    }
    
    void appendStartCDATABlockCallback() {
        PendingStartCDATABlockCallback* callback = new PendingStartCDATABlockCallback;
        
        m_callbacks.append(callback);        
    }
    
    void appendEndCDATABlockCallback() {
        PendingEndCDATABlockCallback* callback = new PendingEndCDATABlockCallback;
        
        m_callbacks.append(callback);        
    }

    void appendCommentCallback(const XML_Char* s) {
        PendingCommentCallback* callback = new PendingCommentCallback;
        
        callback->s = strdup(s);
        
        m_callbacks.append(callback);        
    }
    
    void appendErrorCallback(XMLTokenizer::ErrorType type, const char* message, int lineNumber, int columnNumber) {
        PendingErrorCallback* callback = new PendingErrorCallback;
        
        callback->message = strdup(message);
        callback->type = type;
        callback->lineNumber = lineNumber;
        callback->columnNumber = columnNumber;
        
        m_callbacks.append(callback);
    }

    void callAndRemoveFirstCallback(XMLTokenizer* tokenizer) {
        PendingCallback* cb = m_callbacks.getFirst();
            
        cb->call(tokenizer);
        m_callbacks.removeFirst();
    }
    
    bool isEmpty() const { return m_callbacks.isEmpty(); }
    
private:    
    struct PendingCallback {        
        
        virtual ~PendingCallback() { } 

        virtual void call(XMLTokenizer* tokenizer) = 0;
    };  
    
    struct PendingStartElementNSCallback : public PendingCallback {        
        virtual ~PendingStartElementNSCallback() {
            free(name);
            for (int i=0; i<count; i++)
                free(atts[i]);
            free(atts);
        }
        
        virtual void call(XMLTokenizer* tokenizer) {
            tokenizer->startElementNs(name, (const XML_Char**)(atts));
        }

        XML_Char* name;
        int count;
        XML_Char** atts;
    };
    
    struct PendingEndElementNSCallback : public PendingCallback {
        virtual void call(XMLTokenizer* tokenizer) {
            tokenizer->endElementNs();
        }
    };
    
    struct PendingCharactersCallback : public PendingCallback {
        virtual ~PendingCharactersCallback() {
            free(s);
        }
    
        virtual void call(XMLTokenizer* tokenizer) {
            tokenizer->characters(s, len);
        }
        
        XML_Char* s;
        int len;
    };

    struct PendingProcessingInstructionCallback : public PendingCallback {
        virtual ~PendingProcessingInstructionCallback() {
            free(target);
            free(data);
        }
        
        virtual void call(XMLTokenizer* tokenizer) {
            tokenizer->processingInstruction(target, data);
        }
        
        XML_Char* target;
        XML_Char* data;
    };
    
    struct PendingStartCDATABlockCallback : public PendingCallback {
        virtual void call(XMLTokenizer* tokenizer) {
            tokenizer->startCdata();
        }
    };

    struct PendingEndCDATABlockCallback : public PendingCallback {
        virtual void call(XMLTokenizer* tokenizer) {
            tokenizer->endCdata();
        }
    };
    
    struct PendingCommentCallback : public PendingCallback {
        virtual ~PendingCommentCallback() {
            free(s);
        }
        
        virtual void call(XMLTokenizer* tokenizer) {
            tokenizer->comment(s);
        }

        XML_Char* s;
    };
    
    struct PendingErrorCallback: public PendingCallback {
        virtual ~PendingErrorCallback() {
            free (message);
        }
        
        virtual void call(XMLTokenizer* tokenizer) {
            tokenizer->error(type, message, lineNumber, columnNumber);
        }
        
        XMLTokenizer::ErrorType type;
        char* message;
        int lineNumber;
        int columnNumber;
    };
    
public:
    DeprecatedPtrList<PendingCallback> m_callbacks;
};

// --------------------------------

XMLTokenizer::XMLTokenizer(Document *_doc, FrameView *_view)
    : m_doc(_doc)
    , m_view(_view)
    , m_parser(0)
    , m_currentNode(_doc)
    , m_currentNodeIsReferenced(false)
    , m_sawError(false)
    , m_sawXSLTransform(false)
    , m_sawFirstElement(false)
    , m_parserPaused(false)
    , m_requestingScript(false)
    , m_finishCalled(false)
    , m_errorCount(0)
    , m_pendingScript(0)
    , m_scriptStartLine(0)
    , m_parsingFragment(false)
    , m_pendingCallbacks(new PendingCallbacks)
{
}

XMLTokenizer::XMLTokenizer(DocumentFragment *fragment, Element *parentElement)
    : m_doc(fragment->document())
    , m_view(0)
    , m_parser(0)
    , m_currentNode(fragment)
    , m_currentNodeIsReferenced(fragment)
    , m_sawError(false)
    , m_sawXSLTransform(false)
    , m_sawFirstElement(false)
    , m_parserPaused(false)
    , m_requestingScript(false)
    , m_finishCalled(false)
    , m_errorCount(0)
    , m_pendingScript(0)
    , m_scriptStartLine(0)
    , m_parsingFragment(true)
    , m_pendingCallbacks(new PendingCallbacks)
{
    if (fragment)
        fragment->ref();
    if (m_doc)
        m_doc->ref();
          
    // Add namespaces based on the parent node
    Vector<Element*> elemStack;
    while (parentElement) {
        elemStack.append(parentElement);
        
        Node* n = parentElement->parentNode();
        if (!n || !n->isElementNode())
            break;
        parentElement = static_cast<Element*>(n);
    }
    
    if (elemStack.isEmpty())
        return;
    
    for (Element* element = elemStack.last(); !elemStack.isEmpty(); elemStack.removeLast()) {
        if (NamedAttrMap* attrs = element->attributes()) {
            for (unsigned i = 0; i < attrs->length(); i++) {
                Attribute* attr = attrs->attributeItem(i);
                if (attr->localName() == "xmlns")
                    m_defaultNamespaceURI = attr->value();
                else if (attr->prefix() == "xmlns")
                    m_prefixToNamespaceMap.set(attr->localName(), attr->value());
            }
        }
    }
}

XMLTokenizer::~XMLTokenizer()
{
    setCurrentNode(0);
    if (m_parsingFragment && m_doc)
        m_doc->deref();
    if (m_pendingScript)
        m_pendingScript->deref(this);
}

void XMLTokenizer::setCurrentNode(Node* n)
{
    bool nodeNeedsReference = n && n != m_doc;
    if (nodeNeedsReference)
        n->ref(); 
    if (m_currentNodeIsReferenced) 
        m_currentNode->deref(); 
    m_currentNode = n;
    m_currentNodeIsReferenced = nodeNeedsReference;
}

// use space instead of ':' as separator because ':' can be inside an uri
const XML_Char tripletSep=' ';

inline DeprecatedString toQString(const XML_Char* str, unsigned int len)
{
    return DeprecatedString::fromUtf8(reinterpret_cast<const char *>(str), len);
}

inline DeprecatedString toQString(const XML_Char* str)
{
    return DeprecatedString::fromUtf8(str ? reinterpret_cast<const char *>(str) : "");
}

// triplet is formatted as URI + sep + local_name + sep + prefix.
static inline void splitTriplet(const XML_Char *name, String &uri, String &localname, String &prefix)
{
    String string[3];
    int found = 0;
    const char *start = reinterpret_cast<const char *>(name);    
    
    while(start && (found < 3)) {
        char *next = strchr(start, tripletSep);
        if (next) {
            string[found++] = toQString(start, (next-start));
            start = next+1;
        } else {
            string[found++] = toQString(start);
            break;
        }
    }
    
    switch(found) {
    case 1:
        localname = string[0];
        break;
    case 2:
        uri = string[0];
        localname = string[1];
        break;
    case 3:
        uri = string[0];
        localname = string[1];
        prefix = string[2];
        break;
    }
}

static inline void handleElementNamespaces(Element *newElement, const String &uri, const String &prefix, ExceptionCode &exceptioncode)
{
    if (uri.isEmpty())
        return;
        
    String namespaceQName("xmlns");
    if(!prefix.isEmpty())
        namespaceQName += String(":")+ prefix;
    newElement->setAttributeNS(String("http://www.w3.org/2000/xmlns/"), namespaceQName, uri, exceptioncode);
}
    
static inline void handleElementAttributes(Element *newElement, const XML_Char **atts, ExceptionCode &exceptioncode)
{
    for (int i = 0; atts[i]; i += 2) {
        String attrURI, attrLocalName, attrPrefix;
        splitTriplet(atts[i], attrURI, attrLocalName, attrPrefix);
        String attrQName = attrPrefix.isEmpty() ? attrLocalName : attrPrefix + String(":") + attrLocalName;
        String attrValue = toQString(atts[i+1]);      
        newElement->setAttributeNS(attrURI, attrQName, attrValue, exceptioncode);
        if (exceptioncode) // exception while setting attributes
            return;
    }
}

void XMLTokenizer::startElementNs(const XML_Char *name, const XML_Char **atts)
{
    if (m_parserStopped)
        return;
    
    if (m_parserPaused) {        
        m_pendingCallbacks->appendStartElementNSCallback(name, atts);
        return;
    }
    
    m_sawFirstElement = true;

    exitText();

    String uri, localName, prefix;    
    splitTriplet(name, uri, localName, prefix);
    String qName = prefix.isEmpty() ? localName : prefix + ":" + localName;
    
    if (m_parsingFragment && uri.isEmpty()) {
        if (!prefix.isEmpty())
            uri = String(m_prefixToNamespaceMap.get(prefix.impl()));
        else
            uri = m_defaultNamespaceURI;
    }

    ExceptionCode ec = 0;
    RefPtr<Element> newElement = m_doc->createElementNS(uri, qName, ec);
    if (!newElement) {
        stopParsing();
        return;
    }
    
    handleElementNamespaces(newElement.get(), uri, prefix, ec);
    if (ec) {
        stopParsing();
        return;
    }
    
    handleElementAttributes(newElement.get(), atts, ec);
    if (ec) {
        stopParsing();
        return;
    }

    if (newElement->hasTagName(scriptTag))
        static_cast<HTMLScriptElement*>(newElement.get())->setCreatedByParser(true);
    
    if (newElement->hasTagName(HTMLNames::scriptTag))
        m_scriptStartLine = lineNumber();
    
    if (!m_currentNode->addChild(newElement.get())) {
        stopParsing();
        return;
    }
    
    setCurrentNode(newElement.get());
    if (m_view && !newElement->attached())
        newElement->attach();
}

void XMLTokenizer::endElementNs()
{
    if (m_parserStopped)
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendEndElementNSCallback();
        return;
    }
    
    exitText();

    Node* n = m_currentNode;
    RefPtr<Node> parent = n->parentNode();
    n->finishedParsing();
    
    // don't load external scripts for standalone documents (for now)
    if (n->isElementNode() && m_view && static_cast<Element*>(n)->hasTagName(scriptTag)) {
        ASSERT(!m_pendingScript);
        
        m_requestingScript = true;
        
        Element* scriptElement = static_cast<Element*>(n);        
        String scriptHref;
        
        if (static_cast<Element*>(n)->hasTagName(scriptTag))
            scriptHref = scriptElement->getAttribute(srcAttr);
        
        if (!scriptHref.isEmpty()) {
            // we have a src attribute 
            const AtomicString& charset = scriptElement->getAttribute(charsetAttr);
            if ((m_pendingScript = m_doc->docLoader()->requestScript(scriptHref, charset))) {
                m_scriptElement = scriptElement;
                m_pendingScript->ref(this);
                    
                // m_pendingScript will be 0 if script was already loaded and ref() executed it
                if (m_pendingScript)
                    pauseParsing();
            } else 
                m_scriptElement = 0;

        } else {
            String scriptCode = "";
            for (Node* child = scriptElement->firstChild(); child; child = child->nextSibling()) {
                if (child->isTextNode() || child->nodeType() == Node::CDATA_SECTION_NODE)
                    scriptCode += static_cast<CharacterData*>(child)->data();
            }
            m_view->frame()->loader()->executeScript(m_doc->URL(), m_scriptStartLine - 1, scriptCode);
        }
        
        m_requestingScript = false;
    }

    setCurrentNode(parent.get());
}

void XMLTokenizer::characters(const XML_Char *s, int len)
{
    if (m_parserStopped)
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendCharactersCallback(s, len);
        return;
    }
        
    if (m_currentNode->isTextNode() || enterText()) {
        ExceptionCode ec = 0;
        static_cast<Text*>(m_currentNode)->appendData(toQString(s, len), ec);
    }
}

bool XMLTokenizer::enterText()
{
    RefPtr<Node> newNode = new Text(m_doc, "");
    if (!m_currentNode->addChild(newNode.get()))
        return false;
    setCurrentNode(newNode.get());
    return true;
}

void XMLTokenizer::exitText()
{
    if (m_parserStopped)
        return;

    if (!m_currentNode || !m_currentNode->isTextNode())
        return;

    if (m_view && m_currentNode && !m_currentNode->attached())
        m_currentNode->attach();

    // FIXME: What's the right thing to do if the parent is really 0?
    // Just leaving the current node set to the text node doesn't make much sense.
    if (Node* par = m_currentNode->parentNode())
        setCurrentNode(par);
}

void XMLTokenizer::processingInstruction(const XML_Char *target, const XML_Char *data)
{
    if (m_parserStopped)
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendProcessingInstructionCallback(target, data);
        return;
    }
    
    exitText();

    // ### handle exceptions
    int exception = 0;
    RefPtr<ProcessingInstruction> pi = m_doc->createProcessingInstruction(
        toQString(target), toQString(data), exception);
    if (exception)
        return;

    if (!m_currentNode->addChild(pi.get()))
        return;
    if (m_view && !pi->attached())
        pi->attach();

    // don't load stylesheets for standalone documents
    if (m_doc->frame()) {
        m_sawXSLTransform = !m_sawFirstElement && !pi->checkStyleSheet();
        if (m_sawXSLTransform)
            stopParsing();
    }
}

void XMLTokenizer::comment(const XML_Char *s)
{
    if (m_parserStopped)
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendCommentCallback(s);
        return;
    }
    
    exitText();

    RefPtr<Node> newNode = m_doc->createComment(toQString(s));
    m_currentNode->addChild(newNode.get());
    if (m_view && !newNode->attached())
        newNode->attach();
}

void XMLTokenizer::startCdata()
{
    if (m_parserStopped)
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendStartCDATABlockCallback();
        return;
    }

    exitText();

    RefPtr<Node> newNode = new CDATASection(m_doc, "");
    if (!m_currentNode->addChild(newNode.get()))
        return;
    if (m_view && !newNode->attached())
        newNode->attach();
    setCurrentNode(newNode.get());
}

void XMLTokenizer::endCdata()
{
    if (m_parserStopped)
        return;
    
    if (m_parserPaused) {
        m_pendingCallbacks->appendEndCDATABlockCallback();
        return;
    }

    if (m_currentNode->parentNode() != 0)
        setCurrentNode(m_currentNode->parentNode());
}

static void XMLCALL startElementHandler(void *userdata, const XML_Char *name, const XML_Char **atts)
{
    XMLTokenizer *tokenizer = static_cast<XMLTokenizer *>(userdata);
    tokenizer->startElementNs(name, atts);
}

static void XMLCALL endElementHandler(void *userdata, const XML_Char *name)
{
    XMLTokenizer *tokenizer = static_cast<XMLTokenizer *>(userdata);
    tokenizer->endElementNs();
}

static void charactersHandler(void *userdata, const XML_Char *s, int len)
{
    XMLTokenizer *tokenizer = static_cast<XMLTokenizer *>(userdata);
    tokenizer->characters(s, len);
}

static void processingInstructionHandler(void *userdata, const XML_Char *target, const XML_Char *data)
{
    XMLTokenizer *tokenizer = static_cast<XMLTokenizer *>(userdata);
    tokenizer->processingInstruction(target, data);
}

static void commentHandler(void *userdata, const XML_Char *comment)
{
    XMLTokenizer *tokenizer = static_cast<XMLTokenizer *>(userdata);
    tokenizer->comment(comment);
}

static void startCdataHandler(void *userdata)
{
    XMLTokenizer *tokenizer = static_cast<XMLTokenizer *>(userdata);
    tokenizer->startCdata();
}

static void endCdataHandler(void *userdata)
{
    XMLTokenizer *tokenizer = static_cast<XMLTokenizer *>(userdata);
    tokenizer->endCdata();
}

static int unknownEncodingHandler(void *userdata, const XML_Char *name, XML_Encoding *info)
{
    // Expat doesn't like latin1 so we have to build this map
    // to do conversion correctly.
    // FIXME: Create a wrapper for expat that looks like libxml.
    if (strcasecmp(name, "latin1") == 0)
    {
        for (int i=0; i<256; i++) {
            info->map[i] = i;
        }
        return XML_STATUS_OK;
    }
    return XML_STATUS_ERROR;
}

bool XMLTokenizer::write(const SegmentedString&s, bool /*appendData*/ )
{
    String parseString = s.toString();

    if (m_parserStopped || m_sawXSLTransform)
        return false;

    if (m_parserPaused) {
        m_pendingSrc.append(s);
        return false;
    }
        
    if (!m_parser) {
        static const UChar BOM = 0xFEFF;
        static const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char*>(&BOM);
        m_parser = XML_ParserCreateNS(BOMHighByte == 0xFF ? "UTF-16LE" : "UTF-16BE", tripletSep);
        XML_SetUserData(m_parser, (void *)this);
        XML_SetReturnNSTriplet(m_parser, true);
    
        XML_SetStartElementHandler(m_parser, startElementHandler);
        XML_SetEndElementHandler(m_parser, endElementHandler);
        XML_SetCharacterDataHandler(m_parser, charactersHandler);
        XML_SetProcessingInstructionHandler(m_parser, processingInstructionHandler);
        XML_SetCommentHandler(m_parser, commentHandler);
        XML_SetStartCdataSectionHandler(m_parser, startCdataHandler);
        XML_SetEndCdataSectionHandler(m_parser, endCdataHandler);
        XML_SetUnknownEncodingHandler(m_parser, unknownEncodingHandler, NULL);
    }
    
    enum XML_Status result = XML_Parse(m_parser, (const char*)parseString.characters(), sizeof(UChar) * parseString.length(), false);
    if (result == XML_STATUS_ERROR) {
        reportError();
        return false;
    }
    
    return true;
}

void XMLTokenizer::end()
{
    if (m_parser) {
        XML_Parse(m_parser, 0, 0, true);
        XML_ParserFree(m_parser);
        m_parser = 0;
    }
    
    if (m_sawError)
        insertErrorMessageBlock();
    else {
        exitText();
        m_doc->updateStyleSelector();
    }
    
    setCurrentNode(0);
    m_doc->finishedParsing();    
}

void XMLTokenizer::finish()
{
    if (m_parserPaused)
        m_finishCalled = true;
    else
        end();
}

void XMLTokenizer::reportError()
{
    ErrorType type = nonFatal;
    enum XML_Error code = XML_GetErrorCode(m_parser);
    switch (code) {
        case XML_ERROR_NO_MEMORY:
            type = fatal;
            break;
        case XML_ERROR_FINISHED:
            type = warning;
            break;
        default:
            type = nonFatal;            
    }    
    error(type, XML_ErrorString(code), lineNumber(), columnNumber());
}

void XMLTokenizer::error(ErrorType type, const char* m, int lineNumber, int columnNumber)
{
    if (type == fatal || m_errorCount < maxErrors) {
        switch (type) {
            case warning:
                m_errorMessages += String::format("warning on line %d at column %d: %s", lineNumber, columnNumber, m);
                break;
            case fatal:
            case nonFatal:
                m_errorMessages += String::format("error on line %d at column %d: %s", lineNumber, columnNumber, m);
        }
        ++m_errorCount;
    }
    
    if (type != warning)
        m_sawError = true;
    
    if (type == fatal)
        stopParsing();   
}

static inline RefPtr<Element> createXHTMLParserErrorHeader(Document* doc, const String& errorMessages) 
{
    ExceptionCode ec = 0;
    RefPtr<Element> reportElement = doc->createElementNS(xhtmlNamespaceURI, "parsererror", ec);
    reportElement->setAttribute(styleAttr, "display:block; pre; border: 2px solid #c77; padding: 0 1em 0 1em; margin: 1em; background-color: #fdd; color: black");
    
    RefPtr<Element> h3 = doc->createElementNS(xhtmlNamespaceURI, "h3", ec);
    reportElement->appendChild(h3.get(), ec);
    h3->appendChild(doc->createTextNode("This page contains the following errors:"), ec);
    
    RefPtr<Element> fixed = doc->createElementNS(xhtmlNamespaceURI, "div", ec);
    reportElement->appendChild(fixed.get(), ec);
    fixed->setAttribute(styleAttr, "font-family:monospace;font-size:12px");
    fixed->appendChild(doc->createTextNode(errorMessages), ec);
    
    h3 = doc->createElementNS(xhtmlNamespaceURI, "h3", ec);
    reportElement->appendChild(h3.get(), ec);
    h3->appendChild(doc->createTextNode("Below is a rendering of the page up to the first error."), ec);
    
    return reportElement;
}

void XMLTokenizer::insertErrorMessageBlock()
{
    // One or more errors occurred during parsing of the code. Display an error block to the user above
    // the normal content (the DOM tree is created manually and includes line/col info regarding 
    // where the errors are located)

    // Create elements for display
    ExceptionCode ec = 0;
    Document* doc = m_doc;
    Node* documentElement = doc->documentElement();
    if (!documentElement) {
        RefPtr<Node> rootElement = doc->createElementNS(xhtmlNamespaceURI, "html", ec);
        doc->appendChild(rootElement, ec);
        RefPtr<Node> body = doc->createElementNS(xhtmlNamespaceURI, "body", ec);
        rootElement->appendChild(body, ec);
        documentElement = body.get();
    }

    RefPtr<Element> reportElement = createXHTMLParserErrorHeader(doc, m_errorMessages);
    documentElement->insertBefore(reportElement, documentElement->firstChild(), ec);
    doc->updateRendering();
}

void XMLTokenizer::notifyFinished(CachedResource *finishedObj)
{
    ASSERT(m_pendingScript == finishedObj);
        
    String cachedScriptUrl = m_pendingScript->url();
    String scriptSource = m_pendingScript->script();
    bool errorOccurred = m_pendingScript->errorOccurred();
    m_pendingScript->deref(this);
    m_pendingScript = 0;
    
    RefPtr<Element> e = m_scriptElement;
    m_scriptElement = 0;
    
    if (errorOccurred) 
        EventTargetNodeCast(e.get())->dispatchHTMLEvent(errorEvent, true, false);
    else {
        m_view->frame()->loader()->executeScript(cachedScriptUrl, 0, scriptSource);
        EventTargetNodeCast(e.get())->dispatchHTMLEvent(loadEvent, false, false);
    }
    
    m_scriptElement = 0;
    
    if (!m_requestingScript)
        resumeParsing();
}

bool XMLTokenizer::isWaitingForScripts() const
{
    return m_pendingScript != 0;
}

Tokenizer *newXMLTokenizer(Document *d, FrameView *v)
{
    return new XMLTokenizer(d, v);
}

int XMLTokenizer::lineNumber() const
{
    return XML_GetCurrentLineNumber(m_parser);
}

int XMLTokenizer::columnNumber() const
{
    return XML_GetCurrentColumnNumber(m_parser);
}

void XMLTokenizer::stopParsing()
{
    Tokenizer::stopParsing();
    if (m_parser)
        XML_StopParser(m_parser, 0);
}

void XMLTokenizer::pauseParsing()
{
    if (m_parsingFragment)
        return;
    
    m_parserPaused = true;
}

void XMLTokenizer::resumeParsing()
{
    ASSERT(m_parserPaused);
    
    m_parserPaused = false;
    
    // First, execute any pending callbacks
    while (!m_pendingCallbacks->isEmpty()) {
        m_pendingCallbacks->callAndRemoveFirstCallback(this);
        
        // A callback paused the parser
        if (m_parserPaused)
            return;
    }
    
    // Then, write any pending data
    SegmentedString rest = m_pendingSrc;
    m_pendingSrc.clear();
    write(rest, false);

    // Finally, if finish() has been called and write() didn't result
    // in any further callbacks being queued, call end()
    if (m_finishCalled && m_pendingCallbacks->isEmpty())
        end();
}

// --------------------------------

bool parseXMLDocumentFragment(const String &string, DocumentFragment *fragment, Element *parent)
{   
    XMLTokenizer tokenizer(fragment, parent);
    
    XML_Parser parser = XML_ParserCreateNS(NULL, tripletSep);
    tokenizer.setXMLParser(parser);
    
    XML_SetUserData(parser, (void *)&tokenizer);
    XML_SetReturnNSTriplet(parser, true);

    XML_SetStartElementHandler(parser, startElementHandler);
    XML_SetEndElementHandler(parser, endElementHandler);
    XML_SetCharacterDataHandler(parser, charactersHandler);
    XML_SetProcessingInstructionHandler(parser, processingInstructionHandler);
    XML_SetCommentHandler(parser, commentHandler);
    XML_SetStartCdataSectionHandler(parser, startCdataHandler);
    XML_SetEndCdataSectionHandler(parser, endCdataHandler);

    CString cString = string.utf8();
    int result = XML_Parse(parser, cString.data(), cString.length(), true);

    XML_ParserFree(parser);
    tokenizer.setXMLParser(0);

    return result != XML_STATUS_ERROR;
}

// --------------------------------

struct AttributeParseState {
    HashMap<String, String> attributes;
    bool gotAttributes;
};

static void attributesStartElementHandler(void *userData, const XML_Char *name, const XML_Char **atts)
{
    if (strcmp(name, "attrs") != 0)
        return;
        
    if (atts[0] == 0 )
        return;
    
    AttributeParseState *state = static_cast<AttributeParseState *>(userData);    
    state->gotAttributes = true;
    
    for (int i = 0; atts[i]; i += 2) {
        DeprecatedString attrName = toQString(atts[i]);
        DeprecatedString attrValue = toQString(atts[i+1]);
        state->attributes.set(attrName, attrValue);        
    }
}

HashMap<String, String> parseAttributes(const String& string, bool& attrsOK)
{
    AttributeParseState state;
    state.gotAttributes = false;

    XML_Parser parser = XML_ParserCreateNS(NULL, tripletSep);
    XML_SetUserData(parser, (void *)&state);
    XML_SetReturnNSTriplet(parser, true);

    XML_SetStartElementHandler(parser, attributesStartElementHandler);
    String input = "<?xml version=\"1.0\"?><attrs " + string.deprecatedString() + " />";
    CString cString = input.deprecatedString().utf8();
    if ( XML_Parse(parser, cString.data(), cString.length(), true) != XML_STATUS_ERROR )
        attrsOK = state.gotAttributes;
    XML_ParserFree(parser);

    return state.attributes;
}

}
