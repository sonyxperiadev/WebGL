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
    // is this the correct order???
    m_transform.setConcat(m_transform, m2.m_transform);
    return *this;
}

AffineTransform AffineTransform::operator* (const AffineTransform &m2)
{
    AffineTransform cat;
    
    // is this the correct order???
    cat.m_transform.setConcat(m_transform, m2.m_transform);
    return cat;
}

}
