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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "VerticalTextMap.h"

#include <wtf/Forward.h>
#include <wtf/MessageQueue.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

static const UChar vTextCnvTable[][2] = {
    // TODO: uncomment mappings once we add glyphs for vertical forms.
    // {0x0021, 0xfe15},  // exclamation mark
    {0x0028, 0xfe35},  // left paren
    {0x0029, 0xfe36},  // right paren
    // {0x002c, 0xfe10},  // comma
    {0x003a, 0xfe30},  // colon
    {0x003b, 0x007c},  // hyphen
    // {0x003f, 0xfe16},  // question mark
    // {0x005b, 0xfe14},  // semicolon
    {0x005d, 0xfe47},  // left square bracket
    {0x005f, 0xfe48},  // right square bracket
    {0x007b, 0xfe37},  // left curly bracket
    {0x007d, 0xfe38},  // right curly bracket
    {0x007e, 0x007c},  // tilde to vertical line
    {0x2013, 0xfe32},  // en dash
    {0x2014, 0xfe31},  // em dash
    {0x2015, 0xfe31},  // horizontal bar
    {0x2025, 0xfe30},  // two dot leader
    // TODO: change the mapping 0x2026 -> 0xFE19 once Android has the glyph for 0xFE19.
    {0x2026, 0xfe30},  // three dot leader
    // {0x3001, 0xfe11},  // Ideographic comma
    // {0x3002, 0xfe12},  // Ideographic full stop
    {0x3008, 0xfe3f},  // left angle bracket
    {0x3009, 0xfe40},  // right angle bracket
    {0x300a, 0xfe3d},  // left double angle bracket
    {0x300b, 0xfe3e},  // right double angle bracket
    {0x300c, 0xfe41},  // left corner bracket
    {0x300d, 0xfe42},  // right corner bracket
    {0x300e, 0xfe43},  // left white corner bracket
    {0x300f, 0xfe44},  // right white corner bracket
    {0x3010, 0xfe3b},  // left black lenticular bracket
    {0x3011, 0xfe3c},  // right black lenticular bracket
    {0x3014, 0xfe39},  // left black lenticular bracket
    {0x3015, 0xfe3a},  // right tortise shell bracket
    // {0x3016, 0xfe17},  // left white lenticular bracket
    // {0x3017, 0xfe18},  // right white lenticular bracket
    // {0x3019, 0xfe19},  // horizontal ellipses
    {0x30fc, 0x3021},  // prolonged sound
    {0xfe4f, 0xfe34},  // wavy low line
    {0xff08, 0xfe35},  // full width left paren
    {0xff09, 0xfe36},  // full width right paren
    {0xff3b, 0xfe47},  // full width left square bracket
    {0xff3d, 0xfe48},  // full width right square bracket
    {0xff5b, 0xfe37},  // full width left curly bracket
    {0xff5d, 0xfe38},  // full width right curly bracket
    // {0xff64, 0xfe11},  // halfwidth ideo comma
    // {0xff61, 0xfe12},  // halfwidth ideo full stop
};

namespace WebCore {

static WTF::Mutex verticalTextHashMapMutex;
static HashMap<UChar, UChar>* verticalTextHashMap = 0;

UChar VerticalTextMap::getVerticalForm(UChar c) {
    {
        MutexLocker lock(verticalTextHashMapMutex);
        if (!verticalTextHashMap) {
            // Lazy initialization.
            verticalTextHashMap = new HashMap<UChar, UChar>;
            for (size_t i = 0; i < WTF_ARRAY_LENGTH(vTextCnvTable); ++i)
               verticalTextHashMap->set(vTextCnvTable[i][0], vTextCnvTable[i][1]);
        }
    }
    return verticalTextHashMap->get(c);
}

}
