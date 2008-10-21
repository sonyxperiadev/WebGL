/* 
 *
 * Copyright 2006, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *
 *     http://www.apache.org/licenses/LICENSE-2.0 
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License.
 */

#include "config.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "Path.h"

#include "SkBlurDrawLooper.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkDashPathEffect.h"
#include "SkDevice.h"
#include "SkPaint.h"
#include "SkPorterDuff.h"
#include "WebCoreViewBridge.h"
#include "PlatformGraphicsContext.h"
#include "AffineTransform.h"

#include "android_graphics.h"
#include "SkGradientShader.h"
#include "SkBitmapRef.h"
#include "SkString.h"

using namespace std;

#define GC2Canvas(ctx)  (ctx)->m_data->mPgc->mCanvas

namespace WebCore {

static int RoundToInt(float x)
{
    return (int)roundf(x);
}

/*  TODO / questions

    mAlpha: how does this interact with the alpha in Color? multiply them together?
    mPorterDuffMode: do I always respect this? If so, then
                     the rgb() & 0xFF000000 check will abort drawing too often
    Is Color premultiplied or not? If it is, then I can't blindly pass it to paint.setColor()
*/

class GraphicsContextPlatformPrivate {
public:
    GraphicsContext*             mCG;        // back-ptr to our parent
    PlatformGraphicsContext*     mPgc;

    struct State {
        float               mMiterLimit;
        float               mAlpha;
        SkDrawLooper*       mLooper;
        SkPaint::Cap        mLineCap;
        SkPaint::Join       mLineJoin;
        SkPorterDuff::Mode  mPorterDuffMode;
        int                 mDashRatio; //ratio of the length of a dash to its width
        
        State()
        {
            mMiterLimit      = 4;
            mAlpha           = 1;
            mLooper          = NULL;
            mLineCap         = SkPaint::kDefault_Cap;
            mLineJoin        = SkPaint::kDefault_Join;
            mPorterDuffMode  = SkPorterDuff::kSrcOver_Mode;
            mDashRatio       = 3;
        }
        
        State(const State& other)
        {
            other.mLooper->safeRef();
            memcpy(this, &other, sizeof(State));
        }
        
        ~State()
        {
            mLooper->safeUnref();
        }
        
        SkDrawLooper* setDrawLooper(SkDrawLooper* dl)
        {
            SkRefCnt_SafeAssign(mLooper, dl);
            return dl;
        }

        SkColor applyAlpha(SkColor c) const
        {
            int s = RoundToInt(mAlpha * 256);
            if (s >= 256)
                return c;           
            if (s < 0)
                return 0;
            
            int a = SkAlphaMul(SkColorGetA(c), s);
            return (c & 0x00FFFFFF) | (a << 24);
        }
    };
    
    SkDeque mStateStack;
    State*  mState;
    
    GraphicsContextPlatformPrivate(GraphicsContext* cg, PlatformGraphicsContext* pgc)
            : mCG(cg)
            , mPgc(pgc), mStateStack(sizeof(State))

    {
        State* state = (State*)mStateStack.push_back();
        new (state) State();
        mState = state;
    }
    
    ~GraphicsContextPlatformPrivate()
    {
        if (mPgc && mPgc->deleteUs())
            delete mPgc;

        // we force restores so we don't leak any subobjects owned by our
        // stack of State records.
        while (mStateStack.count() > 0)
            this->restore();
    }
    
    void save()
    {
        State* newState = (State*)mStateStack.push_back();
        new (newState) State(*mState);
        mState = newState;
    }
    
    void restore()
    {
        mState->~State();
        mStateStack.pop_back();
        mState = (State*)mStateStack.back();
    }
    
    void setup_paint_common(SkPaint* paint) const
    {
#ifdef SK_DEBUGx
        {
            SkPaint defaultPaint;
            
            SkASSERT(*paint == defaultPaint);
        }
#endif

        paint->setAntiAlias(true);
        paint->setDither(true);
        paint->setPorterDuffXfermode(mState->mPorterDuffMode);
        paint->setLooper(mState->mLooper);
    }

    void setup_paint_fill(SkPaint* paint) const
    {
        this->setup_paint_common(paint);

        paint->setColor(mState->applyAlpha(mCG->fillColor().rgb()));
    }

    /*  sets up the paint for stroking. Returns true if the style is really
        just a dash of squares (the size of the paint's stroke-width.
    */
    bool setup_paint_stroke(SkPaint* paint, SkRect* rect) {
        this->setup_paint_common(paint);
        
        float width = mCG->strokeThickness();

        //this allows dashing and dotting to work properly for hairline strokes
        if (0 == width) {
            width = 1;
        }

        paint->setColor(mState->applyAlpha(mCG->strokeColor().rgb()));
        paint->setStyle(SkPaint::kStroke_Style);
        paint->setStrokeWidth(SkFloatToScalar(width));
        paint->setStrokeCap(mState->mLineCap);
        paint->setStrokeJoin(mState->mLineJoin);
        paint->setStrokeMiter(SkFloatToScalar(mState->mMiterLimit));

        if (rect != NULL && (RoundToInt(width) & 1)) {
            rect->inset(-SK_ScalarHalf, -SK_ScalarHalf);
        }

        switch (mCG->strokeStyle()) {
            case NoStroke:
            case SolidStroke:
                width = 0;
                break;
            case DashedStroke:
                width = mState->mDashRatio * width;
                break;
                /* no break */
            case DottedStroke:
                break;
        }

        if (width > 0) {
            SkScalar intervals[] = { width, width };
            SkPathEffect* pe = new SkDashPathEffect(intervals, 2, 0);
            paint->setPathEffect(pe)->unref();
            // return true if we're basically a dotted dash of squares
            return RoundToInt(width) == RoundToInt(paint->getStrokeWidth());
        }
        return false;
    }

private:
    // not supported yet
    State& operator=(const State&);
};

////////////////////////////////////////////////////////////////////////////////////////////////

GraphicsContext* GraphicsContext::createOffscreenContext(int width, int height)
{
    PlatformGraphicsContext* pgc = new PlatformGraphicsContext();
    
    SkBitmap    bitmap;
    
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    pgc->mCanvas->setBitmapDevice(bitmap);
    
    GraphicsContext* ctx = new GraphicsContext(pgc);
//printf("-------- create offscreen <canvas> %p\n", ctx);
    return ctx;
}

void GraphicsContext::drawOffscreenContext(GraphicsContext* ctx, const FloatRect* srcRect, const FloatRect& dstRect)
{
    const SkBitmap& bm = GC2Canvas(ctx)->getDevice()->accessBitmap(false);
    SkIRect         src;
    SkRect          dst;
    SkPaint         paint;
    
    paint.setFilterBitmap(true);
    
    GC2Canvas(this)->drawBitmapRect(bm,
                                    srcRect ? android_setrect(&src, *srcRect) : NULL,
                                    *android_setrect(&dst, dstRect),
                                    &paint);
}

FloatRect GraphicsContext::getClipLocalBounds() const
{
    SkRect r;

    if (!GC2Canvas(this)->getClipBounds(&r))
        r.setEmpty();

    return FloatRect(SkScalarToFloat(r.fLeft), SkScalarToFloat(r.fTop),
                     SkScalarToFloat(r.width()), SkScalarToFloat(r.height()));
}

////////////////////////////////////////////////////////////////////////////////////////////////

GraphicsContext::GraphicsContext(PlatformGraphicsContext *gc)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate(this, gc))
{
    setPaintingDisabled(NULL == gc || NULL == gc->mCanvas);
}

GraphicsContext::~GraphicsContext()
{
    delete m_data;
    this->destroyGraphicsContextPrivate(m_common);
}

void GraphicsContext::savePlatformState()
{
    // save our private State
    m_data->save();
    // save our native canvas
    GC2Canvas(this)->save();
}

void GraphicsContext::restorePlatformState()
{
    // restore our native canvas
    GC2Canvas(this)->restore();
    // restore our private State
    m_data->restore();
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    SkRect  r;

    android_setrect(&r, rect);

    if (fillColor().alpha()) {
        m_data->setup_paint_fill(&paint);
        GC2Canvas(this)->drawRect(r, paint);
    }
    
    if (strokeStyle() != NoStroke && strokeColor().alpha()) {
        paint.reset();
        m_data->setup_paint_stroke(&paint, &r);
        GC2Canvas(this)->drawRect(r, paint);
    }
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    StrokeStyle style = strokeStyle();
    if (style == NoStroke)
        return;

    SkPaint paint;
    SkCanvas* canvas = GC2Canvas(this);
    const int idx = SkAbs32(point2.x() - point1.x());
    const int idy = SkAbs32(point2.y() - point1.y());

    // special-case horizontal and vertical lines that are really just dots
    if (m_data->setup_paint_stroke(&paint, NULL) && (0 == idx || 0 == idy)) {
        const SkScalar diameter = paint.getStrokeWidth();
        const SkScalar radius = SkScalarHalf(diameter);
        SkScalar x = SkIntToScalar(SkMin32(point1.x(), point2.x()));
        SkScalar y = SkIntToScalar(SkMin32(point1.y(), point2.y()));
        SkScalar dx, dy;
        int count;
        SkRect bounds;
        
        if (0 == idy) { // horizontal
            bounds.set(x, y - radius, x + SkIntToScalar(idx), y + radius);
            x += radius;
            dx = diameter * 2;
            dy = 0;
            count = idx;
        } else {        // vertical
            bounds.set(x - radius, y, x + radius, y + SkIntToScalar(idy));
            y += radius;
            dx = 0;
            dy = diameter * 2;
            count = idy;
        }

        // the actual count is the number of ONs we hit alternating
        // ON(diameter), OFF(diameter), ...
        {
            SkScalar width = SkScalarDiv(SkIntToScalar(count), diameter);
            // now computer the number of cells (ON and OFF)
            count = SkScalarRound(width);
            // now compute the number of ONs
            count = (count + 1) >> 1;
        }
        
        SkAutoMalloc storage(count * sizeof(SkPoint));
        SkPoint* verts = (SkPoint*)storage.get();
        // now build the array of vertices to past to drawPoints
        for (int i = 0; i < count; i++) {
            verts[i].set(x, y);
            x += dx;
            y += dy;
        }
        
        paint.setStyle(SkPaint::kFill_Style);
        paint.setPathEffect(NULL);
        
        //  clipping to bounds is not required for correctness, but it does
        //  allow us to reject the entire array of points if we are completely
        //  offscreen. This is common in a webpage for android, where most of
        //  the content is clipped out. If drawPoints took an (optional) bounds
        //  parameter, that might even be better, as we would *just* use it for
        //  culling, and not both wacking the canvas' save/restore stack.
        canvas->save(SkCanvas::kClip_SaveFlag);
        canvas->clipRect(bounds);
        canvas->drawPoints(SkCanvas::kPoints_PointMode, count, verts, paint);
        canvas->restore();
    } else {
        SkPoint pts[2];
        android_setpt(&pts[0], point1);
        android_setpt(&pts[1], point2);
        canvas->drawLine(pts[0].fX, pts[0].fY, pts[1].fX, pts[1].fY, paint);
    }
}

static void setrect_for_underline(SkRect* r, GraphicsContext* context, const IntPoint& point, int yOffset, int width)
{
    float lineThickness = context->strokeThickness();
//    if (lineThickness < 1)  // do we really need/want this?
//        lineThickness = 1;

    yOffset += 1;   // we add 1 to have underlines appear below the text
    
    r->fLeft    = SkIntToScalar(point.x());
    r->fTop     = SkIntToScalar(point.y() + yOffset);
    r->fRight   = r->fLeft + SkIntToScalar(width);
    r->fBottom  = r->fTop + SkFloatToScalar(lineThickness);
}

void GraphicsContext::drawLineForText(IntPoint const& pt, int width, bool)
{
    if (paintingDisabled())
        return;
    
    SkRect  r;
    setrect_for_underline(&r, this, pt, 0, width);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(this->strokeColor().rgb());

    GC2Canvas(this)->drawRect(r, paint);
}

void GraphicsContext::drawLineForMisspellingOrBadGrammar(const IntPoint& pt, int width, bool grammar)
{
    if (paintingDisabled())
        return;
    
    SkRect  r;
    setrect_for_underline(&r, this, pt, 0, width);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorRED);    // is this specified somewhere?

    GC2Canvas(this)->drawRect(r, paint);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    SkRect  oval;
    
    android_setrect(&oval, rect);

    if (fillColor().rgb() & 0xFF000000) {
        m_data->setup_paint_fill(&paint);
        GC2Canvas(this)->drawOval(oval, paint);
    }
    if (strokeStyle() != NoStroke) {
        paint.reset();
        m_data->setup_paint_stroke(&paint, &oval);
        GC2Canvas(this)->drawOval(oval, paint);
    }
}

static inline int fast_mod(int value, int max)
{
    int sign = SkExtractSign(value);

    value = SkApplySign(value, sign);
    if (value >= max) {
        value %= max;
    }
    return SkApplySign(value, sign);
}

void GraphicsContext::strokeArc(const IntRect& r, int startAngle, int angleSpan)
{
    if (paintingDisabled())
        return;

    SkPath  path;
    SkPaint paint;
    SkRect  oval;
    
    android_setrect(&oval, r);

    if (strokeStyle() == NoStroke) {
        m_data->setup_paint_fill(&paint);   // we want the fill color
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(SkFloatToScalar(this->strokeThickness()));
    }
    else {
        m_data->setup_paint_stroke(&paint, NULL);
    }

    // we do this before converting to scalar, so we don't overflow SkFixed
    startAngle = fast_mod(startAngle, 360);
    angleSpan = fast_mod(angleSpan, 360);

    path.addArc(oval, SkIntToScalar(-startAngle), SkIntToScalar(-angleSpan));    
    GC2Canvas(this)->drawPath(path, paint);
}

void GraphicsContext::drawConvexPolygon(size_t numPoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    SkPaint paint;
    SkPath  path;

    path.incReserve(numPoints);
    path.moveTo(SkFloatToScalar(points[0].x()), SkFloatToScalar(points[0].y()));
    for (size_t i = 1; i < numPoints; i++)
        path.lineTo(SkFloatToScalar(points[i].x()), SkFloatToScalar(points[i].y()));

    if (fillColor().rgb() & 0xFF000000) {
        m_data->setup_paint_fill(&paint);
        GC2Canvas(this)->drawPath(path, paint);
    }

    if (strokeStyle() != NoStroke) {
        paint.reset();
        m_data->setup_paint_stroke(&paint, NULL);
        GC2Canvas(this)->drawPath(path, paint);
    }
}

#ifdef ANDROID_CANVAS_IMPL

static void check_set_shader(SkPaint* paint, SkShader* s0, SkShader* s1)
{
    if (s0) {
        paint->setShader(s0);
    }
    else if (s1) {
        paint->setShader(s1);
    }
}

void GraphicsContext::fillPath(const Path& webCorePath, PlatformGradient* grad, PlatformPattern* pat)
{
    if (paintingDisabled())
        return;

    SkPaint paint;

    m_data->setup_paint_fill(&paint);
    check_set_shader(&paint, grad, pat);

    const SkPath& path = *webCorePath.platformPath();
    
#if 0
    SkDebugf("---- fillPath\n");
    SkPath::Iter iter(path, false);
    SkPoint      pts[4];
    for (;;) {
        SkString str;
        const SkPoint* p = &pts[1];
        int n = 0;
        const char* name = "";
        switch (iter.next(pts)) {
            case SkPath::kMove_Verb:
                name = " M";
                p = &pts[0];
                n = 1;
                break;
            case SkPath::kLine_Verb:
                name = " L";
                n = 1;
                break;
            case SkPath::kQuad_Verb:
                name = " Q";
                n = 2;
                break;
            case SkPath::kCubic_Verb:
                name = " C";
                n = 3;
                break;
            case SkPath::kClose_Verb:
                name = " X";
                n = 0;
                break;
            case SkPath::kDone_Verb:
                goto DONE;
        }
        str.append(name);
        for (int i = 0; i < n; i++) {
            str.append(" ");
            str.appendScalar(p[i].fX);
            str.append(" ");
            str.appendScalar(p[i].fY);
        }
        SkDebugf("\"%s\"\n", str.c_str());
    }
DONE:
#endif

    GC2Canvas(this)->drawPath(path, paint);
}

void GraphicsContext::strokePath(const Path& webCorePath, PlatformGradient* grad, PlatformPattern* pat)
{
    if (paintingDisabled())
        return;

    SkPaint paint;

    m_data->setup_paint_stroke(&paint, NULL);
    check_set_shader(&paint, grad, pat);
    
    GC2Canvas(this)->drawPath(*webCorePath.platformPath(), paint);
}

void GraphicsContext::fillRect(const FloatRect& rect, PlatformGradient* grad, PlatformPattern* pat)
{
    if (paintingDisabled())
        return;

    SkRect  r;
    SkPaint paint;

    m_data->setup_paint_fill(&paint);
    check_set_shader(&paint, grad, pat);

    GC2Canvas(this)->drawRect(*android_setrect(&r, rect), paint);
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
                                      const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    SkPath  path;
    SkScalar radii[8];
    SkRect  r;
    
    radii[0] = SkIntToScalar(topLeft.width());
    radii[1] = SkIntToScalar(topLeft.height());
    radii[2] = SkIntToScalar(topRight.width());
    radii[3] = SkIntToScalar(topRight.height());
    radii[4] = SkIntToScalar(bottomRight.width());
    radii[5] = SkIntToScalar(bottomRight.height());
    radii[6] = SkIntToScalar(bottomLeft.width());
    radii[7] = SkIntToScalar(bottomLeft.height());
    path.addRoundRect(*android_setrect(&r, rect), radii);

    m_data->setup_paint_fill(&paint);
    GC2Canvas(this)->drawPath(path, paint);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth, PlatformGradient* grad, PlatformPattern* pat)
{
    if (paintingDisabled())
        return;

    SkRect  r;
    SkPaint paint;

    m_data->setup_paint_stroke(&paint, NULL);
    paint.setStrokeWidth(SkFloatToScalar(lineWidth));
    check_set_shader(&paint, grad, pat);

    GC2Canvas(this)->drawRect(*android_setrect(&r, rect), paint);
}

static U8CPU F2B(float x)
{
    return (int)(x * 255);
}

static SkColor make_color(float a, float r, float g, float b)
{
    return SkColorSetARGB(F2B(a), F2B(r), F2B(g), F2B(b));
}

PlatformGradient* GraphicsContext::newPlatformLinearGradient(const FloatPoint& p0, const FloatPoint& p1,
                                                       const float stopData[5], int count)
{
    SkPoint pts[2];
    
    android_setpt(&pts[0], p0);
    android_setpt(&pts[1], p1);
    
    SkAutoMalloc    storage(count * (sizeof(SkColor) + sizeof(SkScalar)));
    SkColor*        colors = (SkColor*)storage.get();
    SkScalar*       pos = (SkScalar*)(colors + count);
    
    for (int i = 0; i < count; i++)
    {
        pos[i] = SkFloatToScalar(stopData[0]);
        colors[i] = make_color(stopData[4], stopData[1], stopData[2], stopData[3]);
        stopData += 5;
    }

    return SkGradientShader::CreateLinear(pts, colors, pos, count,
                                          SkShader::kClamp_TileMode);
}

PlatformGradient* GraphicsContext::newPlatformRadialGradient(const FloatPoint& p0, float r0,
                                                             const FloatPoint& p1, float r1,
                                                             const float stopData[5], int count)
{
    SkPoint center;
    
    android_setpt(&center, p1);
    
    SkAutoMalloc    storage(count * (sizeof(SkColor) + sizeof(SkScalar)));
    SkColor*        colors = (SkColor*)storage.get();
    SkScalar*       pos = (SkScalar*)(colors + count);
    
    for (int i = 0; i < count; i++)
    {
        pos[i] = SkFloatToScalar(stopData[0]);
        colors[i] = make_color(stopData[4], stopData[1], stopData[2], stopData[3]);
        stopData += 5;
    }

    return SkGradientShader::CreateRadial(center, SkFloatToScalar(r1),
                                          colors, pos, count,
                                          SkShader::kClamp_TileMode);
}

void GraphicsContext::freePlatformGradient(PlatformGradient* shader)
{
    shader->safeUnref();
}

PlatformPattern* GraphicsContext::newPlatformPattern(Image* image,
                                                     Image::TileRule hRule,
                                                     Image::TileRule vRule)
{
//printf("----------- Image %p, [%d %d] %d %d\n", image, image->width(), image->height(), hRule, vRule);
    if (NULL == image)
        return NULL;

    SkBitmapRef* bm = image->nativeImageForCurrentFrame();
    if (NULL == bm)
        return NULL;

    return SkShader::CreateBitmapShader(bm->bitmap(),
                                        android_convert_TileRule(hRule),
                                        android_convert_TileRule(vRule));
}

void GraphicsContext::freePlatformPattern(PlatformPattern* shader)
{
    shader->safeUnref();
}

#endif

#if 0
static int getBlendedColorComponent(int c, int a)
{
    // We use white.
    float alpha = (float)(a) / 255;
    int whiteBlend = 255 - a;
    c -= whiteBlend;
    return (int)(c/alpha);
}
#endif

void GraphicsContext::fillRect(const IntRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    if (color.rgb() & 0xFF000000) {
        SkPaint paint;
        SkRect  r;
        
        android_setrect(&r, rect);
        m_data->setup_paint_common(&paint);
        paint.setColor(color.rgb());
        GC2Canvas(this)->drawRect(r, paint);
    }
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    if (color.rgb() & 0xFF000000) {
        SkPaint paint;
        SkRect  r;
        
        android_setrect(&r, rect);
        m_data->setup_paint_common(&paint);
        paint.setColor(color.rgb());
        GC2Canvas(this)->drawRect(r, paint);
    }
}

void GraphicsContext::clip(const IntRect& rect)
{
    if (paintingDisabled())
        return;
    
    SkRect  r;
    
    GC2Canvas(this)->clipRect(*android_setrect(&r, rect));
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;
    
//    path.platformPath()->dump(false, "clip path");

    GC2Canvas(this)->clipPath(*path.platformPath());
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    if (paintingDisabled())
        return;
    
//printf("-------- addInnerRoundedRectClip: [%d %d %d %d] thickness=%d\n", rect.x(), rect.y(), rect.width(), rect.height(), thickness);

    SkPath  path;
    SkRect r;
    android_setrect(&r, rect);
    path.addOval(r, SkPath::kCW_Direction);
    // only perform the inset if we won't invert r
    if (2*thickness < rect.width() && 2*thickness < rect.height())
    {
        r.inset(SkIntToScalar(thickness) ,SkIntToScalar(thickness));
        path.addOval(r, SkPath::kCCW_Direction);
    }
    GC2Canvas(this)->clipPath(path);
}

void GraphicsContext::clipOut(const IntRect& r)
{
    if (paintingDisabled())
        return;
    
    SkRect  rect;
    
    GC2Canvas(this)->clipRect(*android_setrect(&rect, r), SkRegion::kDifference_Op);
}

void GraphicsContext::clipOutEllipseInRect(const IntRect& r)
{
    if (paintingDisabled())
        return;
    
    SkRect  oval;
    SkPath  path;

    path.addOval(*android_setrect(&oval, r), SkPath::kCCW_Direction);
    GC2Canvas(this)->clipPath(path, SkRegion::kDifference_Op);
}

void GraphicsContext::clipOut(const Path& p)
{
    if (paintingDisabled())
        return;
    
    GC2Canvas(this)->clipPath(*p.platformPath(), SkRegion::kDifference_Op);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

#if SVG_SUPPORT
KRenderingDeviceContext* GraphicsContext::createRenderingDeviceContext()
{
    return new KRenderingDeviceContextQuartz(platformContext());
}
#endif

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    SkCanvas* canvas = GC2Canvas(this);

    if (opacity < 1)
    {
        canvas->saveLayerAlpha(NULL, (int)(opacity * 255), SkCanvas::kHasAlphaLayer_SaveFlag);
    }
    else
        canvas->save();
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    GC2Canvas(this)->restore();
}

void GraphicsContext::setShadow(const IntSize& size, int blur, const Color& color)
{
    if (paintingDisabled())
        return;

    if (blur > 0)
    {
        SkColor c;
        
        if (color.isValid())
            c = color.rgb();
        else
            c = SkColorSetARGB(0xFF/3, 0, 0, 0);    // "std" Apple shadow color
        
        SkDrawLooper* dl = new SkBlurDrawLooper(SkIntToScalar(blur),
                                                SkIntToScalar(size.width()),
                                                SkIntToScalar(size.height()),
                                                c);
        m_data->mState->setDrawLooper(dl)->unref();
    }
    else
        m_data->mState->setDrawLooper(NULL);
}

void GraphicsContext::clearShadow()
{
    if (paintingDisabled())
        return;

    m_data->mState->setDrawLooper(NULL);
}

void GraphicsContext::drawFocusRing(const Color& color)
{
    // Do nothing, since we draw the focus ring independently.
}

PlatformGraphicsContext* GraphicsContext::platformContext() const
{
    ASSERT(!paintingDisabled());
    return m_data->mPgc;
}

void GraphicsContext::setMiterLimit(float limit)
{
    m_data->mState->mMiterLimit = limit;
}

void GraphicsContext::setAlpha(float alpha)
{
    m_data->mState->mAlpha = alpha;
}

void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    m_data->mState->mPorterDuffMode = android_convert_compositeOp(op);
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;
    
    SkPaint paint;
    SkRect  r;
    
    android_setrect(&r, rect);
    m_data->setup_paint_fill(&paint);
    paint.setPorterDuffXfermode(SkPorterDuff::kClear_Mode);
    GC2Canvas(this)->drawRect(r, paint);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (paintingDisabled())
        return;
    
    SkPaint paint;
    SkRect  r;
    
    android_setrect(&r, rect);

    m_data->setup_paint_stroke(&paint, NULL);
    paint.setStrokeWidth(SkFloatToScalar(lineWidth));
    GC2Canvas(this)->drawRect(r, paint);
}

void GraphicsContext::setLineCap(LineCap cap)
{
    switch (cap) {
    case ButtCap:
        m_data->mState->mLineCap = SkPaint::kButt_Cap;
        break;
    case RoundCap:
        m_data->mState->mLineCap = SkPaint::kRound_Cap;
        break;
    case SquareCap:
        m_data->mState->mLineCap = SkPaint::kSquare_Cap;
        break;
    default:
        SkDEBUGF(("GraphicsContext::setLineCap: unknown LineCap %d\n", cap));
        break;
    }
}

void GraphicsContext::setLineJoin(LineJoin join)
{
    switch (join) {
    case MiterJoin:
        m_data->mState->mLineJoin = SkPaint::kMiter_Join;
        break;
    case RoundJoin:
        m_data->mState->mLineJoin = SkPaint::kRound_Join;
        break;
    case BevelJoin:
        m_data->mState->mLineJoin = SkPaint::kBevel_Join;
        break;
    default:
        SkDEBUGF(("GraphicsContext::setLineJoin: unknown LineJoin %d\n", join));
        break;
    }
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;
    GC2Canvas(this)->scale(SkFloatToScalar(size.width()), SkFloatToScalar(size.height()));
}

void GraphicsContext::rotate(float angleInRadians)
{
    if (paintingDisabled())
        return;
    GC2Canvas(this)->rotate(SkFloatToScalar(angleInRadians * (180.0f / 3.14159265f)));
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;
    GC2Canvas(this)->translate(SkFloatToScalar(x), SkFloatToScalar(y));
}

void GraphicsContext::concatCTM(const AffineTransform& xform)
{
    if (paintingDisabled())
        return;
    
//printf("-------------- GraphicsContext::concatCTM\n");
    GC2Canvas(this)->concat((SkMatrix) xform);
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect)
{
    if (paintingDisabled())
        return FloatRect();
    
    const SkMatrix& matrix = GC2Canvas(this)->getTotalMatrix();
    SkRect r;
    android_setrect(&r, rect);
    matrix.mapRect(&r);
    FloatRect result(SkScalarToFloat(r.fLeft), SkScalarToFloat(r.fTop), SkScalarToFloat(r.width()), SkScalarToFloat(r.height()));
    return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
    // appears to be PDF specific, so we ignore it
#if 0
    if (paintingDisabled())
        return;
        
    CFURLRef urlRef = link.createCFURL();
    if (urlRef) {
        CGContextRef context = platformContext();
        
        // Get the bounding box to handle clipping.
        CGRect box = CGContextGetClipBoundingBox(context);

        IntRect intBox((int)box.origin.x, (int)box.origin.y, (int)box.size.width, (int)box.size.height);
        IntRect rect = destRect;
        rect.intersect(intBox);

        CGPDFContextSetURLForRect(context, urlRef,
            CGRectApplyAffineTransform(rect, CGContextGetCTM(context)));

        CFRelease(urlRef);
    }
#endif
}

// we don't need to push these down, since we query the current state and build our paint at draw-time

void GraphicsContext::setPlatformStrokeColor(const Color&) {}
void GraphicsContext::setPlatformStrokeThickness(float) {}
void GraphicsContext::setPlatformFillColor(const Color&) {}


// functions new to Feb-19 tip of tree merge:
AffineTransform GraphicsContext::getCTM() const {
    notImplemented();
    return AffineTransform();
}

}
