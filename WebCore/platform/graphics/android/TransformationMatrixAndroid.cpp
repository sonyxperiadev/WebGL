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
#include "TransformationMatrix.h"

#include "FloatRect.h"
#include "IntRect.h"

#include "android_graphics.h"

namespace WebCore {

static const double deg2rad = 0.017453292519943295769; // pi/180

TransformationMatrix::TransformationMatrix()
{
    m_transform.reset();
}
    
TransformationMatrix::TransformationMatrix(const SkMatrix& mat) : m_transform(mat) {}

TransformationMatrix::TransformationMatrix(double a, double b, double c, double d, double tx, double ty)
{
    m_transform.reset();

    m_transform.set(SkMatrix::kMScaleX, SkDoubleToScalar(a));
    m_transform.set(SkMatrix::kMSkewX, SkDoubleToScalar(b));
    m_transform.set(SkMatrix::kMTransX, SkDoubleToScalar(tx));

    m_transform.set(SkMatrix::kMScaleY, SkDoubleToScalar(d));
    m_transform.set(SkMatrix::kMSkewX, SkDoubleToScalar(c));
    m_transform.set(SkMatrix::kMTransX, SkDoubleToScalar(ty));
}

void TransformationMatrix::setMatrix(double a, double b, double c, double d, double tx, double ty)
{
    m_transform.set(SkMatrix::kMScaleX, SkDoubleToScalar(a));
    m_transform.set(SkMatrix::kMSkewX, SkDoubleToScalar(b));
    m_transform.set(SkMatrix::kMTransX, SkDoubleToScalar(tx));

    m_transform.set(SkMatrix::kMScaleY, SkDoubleToScalar(d));
    m_transform.set(SkMatrix::kMSkewX, SkDoubleToScalar(c));
    m_transform.set(SkMatrix::kMTransX, SkDoubleToScalar(ty));
}

void TransformationMatrix::map(double x, double y, double *x2, double *y2) const
{
    SkPoint pt;

    m_transform.mapXY(SkDoubleToScalar(x), SkDoubleToScalar(y), &pt);
    *x2 = SkScalarToDouble(pt.fX);
    *y2 = SkScalarToDouble(pt.fY);
}

IntRect TransformationMatrix::mapRect(const IntRect &rect) const
{
    SkRect  src, dst;
    SkIRect ir;
    
    android_setrect(&src, rect);
    m_transform.mapRect(&dst, src);
    // we round out to mimic enclosingIntRect()
    dst.roundOut(&ir);

    return IntRect(ir.fLeft, ir.fTop, ir.width(), ir.height());
}

FloatRect TransformationMatrix::mapRect(const FloatRect &rect) const
{
    SkRect  r;
    
    android_setrect(&r, rect);
    m_transform.mapRect(&r);

    return FloatRect(r.fLeft, r.fTop, r.width(), r.height());
}

bool TransformationMatrix::isIdentity() const
{
    return m_transform.isIdentity();
}

void TransformationMatrix::reset()
{
    m_transform.reset();
}

double TransformationMatrix::a() const
{
    return SkScalarToDouble(m_transform[0]);
}

void TransformationMatrix::setA(double a)
{
    m_transform.set(0, SkDoubleToScalar(a));
}

double TransformationMatrix::b() const
{
    return SkScalarToDouble(m_transform[1]);
}

void TransformationMatrix::setB(double b)
{
    m_transform.set(1, SkDoubleToScalar(b));
}

double TransformationMatrix::c() const
{
    return SkScalarToDouble(m_transform[3]);
}

void TransformationMatrix::setC(double c)
{
    m_transform.set(3, SkDoubleToScalar(c));
}

double TransformationMatrix::d() const {
    return SkScalarToDouble(m_transform[4]);
}

void TransformationMatrix::setD(double d)
{
    m_transform.set(4, SkDoubleToScalar(d));
}

double TransformationMatrix::e() const
{
    return SkScalarToDouble(m_transform[2]);
}

void TransformationMatrix::setE(double e)
{
    m_transform.set(2, SkDoubleToScalar(e));
}

double TransformationMatrix::f() const {
    return SkScalarToDouble(m_transform[5]);
}
void TransformationMatrix::setF(double f) {
    m_transform.set(5, SkDoubleToScalar(f));
}
    
TransformationMatrix &TransformationMatrix::scale(double sx, double sy)
{
    m_transform.preScale(SkDoubleToScalar(sx), SkDoubleToScalar(sy));
    return *this;
}

TransformationMatrix &TransformationMatrix::rotate(double d)
{
    m_transform.preRotate(SkDoubleToScalar(d));
    return *this;
}

TransformationMatrix &TransformationMatrix::translate(double tx, double ty)
{
    m_transform.preTranslate(SkDoubleToScalar(tx), SkDoubleToScalar(ty));
    return *this;
}

TransformationMatrix &TransformationMatrix::shear(double sx, double sy)
{
    m_transform.preSkew(SkDoubleToScalar(sx), SkDoubleToScalar(sy));
    return *this;
}

double TransformationMatrix::det() const
{
    return  SkScalarToDouble(m_transform[SkMatrix::kMScaleX]) * SkScalarToDouble(m_transform[SkMatrix::kMScaleY]) - 
            SkScalarToDouble(m_transform[SkMatrix::kMSkewX]) * SkScalarToDouble(m_transform[SkMatrix::kMSkewY]);
}

TransformationMatrix TransformationMatrix::inverse() const
{
    // the constructor initializes inverse to the identity
    TransformationMatrix inverse;
    
    // if we are not invertible, inverse will stay identity
    m_transform.invert(&inverse.m_transform);

    return inverse;
}

TransformationMatrix::operator SkMatrix() const
{
    return m_transform;
}

bool TransformationMatrix::operator==(const TransformationMatrix &m2) const
{
    return m_transform == m2.m_transform;
}

TransformationMatrix &TransformationMatrix::operator*= (const TransformationMatrix &m2)
{
    m_transform.setConcat(m2.m_transform, m_transform);
    return *this;
}

TransformationMatrix TransformationMatrix::operator* (const TransformationMatrix &m2)
{
    TransformationMatrix cat;
    
    cat.m_transform.setConcat(m2.m_transform, m_transform);
    return cat;
}

}
