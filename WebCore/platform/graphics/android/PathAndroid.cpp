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
#include "Path.h"
#include "FloatRect.h"
#include "AffineTransform.h"

#include "SkPath.h"
#include "SkRegion.h"

#include "android_graphics.h"

namespace WebCore {

Path::Path()
{
    m_path = new SkPath;
//    m_path->setFlags(SkPath::kWinding_FillType);
}

Path::Path(const Path& other)
{
    m_path = new SkPath(*other.m_path);
}

Path::~Path()
{
    delete m_path;
}

Path& Path::operator=(const Path& other)
{
    *m_path = *other.m_path;
    return *this;
}

bool Path::isEmpty() const
{
    return m_path->isEmpty();
}

bool Path::contains(const FloatPoint& point, WindRule rule) const
{
    SkRegion    rgn, clip;
    
    int x = (int)floorf(point.x());
    int y = (int)floorf(point.y());
    clip.setRect(x, y, x + 1, y + 1);
    
    SkPath::FillType ft = m_path->getFillType();    // save
    m_path->setFillType(rule == RULE_NONZERO ? SkPath::kWinding_FillType : SkPath::kEvenOdd_FillType);

    bool contains = rgn.setPath(*m_path, clip);
    
    m_path->setFillType(ft);    // restore
    return contains;
}

void Path::translate(const FloatSize& size)
{
    m_path->offset(SkFloatToScalar(size.width()), SkFloatToScalar(size.height()));
}

FloatRect Path::boundingRect() const
{
    SkRect  r;
    
    m_path->computeBounds(&r, SkPath::kExact_BoundsType);
    return FloatRect(   SkScalarToFloat(r.fLeft),
                        SkScalarToFloat(r.fTop),
                        SkScalarToFloat(r.width()),
                        SkScalarToFloat(r.height()));
}

void Path::moveTo(const FloatPoint& point)
{
    m_path->moveTo(SkFloatToScalar(point.x()), SkFloatToScalar(point.y()));
}

void Path::addLineTo(const FloatPoint& p)
{
    m_path->lineTo(SkFloatToScalar(p.x()), SkFloatToScalar(p.y()));
}

void Path::addQuadCurveTo(const FloatPoint& cp, const FloatPoint& ep)
{
    m_path->quadTo( SkFloatToScalar(cp.x()), SkFloatToScalar(cp.y()),
                    SkFloatToScalar(ep.x()), SkFloatToScalar(ep.y()));
}

void Path::addBezierCurveTo(const FloatPoint& p1, const FloatPoint& p2, const FloatPoint& ep)
{
    m_path->cubicTo(SkFloatToScalar(p1.x()), SkFloatToScalar(p1.y()),
                    SkFloatToScalar(p2.x()), SkFloatToScalar(p2.y()),
                    SkFloatToScalar(ep.x()), SkFloatToScalar(ep.y()));
}

void Path::addArcTo(const FloatPoint& p1, const FloatPoint& p2, float radius)
{
    m_path->arcTo(SkFloatToScalar(p1.x()), SkFloatToScalar(p1.y()),
                  SkFloatToScalar(p2.x()), SkFloatToScalar(p2.y()),
                  SkFloatToScalar(radius));
}

void Path::closeSubpath()
{
    m_path->close();
}

static const float gPI = 3.1415926f;

void Path::addArc(const FloatPoint& p, float r, float sa, float ea,
                  bool clockwise) {
    SkScalar    cx = SkFloatToScalar(p.x());
    SkScalar    cy = SkFloatToScalar(p.y());
    SkScalar    radius = SkFloatToScalar(r);

    SkRect  oval;
    oval.set(cx - radius, cy - radius, cx + radius, cy + radius);
    
    float sweep = ea - sa;
    // check for a circle
    if (sweep >= 2*gPI || sweep <= -2*gPI) {
        m_path->addOval(oval);
    } else {
        SkScalar startDegrees = SkFloatToScalar(sa * 180 / gPI);
        SkScalar sweepDegrees = SkFloatToScalar(sweep * 180 / gPI);

        if (clockwise && sweepDegrees > 0) {
            sweepDegrees -= SkIntToScalar(360);
        } else if (!clockwise && sweepDegrees < 0) {
            sweepDegrees = SkIntToScalar(360) - sweepDegrees;
        }

//        SkDebugf("addArc sa=%g ea=%g cw=%d start=%g sweep=%g\n", sa, ea, clockwise,
//                 SkScalarToFloat(startDegrees), SkScalarToFloat(sweepDegrees));

        m_path->arcTo(oval, startDegrees, sweepDegrees, false);
    }
}

void Path::addRect(const FloatRect& rect)
{
    SkRect  r;
    
    android_setrect(&r, rect);
    m_path->addRect(r);
}

void Path::addEllipse(const FloatRect& rect)
{
    SkRect  r;
    
    android_setrect(&r, rect);
    m_path->addOval(r);
}

void Path::clear()
{
    m_path->reset();
}

static FloatPoint* setfpts(FloatPoint dst[], const SkPoint src[], int count)
{
    for (int i = 0; i < count; i++)
    {
        dst[i].setX(SkScalarToFloat(src[i].fX));
        dst[i].setY(SkScalarToFloat(src[i].fY));
    }
    return dst;
}

void Path::apply(void* info, PathApplierFunction function) const
{
    SkPath::Iter    iter(*m_path, false);
    SkPoint         pts[4];
    
    PathElement     elem;
    FloatPoint      fpts[3];

    for (;;)
    {
        switch (iter.next(pts)) {
        case SkPath::kMove_Verb:
            elem.type = PathElementMoveToPoint;
            elem.points = setfpts(fpts, &pts[0], 1);
            break;
        case SkPath::kLine_Verb:
            elem.type = PathElementAddLineToPoint;
            elem.points = setfpts(fpts, &pts[1], 1);
            break;
        case SkPath::kQuad_Verb:
            elem.type = PathElementAddQuadCurveToPoint;
            elem.points = setfpts(fpts, &pts[1], 2);
            break;
        case SkPath::kCubic_Verb:
            elem.type = PathElementAddCurveToPoint;
            elem.points = setfpts(fpts, &pts[1], 3);
            break;
        case SkPath::kClose_Verb:
            elem.type = PathElementCloseSubpath;
            elem.points = NULL;
            break;
        case SkPath::kDone_Verb:
            return;
        }
        function(info, &elem);
    }
}

void Path::transform(const AffineTransform& xform)
{
    m_path->transform(xform);
}

}
