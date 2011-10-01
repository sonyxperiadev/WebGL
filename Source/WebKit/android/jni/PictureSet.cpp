/*
 * Copyright 2008, The Android Open Source Project
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

#define LOG_NDEBUG 0
#define LOG_TAG "pictureset"

//#include <config.h>
#include "CachedPrefix.h"
#include "android_graphics.h"
#include "PictureSet.h"
#include "SkBounder.h"
#include "SkCanvas.h"
#include "SkPicture.h"
#include "SkRect.h"
#include "SkRegion.h"
#include "SkStream.h"
#include "TimeCounter.h"

#define MAX_DRAW_TIME 100
#define MIN_SPLITTABLE 400
#define MAX_ADDITIONAL_AREA 0.65
#define MAX_ADDITIONAL_PICTURES 32

#define BUCKET_SIZE 1024
#define MAX_BUCKET_COUNT_X 16
#define MAX_BUCKET_COUNT_Y 64

#include <wtf/CurrentTime.h>

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "PictureSet", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "PictureSet", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

#if PICTURE_SET_DEBUG
class MeasureStream : public SkWStream {
public:
    MeasureStream() : mTotal(0) {}
    virtual bool write(const void* , size_t size) {
        mTotal += size;
        return true;
    }
    size_t mTotal;
};
#endif

namespace android {

PictureSet::PictureSet() :
#ifdef FAST_PICTURESET
    mBucketSizeX(BUCKET_SIZE), mBucketSizeY(BUCKET_SIZE),
    mBucketCountX(0), mBucketCountY(0),
#endif
    mHeight(0), mWidth(0)
{
    setDimensions(0, 0);
    mBaseArea = mAdditionalArea = 0;
}

PictureSet::PictureSet(SkPicture* picture) :
#ifdef FAST_PICTURESET
    mBucketSizeX(BUCKET_SIZE), mBucketSizeY(BUCKET_SIZE),
    mBucketCountX(0), mBucketCountY(0),
#endif
    mHeight(0), mWidth(0)
{
    mBaseArea = mAdditionalArea = 0;
    if (!picture) {
        setDimensions(0, 0);
        return;
    }
    setDimensions(picture->width(), picture->height());
    mBaseArea = mWidth * mHeight;
#ifdef FAST_PICTURESET
    SkIRect area;
    area.set(0, 0, mWidth, mHeight);
    splitAdd(area);
    WTF::Vector<Bucket*>* buckets = bucketsToUpdate();
    for (unsigned int i = 0; i < buckets->size(); i++) {
        Bucket* bucket = (*buckets)[i];
        for (unsigned int j = 0; j < bucket->size(); j++) {
            BucketPicture& bucketPicture = (*bucket)[j];
            const SkIRect& inval = bucketPicture.mRealArea;
            SkPicture *splitPicture = new SkPicture();
            SkCanvas *canvas = splitPicture->beginRecording(
                    inval.width(), inval.height(),
                    SkPicture::kUsePathBoundsForClip_RecordingFlag);
            canvas->translate(-inval.fLeft, -inval.fTop);
            picture->draw(canvas);
            splitPicture->endRecording();
            SkSafeUnref(bucketPicture.mPicture);
            bucketPicture.mPicture = splitPicture;
        }
    }
    buckets->clear();
#else
    Pictures pictureAndBounds;
    pictureAndBounds.mPicture = picture;
    SkSafeRef(pictureAndBounds.mPicture);
    pictureAndBounds.mEmpty = false;
    pictureAndBounds.mArea.setRect(0, 0, mWidth, mHeight);
    pictureAndBounds.mSplit = false;
    pictureAndBounds.mBase = true;
    pictureAndBounds.mElapsed = 0;
    pictureAndBounds.mWroteElapsed = false;
    mPictures.append(pictureAndBounds);
#endif // FAST_PICTURESET
}

PictureSet::~PictureSet()
{
    clear();
}

#ifdef FAST_PICTURESET
#else
void PictureSet::add(const Pictures* temp)
{
    Pictures pictureAndBounds = *temp;
    SkSafeRef(pictureAndBounds.mPicture);
    pictureAndBounds.mWroteElapsed = false;
    mPictures.append(pictureAndBounds);
}
#endif // FAST_PICTURESET

void PictureSet::add(const SkRegion& area, SkPicture* picture,
                     uint32_t elapsed, bool split)
{
    if (area.isRect()) {
#ifdef FAST_PICTURESET
        splitAdd(area.getBounds());
#else
        add(area, picture, elapsed, split, false);
#endif // FAST_PICTURESET
    } else {
        SkRegion::Iterator cliperator(area);
        while (!cliperator.done()) {
            SkIRect ir = cliperator.rect();
#ifdef FAST_PICTURESET
            splitAdd(ir);
#else
            SkRegion newArea;
            newArea.setRect(ir);
            add(newArea, picture, elapsed, split, false);
#endif // FAST_PICTURESET
            cliperator.next();
        }
    }
}

#ifdef FAST_PICTURESET

Bucket* PictureSet::getBucket(int x, int y)
{
    // only create buckets for valid, positive coordinates, ignore and return
    // NULL otherwise
    if (x < 0 || y < 0)
        return 0;

    BucketPosition position(x+1, y+1);
    if (!mBuckets.contains(position)) {
        XLOG("PictureSet::getBucket(%d, %d) adding new bucket", x, y);
        Bucket* bucket = new Bucket();
        mBuckets.add(position, bucket);
    }
    return mBuckets.get(position);
}

void PictureSet::displayBucket(Bucket* bucket)
{
    BucketPicture* first = bucket->begin();
    BucketPicture* last = bucket->end();
    for (BucketPicture* current = first; current != last; current++) {
        XLOGC("- in %x, bucketPicture %d,%d,%d,%d - %dx%d, picture: %x, base: %x",
              bucket,
              current->mArea.fLeft,
              current->mArea.fTop,
              current->mArea.fRight,
              current->mArea.fBottom,
              current->mArea.width(),
              current->mArea.height(),
              current->mPicture,
              current->mBase);
    }
}

void PictureSet::displayBuckets()
{
    XLOGC("\n\n****** DISPLAY BUCKETS ON PictureSet %x ******", this);
    for (BucketMap::iterator iter = mBuckets.begin(); iter != mBuckets.end(); ++iter) {
        XLOGC("\n*** Bucket %x for %d, %d", iter->second, iter->first.first, iter->first.second);
        displayBucket(iter->second);
    }
    XLOGC("\n****** END OF DISPLAY BUCKETS ******\n\n");
}

// When we receive an inval in a Bucket, we try to see if we intersect with
// existing invals/pictures in the Bucket.
void PictureSet::addToBucket(Bucket* bucket, int dx, int dy, SkIRect& rect)
{
    bool resetBase = false;

    SkIRect totalArea = rect;
    BucketPicture* first = bucket->begin();
    BucketPicture* last = bucket->end();

    // If the inval covers a large area of the base inval, let's repaint the
    // entire bucket.
    if (rect.width() * rect.height() > MAX_ADDITIONAL_AREA * mBucketSizeX * mBucketSizeY)
        resetBase = true;

    // let's gather all the BucketPicture intersecting with the new invalidated
    // area, collect their area and remove their picture
    for (BucketPicture* current = first; current != last; current++) {
        bool remove = resetBase;
        bool intersect = false;

        if (!remove)
            intersect = SkIRect::Intersects(current->mArea, rect);
        // If the current picture is not a base, and we intersect, remove it
        if (!remove && !current->mBase && intersect)
            remove = true;
        // If the current picture is a base, check if the new inval completely
        // contains the base, and if so remove it.
        if (!remove && current->mBase && rect.contains(current->mArea))
            remove = true;
        // If the current picture is a base and it intersects,
        // also check that it fully covers the bucket -- otherwise,
        // let's aggregate it with the new inval.
        if (!remove && current->mBase && intersect
            && (current->mArea.width() < mBucketSizeX || current->mArea.height() < mBucketSizeY)) {
            remove = true;
        }

        if (remove) {
            totalArea.join(current->mArea);
            current->mBase = false;
            current->mArea.setEmpty();
            SkSafeUnref(current->mPicture);
            current->mPicture = 0;
        }
    }

    // Now, let's add the new BucketPicture to the list, with the correct
    // area that needs to be repainted
    SkRegion region;
    SkIRect area = totalArea;
    area.offset(dx, dy);
    BucketPicture picture = { 0, totalArea, area, false };

    bucket->append(picture);

    first = bucket->begin();
    last = bucket->end();

    bool clearUp = false;
    if (last - first > MAX_ADDITIONAL_PICTURES) {
        // too many pictures in the bucket, let's collapse
        clearUp = true;
    }

    float bucketBaseArea = 0;
    float bucketAdditionalArea = 0;
    for (BucketPicture* current = first; current != last; current++) {
        float area = current->mArea.width() * current->mArea.height();
        if (current->mBase)
            bucketBaseArea += area;
        else
            bucketAdditionalArea += area;
    }

    if (bucketBaseArea > 0 && bucketBaseArea * MAX_ADDITIONAL_AREA <= bucketAdditionalArea) {
        // additional area too large, not worth maintaining
        clearUp = true;
    }

    // To clear things up, we just need to mark the pictures' area as empty
    // We only keep the base surface.
    if (clearUp) {
        for (BucketPicture* current = first; current != last; current++) {
            if (!current->mBase)
                current->mArea.setEmpty();
            SkSafeUnref(current->mPicture);
            current->mPicture = 0;
        }
    }

    // let's do a pass to collapse out empty areas
    BucketPicture* writer = first;
    for (BucketPicture* current = first; current != last; current++) {
        if (current && current->mArea.isEmpty())
            continue;
        *writer++ = *current;
    }

    bucket->shrink(writer - first);

    // let's recompute the bases
    first = bucket->begin();
    last = bucket->end();
    SkRegion drawn;
    drawn.setEmpty();
    for (BucketPicture* current = first; current != last; current++) {
        if (drawn.contains(current->mArea) == false) {
            current->mBase = true;
        }
        drawn.op(current->mArea, SkRegion::kUnion_Op);
    }
}

void PictureSet::gatherBucketsForArea(WTF::Vector<Bucket*>& list, const SkIRect& rect)
{
    XLOG("\n--- gatherBucketsForArea for rect %d, %d, %d, %d (%d x %d)",
          rect.fLeft, rect.fTop, rect.fRight, rect.fBottom,
          rect.width(), rect.height());

    if (!mBucketSizeX || !mBucketSizeY) {
        XLOGC("PictureSet::gatherBucketsForArea() called with bad bucket size: x=%d y=%d",
              mBucketSizeX, mBucketSizeY);
        return;
    }

    int x = rect.fLeft;
    int y = rect.fTop;
    int firstTileX = rect.fLeft / mBucketSizeX;
    int firstTileY = rect.fTop / mBucketSizeY;
    int lastTileX = rect.fRight / mBucketSizeX;
    int lastTileY = rect.fBottom / mBucketSizeY;

    for (int i = firstTileX; i <= lastTileX; i++) {
        for (int j = firstTileY; j <= lastTileY; j++) {
            Bucket* bucket = getBucket(i, j);
            XLOG("gather bucket %x for %d, %d", bucket, i+1, j+1);
            if (bucket)
                list.append(bucket);
        }
    }
}

// When we receive a new inval rect, we first find the Buckets that intersect
// with it; then we split the original inval into a serie of invals (one for
// each Bucket we intersect with). We then send that inval to the Bucket.
void PictureSet::splitAdd(const SkIRect& rect)
{
    XLOG("\n--- splitAdd for rect %d, %d, %d, %d (%d x %d)",
          rect.fLeft, rect.fTop, rect.fRight, rect.fBottom,
          rect.width(), rect.height());

    if (!mBucketSizeX || !mBucketSizeY) {
        XLOGC("PictureSet::splitAdd() called with bad bucket size: x=%d y=%d",
              mBucketSizeX, mBucketSizeY);
        return;
    }

    // TODO: reuse gatherBucketsForArea() (change Bucket to be a class)
    int x = rect.fLeft;
    int y = rect.fTop;
    int firstTileX = rect.fLeft / mBucketSizeX;
    int firstTileY = rect.fTop / mBucketSizeY;
    int lastTileX = rect.fRight / mBucketSizeX;
    int lastTileY = rect.fBottom / mBucketSizeY;

    XLOG("--- firstTile(%d, %d) lastTile(%d, %d)",
          firstTileX, firstTileY,
          lastTileX, lastTileY);

    for (int i = firstTileX; i <= lastTileX; i++) {
        for (int j = firstTileY; j <= lastTileY; j++) {
            Bucket* bucket = getBucket(i, j);
            if (!bucket)
                continue;

            SkIRect newRect;
            int deltaX = i * mBucketSizeX;
            int deltaY = j * mBucketSizeY;
            int left = (i == firstTileX) ? rect.fLeft - deltaX : 0;
            int top = (j == firstTileY) ? rect.fTop - deltaY : 0;
            int right = (i == lastTileX) ? rect.fRight % mBucketSizeX : mBucketSizeX;
            int bottom = (j == lastTileY) ? rect.fBottom % mBucketSizeY : mBucketSizeY;

            newRect.set(left, top, right, bottom);
            addToBucket(bucket, deltaX, deltaY, newRect);
            mUpdatedBuckets.append(bucket);
        }
    }

    XLOG("--- splitAdd DONE\n");
}

#endif // FAST_PICTURESET

// This function is used to maintain the list of Pictures.
// Pictures contain an SkPicture covering a specific area; some
// Pictures are "base" Pictures -- i.e. there is no Pictures
// underneath them.
// The idea here is to keep a balance between the number of Pictures
// we have (more Pictures slow us down) and the area of Pictures that
// need to be repainted (obviously, smaller areas are better).
// To do so, we try to not update/repaint the base pictures -- by
// construction, they usually cover a large area (the entire page).
// We only reset a base picture if the new invalidated area entirely
// contains it.
// Most of the time we thus work on smaller pictures on top of the
// base ones; We compute the total area of all pictures intersecting
// with the passed invalidated area (as they would need to be invalidated),
// and use that as the basis for the correct area we want to invalidate
// (we then can simply delete the pictures we intersect with).
// In addition, we do a couple of things to limit the total number of pictures
// we keep in the list:
// - if the total area of additional textures reach 65% of the base pictures,
//   we delete the additional pictures and mark the base pictures as
//   needing a full repaint
// - we limit the number of pictures to 32 -- above that, we do the same
//   things (deleting additional pictures + full repaint of base pictures)
#ifdef FAST_PICTURESET
#else
void PictureSet::add(const SkRegion& area, SkPicture* picture,
    uint32_t elapsed, bool split, bool empty)
{
    bool checkForNewBases = false;

    Pictures* first = mPictures.begin();
    Pictures* last = mPictures.end();
#ifdef DEBUG
    XLOG("--- before adding the new inval ---");
    for (Pictures* working = mPictures.begin(); working != mPictures.end(); working++) {
        SkIRect currentArea = working->mArea.getBounds();
        XLOG("picture %d (%d, %d, %d, %d - %d x %d) (isRect? %c) base: %c",
             working - first,
             currentArea.fLeft, currentArea.fTop, currentArea.fRight, currentArea.fBottom,
             currentArea.width(), currentArea.height(),
             working->mArea.isRect() ? 'Y' : 'N',
             working->mBase ? 'Y' : 'N');
    }
    XLOG("----------------------------------");
#endif

    // let's gather all the Pictures intersecting with the new invalidated
    // area, collect their area and remove their picture
    SkIRect totalArea = area.getBounds();
    for (Pictures* working = first; working != last; working++) {
        SkIRect inval = area.getBounds();
        bool remove = false;
        if (!working->mBase && working->mArea.intersects(inval))
            remove = true;
        if (working->mBase) {
            SkIRect baseArea = working->mArea.getBounds();
            if (area.contains(baseArea)) {
                remove = true;
                checkForNewBases = true;
            }
        }

        if (remove) {
            SkIRect currentArea = working->mArea.getBounds();
            if (working->mBase)
                mBaseArea -= currentArea.width() * currentArea.height();
            else
                mAdditionalArea -= currentArea.width() * currentArea.height();

            totalArea.join(currentArea);
            XLOG("picture %d (%d, %d, %d, %d - %d x %d) (isRect? %c) intersects with the new inval area (%d, %d, %d, %d - %d x %d) (isRect? %c, we remove it",
                 working - first,
                 currentArea.fLeft, currentArea.fTop, currentArea.fRight, currentArea.fBottom,
                 currentArea.width(), currentArea.height(),
                 working->mArea.isRect() ? 'Y' : 'N',
                 inval.fLeft, inval.fTop, inval.fRight, inval.fBottom,
                 inval.width(), inval.height(),
                 area.isRect() ? 'Y' : 'N');
            working->mArea.setEmpty();
            SkSafeUnref(working->mPicture);
            working->mPicture = 0;
        }
    }

    // Now we can add the new Picture to the list, with the correct area
    // that need to be repainted
    SkRegion collect;
    collect.setRect(totalArea);
    Pictures pictureAndBounds = {collect, 0, collect.getBounds(),
        elapsed, split, false, false, empty};

#ifdef FAST_PICTURESET
    if (mPictures.size() == 0)
        checkForNewBases = true;
#endif

    mPictures.append(pictureAndBounds);
    mAdditionalArea += totalArea.width() * totalArea.height();
    last = mPictures.end();
    first = mPictures.begin();

    // Then, let's see if we have to clear up the pictures in order to keep
    // the total number of pictures under our limit
    bool clearUp = false;
    if (last - first > MAX_ADDITIONAL_PICTURES) {
        XLOG("--- too many pictures, only keeping the bases : %d", last - first);
        clearUp = true;
    }

    if (!clearUp) {
        if (mBaseArea > 0 && mBaseArea * MAX_ADDITIONAL_AREA <= mAdditionalArea) {
            XLOG("+++ the sum of the additional area is > %.2f\% of the base Area (%.2f (%.2f) <= %.2f",
                 MAX_ADDITIONAL_AREA * 100, mBaseArea * 0.65, mBaseArea, mAdditionalArea);
            clearUp = true;
        }
    }

    if (clearUp) {
        for (Pictures* working = mPictures.begin(); working != mPictures.end(); working++) {
            if (!working->mBase)
                working->mArea.setEmpty();
            SkSafeUnref(working->mPicture);
            working->mPicture = 0;
        }
    }

#ifdef DEBUG
    XLOG("--- after adding the new inval, but before collapsing ---");
    for (Pictures* working = mPictures.begin(); working != mPictures.end(); working++) {
        SkIRect currentArea = working->mArea.getBounds();
        XLOG("picture %d (%d, %d, %d, %d - %d x %d) (isRect? %c) base: %c",
             working - first,
             currentArea.fLeft, currentArea.fTop, currentArea.fRight, currentArea.fBottom,
             currentArea.width(), currentArea.height(),
             working->mArea.isRect() ? 'Y' : 'N',
             working->mBase ? 'Y' : 'N');
    }
    XLOG("----------------------------------");
    XLOG("let's collapse...");
#endif

    // Finally, let's do a pass to collapse out empty regions
    Pictures* writer = first;
    for (Pictures* working = first; working != last; working++) {
        if (working && working->mArea.isEmpty())
            continue;
        *writer++ = *working;
    }
    XLOG("shiking of %d elements", writer - first);
    mPictures.shrink(writer - first);

#ifdef DEBUG
    XLOG("--- after adding the new inval ---");
    for (Pictures* working = mPictures.begin(); working != mPictures.end(); working++) {
        SkIRect currentArea = working->mArea.getBounds();
        XLOG("picture %d (%d, %d, %d, %d - %d x %d) (isRect? %c) base: %c picture %x",
             working - first,
             currentArea.fLeft, currentArea.fTop, currentArea.fRight, currentArea.fBottom,
             currentArea.width(), currentArea.height(),
             working->mArea.isRect() ? 'Y' : 'N',
             working->mBase ? 'Y' : 'N', working->mPicture);
    }
    XLOG("----------------------------------");
#endif

    // Base pictures might have been removed/added -- let's recompute them
    SkRegion drawn;
    if (checkForNewBases) {
        drawn.setEmpty();
        Pictures* last = mPictures.end();
        XLOG("checkForNewBases...");
        for (Pictures* working = mPictures.begin(); working != last; working++) {
            SkRegion& area = working->mArea;
            const SkIRect& a = area.getBounds();
            if (drawn.contains(working->mArea) == false) {
                working->mBase = true;
                float area = a.width() * a.height();
                mBaseArea += area;
                mAdditionalArea -= area;
            }
            drawn.op(working->mArea, SkRegion::kUnion_Op);
        }
    }
}
#endif // FAST_PICTURESET

void PictureSet::setDimensions(int width, int height, SkRegion* inval)
{
    // Note that setDimensions() may be called by our ctor and should behave accordingly
    if (mWidth == width && mHeight == height)
        return;
    DBG_SET_LOGD("%p old:(w=%d,h=%d) new:(w=%d,h=%d)", this,
        mWidth, mHeight, width, height);
    bool clearCache = false;
    if (inval) {
        if (mWidth == width && height > mHeight) { // only grew vertically
            SkIRect rect;
            rect.set(0, mHeight, width, height);
            inval->op(rect, SkRegion::kUnion_Op);
        } else {
            clearCache = true;
            inval->setRect(0, 0, width, height);
        }
    }
#ifdef FAST_PICTURESET
    // First figure out how large each bucket would be if we used all of the buckets
    int tmpSizeX = (width + MAX_BUCKET_COUNT_X - 1) / MAX_BUCKET_COUNT_X;
    int tmpSizeY = (height + MAX_BUCKET_COUNT_Y - 1) / MAX_BUCKET_COUNT_Y;

    // Then round the bucket size up to the nearest chunk
    int bucketSizeX = ((tmpSizeX - 1) / BUCKET_SIZE + 1) * BUCKET_SIZE;
    int bucketSizeY = ((tmpSizeY - 1) / BUCKET_SIZE + 1) * BUCKET_SIZE;

    int bucketCountX = (width + bucketSizeX - 1) / bucketSizeX;
    int bucketCountY = (height + bucketSizeY - 1) / bucketSizeY;

    // Clear the cache if the horizontal bucket count changed or the vertical
    // count shrank
    if (bucketCountX != mBucketCountX || bucketCountY < mBucketCountY)
        clearCache = true;

    // Or if the bucket size changed
    if (bucketSizeX != mBucketSizeX || bucketSizeY != mBucketSizeY)
        clearCache = true;

    XLOG("old width=%d height=%d bucketSizeX=%d bucketSizeY=%d bucketCountX=%d bucketCountY=%d clearCache=%d",
         mWidth, mHeight, mBucketSizeX, mBucketSizeY, mBucketCountX, mBucketCountY, clearCache);
    XLOG("new width=%d height=%d bucketSizeX=%d bucketSizeY=%d bucketCountX=%d bucketCountY=%d clearCache=%d",
         width, height, bucketSizeX, bucketSizeY, bucketCountX, bucketCountY, clearCache);
#endif
    if (clearCache)
        clear();
    mWidth = width;
    mHeight = height;
#ifdef FAST_PICTURESET
    mBucketSizeX = bucketSizeX;
    mBucketSizeY = bucketSizeY;
    mBucketCountX = bucketCountX;
    mBucketCountY = bucketCountY;
#endif
}

void PictureSet::clear()
{
    DBG_SET_LOG("");
#ifdef FAST_PICTURESET
    for (BucketMap::iterator iter = mBuckets.begin(); iter != mBuckets.end(); ++iter) {
         Bucket* bucket = iter->second;
         BucketPicture* first = bucket->begin();
         BucketPicture* last = bucket->end();
         for (BucketPicture* current = first; current != last; current++) {
             SkSafeUnref(current->mPicture);
             current->mPicture = 0;
         }
         bucket->clear();
    }
    mBuckets.clear();
    mBucketSizeX = mBucketSizeY = BUCKET_SIZE;
#else
    Pictures* last = mPictures.end();
    for (Pictures* working = mPictures.begin(); working != last; working++) {
        working->mArea.setEmpty();
        SkSafeUnref(working->mPicture);
    }
    mPictures.clear();
#endif // FAST_PICTURESET
    mWidth = mHeight = 0;
}

bool PictureSet::draw(SkCanvas* canvas)
{
#ifdef FAST_PICTURESET
    XLOG("PictureSet %x draw on canvas %x", this, canvas);
    SkRect bounds;
    if (canvas->getClipBounds(&bounds) == false)
        return false;
    SkIRect irect;
    bounds.roundOut(&irect);

    WTF::Vector<Bucket*> list;
    gatherBucketsForArea(list, irect);

    XLOG("PictureSet draw on canvas %x, we have %d buckets", canvas, list.size());
    for (unsigned int i = 0; i < list.size(); i++) {
        Bucket* bucket = list[i];
        XLOG("We paint using bucket %x with %d pictures", bucket, bucket->size());
        for (unsigned int j = 0; j < bucket->size(); j++)  {
            BucketPicture& picture = bucket->at(j);
            if (!picture.mPicture)
                continue;
            int saved = canvas->save();
            SkRect pathBounds;
            pathBounds.set(picture.mRealArea);
            XLOG("[%d/%d] draw on canvas with clip %d, %d, %d, %d - %d x %d",
                  j, bucket->size(),
                  picture.mRealArea.fLeft,
                  picture.mRealArea.fTop,
                  picture.mRealArea.fRight,
                  picture.mRealArea.fBottom,
                  picture.mRealArea.width(),
                  picture.mRealArea.height());
            canvas->clipRect(pathBounds);
            canvas->translate(pathBounds.fLeft, pathBounds.fTop);
            canvas->save();
            canvas->drawPicture(*picture.mPicture);
            canvas->restoreToCount(saved);
        }
    }
    return false;

#else

    validate(__FUNCTION__);
    Pictures* first = mPictures.begin();
    Pictures* last = mPictures.end();
    Pictures* working;
    SkRect bounds;
    if (canvas->getClipBounds(&bounds) == false)
        return false;
    SkIRect irect;
    bounds.roundOut(&irect);
    for (working = last; working != first; ) {
        --working;
        if (working->mArea.contains(irect)) {
#if PICTURE_SET_DEBUG
            const SkIRect& b = working->mArea.getBounds();
            DBG_SET_LOGD("contains working->mArea={%d,%d,%d,%d}"
                " irect={%d,%d,%d,%d}", b.fLeft, b.fTop, b.fRight, b.fBottom,
                irect.fLeft, irect.fTop, irect.fRight, irect.fBottom);
#endif
            first = working;
            break;
        }
    }
    DBG_SET_LOGD("%p first=%d last=%d", this, first - mPictures.begin(),
        last - mPictures.begin());
    uint32_t maxElapsed = 0;
    for (working = first; working != last; working++) {
        const SkRegion& area = working->mArea;
        if (area.quickReject(irect)) {
#if PICTURE_SET_DEBUG
            const SkIRect& b = area.getBounds();
            DBG_SET_LOGD("[%d] %p quickReject working->mArea={%d,%d,%d,%d}"
                " irect={%d,%d,%d,%d}", working - first, working,
                b.fLeft, b.fTop, b.fRight, b.fBottom,
                irect.fLeft, irect.fTop, irect.fRight, irect.fBottom);
#endif
            working->mElapsed = 0;
            continue;
        }
        int saved = canvas->save();
        SkRect pathBounds;
        if (area.isComplex()) {
            SkPath pathClip;
            area.getBoundaryPath(&pathClip);
            canvas->clipPath(pathClip);
            pathBounds = pathClip.getBounds();
        } else {
            pathBounds.set(area.getBounds());
            canvas->clipRect(pathBounds);
        }
        canvas->translate(pathBounds.fLeft, pathBounds.fTop);
        canvas->save();
        uint32_t startTime = getThreadMsec();
        canvas->drawPicture(*working->mPicture);
        size_t elapsed = working->mElapsed = getThreadMsec() - startTime;
        working->mWroteElapsed = true;
        if (maxElapsed < elapsed && (pathBounds.width() >= MIN_SPLITTABLE ||
                pathBounds.height() >= MIN_SPLITTABLE))
            maxElapsed = elapsed;
        canvas->restoreToCount(saved);
#define DRAW_TEST_IMAGE 01
#if DRAW_TEST_IMAGE && PICTURE_SET_DEBUG
        SkColor color = 0x3f000000 | (0xffffff & (unsigned) working);
        canvas->drawColor(color);
        SkPaint paint;
        color ^= 0x00ffffff;
        paint.setColor(color);
        char location[256];
        for (int x = area.getBounds().fLeft & ~0x3f;
                x < area.getBounds().fRight; x += 0x40) {
            for (int y = area.getBounds().fTop & ~0x3f;
                    y < area.getBounds().fBottom; y += 0x40) {
                int len = snprintf(location, sizeof(location) - 1, "(%d,%d)", x, y);
                canvas->drawText(location, len, x, y, paint);
            }
        }
#endif
        DBG_SET_LOGD("[%d] %p working->mArea={%d,%d,%d,%d} elapsed=%d base=%s",
            working - first, working,
            area.getBounds().fLeft, area.getBounds().fTop,
            area.getBounds().fRight, area.getBounds().fBottom,
            working->mElapsed, working->mBase ? "true" : "false");
    }
 //   dump(__FUNCTION__);
    return maxElapsed >= MAX_DRAW_TIME;
#endif // FAST_PICTURESET
}

void PictureSet::dump(const char* label) const
{
#if PICTURE_SET_DUMP
    DBG_SET_LOGD("%p %s (%d) (w=%d,h=%d)", this, label, mPictures.size(),
        mWidth, mHeight);
    const Pictures* last = mPictures.end();
    for (const Pictures* working = mPictures.begin(); working != last; working++) {
        const SkIRect& bounds = working->mArea.getBounds();
        const SkIRect& unsplit = working->mUnsplit;
        MeasureStream measure;
        if (working->mPicture != NULL)
            working->mPicture->serialize(&measure);
        LOGD(" [%d]"
            " mArea.bounds={%d,%d,r=%d,b=%d}"
            " mPicture=%p"
            " mUnsplit={%d,%d,r=%d,b=%d}"
            " mElapsed=%d"
            " mSplit=%s"
            " mWroteElapsed=%s"
            " mBase=%s"
            " pict-size=%d",
            working - mPictures.begin(),
            bounds.fLeft, bounds.fTop, bounds.fRight, bounds.fBottom,
            working->mPicture,
            unsplit.fLeft, unsplit.fTop, unsplit.fRight, unsplit.fBottom,
            working->mElapsed, working->mSplit ? "true" : "false",
            working->mWroteElapsed ? "true" : "false",
            working->mBase ? "true" : "false",
            measure.mTotal);
    }
#endif
}

class IsEmptyBounder : public SkBounder {
    virtual bool onIRect(const SkIRect& rect) {
        return false;
    }
};

class IsEmptyCanvas : public SkCanvas {
public:
    IsEmptyCanvas(SkBounder* bounder, SkPicture* picture) : 
            mPicture(picture), mEmpty(true) {
        setBounder(bounder);
    }
    
    void notEmpty() {
        mEmpty = false;
        mPicture->abortPlayback();    
    }

    virtual bool clipPath(const SkPath&, SkRegion::Op) {
        // this can be expensive to actually do, and doesn't affect the
        // question of emptiness, so we make it a no-op
        return true;
    }

    virtual void commonDrawBitmap(const SkBitmap& bitmap, const SkIRect* rect,
            const SkMatrix& , const SkPaint& ) {
        if (bitmap.width() <= 1 || bitmap.height() <= 1)
            return;
        DBG_SET_LOGD("abort {%d,%d}", bitmap.width(), bitmap.height());
        notEmpty();
    }

    virtual void drawPaint(const SkPaint& paint) {
    }

    virtual void drawPath(const SkPath& , const SkPaint& paint) {
        DBG_SET_LOG("abort");
        notEmpty();
    }

    virtual void drawPoints(PointMode , size_t , const SkPoint [],
                            const SkPaint& paint) {
    }
    
    virtual void drawRect(const SkRect& , const SkPaint& paint) {
        // wait for visual content
        if (paint.getColor() != SK_ColorWHITE)
            notEmpty();
    }

    virtual void drawSprite(const SkBitmap& , int , int ,
                            const SkPaint* paint = NULL) {
        DBG_SET_LOG("abort");
        notEmpty();
    }
    
    virtual void drawText(const void* , size_t byteLength, SkScalar , 
                          SkScalar , const SkPaint& paint) {
        DBG_SET_LOGD("abort %d", byteLength);
        notEmpty();
    }

    virtual void drawPosText(const void* , size_t byteLength, 
                             const SkPoint [], const SkPaint& paint) {
        DBG_SET_LOGD("abort %d", byteLength);
        notEmpty();
    }

    virtual void drawPosTextH(const void* , size_t byteLength,
                              const SkScalar [], SkScalar ,
                              const SkPaint& paint) {
        DBG_SET_LOGD("abort %d", byteLength);
        notEmpty();
    }

    virtual void drawTextOnPath(const void* , size_t byteLength, 
                                const SkPath& , const SkMatrix* , 
                                const SkPaint& paint) {
        DBG_SET_LOGD("abort %d", byteLength);
        notEmpty();
    }

    virtual void drawPicture(SkPicture& picture) {
        SkCanvas::drawPicture(picture);
    }
    
    SkPicture* mPicture;
    bool mEmpty;
};

bool PictureSet::emptyPicture(SkPicture* picture) const
{
    IsEmptyBounder isEmptyBounder;
    IsEmptyCanvas checker(&isEmptyBounder, picture);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, mWidth, mHeight);
    checker.setBitmapDevice(bitmap);
    checker.drawPicture(*picture);
    return checker.mEmpty;
}

bool PictureSet::isEmpty() const
{
#ifdef FAST_PICTURESET
    // For now, just assume the pictureset is *not* empty
    // if the hashmap contains something
    for (BucketMap::const_iterator iter = mBuckets.begin(); iter != mBuckets.end(); ++iter) {
        if (iter->second->size() > 0)
            return false;
    }
    return true;
#else
    const Pictures* last = mPictures.end();
    for (const Pictures* working = mPictures.begin(); working != last; working++) {
        if (!working->mEmpty)
            return false;
    }
    return true;
#endif // FAST_PICTURESET
}

void PictureSet::set(const PictureSet& src)
{
    DBG_SET_LOGD("start %p src=%p", this, &src);
    clear();
    setDimensions(src.mWidth, src.mHeight);
#ifdef FAST_PICTURESET
    XLOG("\n--- set picture ---");
    for (BucketMap::const_iterator iter = src.mBuckets.begin();
         iter != src.mBuckets.end(); ++iter) {
         Bucket* sourceBucket = iter->second;
         Bucket* targetBucket = getBucket(iter->first.first-1, iter->first.second-1);
         BucketPicture* first = sourceBucket->begin();
         BucketPicture* last = sourceBucket->end();
         XLOG("set from bucket %x (%d, %d), %d pictures", sourceBucket,
              iter->first.first, iter->first.second, sourceBucket->size());
         for (BucketPicture* current = first; current != last; current++) {
             XLOG("set picture %x from bucket %x in bucket %x (%d, %d)",
                  current->mPicture, sourceBucket, targetBucket,
                  iter->first.first, iter->first.second);
             SkSafeRef(current->mPicture);
             BucketPicture picture = { current->mPicture, current->mArea,
                                       current->mRealArea, current->mBase };
             targetBucket->append(picture);
         }
    }
    XLOG("--- DONE set picture ---\n");
#else
    const Pictures* last = src.mPictures.end();
    for (const Pictures* working = src.mPictures.begin(); working != last; working++)
        add(working);
 //   dump(__FUNCTION__);
    validate(__FUNCTION__);
    DBG_SET_LOG("end");
#endif // FAST_PICTURESET
}

#ifdef FAST_PICTURESET
#else

bool PictureSet::reuseSubdivided(const SkRegion& inval)
{
    validate(__FUNCTION__);

    if (inval.isComplex())
        return false;
    Pictures* working, * last = mPictures.end();
    const SkIRect& invalBounds = inval.getBounds();
    bool steal = false;
    for (working = mPictures.begin(); working != last; working++) {
        if (working->mSplit && invalBounds == working->mUnsplit) {
            steal = true;
            continue;
        }
        if (steal == false)
            continue;
        SkRegion temp = SkRegion(inval);
        temp.op(working->mArea, SkRegion::kIntersect_Op);
        if (temp.isEmpty() || temp == working->mArea)
            continue;
        return false;
    }
    if (steal == false)
        return false;
    for (working = mPictures.begin(); working != last; working++) {
        if ((working->mSplit == false || invalBounds != working->mUnsplit) &&
                inval.contains(working->mArea) == false)
            continue;
        SkSafeUnref(working->mPicture);
        working->mPicture = NULL;
    }
    return true;
}

void PictureSet::setDrawTimes(const PictureSet& src)
{
    validate(__FUNCTION__);
    if (mWidth != src.mWidth || mHeight != src.mHeight)
        return;
    Pictures* last = mPictures.end();
    Pictures* working = mPictures.begin();
    if (working == last)
        return;
    const Pictures* srcLast = src.mPictures.end();
    const Pictures* srcWorking = src.mPictures.begin();
    for (; srcWorking != srcLast; srcWorking++) {
        if (srcWorking->mWroteElapsed == false)
            continue;
        while ((srcWorking->mArea != working->mArea ||
                srcWorking->mPicture != working->mPicture)) {
            if (++working == last)
                return;
        }
        DBG_SET_LOGD("%p [%d] [%d] {%d,%d,r=%d,b=%d} working->mElapsed=%d <- %d",
            this, working - mPictures.begin(), srcWorking - src.mPictures.begin(),
            working->mArea.getBounds().fLeft, working->mArea.getBounds().fTop,
            working->mArea.getBounds().fRight, working->mArea.getBounds().fBottom,
            working->mElapsed, srcWorking->mElapsed);
        working->mElapsed = srcWorking->mElapsed;
    }
}

void PictureSet::setPicture(size_t i, SkPicture* p)
{
    SkSafeUnref(mPictures[i].mPicture);
    mPictures[i].mPicture = p;
    mPictures[i].mEmpty = emptyPicture(p);
}

void PictureSet::split(PictureSet* out) const
{
    dump(__FUNCTION__);
    DBG_SET_LOGD("%p", this);
    SkIRect totalBounds;
    out->mWidth = mWidth;
    out->mHeight = mHeight;
    totalBounds.set(0, 0, mWidth, mHeight);
    SkRegion* total = new SkRegion(totalBounds);
    const Pictures* last = mPictures.end();
    const Pictures* working;
    uint32_t balance = 0;
    int multiUnsplitFastPictures = 0; // > 1 has more than 1
    for (working = mPictures.begin(); working != last; working++) {
        if (working->mElapsed >= MAX_DRAW_TIME || working->mSplit)
            continue;
        if (++multiUnsplitFastPictures > 1)
            break;
    }
    for (working = mPictures.begin(); working != last; working++) {
        uint32_t elapsed = working->mElapsed;
        if (elapsed < MAX_DRAW_TIME) {
            bool split = working->mSplit;
            DBG_SET_LOGD("elapsed=%d working=%p total->getBounds()="
                "{%d,%d,r=%d,b=%d} split=%s", elapsed, working,
                total->getBounds().fLeft, total->getBounds().fTop,
                total->getBounds().fRight, total->getBounds().fBottom,
                split ? "true" : "false");
            if (multiUnsplitFastPictures <= 1 || split) {
                total->op(working->mArea, SkRegion::kDifference_Op);
                out->add(working->mArea, working->mPicture, elapsed, split,
                    working->mEmpty);
            } else if (balance < elapsed)
                balance = elapsed;
            continue;
        }
        total->op(working->mArea, SkRegion::kDifference_Op);
        const SkIRect& bounds = working->mArea.getBounds();
        int width = bounds.width();
        int height = bounds.height();
        int across = 1;
        int down = 1;
        while (height >= MIN_SPLITTABLE || width >= MIN_SPLITTABLE) {
            if (height >= width) {
                height >>= 1;
                down <<= 1;
            } else {
                width >>= 1;
                across <<= 1 ;
            }
            if ((elapsed >>= 1) < MAX_DRAW_TIME)
                break;
        }
        width = bounds.width();
        height = bounds.height();
        int top = bounds.fTop;
        for (int indexY = 0; indexY < down; ) {
            int bottom = bounds.fTop + height * ++indexY / down;
            int left = bounds.fLeft;
            for (int indexX = 0; indexX < across; ) {
                int right = bounds.fLeft + width * ++indexX / across;
                SkIRect cBounds;
                cBounds.set(left, top, right, bottom);
                out->add(SkRegion(cBounds), (across | down) != 1 ? NULL :
                    working->mPicture, elapsed, true, 
                    (across | down) != 1 ? false : working->mEmpty);
                left = right;
            }
            top = bottom;
        }
    }
    DBG_SET_LOGD("%p w=%d h=%d total->isEmpty()=%s multiUnsplitFastPictures=%d",
        this, mWidth, mHeight, total->isEmpty() ? "true" : "false",
        multiUnsplitFastPictures);
    if (!total->isEmpty() && multiUnsplitFastPictures > 1)
        out->add(*total, NULL, balance, false, false);
    delete total;
    validate(__FUNCTION__);
    out->dump("split-out");
}

#endif // FAST_PICTURESET

bool PictureSet::validate(const char* funct) const
{
#ifdef FAST_PICTURESET
    return true;
#else
    bool valid = true;
#if PICTURE_SET_VALIDATE
    SkRegion all;
    const Pictures* first = mPictures.begin();
    for (const Pictures* working = mPictures.end(); working != first; ) {
        --working;
        const SkPicture* pict = working->mPicture;
        const SkRegion& area = working->mArea;
        const SkIRect& bounds = area.getBounds();
        bool localValid = false;
        if (working->mUnsplit.isEmpty())
            LOGD("%s working->mUnsplit.isEmpty()", funct);
        else if (working->mUnsplit.contains(bounds) == false)
            LOGD("%s working->mUnsplit.contains(bounds) == false", funct);
        else if (working->mElapsed >= 1000)
            LOGD("%s working->mElapsed >= 1000", funct);
        else if ((working->mSplit & 0xfe) != 0)
            LOGD("%s (working->mSplit & 0xfe) != 0", funct);
        else if ((working->mWroteElapsed & 0xfe) != 0)
            LOGD("%s (working->mWroteElapsed & 0xfe) != 0", funct);
        else if (pict != NULL) {
            int pictWidth = pict->width();
            int pictHeight = pict->height();
            if (pictWidth < bounds.width())
                LOGD("%s pictWidth=%d < bounds.width()=%d", funct, pictWidth, bounds.width());
            else if (pictHeight < bounds.height())
                LOGD("%s pictHeight=%d < bounds.height()=%d", funct, pictHeight, bounds.height());
            else if (working->mArea.isEmpty())
                LOGD("%s working->mArea.isEmpty()", funct);
            else
                localValid = true;
        } else
            localValid = true;
        working->mArea.validate();
        if (localValid == false) {
            if (all.contains(area) == true)
                LOGD("%s all.contains(area) == true", funct);
            else
                localValid = true;
        }
        valid &= localValid;
        all.op(area, SkRegion::kUnion_Op);
    }
    const SkIRect& allBounds = all.getBounds();
    if (valid) {
        valid = false;
        if (allBounds.width() != mWidth)
            LOGD("%s allBounds.width()=%d != mWidth=%d", funct, allBounds.width(), mWidth);
        else if (allBounds.height() != mHeight)
            LOGD("%s allBounds.height()=%d != mHeight=%d", funct, allBounds.height(), mHeight);
        else
            valid = true;
    }
    while (valid == false)
        ;
#endif
    return valid;
#endif // FAST_PICTURESET
}

} /* namespace android */
