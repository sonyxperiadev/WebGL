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

#ifndef SkBitmapRef_DEFINED
#define SkBitmapRef_DEFINED 

#include "SkRefCnt.h"
#include "SkBitmap.h"

class SkBitmapRef : public SkRefCnt {
public:
    SkBitmapRef() : fOrigWidth(0), fOrigHeight(0), fAccessed(false) {}
    explicit SkBitmapRef(const SkBitmap& src)
        : fBitmap(src),
          fOrigWidth(src.width()),
          fOrigHeight(src.height()),
          fAccessed(false) {}

    const SkBitmap& bitmap() const { return fBitmap; }
    SkBitmap& bitmap() { return fBitmap; }

    int origWidth() const { return fOrigWidth; }
    int origHeight() const { return fOrigHeight; }
    
    void setOrigSize(int width, int height) {
        fOrigWidth = width;
        fOrigHeight = height;
    }
    // return true if this is not the first access
    // mark it true so all subsequent calls return true
    bool accessed() { bool result = fAccessed; 
        fAccessed = true; return result; }

private:
    SkBitmap fBitmap;
    int      fOrigWidth, fOrigHeight;
    bool     fAccessed;
};

#endif
