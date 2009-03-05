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
#include "ImageDecoder.h"
#include "ImageSource.h"
#include "IntSize.h"
#include "NotImplemented.h"
#include "SharedBuffer.h"
#include "PlatformString.h"

#include "JavaSharedClient.h"

#include "SkBitmapRef.h"
#include "SkImageRef.h"
#include "SkImageDecoder.h"
#include "SkStream.h"
#include "SkTemplates.h"

SkPixelRef* SkCreateRLEPixelRef(const SkBitmap& src);

//#define TRACE_SUBSAMPLE_BITMAPS
//#define TRACE_RLE_BITMAPS

#include "SkImageRef_GlobalPool.h"
#include "SkImageRef_ashmem.h"

// made this up, so we don't waste a file-descriptor on small images, plus
// we don't want to lose too much on the round-up to a page size (4K)
#define MIN_ASHMEM_ALLOC_SIZE   (32*1024)

// don't use RLE for images smaller than this, since they incur a drawing cost
// (and don't work as patterns yet) we only want to use RLE when we must
#define MIN_RLE_ALLOC_SIZE      (512*1024)

static bool should_use_ashmem(const SkBitmap& bm) {
    return bm.getSize() >= MIN_ASHMEM_ALLOC_SIZE;
}

/*  Images larger than this should be subsampled. Using ashmem, the decoded
    pixels will be purged as needed, so this value can be pretty large. Making
    it too small hurts image quality (e.g. abc.com background). 2Meg works for
    the sites I've tested, but if we hit important sites that need more, we
    should try increasing it and see if it has negative impact on performance
    (i.e. we end up thrashing because we need to keep decoding images that have
    been purged.
 
    Perhaps this value should be some fraction of the available RAM...
*/
static size_t computeMaxBitmapSizeForCache() {
    return 2*1024*1024;
}

/* 8bit images larger than this should be recompressed in RLE, to reduce
    on the imageref cache.
 
    Downside: performance, since we have to decode/encode
    and then rle-decode when we draw.
*/
static bool shouldReencodeAsRLE(const SkBitmap& bm) {
    const SkBitmap::Config c = bm.config();
    return (SkBitmap::kIndex8_Config == c || SkBitmap::kA8_Config == c)
            &&
            bm.width() >= 64 // narrower than this won't compress well in RLE
            &&
            bm.getSize() > MIN_RLE_ALLOC_SIZE;
}

///////////////////////////////////////////////////////////////////////////////

class PrivateAndroidImageSourceRec : public SkBitmapRef {
public:
    PrivateAndroidImageSourceRec(const SkBitmap& bm, int origWidth,
                                 int origHeight, int sampleSize)
            : SkBitmapRef(bm), fSampleSize(sampleSize), fAllDataReceived(false) {
        this->setOrigSize(origWidth, origHeight);
    }

    int  fSampleSize;
    bool fAllDataReceived;
};

using namespace android;

namespace WebCore {

class SharedBufferStream : public SkMemoryStream {
public:
    SharedBufferStream(SharedBuffer* buffer)
            : SkMemoryStream(buffer->data(), buffer->size(), false) {
        fBuffer = buffer;
        buffer->ref();
    }
    
    virtual ~SharedBufferStream() {
        // we can't necessarily call fBuffer->deref() here, as we may be
        // in a different thread from webkit, and SharedBuffer is not
        // threadsafe. Therefore we defer it until it can be executed in the
        // webkit thread.
//        SkDebugf("-------- enqueue buffer %p for deref\n", fBuffer);
        JavaSharedClient::EnqueueFunctionPtr(CallDeref, fBuffer);
    }
    
private:
    // don't allow this to change our data. should not get called, but we
    // override here just to be sure
    virtual void setMemory(const void* data, size_t length, bool copyData) {
        sk_throw();
    }
    
    // we share ownership of this with webkit
    SharedBuffer* fBuffer;
    
    // will be invoked in the webkit thread
    static void CallDeref(void* buffer) {
//        SkDebugf("-------- call deref on buffer %p\n", buffer);
        ((SharedBuffer*)buffer)->deref();
    }
};
                           

///////////////////////////////////////////////////////////////////////////////

ImageSource::ImageSource() {
    m_decoder.m_image = NULL;
}

ImageSource::~ImageSource() {
    delete m_decoder.m_image;
}

bool ImageSource::initialized() const {
    return m_decoder.m_image != NULL;
}

static int computeSampleSize(const SkBitmap& bitmap) {
    const size_t maxSize = computeMaxBitmapSizeForCache();
    size_t size = bitmap.getSize();
    int sampleSize = 1;
    
    while (size > maxSize) {
        sampleSize <<= 1;
        size >>= 2;
    }
    
#ifdef TRACE_SUBSAMPLE_BITMAPS
    if (sampleSize > 1) {
        SkDebugf("------- bitmap [%d %d] config=%d origSize=%d predictSize=%d sampleSize=%d\n",
                 bitmap.width(), bitmap.height(), bitmap.config(),
                 bitmap.getSize(), size, sampleSize);
    }
#endif
    return sampleSize;
}

static SkPixelRef* convertToRLE(SkBitmap* bm, const void* data, size_t len) {    
    if (!shouldReencodeAsRLE(*bm)) {
        return NULL;
    }
    
    SkBitmap tmp;
    
    if (!SkImageDecoder::DecodeMemory(data, len, &tmp) || !tmp.getPixels()) {
        return NULL;
    }
    
    SkPixelRef* ref = SkCreateRLEPixelRef(tmp);
    if (NULL == ref) {
        return NULL;
    }
    
#ifdef TRACE_RLE_BITMAPS
    SkDebugf("---- reencode bitmap as RLE: [%d %d] encodeSize=%d\n",
             tmp.width(), tmp.height(), len);
#endif

    bm->setConfig(SkBitmap::kRLE_Index8_Config, tmp.width(), tmp.height());
    return ref;
}

void ImageSource::clearURL() 
{
    m_decoder.m_url.reset(); 
}

void ImageSource::setURL(const String& url) 
{ 
    if (url.startsWith("data:")) {
        clearURL();
    } else {
        m_decoder.m_url.setUTF16(url.characters(), url.length()); 
    }
}

void ImageSource::setData(SharedBuffer* data, bool allDataReceived)
{
    if (NULL == m_decoder.m_image) {
        SkBitmap tmp;
        
        SkMemoryStream stream(data->data(), data->size(), false);
        SkImageDecoder* codec = SkImageDecoder::Factory(&stream);
        SkAutoTDelete<SkImageDecoder> ad(codec);
        
        if (!codec || !codec->decode(&stream, &tmp, SkBitmap::kNo_Config,
                                       SkImageDecoder::kDecodeBounds_Mode)) {
            return;
        }

        int origW = tmp.width();
        int origH = tmp.height();
        int sampleSize = computeSampleSize(tmp);
        if (sampleSize > 1) {
            codec->setSampleSize(sampleSize);
            stream.rewind();
            if (!codec->decode(&stream, &tmp, SkBitmap::kNo_Config,
                                 SkImageDecoder::kDecodeBounds_Mode)) {
                return;
            }
        }

        m_decoder.m_image = new PrivateAndroidImageSourceRec(tmp, origW, origH,
                                                     sampleSize);
        
//        SkDebugf("----- started: [%d %d] %s\n", origW, origH, m_decoder.m_url.c_str());
    }

    PrivateAndroidImageSourceRec* decoder = m_decoder.m_image;
    if (allDataReceived && !decoder->fAllDataReceived) {
        decoder->fAllDataReceived = true;

        SkBitmap* bm = &decoder->bitmap();
        SkPixelRef* ref = convertToRLE(bm, data->data(), data->size());

        if (NULL == ref) {
            SkStream* strm = new SharedBufferStream(data);
            // imageref now owns the stream object
            if (should_use_ashmem(*bm)) {
//            SkDebugf("---- use ashmem for image [%d %d]\n", bm->width(), bm->height());
                ref = new SkImageRef_ashmem(strm, bm->config(), decoder->fSampleSize);
            } else {
//            SkDebugf("---- use globalpool for image [%d %d]\n", bm->width(), bm->height());
                ref = new SkImageRef_GlobalPool(strm, bm->config(), decoder->fSampleSize);
            }
            
      //      SkDebugf("----- allDataReceived [%d %d]\n", bm->width(), bm->height());
        }

        // we promise to never change the pixels (makes picture recording fast)
        ref->setImmutable();
        // give it the URL if we have one
        ref->setURI(m_decoder.m_url);
        // our bitmap is now the only owner of the imageref
        bm->setPixelRef(ref)->unref();
        
//        SkDebugf("---- finished: [%d %d] %s\n", bm->width(), bm->height(), ref->getURI());
    }
}

bool ImageSource::isSizeAvailable()
{
    return m_decoder.m_image != NULL;
}

IntSize ImageSource::size() const
{
    if (m_decoder.m_image) {
        return IntSize(m_decoder.m_image->origWidth(), m_decoder.m_image->origHeight());
    }
    return IntSize(0, 0);
}

int ImageSource::repetitionCount()
{
    return 1;
    // A property with value 0 means loop forever.
}

size_t ImageSource::frameCount() const
{
    // i.e. 0 frames if we're not decoded, or 1 frame if we are
    return m_decoder.m_image != NULL;
}

SkBitmapRef* ImageSource::createFrameAtIndex(size_t index)
{
    SkASSERT(index == 0);
    SkASSERT(m_decoder.m_image != NULL);
    m_decoder.m_image->ref();
    return m_decoder.m_image;
}

float ImageSource::frameDurationAtIndex(size_t index)
{
    SkASSERT(index == 0);
    float duration = 0;

    // Many annoying ads specify a 0 duration to make an image flash as quickly as possible.
    // We follow Firefox's behavior and use a duration of 100 ms for any frames that specify
    // a duration of <= 10 ms. See gfxImageFrame::GetTimeout in Gecko or Radar 4051389 for more.
    if (duration <= 0.010f)
        duration = 0.100f;
    return duration;
}

bool ImageSource::frameHasAlphaAtIndex(size_t index)
{
    SkASSERT(0 == index);

    if (NULL == m_decoder.m_image)
        return true;    // if we're not sure, assume the worse-case
    const PrivateAndroidImageSourceRec& decoder = *m_decoder.m_image;
    // if we're 16bit, we know even without all the data available
    if (decoder.bitmap().getConfig() == SkBitmap::kRGB_565_Config)
        return false;

    if (!decoder.fAllDataReceived)
        return true;    // if we're not sure, assume the worse-case
    
    return !decoder.bitmap().isOpaque();
}

bool ImageSource::frameIsCompleteAtIndex(size_t index)
{
    SkASSERT(0 == index);
	return m_decoder.m_image && m_decoder.m_image->fAllDataReceived;
}

void ImageSource::clear(bool destroyAll, size_t clearBeforeFrame, SharedBuffer* data, bool allDataReceived)
{
    // do nothing, since the cache is managed elsewhere
}

IntSize ImageSource::frameSizeAtIndex(size_t index) const
{
    // for now, all (1) of our frames are the same size
    return this->size();
}

String ImageSource::filenameExtension() const
{
    // FIXME: need to add virtual to our decoders to return "jpg/png/gif/..."
    return String();
}

}
