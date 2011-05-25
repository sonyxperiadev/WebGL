/*
 * Copyright 2010, The Android Open Source Project
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

#define LOG_TAG "webarchive"

#include "config.h"
#include "WebArchiveAndroid.h"

#if ENABLE(WEB_ARCHIVE)

#include "Base64.h"
#include <libxml/encoding.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include <libxml/xmlwriter.h>
#include <wtf/text/CString.h>

namespace WebCore {

static const xmlChar* const archiveTag = BAD_CAST "Archive";
static const xmlChar* const archiveResourceTag = BAD_CAST "ArchiveResource";
static const xmlChar* const mainResourceTag = BAD_CAST "mainResource";
static const xmlChar* const subresourcesTag = BAD_CAST "subresources";
static const xmlChar* const subframesTag = BAD_CAST "subframes";
static const xmlChar* const urlFieldTag = BAD_CAST "url";
static const xmlChar* const mimeFieldTag = BAD_CAST "mimeType";
static const xmlChar* const encodingFieldTag = BAD_CAST "textEncoding";
static const xmlChar* const frameFieldTag = BAD_CAST "frameName";
static const xmlChar* const dataFieldTag = BAD_CAST "data";

PassRefPtr<WebArchiveAndroid> WebArchiveAndroid::create(PassRefPtr<ArchiveResource> mainResource,
        Vector<PassRefPtr<ArchiveResource> >& subresources,
        Vector<PassRefPtr<Archive> >& subframeArchives)
{
    if (mainResource)
        return adoptRef(new WebArchiveAndroid(mainResource, subresources, subframeArchives));
    return 0;
}

PassRefPtr<WebArchiveAndroid> WebArchiveAndroid::create(Frame* frame)
{
    PassRefPtr<ArchiveResource> mainResource = frame->loader()->documentLoader()->mainResource();
    Vector<PassRefPtr<ArchiveResource> > subresources;
    Vector<PassRefPtr<Archive> > subframes;
    int children = frame->tree()->childCount();

    frame->loader()->documentLoader()->getSubresources(subresources);

    for (int child = 0; child < children; child++)
        subframes.append(create(frame->tree()->child(child)));

    return create(mainResource, subresources, subframes);
}

WebArchiveAndroid::WebArchiveAndroid(PassRefPtr<ArchiveResource> mainResource,
        Vector<PassRefPtr<ArchiveResource> >& subresources,
        Vector<PassRefPtr<Archive> >& subframeArchives)
{
    setMainResource(mainResource);

    for (Vector<PassRefPtr<ArchiveResource> >::iterator subresourcesIterator = subresources.begin();
         subresourcesIterator != subresources.end();
         subresourcesIterator++) {
        addSubresource(*subresourcesIterator);
    }

    for (Vector<PassRefPtr<Archive> >::iterator subframesIterator = subframeArchives.begin();
         subframesIterator != subframeArchives.end();
         subframesIterator++) {
        addSubframeArchive(*subframesIterator);
    }
}

static bool loadArchiveResourceField(xmlNodePtr resourceNode, const xmlChar* fieldName, Vector<char>* outputData)
{
    if (!outputData)
        return false;

    outputData->clear();

    const char* base64Data = 0;

    for (xmlNodePtr fieldNode = resourceNode->xmlChildrenNode;
         fieldNode;
         fieldNode = fieldNode->next) {
        if (xmlStrEqual(fieldNode->name, fieldName)) {
            base64Data = (const char*)xmlNodeGetContent(fieldNode->xmlChildrenNode);
            if (!base64Data) {
                /* Empty fields seem to break if they aren't null terminated. */
                outputData->append('\0');
                return true;
            }
            break;
        }
    }
    if (!base64Data) {
        LOGD("loadArchiveResourceField: Failed to load field.");
        return false;
    }

    const int base64Size = xmlStrlen(BAD_CAST base64Data);

    const int result = base64Decode(base64Data, base64Size, *outputData);
    if (!result) {
        LOGD("loadArchiveResourceField: Failed to decode field.");
        return false;
    }

    return true;
}

static PassRefPtr<SharedBuffer> loadArchiveResourceFieldBuffer(xmlNodePtr resourceNode, const xmlChar* fieldName)
{
    Vector<char> fieldData;

    if (loadArchiveResourceField(resourceNode, fieldName, &fieldData))
        return SharedBuffer::create(fieldData.data(), fieldData.size());

    return 0;
}

static String loadArchiveResourceFieldString(xmlNodePtr resourceNode, const xmlChar* fieldName)
{
    Vector<char> fieldData;

    if (loadArchiveResourceField(resourceNode, fieldName, &fieldData))
        return String::fromUTF8(fieldData.data(), fieldData.size());

    return String();
}

static KURL loadArchiveResourceFieldURL(xmlNodePtr resourceNode, const xmlChar* fieldName)
{
    Vector<char> fieldData;

    if (loadArchiveResourceField(resourceNode, fieldName, &fieldData))
        return KURL(ParsedURLString, String::fromUTF8(fieldData.data(), fieldData.size()));

    return KURL();
}

static PassRefPtr<ArchiveResource> loadArchiveResource(xmlNodePtr resourceNode)
{
    if (!xmlStrEqual(resourceNode->name, archiveResourceTag)) {
        LOGD("loadArchiveResource: Malformed resource.");
        return 0;
    }

    KURL url = loadArchiveResourceFieldURL(resourceNode, urlFieldTag);
    if (url.isNull()) {
        LOGD("loadArchiveResource: Failed to load resource.");
        return 0;
    }

    String mimeType = loadArchiveResourceFieldString(resourceNode, mimeFieldTag);
    if (mimeType.isNull()) {
        LOGD("loadArchiveResource: Failed to load resource.");
        return 0;
    }

    String textEncoding = loadArchiveResourceFieldString(resourceNode, encodingFieldTag);
    if (textEncoding.isNull()) {
        LOGD("loadArchiveResource: Failed to load resource.");
        return 0;
    }

    String frameName = loadArchiveResourceFieldString(resourceNode, frameFieldTag);
    if (frameName.isNull()) {
        LOGD("loadArchiveResource: Failed to load resource.");
        return 0;
    }

    PassRefPtr<SharedBuffer> data = loadArchiveResourceFieldBuffer(resourceNode, dataFieldTag);
    if (!data) {
        LOGD("loadArchiveResource: Failed to load resource.");
        return 0;
    }

    return ArchiveResource::create(data, url, mimeType, textEncoding, frameName);
}

static PassRefPtr<WebArchiveAndroid> loadArchive(xmlNodePtr archiveNode)
{
    xmlNodePtr resourceNode = 0;

    PassRefPtr<ArchiveResource> mainResource;
    Vector<PassRefPtr<ArchiveResource> > subresources;
    Vector<PassRefPtr<Archive> > subframes;

    if (!xmlStrEqual(archiveNode->name, archiveTag)) {
        LOGD("loadArchive: Malformed archive.");
        return 0;
    }

    for (resourceNode = archiveNode->xmlChildrenNode;
         resourceNode;
         resourceNode = resourceNode->next) {
        if (xmlStrEqual(resourceNode->name, mainResourceTag)) {
            resourceNode = resourceNode->xmlChildrenNode;
            if (!resourceNode)
                break;
            mainResource = loadArchiveResource(resourceNode);
            break;
        }
    }
    if (!mainResource) {
        LOGD("loadArchive: Failed to load main resource.");
        return 0;
    }

    for (resourceNode = archiveNode->xmlChildrenNode;
         resourceNode;
         resourceNode = resourceNode->next) {
        if (xmlStrEqual(resourceNode->name, subresourcesTag)) {
            for (resourceNode = resourceNode->xmlChildrenNode;
                 resourceNode;
                 resourceNode = resourceNode->next) {
                PassRefPtr<ArchiveResource> subresource = loadArchiveResource(resourceNode);
                if (!subresource) {
                    LOGD("loadArchive: Failed to load subresource.");
                    break;
                }
                subresources.append(subresource);
            }
            break;
        }
    }

    for (resourceNode = archiveNode->xmlChildrenNode;
         resourceNode;
         resourceNode = resourceNode->next) {
        if (xmlStrEqual(resourceNode->name, subframesTag)) {
            for (resourceNode = resourceNode->xmlChildrenNode;
                 resourceNode;
                 resourceNode = resourceNode->next) {
                PassRefPtr<WebArchiveAndroid> subframe = loadArchive(resourceNode);
                if (!subframe) {
                    LOGD("loadArchive: Failed to load subframe.");
                    break;
                }
                subframes.append(subframe);
            }
            break;
        }
    }

    return WebArchiveAndroid::create(mainResource, subresources, subframes);
}

static PassRefPtr<WebArchiveAndroid> createArchiveForError()
{
    /* When an archive cannot be loaded, we return an empty archive instead. */
    PassRefPtr<ArchiveResource> mainResource = ArchiveResource::create(
        SharedBuffer::create(), KURL(ParsedURLString, String::fromUTF8("file:///dummy")),
        String::fromUTF8("text/plain"), String(""), String(""));
    Vector<PassRefPtr<ArchiveResource> > subresources;
    Vector<PassRefPtr<Archive> > subframes;

    return WebArchiveAndroid::create(mainResource, subresources, subframes);
}

PassRefPtr<WebArchiveAndroid> WebArchiveAndroid::create(SharedBuffer* buffer)
{
    const char* const noBaseUrl = "";
    const char* const defaultEncoding = 0;
    const int noParserOptions = 0;

    xmlDocPtr doc = xmlReadMemory(buffer->data(), buffer->size(), noBaseUrl, defaultEncoding, noParserOptions);
    if (!doc) {
        LOGD("create: Failed to parse document.");
        return createArchiveForError();
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) {
        LOGD("create: Empty document.");
        xmlFreeDoc(doc);
        return createArchiveForError();
    }

    RefPtr<WebArchiveAndroid> archive = loadArchive(root);
    if (!archive) {
        LOGD("create: Failed to load archive.");
        xmlFreeDoc(doc);
        return createArchiveForError();
    }

    xmlFreeDoc(doc);
    return archive.release();
}

static bool saveArchiveResourceField(xmlTextWriterPtr writer, const xmlChar* tag, const char* data, int size)
{
    int result = xmlTextWriterStartElement(writer, tag);
    if (result < 0) {
        LOGD("saveArchiveResourceField: Failed to start element.");
        return false;
    }

    if (size > 0) {
        Vector<char> base64Data;
        base64Encode(data, size, base64Data, false);
        if (base64Data.isEmpty()) {
            LOGD("saveArchiveResourceField: Failed to base64 encode data.");
            return false;
        }

        result = xmlTextWriterWriteRawLen(writer, BAD_CAST base64Data.data(), base64Data.size());
        if (result < 0) {
            LOGD("saveArchiveResourceField: Failed to write data.");
            return false;
        }
    }

    result = xmlTextWriterEndElement(writer);
    if (result < 0) {
        LOGD("saveArchiveResourceField: Failed to end element.");
        return false;
    }

    return true;
}

static bool saveArchiveResourceField(xmlTextWriterPtr writer, const xmlChar* tag, SharedBuffer* buffer)
{
    return saveArchiveResourceField(writer, tag, buffer->data(), buffer->size());
}

static bool saveArchiveResourceField(xmlTextWriterPtr writer, const xmlChar* tag, const String& string)
{
    CString utf8String = string.utf8();

    return saveArchiveResourceField(writer, tag, utf8String.data(), utf8String.length());
}

static bool saveArchiveResource(xmlTextWriterPtr writer, PassRefPtr<ArchiveResource> resource)
{
    int result = xmlTextWriterStartElement(writer, archiveResourceTag);
    if (result < 0) {
        LOGD("saveArchiveResource: Failed to start element.");
        return false;
    }

    if (!saveArchiveResourceField(writer, urlFieldTag, resource->url().string())
        || !saveArchiveResourceField(writer, mimeFieldTag, resource->mimeType())
        || !saveArchiveResourceField(writer, encodingFieldTag, resource->textEncoding())
        || !saveArchiveResourceField(writer, frameFieldTag, resource->frameName())
        || !saveArchiveResourceField(writer, dataFieldTag, resource->data()))
        return false;

    result = xmlTextWriterEndElement(writer);
    if (result < 0) {
        LOGD("saveArchiveResource: Failed to end element.");
        return false;
    }

    return true;
}

static bool saveArchive(xmlTextWriterPtr writer, PassRefPtr<Archive> archive)
{
    int result = xmlTextWriterStartElement(writer, archiveTag);
    if (result < 0) {
        LOGD("saveArchive: Failed to start element.");
        return false;
    }

    result = xmlTextWriterStartElement(writer, mainResourceTag);
    if (result < 0) {
        LOGD("saveArchive: Failed to start element.");
        return false;
    }

    if (!saveArchiveResource(writer, archive->mainResource()))
        return false;

    result = xmlTextWriterEndElement(writer);
    if (result < 0) {
        LOGD("saveArchive: Failed to end element.");
        return false;
    }

    result = xmlTextWriterStartElement(writer, subresourcesTag);
    if (result < 0) {
        LOGD("saveArchive: Failed to start element.");
        return false;
    }

    for (Vector<const RefPtr<ArchiveResource> >::iterator subresource = archive->subresources().begin();
         subresource != archive->subresources().end();
         subresource++) {
        if (!saveArchiveResource(writer, *subresource))
            return false;
    }

    result = xmlTextWriterEndElement(writer);
    if (result < 0) {
        LOGD("saveArchive: Failed to end element.");
        return false;
    }

    result = xmlTextWriterStartElement(writer, subframesTag);
    if (result < 0) {
        LOGD("saveArchive: Failed to start element.");
        return false;
    }

    for (Vector<const RefPtr<Archive> >::iterator subframe = archive->subframeArchives().begin();
            subframe != archive->subframeArchives().end();
            subframe++) {
        if (!saveArchive(writer, *subframe))
            return false;
    }

    result = xmlTextWriterEndElement(writer);
    if (result < 0) {
        LOGD("saveArchive: Failed to end element.");
        return true;
    }

    return true;
}

bool WebArchiveAndroid::saveWebArchive(xmlTextWriterPtr writer)
{
    const char* const defaultXmlVersion = 0;
    const char* const defaultEncoding = 0;
    const char* const defaultStandalone = 0;

    int result = xmlTextWriterStartDocument(writer, defaultXmlVersion, defaultEncoding, defaultStandalone);
    if (result < 0) {
        LOGD("saveWebArchive: Failed to start document.");
        return false;
    }

    if (!saveArchive(writer, this))
        return false;

    result = xmlTextWriterEndDocument(writer);
    if (result< 0) {
        LOGD("saveWebArchive: Failed to end document.");
        return false;
    }

    return true;
}

}

#endif // ENABLE(WEB_ARCHIVE)
