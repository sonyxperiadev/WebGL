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
#include "AffineTransform.h"

#include "FloatRect.h"
#include "IntRect.h"

#include "android_graphics.h"

namespace WebCore {

static const double deg2rad = 0.017453292519943295769; // pi/180

AffineTransform::AffineTransform()
{
    m_transform.reset();
}
    
AffineTransform::AffineTransform(const SkMatrix& mat) : m_transform(mat) {}

AffineTransform::AffineTransform(double a, double b, double c, double d, double tx, double ty)
{
    m_transform.reset();

    m_transform.set(SkMatrix::kMScaleX, SkDoubleToScalar(a));
    m_transform.set(SkMatrix::kMSkewX, SkDoubleToScalar(b));
    m_transform.set(SkMatrix::kMTransX, SkDoubleToScalar(tx));

    m_transform.set(SkMatrix::kMScaleY, SkDoubleToScalar(d));
    m_transform.set(SkMatrix::kMSkewX, SkDoubleToScalar(c));
    m_transform.set(SkMatrix::kMTransX, SkDoubleToScalar(ty));
}

void AffineTransform::setMatrix(double a, double b, double c, double d, double tx, double ty)
{
    m_transform.set(SkMatrix::kMScaleX, SkDoubleToScalar(a));
    m_transform.set(SkMatrix::kMSkewX, SkDoubleToScalar(b));
    m_transform.set(SkMatrix::kMTransX, SkDoubleToScalar(tx));

    m_transform.set(SkMatrix::kMScaleY, SkDoubleToScalar(d));
    m_transform.set(SkMatrix::kMSkewX, SkDoubleToScalar(c));
    m_transform.set(SkMatrix::kMTransX, SkDoubleToScalar(ty));
}

void AffineTransform::map(double x, double y, double *x2, double *y2) const
{
    SkPoint src, dst;
    src.set(SkDoubleToScalar(x), SkDoubleToScalar(y));
    m_transform.mapPoints(&dst, &src, 1);

    *x2 = SkScalarToDouble(dst.fX);
    *y2 = SkScalarToDouble(dst.fY);
}

IntRect AffineTransform::mapRect(const IntRect &rect) const
{
    SkRect  src, dst;
    SkIRect ir;
    
    android_setrect(&src, rect);
    m_transform.mapRect(&dst, src);
    dst.round(&ir);

    return IntRect(ir.fLeft, ir.fTop, ir.width(), ir.height());
}

FloatRect AffineTransform::mapRect(const FloatRect &rect) const
{
    SkRect  src, dst;
    SkIRect ir;
    
    android_setrect(&src, rect);
    m_transform.mapRect(&dst, src);
    dst.round(&ir);

    return IntRect(ir.fLeft, ir.fTop, ir.width(), ir.height());
}

bool AffineTransform::isIdentity() const
{
    return m_transform.isIdentity();
}

void AffineTransform::reset()
{
    m_transform.reset();
}

    double AffineTransform::a() const {
        return SkScalarToDouble(m_transform[0]);
    }
    void AffineTransform::setA(double a) {
        m_transform.set(0, SkDoubleToScalar(a));
    }

    double AffineTransform::b() const {
        return SkScalarToDouble(m_transform[1]);
    }
    void AffineTransform::setB(double b) {
        m_transform.set(1, SkDoubleToScalar(b));
    }
    
    double AffineTransform::c() const {
        return SkScalarToDouble(m_transform[3]);
    }
    void AffineTransform::setC(double c) {
        m_transform.set(3, SkDoubleToScalar(c));
    }
    
    double AffineTransform::d() const {
        return SkScalarToDouble(m_transform[4]);
    }
    void AffineTransform::setD(double d) {
        m_transform.set(4, SkDoubleToScalar(d));
    }
    
    double AffineTransform::e() const {
        return SkScalarToDouble(m_transform[2]);
    }
    void AffineTransform::setE(double e) {
        m_transform.set(2, SkDoubleToScalar(e));
    }
    
    double AffineTransform::f() const {
        return SkScalarToDouble(m_transform[5]);
    }
    void AffineTransform::setF(double f) {
        m_transform.set(5, SkDoubleToScalar(f));
    }
    
AffineTransform &AffineTransform::scale(double sx, double sy)
{
    m_transform.preScale(SkDoubleToScalar(sx), SkDoubleToScalar(sy));
    return *this;
}

AffineTransform &AffineTransform::rotate(double d)
{
    m_transform.preRotate(SkDoubleToScalar(d));
    return *this;
}

AffineTransform &AffineTransform::translate(double tx, double ty)
{
    m_transform.preTranslate(SkDoubleToScalar(tx), SkDoubleToScalar(ty));
    return *this;
}

AffineTransform &AffineTransform::shear(double sx, double sy)
{
    m_transform.preSkew(SkDoubleToScalar(sx), SkDoubleToScalar(sy));
    return *this;
}

double AffineTransform::det() const
{
    return  SkScalarToDouble(m_transform[SkMatrix::kMScaleX]) * SkScalarToDouble(m_transform[SkMatrix::kMScaleY]) - 
            SkScalarToDouble(m_transform[SkMatrix::kMSkewX]) * SkScalarToDouble(m_transform[SkMatrix::kMSkewY]);
}

AffineTransform AffineTransform::inverse() const
{
    AffineTransform inverse;
    
    m_transform.invert(&inverse.m_transform);

    return inverse;
}

AffineTransform::operator SkMatrix() const
{
    return m_transform;
}

bool AffineTransform::operator==(const AffineTransform &m2) const
{
    return m_transform == m2.m_transform;
}

AffineTransform &AffineTransform::operator*= (const AffineTransform &m2)
{
    m_transform.setConcat(m2.m_transform, m_transform);
    return *this;
}

AffineTransform AffineTransform::operator* (const AffineTransform &m2)
{
    AffineTransform cat;
    
    cat.m_transform.setConcat(m2.m_transform, m_transform);
    return cat;
}

}
