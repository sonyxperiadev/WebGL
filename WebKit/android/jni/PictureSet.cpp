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

PictureSet::PictureSet()
{
    mWidth = mHeight = 0;
}

PictureSet::~PictureSet()
{
    clear();
}

void PictureSet::add(const Pictures* temp)
{
    Pictures pictureAndBounds = *temp;
    SkSafeRef(pictureAndBounds.mPicture);
    pictureAndBounds.mWroteElapsed = false;
    mPictures.append(pictureAndBounds);
}

void PictureSet::add(const SkRegion& area, SkPicture* picture,
    uint32_t elapsed, bool split, bool empty)
{
    DBG_SET_LOGD("%p area={%d,%d,r=%d,b=%d} pict=%p elapsed=%d split=%d", this,
        area.getBounds().fLeft, area.getBounds().fTop,
        area.getBounds().fRight, area.getBounds().fBottom, picture,
        elapsed, split);
    SkSafeRef(picture);
    /* if nothing is drawn beneath part of the new picture, mark it as a base */
    SkRegion diff = SkRegion(area);
    Pictures* last = mPictures.end();
    for (Pictures* working = mPictures.begin(); working != last; working++)
        diff.op(working->mArea, SkRegion::kDifference_Op);
    Pictures pictureAndBounds = {area, picture, area.getBounds(),
        elapsed, split, false, diff.isEmpty() == false, empty};
    mPictures.append(pictureAndBounds);
}

/*
Pictures are discarded when they are fully drawn over.
When a picture is partially drawn over, it is discarded if it is not a base, and
its rectangular bounds is reduced if it is a base.
*/
bool PictureSet::build()
{
    bool rebuild = false;
    DBG_SET_LOGD("%p", this);
    // walk pictures back to front, removing or trimming obscured ones
    SkRegion drawn;
    SkRegion inval;
    Pictures* first = mPictures.begin();
    Pictures* last = mPictures.end();
    Pictures* working;
    bool checkForNewBases = false;
    for (working = last; working != first; ) {
        --working;
        SkRegion& area = working->mArea;
        SkRegion visibleArea(area);
        visibleArea.op(drawn, SkRegion::kDifference_Op);
#if PICTURE_SET_DEBUG
        const SkIRect& a = area.getBounds();
        const SkIRect& d = drawn.getBounds();
        const SkIRect& i = inval.getBounds();
        const SkIRect& v = visibleArea.getBounds();
        DBG_SET_LOGD("%p [%d] area={%d,%d,r=%d,b=%d} drawn={%d,%d,r=%d,b=%d}"
            " inval={%d,%d,r=%d,b=%d} vis={%d,%d,r=%d,b=%d}",
            this, working - first,
            a.fLeft, a.fTop, a.fRight, a.fBottom,
            d.fLeft, d.fTop, d.fRight, d.fBottom,
            i.fLeft, i.fTop, i.fRight, i.fBottom,
            v.fLeft, v.fTop, v.fRight, v.fBottom);
#endif
        bool tossPicture = false;
        if (working->mBase == false) {
            if (area != visibleArea) {
                if (visibleArea.isEmpty() == false) {
                    DBG_SET_LOGD("[%d] partially overdrawn", working - first);
                    inval.op(visibleArea, SkRegion::kUnion_Op);
                } else
                    DBG_SET_LOGD("[%d] fully hidden", working - first);
                area.setEmpty();
                tossPicture = true;
            }
        } else {
            const SkIRect& visibleBounds = visibleArea.getBounds();
            const SkIRect& areaBounds = area.getBounds();
            if (visibleBounds != areaBounds) {
                DBG_SET_LOGD("[%d] base to be reduced", working - first);
                area.setRect(visibleBounds);
                checkForNewBases = tossPicture = true;
            }
            if (area.intersects(inval)) {
                DBG_SET_LOGD("[%d] base to be redrawn", working - first);
                tossPicture = true;
            }
        }
        if (tossPicture) {
            SkSafeUnref(working->mPicture);
            working->mPicture = NULL; // mark to redraw
        }
        if (working->mPicture == NULL) // may have been set to null elsewhere
            rebuild = true;
        drawn.op(area, SkRegion::kUnion_Op);
    }
    // collapse out empty regions
    Pictures* writer = first;
    for (working = first; working != last; working++) {
        if (working->mArea.isEmpty())
            continue;
        *writer++ = *working;
    }
#if PICTURE_SET_DEBUG
    if ((unsigned) (writer - first) != mPictures.size())
        DBG_SET_LOGD("shrink=%d (was %d)", writer - first, mPictures.size());
#endif
    mPictures.shrink(writer - first);
    /* When a base is discarded because it was entirely drawn over, all  
       remaining pictures are checked to see if one has become a base. */
    if (checkForNewBases) {
        drawn.setEmpty();
        Pictures* last = mPictures.end();
        for (working = mPictures.begin(); working != last; working++) {
            SkRegion& area = working->mArea;
            if (drawn.contains(working->mArea) == false) {
                working->mBase = true;
                DBG_SET_LOGD("[%d] new base", working - mPictures.begin());
            }
            drawn.op(working->mArea, SkRegion::kUnion_Op);
        }
    }
    validate(__FUNCTION__);
    return rebuild;
}

void PictureSet::checkDimensions(int width, int height, SkRegion* inval)
{
    if (mWidth == width && mHeight == height)
        return;
    DBG_SET_LOGD("%p old:(w=%d,h=%d) new:(w=%d,h=%d)", this, 
        mWidth, mHeight, width, height);
    if (mWidth == width && height > mHeight) { // only grew vertically
        SkIRect rect;
        rect.set(0, mHeight, width, height - mHeight);
        inval->op(rect, SkRegion::kUnion_Op);
    } else {
        clear(); // if both width/height changed, clear the old cache
        inval->setRect(0, 0, width, height);
    }
    mWidth = width;
    mHeight = height;
}

void PictureSet::clear()
{
    DBG_SET_LOG("");
    Pictures* last = mPictures.end();
    for (Pictures* working = mPictures.begin(); working != last; working++) {
        working->mArea.setEmpty();
        SkSafeUnref(working->mPicture);
    }
    mPictures.clear();
    mWidth = mHeight = 0;
}

bool PictureSet::draw(SkCanvas* canvas)
{
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
    const Pictures* last = mPictures.end();
    for (const Pictures* working = mPictures.begin(); working != last; working++) {
        if (!working->mEmpty)
            return false;
    }
    return true;
}

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

void PictureSet::set(const PictureSet& src)
{
    DBG_SET_LOGD("start %p src=%p", this, &src);
    clear();
    mWidth = src.mWidth;
    mHeight = src.mHeight;
    const Pictures* last = src.mPictures.end();
    for (const Pictures* working = src.mPictures.begin(); working != last; working++)
        add(working);
 //   dump(__FUNCTION__);
    validate(__FUNCTION__);
    DBG_SET_LOG("end");
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

bool PictureSet::validate(const char* funct) const
{
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
}

} /* namespace android */
