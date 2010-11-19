/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef XMLDocumentParser_h
#define XMLDocumentParser_h

#if USE(EXPAT)

#include "CachedResourceClient.h"
#include "SegmentedString.h"
#include "StringHash.h"
#include "Tokenizer.h"
#include <libexpat/expat.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

    class Node;
    class CachedScript;
    class DocumentFragment;
    class Document;
    class Element;
    class FrameView;
    class PendingCallbacks;

    class XMLTokenizer : public Tokenizer, public CachedResourceClient {
    public:
        XMLTokenizer(Document *, FrameView * = 0);
        XMLTokenizer(DocumentFragment *, Element *);
        ~XMLTokenizer();
    
        enum ErrorType { warning, nonFatal, fatal };
    
        // from Tokenizer
        virtual bool write(const SegmentedString &str, bool);
        virtual void finish();
        virtual bool isWaitingForScripts() const;
        virtual void stopParsing();
        virtual bool wellFormed() const { return !m_sawError; }
        virtual int lineNumber() const;
        virtual int columnNumber() const;

        // from CachedObjectClient
        virtual void notifyFinished(CachedResource *finishedObj);
    
        // callbacks from parser expat
        void startElementNs(const XML_Char *name, const XML_Char **atts);
        void endElementNs();
        void characters(const XML_Char *s, int len);
        void processingInstruction(const XML_Char *target, const XML_Char *data);
        void comment(const XML_Char *s);
        void startCdata();
        void endCdata();
        
        void error(ErrorType type, const char* m, int lineNumber, int columnNumber);
        
        // utilities
        XML_Parser getXMLParser() const { return m_parser; }
        void setXMLParser(XML_Parser parser) { m_parser = parser; }
    
    private:
        void setCurrentNode(Node*);
        
        void end();

        void pauseParsing();
        void resumeParsing();

        void reportError();
        void insertErrorMessageBlock();
        
        bool enterText();
        void exitText();
        
        Document *m_doc;
        FrameView *m_view;
        
        XML_Parser m_parser;
        
        Node *m_currentNode;
        bool m_currentNodeIsReferenced;
        
        bool m_sawError;
        bool m_sawXSLTransform;
        bool m_sawFirstElement;
        
        bool m_parserPaused;
        bool m_requestingScript;
        bool m_finishCalled;
                
        int m_errorCount;                
        String m_errorMessages;
        
        CachedScript *m_pendingScript;
        RefPtr<Element> m_scriptElement;
        int m_scriptStartLine;
        
        bool m_parsingFragment;
        String m_defaultNamespaceURI;
        
        typedef HashMap<String, String> PrefixForNamespaceMap;
        PrefixForNamespaceMap m_prefixToNamespaceMap;
        
        OwnPtr<PendingCallbacks> m_pendingCallbacks;
        SegmentedString m_pendingSrc;
    };

HashMap<String, String> parseAttributes(const String&, bool& attrsOK);
bool parseXMLDocumentFragment(const String&, DocumentFragment*, Element* parent = 0);

} // namespace WebCore

#else   // USE(EXPAT)

#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "FragmentScriptingPermission.h"
#include "ScriptableDocumentParser.h"
#include "SegmentedString.h"
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/text/StringHash.h>

#if USE(QXMLSTREAM)
#include <qxmlstream.h>
#else
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#endif

namespace WebCore {

    class Node;
    class CachedScript;
    class CachedResourceLoader;
    class DocumentFragment;
    class Document;
    class Element;
    class FrameView;
    class PendingCallbacks;
    class ScriptElement;

#if !USE(QXMLSTREAM)
    class XMLParserContext : public RefCounted<XMLParserContext> {
    public:
        static PassRefPtr<XMLParserContext> createMemoryParser(xmlSAXHandlerPtr, void*, const char*);
        static PassRefPtr<XMLParserContext> createStringParser(xmlSAXHandlerPtr, void*);
        ~XMLParserContext();
        xmlParserCtxtPtr context() const { return m_context; }

    private:
        XMLParserContext(xmlParserCtxtPtr context)
            : m_context(context)
        {
        }
        xmlParserCtxtPtr m_context;
    };
#endif

    class XMLDocumentParser : public ScriptableDocumentParser, public CachedResourceClient {
    public:
        static PassRefPtr<XMLDocumentParser> create(Document* document, FrameView* view)
        {
            return adoptRef(new XMLDocumentParser(document, view));
        }
        static PassRefPtr<XMLDocumentParser> create(DocumentFragment* fragment, Element* element, FragmentScriptingPermission permission)
        {
            return adoptRef(new XMLDocumentParser(fragment, element, permission));
        }

        ~XMLDocumentParser();

        // Exposed for callbacks:
        enum ErrorType { warning, nonFatal, fatal };
        void handleError(ErrorType, const char* message, int lineNumber, int columnNumber);

        void setIsXHTMLDocument(bool isXHTML) { m_isXHTMLDocument = isXHTML; }
        bool isXHTMLDocument() const { return m_isXHTMLDocument; }
#if ENABLE(XHTMLMP)
        void setIsXHTMLMPDocument(bool isXHTML) { m_isXHTMLMPDocument = isXHTML; }
        bool isXHTMLMPDocument() const { return m_isXHTMLMPDocument; }
#endif
#if ENABLE(WML)
        bool isWMLDocument() const;
#endif

        static bool parseDocumentFragment(const String&, DocumentFragment*, Element* parent = 0, FragmentScriptingPermission = FragmentScriptingAllowed);

        // WMLErrorHandling uses these functions.
        virtual bool wellFormed() const { return !m_sawError; }
        virtual int lineNumber() const;
        TextPosition0 textPosition() const;

        static bool supportsXMLVersion(const String&);

    private:
        XMLDocumentParser(Document*, FrameView* = 0);
        XMLDocumentParser(DocumentFragment*, Element*, FragmentScriptingPermission);

        // From DocumentParser
        virtual void insert(const SegmentedString&);
        virtual void append(const SegmentedString&);
        virtual void finish();
        virtual bool finishWasCalled();
        virtual bool isWaitingForScripts() const;
        virtual void stopParsing();
        virtual void detach();

        // from CachedResourceClient
        virtual void notifyFinished(CachedResource*);

        void end();

        void pauseParsing();
        void resumeParsing();

        bool appendFragmentSource(const String&);

        int columnNumber() const;

        // This method is introduced to temporary legalize existing line/column
        // coordinate bug: it is believed that numbers that originally were zero-based
        // eventually becomes one-based.
        // FIXME: Investigate and get rid of this method.
        TextPosition1 textPositionOneBased() const;

#if USE(QXMLSTREAM)
private:
        void parse();
        void startDocument();
        void parseStartElement();
        void parseEndElement();
        void parseCharacters();
        void parseProcessingInstruction();
        void parseCdata();
        void parseComment();
        void endDocument();
        void parseDtd();
        bool hasError() const;
#else
public:
        // callbacks from parser SAX
        void error(ErrorType, const char* message, va_list args) WTF_ATTRIBUTE_PRINTF(3, 0);
        void startElementNs(const xmlChar* xmlLocalName, const xmlChar* xmlPrefix, const xmlChar* xmlURI, int nb_namespaces,
                            const xmlChar** namespaces, int nb_attributes, int nb_defaulted, const xmlChar** libxmlAttributes);
        void endElementNs();
        void characters(const xmlChar* s, int len);
        void processingInstruction(const xmlChar* target, const xmlChar* data);
        void cdataBlock(const xmlChar* s, int len);
        void comment(const xmlChar* s);
        void startDocument(const xmlChar* version, const xmlChar* encoding, int standalone);
        void internalSubset(const xmlChar* name, const xmlChar* externalID, const xmlChar* systemID);
        void endDocument();
#endif
    private:
        void initializeParserContext(const char* chunk = 0);

        void pushCurrentNode(Node*);
        void popCurrentNode();
        void clearCurrentNodeStack();

        void insertErrorMessageBlock();

        void enterText();
        void exitText();

        void doWrite(const String&);
        void doEnd();

        FrameView* m_view;

        String m_originalSourceForTransform;

#if USE(QXMLSTREAM)
        QXmlStreamReader m_stream;
        bool m_wroteText;
#else
        xmlParserCtxtPtr context() const { return m_context ? m_context->context() : 0; };
        RefPtr<XMLParserContext> m_context;
        OwnPtr<PendingCallbacks> m_pendingCallbacks;
        Vector<xmlChar> m_bufferedText;
#endif
        Node* m_currentNode;
        Vector<Node*> m_currentNodeStack;

        bool m_sawError;
        bool m_sawXSLTransform;
        bool m_sawFirstElement;
        bool m_isXHTMLDocument;
#if ENABLE(XHTMLMP)
        bool m_isXHTMLMPDocument;
        bool m_hasDocTypeDeclaration;
#endif

        bool m_parserPaused;
        bool m_requestingScript;
        bool m_finishCalled;

        int m_errorCount;
        int m_lastErrorLine;
        int m_lastErrorColumn;
        String m_errorMessages;

        CachedResourceHandle<CachedScript> m_pendingScript;
        RefPtr<Element> m_scriptElement;
        TextPosition1 m_scriptStartPosition;

        bool m_parsingFragment;
        AtomicString m_defaultNamespaceURI;

        typedef HashMap<AtomicString, AtomicString> PrefixForNamespaceMap;
        PrefixForNamespaceMap m_prefixToNamespaceMap;
        SegmentedString m_pendingSrc;
        FragmentScriptingPermission m_scriptingPermission;
    };

#if ENABLE(XSLT)
void* xmlDocPtrForString(CachedResourceLoader*, const String& source, const String& url);
#endif

HashMap<String, String> parseAttributes(const String&, bool& attrsOK);

} // namespace WebCore

#endif // USE(EXPAT)

#endif // XMLDocumentParser_h
