/* include/graphics/SkBitmapRef.h
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
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
