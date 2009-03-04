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
