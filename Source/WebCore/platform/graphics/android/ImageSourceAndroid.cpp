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

#include "config.h"
#include "BitmapAllocatorAndroid.h"
#include "ImageSource.h"
#include "IntSize.h"
#include "NotImplemented.h"
#include "SharedBuffer.h"
#include "PlatformString.h"

#include "SkBitmapRef.h"
#include "SkImageDecoder.h"
#include "SkImageRef.h"
#include "SkStream.h"
#include "SkTemplates.h"

#ifdef ANDROID_ANIMATED_GIF
    #include "EmojiFont.h"
    #include "GIFImageDecoder.h"

    using namespace android;
#endif

// TODO: We should make use of some of the common code in platform/graphics/ImageSource.cpp.

SkPixelRef* SkCreateRLEPixelRef(const SkBitmap& src);

//#define TRACE_SUBSAMPLE_BITMAPS
//#define TRACE_RLE_BITMAPS


// flag to tell us when we're on a large-ram device (e.g. >= 256M)
#ifdef ANDROID_LARGE_MEMORY_DEVICE
    // don't use RLE for images smaller than this, since they incur a drawing cost
    // (and don't work as patterns yet) we only want to use RLE when we must
    #define MIN_RLE_ALLOC_SIZE          (8*1024*1024)

    // see dox for computeMaxBitmapSizeForCache()
    #define MAX_SIZE_BEFORE_SUBSAMPLE   (32*1024*1024)

    // preserve quality for 24/32bit src
    static const SkBitmap::Config gPrefConfigTable[6] = {
        SkBitmap::kIndex8_Config,       // src: index, opaque
        SkBitmap::kIndex8_Config,       // src: index, alpha
        SkBitmap::kRGB_565_Config,      // src: 16bit, opaque
        SkBitmap::kARGB_8888_Config,    // src: 16bit, alpha  (promote to 32bit)
        SkBitmap::kARGB_8888_Config,    // src: 32bit, opaque
        SkBitmap::kARGB_8888_Config,    // src: 32bit, alpha
    };
#else
    #define MIN_RLE_ALLOC_SIZE          (2*1024*1024)
    #define MAX_SIZE_BEFORE_SUBSAMPLE   (2*1024*1024)

    // tries to minimize memory usage (i.e. demote opaque 32bit -> 16bit)
    static const SkBitmap::Config gPrefConfigTable[6] = {
        SkBitmap::kIndex8_Config,       // src: index, opaque
        SkBitmap::kIndex8_Config,       // src: index, alpha
        SkBitmap::kRGB_565_Config,      // src: 16bit, opaque
        SkBitmap::kARGB_8888_Config,    // src: 16bit, alpha  (promote to 32bit)
        SkBitmap::kRGB_565_Config,      // src: 32bit, opaque (demote to 16bit)
        SkBitmap::kARGB_8888_Config,    // src: 32bit, alpha
    };
#endif

/*  Images larger than this should be subsampled. Using ashmem, the decoded
    pixels will be purged as needed, so this value can be pretty large. Making
    it too small hurts image quality (e.g. abc.com background). 2Meg works for
    the sites I've tested, but if we hit important sites that need more, we
    should try increasing it and see if it has negative impact on performance
    (i.e. we end up thrashing because we need to keep decoding images that have
    been purged.
 
    Perhaps this value should be some fraction of the available RAM...
*/
size_t computeMaxBitmapSizeForCache() {
    return MAX_SIZE_BEFORE_SUBSAMPLE;
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

namespace WebCore {

ImageSource::ImageSource(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : m_alphaOption(alphaOption)
    , m_gammaAndColorProfileOption(gammaAndColorProfileOption)
{
    m_decoder.m_image = NULL;
#ifdef ANDROID_ANIMATED_GIF
    m_decoder.m_gifDecoder = 0;
#endif
}

ImageSource::~ImageSource() {
    delete m_decoder.m_image;
#ifdef ANDROID_ANIMATED_GIF
    delete m_decoder.m_gifDecoder;
#endif
}

bool ImageSource::initialized() const {
    return
#ifdef ANDROID_ANIMATED_GIF
        m_decoder.m_gifDecoder ||
#endif
        m_decoder.m_image != NULL;
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
    m_decoder.m_url.setUTF16(url.characters(), url.length());
}

#ifdef ANDROID_ANIMATED_GIF
// we only animate small GIFs for now, to save memory
// also, we only support this in Japan, hence the Emoji check
static bool should_use_animated_gif(int width, int height) {
#ifdef ANDROID_LARGE_MEMORY_DEVICE
    return true;
#else
    return EmojiFont::IsAvailable() &&
           width <= 32 && height <= 32;
#endif
}
#endif

void ImageSource::setData(SharedBuffer* data, bool allDataReceived)
{
#ifdef ANDROID_ANIMATED_GIF
    // This is only necessary if we allow ourselves to partially decode GIF
    bool disabledAnimatedGif = false;
    if (m_decoder.m_gifDecoder
            && !m_decoder.m_gifDecoder->failed()) {
        m_decoder.m_gifDecoder->setData(data, allDataReceived);
        if (!allDataReceived || m_decoder.m_gifDecoder->frameCount() != 1)
            return;
        disabledAnimatedGif = true;
        delete m_decoder.m_gifDecoder;
        m_decoder.m_gifDecoder = 0;
    }
#endif
    if (NULL == m_decoder.m_image
#ifdef ANDROID_ANIMATED_GIF
          && !m_decoder.m_gifDecoder
#endif
                                            ) {
        SkBitmap tmp;

        SkMemoryStream stream(data->data(), data->size(), false);
        SkImageDecoder* codec = SkImageDecoder::Factory(&stream);
        if (!codec)
            return;

        SkAutoTDelete<SkImageDecoder> ad(codec);
        codec->setPrefConfigTable(gPrefConfigTable);
        if (!codec->decode(&stream, &tmp, SkImageDecoder::kDecodeBounds_Mode))
            return;

        int origW = tmp.width();
        int origH = tmp.height();

#ifdef ANDROID_ANIMATED_GIF
        // First, check to see if this is an animated GIF
        const char* contents = data->data();
        if (data->size() > 3 && strncmp(contents, "GIF8", 4) == 0
                && should_use_animated_gif(origW, origH)
                && !disabledAnimatedGif) {
            // This means we are looking at a GIF, so create special
            // GIF Decoder
            // Need to wait for all data received if we are assigning an
            // allocator (which we are not at the moment).
            if (!m_decoder.m_gifDecoder /*&& allDataReceived*/)
                m_decoder.m_gifDecoder = new GIFImageDecoder(m_alphaOption, m_gammaAndColorProfileOption);
            int frameCount = 0;
            if (!m_decoder.m_gifDecoder->failed()) {
                m_decoder.m_gifDecoder->setData(data, allDataReceived);
                if (!allDataReceived)
                    return;
                frameCount = m_decoder.m_gifDecoder->frameCount();
            }
            if (frameCount != 1)
                return;
            delete m_decoder.m_gifDecoder;
            m_decoder.m_gifDecoder = 0;
        }
#endif
        
        int sampleSize = computeSampleSize(tmp);
        if (sampleSize > 1) {
            codec->setSampleSize(sampleSize);
            stream.rewind();
            if (!codec->decode(&stream, &tmp,
                               SkImageDecoder::kDecodeBounds_Mode)) {
                return;
            }
        }

        m_decoder.m_image = new PrivateAndroidImageSourceRec(tmp, origW, origH,
                                                     sampleSize);
        
//        SkDebugf("----- started: [%d %d] %s\n", origW, origH, m_decoder.m_url.c_str());
    }

    PrivateAndroidImageSourceRec* decoder = m_decoder.m_image;
    if (allDataReceived && decoder && !decoder->fAllDataReceived) {
        decoder->fAllDataReceived = true;

        SkBitmap* bm = &decoder->bitmap();
        SkPixelRef* ref = convertToRLE(bm, data->data(), data->size());

        if (ref) {
            bm->setPixelRef(ref)->unref();
        } else {
            BitmapAllocatorAndroid alloc(data, decoder->fSampleSize);
            if (!alloc.allocPixelRef(bm, NULL)) {
                return;
            }
            ref = bm->pixelRef();
        }

        // we promise to never change the pixels (makes picture recording fast)
        ref->setImmutable();
        // give it the URL if we have one
        ref->setURI(m_decoder.m_url);
    }
}

bool ImageSource::isSizeAvailable()
{
    return
#ifdef ANDROID_ANIMATED_GIF
            (m_decoder.m_gifDecoder
                    && m_decoder.m_gifDecoder->isSizeAvailable()) ||
#endif
            m_decoder.m_image != NULL;
}

IntSize ImageSource::size() const
{
#ifdef ANDROID_ANIMATED_GIF
    if (m_decoder.m_gifDecoder)
        return m_decoder.m_gifDecoder->size();
#endif
    if (m_decoder.m_image) {
        return IntSize(m_decoder.m_image->origWidth(), m_decoder.m_image->origHeight());
    }
    return IntSize(0, 0);
}

int ImageSource::repetitionCount()
{
#ifdef ANDROID_ANIMATED_GIF
    if (m_decoder.m_gifDecoder)
        return m_decoder.m_gifDecoder->repetitionCount();
    if (!m_decoder.m_image) return 0;
#endif
    return 1;
    // A property with value 0 means loop forever.
}

size_t ImageSource::frameCount() const
{
#ifdef ANDROID_ANIMATED_GIF
    if (m_decoder.m_gifDecoder) {
        return m_decoder.m_gifDecoder->failed() ? 0
                : m_decoder.m_gifDecoder->frameCount();
    }
#endif
    // i.e. 0 frames if we're not decoded, or 1 frame if we are
    return m_decoder.m_image != NULL;
}

SkBitmapRef* ImageSource::createFrameAtIndex(size_t index)
{
#ifdef ANDROID_ANIMATED_GIF
    if (m_decoder.m_gifDecoder) {
        ImageFrame* buffer =
                m_decoder.m_gifDecoder->frameBufferAtIndex(index);
        if (!buffer || buffer->status() == ImageFrame::FrameEmpty)
            return 0;
        SkBitmap& bitmap = buffer->bitmap();
        SkPixelRef* pixelRef = bitmap.pixelRef();
        if (pixelRef)
            pixelRef->setURI(m_decoder.m_url);
        return new SkBitmapRef(bitmap);
    }
#else
    SkASSERT(index == 0);
#endif
    SkASSERT(m_decoder.m_image != NULL);
    m_decoder.m_image->ref();
    return m_decoder.m_image;
}

float ImageSource::frameDurationAtIndex(size_t index)
{
    float duration = 0;
#ifdef ANDROID_ANIMATED_GIF
    if (m_decoder.m_gifDecoder) {
        ImageFrame* buffer
                = m_decoder.m_gifDecoder->frameBufferAtIndex(index);
        if (!buffer || buffer->status() == ImageFrame::FrameEmpty)
            return 0;
        duration = buffer->duration() / 1000.0f;
    }
#else
    SkASSERT(index == 0);
#endif

    // Many annoying ads specify a 0 duration to make an image flash as quickly as possible.
    // We follow Firefox's behavior and use a duration of 100 ms for any frames that specify
    // a duration of <= 10 ms. See gfxImageFrame::GetTimeout in Gecko or Radar 4051389 for more.
    if (duration <= 0.010f)
        duration = 0.100f;
    return duration;
}

bool ImageSource::frameHasAlphaAtIndex(size_t index)
{
#ifdef ANDROID_ANIMATED_GIF
    if (m_decoder.m_gifDecoder) {
        ImageFrame* buffer =
                m_decoder.m_gifDecoder->frameBufferAtIndex(index);
        if (!buffer || buffer->status() == ImageFrame::FrameEmpty)
            return false;

        return buffer->hasAlpha();
    }
#else
    SkASSERT(0 == index);
#endif

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
#ifdef ANDROID_ANIMATED_GIF
    if (m_decoder.m_gifDecoder) {
        ImageFrame* buffer =
                m_decoder.m_gifDecoder->frameBufferAtIndex(index);
        return buffer && buffer->status() == ImageFrame::FrameComplete;
    }
#else
    SkASSERT(0 == index);
#endif
	return m_decoder.m_image && m_decoder.m_image->fAllDataReceived;
}

void ImageSource::clear(bool destroyAll, size_t clearBeforeFrame, SharedBuffer* data, bool allDataReceived)
{
#ifdef ANDROID_ANIMATED_GIF
    if (!destroyAll) {
        if (m_decoder.m_gifDecoder)
            m_decoder.m_gifDecoder->clearFrameBufferCache(clearBeforeFrame);
        return;
    }
    
    delete m_decoder.m_gifDecoder;
    m_decoder.m_gifDecoder = 0;
    if (data)
        setData(data, allDataReceived);
#endif
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
#ifdef ANDROID_ANIMATED_GIF
    if (m_decoder.m_gifDecoder)
        return m_decoder.m_gifDecoder->filenameExtension();
#endif
    return String();
}

bool ImageSource::getHotSpot(IntPoint&) const
{
    return false;
}

size_t ImageSource::bytesDecodedToDetermineProperties() const
{
    return 0;
}

}
