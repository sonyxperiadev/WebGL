/* 
**
** Copyright 2007, The Android Open Source Project
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

#include "config.h"

namespace std
{
    void sort(const void** start, const void** end, const void** temp, Comparator comp) 
    {
        size_t endlen = end - start;
        size_t midlen = endlen / 2;
        const void** mid = start + midlen;
        if (midlen > 1)
            sort(start, mid, temp, comp);
        if (end - mid > 1)
            sort(mid, end, temp, comp);
        memcpy(temp, start, midlen * sizeof(*start));
        size_t i = 0;
        size_t j = midlen;
        size_t off = 0;
        while (i < midlen && j < endlen) 
            start[off++] = (*comp)(start[j], temp[i]) ? start[j++] : temp[i++];
        if (i < midlen)
            memcpy(&start[off], &temp[i], (midlen - i) * sizeof(*start));
    }

    void sort(const void** start, const void** end, Comparator comp) {
        if (end - start > 1) {
            const void** temp = new sortType[(end - start) / 2];
            sort(start, end, temp, comp);
            delete[] temp;
        }
    }
}
