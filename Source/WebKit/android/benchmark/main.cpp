/*
 * Copyright 2009, The Android Open Source Project
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

#define LOG_TAG "webcore_test"

#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <utils/Log.h>

namespace android {
extern void benchmark(const char*, int, int ,int);
}

int main(int argc, char** argv) {
    int width = 800;
    int height = 600;
    int reloadCount = 0;
    while (true) {
        int c = getopt(argc, argv, "d:r:");
        if (c == -1)
            break;
        else if (c == 'd') {
            char* x = strchr(optarg, 'x');
            if (x) {
                width = atoi(optarg);
                height = atoi(x + 1);
                LOGD("Rendering page at %dx%d", width, height);
            }
        } else if (c == 'r') {
            reloadCount = atoi(optarg);
            if (reloadCount < 0)
                reloadCount = 0;
            LOGD("Reloading %d times", reloadCount);
        }
    }
    if (optind >= argc) {
        LOGE("Please supply a file to read\n");
        return 1;
    }

    android::benchmark(argv[optind], reloadCount, width, height);
}
