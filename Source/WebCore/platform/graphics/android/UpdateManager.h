/*
 * Copyright 2011, The Android Open Source Project
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

#ifndef UpdateManager_h
#define UpdateManager_h

class SkCanvas;
class SkRegion;

namespace WebCore {

class BaseTile;

// UpdateManager Architecture

// The UpdateManager is used to defer updates and invalidations to a layer,
// so that the layer can finish painting one version completely without being
// interrupted by new invals/content

class UpdateManager {
public:
    UpdateManager();

    ~UpdateManager();

    // swap deferred members in place of painting members
    void swap();

    void updateInval(const SkRegion& invalRegion);

    void updatePicture(SkPicture* picture);

    bool paint(BaseTile* tile, SkCanvas* canvas, unsigned int* pictureUsed);

    void clearPaintingInval();

    const SkRegion& getPaintingInval() {
        return m_paintingInval;
    }

    SkPicture* getPaintingPicture() {
        // NOTE: only modified on UI thread, so UI thread access doesn't need mutex
        return m_paintingPicture;
    }

private:
    // protect m_paintingPicture
    //    swap() on UI thread modifies
    //    paint() on texture gen thread reads
    android::Mutex m_paintingPictureSync;
    SkPicture* m_paintingPicture;

    // inval region to be redrawn with the current paintingPicture
    SkRegion m_paintingInval;

    // most recently received picture, moved into painting on swap()
    SkPicture* m_deferredPicture;

    // all invals since last swap(), merged with painting inval on swap()
    SkRegion m_deferredInval;
};

} // namespace WebCore

#endif //#define UpdateManager_h
