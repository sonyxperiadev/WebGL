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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// must include config.h first for webkit to fiddle with new/delete
#include "config.h"
#include "SkANP.h"

#ifdef SK_SCALAR_IS_FIXED
static void fromFloat(SkScalar dst[], const float src[], int n) {
    for (int i = 0; i < n; i++) {
        dst[i] = SkFloatToScalar(src[i]);
    }
}

static void toFloat(float dst[], const SkScalar src[], int n) {
    for (int i = 0; i < n; i++) {
        dst[i] = SkScalarToFloat(src[i]);
    }
}
#endif

static ANPMatrix* anp_newMatrix() {
    return new ANPMatrix;
}

static void anp_deleteMatrix(ANPMatrix* matrix) {
    delete matrix;
}

static ANPMatrixFlag anp_getFlags(const ANPMatrix* matrix) {
    return matrix->getType();
}

static void anp_copy(ANPMatrix* dst, const ANPMatrix* src) {
    *dst = *src;
}

static void anp_get3x3(const ANPMatrix* matrix, float dst[9]) {
    for (int i = 0; i < 9; i++) {
        dst[i] = SkScalarToFloat(matrix->get(i));
    }
}

static void anp_set3x3(ANPMatrix* matrix, const float src[9]) {
    for (int i = 0; i < 9; i++) {
        matrix->set(i, SkFloatToScalar(src[i]));
    }
}

static void anp_setIdentity(ANPMatrix* matrix) {
    matrix->reset();
}

static void anp_preTranslate(ANPMatrix* matrix, float tx, float ty) {
    matrix->preTranslate(SkFloatToScalar(tx), SkFloatToScalar(ty));
}

static void anp_postTranslate(ANPMatrix* matrix, float tx, float ty) {
    matrix->postTranslate(SkFloatToScalar(tx), SkFloatToScalar(ty));
}

static void anp_preScale(ANPMatrix* matrix, float sx, float sy) {
    matrix->preScale(SkFloatToScalar(sx), SkFloatToScalar(sy));
}

static void anp_postScale(ANPMatrix* matrix, float sx, float sy) {
    matrix->postScale(SkFloatToScalar(sx), SkFloatToScalar(sy));
}

static void anp_preSkew(ANPMatrix* matrix, float kx, float ky) {
    matrix->preSkew(SkFloatToScalar(kx), SkFloatToScalar(ky));
}

static void anp_postSkew(ANPMatrix* matrix, float kx, float ky) {
    matrix->postSkew(SkFloatToScalar(kx), SkFloatToScalar(ky));
}

static void anp_preRotate(ANPMatrix* matrix, float degrees) {
    matrix->preRotate(SkFloatToScalar(degrees));
}

static void anp_postRotate(ANPMatrix* matrix, float degrees) {
    matrix->postRotate(SkFloatToScalar(degrees));
}

static void anp_preConcat(ANPMatrix* matrix, const ANPMatrix* other) {
    matrix->preConcat(*other);
}

static void anp_postConcat(ANPMatrix* matrix, const ANPMatrix* other) {
    matrix->postConcat(*other);
}

static bool anp_invert(ANPMatrix* dst, const ANPMatrix* src) {
    return src->invert(dst);
}

static void anp_mapPoints(ANPMatrix* matrix, float dst[], const float src[],
                          int32_t count) {
#ifdef SK_SCALAR_IS_FLOAT
    matrix->mapPoints(reinterpret_cast<SkPoint*>(dst),
                      reinterpret_cast<const SkPoint*>(src), count);
#else
    const int N = 64;
    SkPoint tmp[N];
    do {
        int n = count;
        if (n > N) {
            n = N;
        }
        fromFloat(&tmp[0].fX, src, n*2);
        matrix->mapPoints(tmp, n);
        toFloat(dst, &tmp[0].fX, n*2);
        count -= n;
    } while (count > 0);
#endif
}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void ANPMatrixInterfaceV0_Init(ANPInterface* value) {
    ANPMatrixInterfaceV0* i = reinterpret_cast<ANPMatrixInterfaceV0*>(value);

    ASSIGN(i, newMatrix);
    ASSIGN(i, deleteMatrix);
    ASSIGN(i, getFlags);
    ASSIGN(i, copy);
    ASSIGN(i, get3x3);
    ASSIGN(i, set3x3);
    ASSIGN(i, setIdentity);
    ASSIGN(i, preTranslate);
    ASSIGN(i, postTranslate);
    ASSIGN(i, preScale);
    ASSIGN(i, postScale);
    ASSIGN(i, preSkew);
    ASSIGN(i, postSkew);
    ASSIGN(i, preRotate);
    ASSIGN(i, postRotate);
    ASSIGN(i, preConcat);
    ASSIGN(i, postConcat);
    ASSIGN(i, invert);
    ASSIGN(i, mapPoints);
}
