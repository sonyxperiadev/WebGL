/*
 * Copyright 2006, The Android Open Source Project
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

#include "config.h"
#include "GraphicsContext.h"

#include "AffineTransform.h"
#include "Gradient.h"
#include "GraphicsContextPrivate.h"
#include "NotImplemented.h"
#include "Path.h"
#include "Pattern.h"
#include "PlatformGraphicsContext.h"
#include "SkBitmapRef.h"
#include "SkBlurDrawLooper.h"
#include "SkBlurMaskFilter.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkDashPathEffect.h"
#include "SkDevice.h"
#include "SkGradientShader.h"
#include "SkPaint.h"
#include "SkString.h"
#include "SkiaUtils.h"
#include "TransformationMatrix.h"
#include "android_graphics.h"

using namespace std;

#define GC2Canvas(ctx)  (ctx)->m_data->mPgc->mCanvas

namespace WebCore {

static int RoundToInt(float x)
{
    return (int)roundf(x);
}

template <typename T> T* deepCopyPtr(const T* src) {
    return src ? new T(*src) : NULL;
}

/*  TODO / questions

    mAlpha: how does this interact with the alpha in Color? multiply them together?
    mMode: do I always respect this? If so, then
                     the rgb() & 0xFF000000 check will abort drawing too often
    Is Color premultiplied or not? If it is, then I can't blindly pass it to paint.setColor()
*/

struct ShadowRec {
    SkScalar    mBlur;  // >0 means valid shadow
    SkScalar    mDx;
    SkScalar    mDy;
    SkColor     mColor;
};

class GraphicsContextPlatformPrivate {
public:
    GraphicsContext*             mCG;        // back-ptr to our parent
    PlatformGraphicsContext*     mPgc;

    struct State {
        SkPath*             mPath;
        SkPathEffect* mPathEffect;
        float               mMiterLimit;
        float               mAlpha;
        float               mStrokeThickness;
        SkPaint::Cap        mLineCap;
        SkPaint::Join       mLineJoin;
        SkXfermode::Mode    mMode;
        int                 mDashRatio; //ratio of the length of a dash to its width
        ShadowRec           mShadow;
        SkColor             mFillColor;
        SkColor             mStrokeColor;
        bool                mUseAA;
        
        State() {
            mPath            = NULL;    // lazily allocated
            mPathEffect = 0;
            mMiterLimit      = 4;
            mAlpha           = 1;
            mStrokeThickness = 0.0f;  // Same as default in GraphicsContextPrivate.h
            mLineCap         = SkPaint::kDefault_Cap;
            mLineJoin        = SkPaint::kDefault_Join;
            mMode            = SkXfermode::kSrcOver_Mode;
            mDashRatio       = 3;
            mUseAA           = true;
            mShadow.mBlur    = 0;
            mFillColor       = SK_ColorBLACK;
            mStrokeColor     = SK_ColorBLACK;
        }
        
        State(const State& other) {
            memcpy(this, &other, sizeof(State));
            mPath = deepCopyPtr<SkPath>(other.mPath);
            mPathEffect->safeRef();
        }
        
        ~State() {
            delete mPath;
            mPathEffect->safeUnref();
        }
        
        void setShadow(int radius, int dx, int dy, SkColor c) {
            // cut the radius in half, to visually match the effect seen in
            // safari browser
            mShadow.mBlur = SkScalarHalf(SkIntToScalar(radius));
            mShadow.mDx = SkIntToScalar(dx);
            mShadow.mDy = SkIntToScalar(dy);
            mShadow.mColor = c;
        }
        
        bool setupShadowPaint(SkPaint* paint, SkPoint* offset) {
            if (mShadow.mBlur > 0) {
                paint->setAntiAlias(true);
                paint->setDither(true);
                paint->setXfermodeMode(mMode);
                paint->setColor(mShadow.mColor);
                paint->setMaskFilter(SkBlurMaskFilter::Create(mShadow.mBlur,
                                SkBlurMaskFilter::kNormal_BlurStyle))->unref();
                offset->set(mShadow.mDx, mShadow.mDy);
                return true;
            }
            return false;
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
            , mPgc(pgc), mStateStack(sizeof(State)) {
        State* state = (State*)mStateStack.push_back();
        new (state) State();
        mState = state;
    }
    
    ~GraphicsContextPlatformPrivate() {
        if (mPgc && mPgc->deleteUs())
            delete mPgc;

        // we force restores so we don't leak any subobjects owned by our
        // stack of State records.
        while (mStateStack.count() > 0)
            this->restore();
    }
    
    void save() {
        State* newState = (State*)mStateStack.push_back();
        new (newState) State(*mState);
        mState = newState;
    }
    
    void restore() {
        mState->~State();
        mStateStack.pop_back();
        mState = (State*)mStateStack.back();
    }
    
    void setFillColor(const Color& c) {
        mState->mFillColor = c.rgb();
    }

    void setStrokeColor(const Color& c) {
        mState->mStrokeColor = c.rgb();
    }
    
    void setStrokeThickness(float f) {
        mState->mStrokeThickness = f;
    }
    
    void beginPath() {
        if (mState->mPath) {
            mState->mPath->reset();
        }
    }
    
    void addPath(const SkPath& other) {
        if (!mState->mPath) {
            mState->mPath = new SkPath(other);
        } else {
            mState->mPath->addPath(other);
        }
    }
    
    // may return null
    SkPath* getPath() const { return mState->mPath; }
    
    void setup_paint_common(SkPaint* paint) const {
        paint->setAntiAlias(mState->mUseAA);
        paint->setDither(true);
        paint->setXfermodeMode(mState->mMode);
        if (mState->mShadow.mBlur > 0) {
            SkDrawLooper* looper = new SkBlurDrawLooper(mState->mShadow.mBlur,
                                                        mState->mShadow.mDx,
                                                        mState->mShadow.mDy,
                                                        mState->mShadow.mColor);
            paint->setLooper(looper)->unref();
        }
        
        /* need to sniff textDrawingMode(), which returns the bit_OR of...
             const int cTextInvisible = 0;
             const int cTextFill = 1;
             const int cTextStroke = 2;
             const int cTextClip = 4;
         */         
    }

    void setup_paint_fill(SkPaint* paint) const {
        this->setup_paint_common(paint);
        paint->setColor(mState->applyAlpha(mState->mFillColor));
    }

    void setup_paint_bitmap(SkPaint* paint) const {
        this->setup_paint_common(paint);
        // we only want the global alpha for bitmaps,
        // so just give applyAlpha opaque black
        paint->setColor(mState->applyAlpha(0xFF000000));
    }

    /*  sets up the paint for stroking. Returns true if the style is really
        just a dash of squares (the size of the paint's stroke-width.
    */
    bool setup_paint_stroke(SkPaint* paint, SkRect* rect) {
        this->setup_paint_common(paint);
        paint->setColor(mState->applyAlpha(mState->mStrokeColor));
                         
        float width = mState->mStrokeThickness;

        // this allows dashing and dotting to work properly for hairline strokes
        // FIXME: Should we only do this for dashed and dotted strokes?
        if (0 == width) {
            width = 1;
        }

//        paint->setColor(mState->applyAlpha(mCG->strokeColor().rgb()));
        paint->setStyle(SkPaint::kStroke_Style);
        paint->setStrokeWidth(SkFloatToScalar(width));
        paint->setStrokeCap(mState->mLineCap);
        paint->setStrokeJoin(mState->mLineJoin);
        paint->setStrokeMiter(SkFloatToScalar(mState->mMiterLimit));

        if (rect != NULL && (RoundToInt(width) & 1)) {
            rect->inset(-SK_ScalarHalf, -SK_ScalarHalf);
        }

        SkPathEffect* pe = mState->mPathEffect;
        if (pe) {
            paint->setPathEffect(pe);
            return false;
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
            pe = new SkDashPathEffect(intervals, 2, 0);
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

static SkShader::TileMode SpreadMethod2TileMode(GradientSpreadMethod sm) {
    SkShader::TileMode mode = SkShader::kClamp_TileMode;
    
    switch (sm) {
        case SpreadMethodPad:
            mode = SkShader::kClamp_TileMode;
            break;
        case SpreadMethodReflect:
            mode = SkShader::kMirror_TileMode;
            break;
        case SpreadMethodRepeat:
            mode = SkShader::kRepeat_TileMode;
            break;
    }
    return mode;
}

static void extactShader(SkPaint* paint, Pattern* pat, Gradient* grad)
{
    if (pat) {
        // platformPattern() returns a cached obj
        paint->setShader(pat->platformPattern(AffineTransform()));
    } else if (grad) {
        // grad->getShader() returns a cached obj
        GradientSpreadMethod sm = grad->spreadMethod();
        paint->setShader(grad->getShader(SpreadMethod2TileMode(sm)));
    }
}
    
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

bool GraphicsContext::willFill() const {
    return m_data->mState->mFillColor != 0;
}

bool GraphicsContext::willStroke() const {
    return m_data->mState->mStrokeColor != 0;
}
    
const SkPath* GraphicsContext::getCurrPath() const {
    return m_data->mState->mPath;
}
    
// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    SkRect  r(rect);

    if (fillColor().alpha()) {
        m_data->setup_paint_fill(&paint);
        GC2Canvas(this)->drawRect(r, paint);
    }

    /*  According to GraphicsContext.h, stroking inside drawRect always means
        a stroke of 1 inside the rect.
     */
    if (strokeStyle() != NoStroke && strokeColor().alpha()) {
        paint.reset();
        m_data->setup_paint_stroke(&paint, &r);
        paint.setPathEffect(NULL);              // no dashing please
        paint.setStrokeWidth(SK_Scalar1);       // always just 1.0 width
        r.inset(SK_ScalarHalf, SK_ScalarHalf);  // ensure we're "inside"
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
        SkPoint pts[2] = { point1, point2 };
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
    SkRect  oval(rect);
    
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
    SkRect  oval(r);
    
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

    if (GC2Canvas(this)->quickReject(path, shouldAntialias ? 
            SkCanvas::kAA_EdgeType : SkCanvas::kBW_EdgeType)) {
        return;
    }
    
    if (fillColor().rgb() & 0xFF000000) {
        m_data->setup_paint_fill(&paint);
        paint.setAntiAlias(shouldAntialias);
        GC2Canvas(this)->drawPath(path, paint);
    }

    if (strokeStyle() != NoStroke) {
        paint.reset();
        m_data->setup_paint_stroke(&paint, NULL);
        paint.setAntiAlias(shouldAntialias);
        GC2Canvas(this)->drawPath(path, paint);
    }
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
                                      const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color, ColorSpace)
{
    if (paintingDisabled())
        return;
    
    SkPaint paint;
    SkPath  path;
    SkScalar radii[8];
    
    radii[0] = SkIntToScalar(topLeft.width());
    radii[1] = SkIntToScalar(topLeft.height());
    radii[2] = SkIntToScalar(topRight.width());
    radii[3] = SkIntToScalar(topRight.height());
    radii[4] = SkIntToScalar(bottomRight.width());
    radii[5] = SkIntToScalar(bottomRight.height());
    radii[6] = SkIntToScalar(bottomLeft.width());
    radii[7] = SkIntToScalar(bottomLeft.height());
    path.addRoundRect(rect, radii);
    
    m_data->setup_paint_fill(&paint);
    GC2Canvas(this)->drawPath(path, paint);
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    SkPaint paint;
    
    m_data->setup_paint_fill(&paint);

    extactShader(&paint,
                 m_common->state.fillPattern.get(),
                 m_common->state.fillGradient.get());

    GC2Canvas(this)->drawRect(rect, paint);
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, ColorSpace)
{
    if (paintingDisabled())
        return;

    if (color.rgb() & 0xFF000000) {
        SkPaint paint;
        
        m_data->setup_paint_common(&paint);
        paint.setColor(color.rgb());    // punch in the specified color
        paint.setShader(NULL);          // in case we had one set

        /*  Sometimes we record and draw portions of the page, using clips
            for each portion. The problem with this is that webkit, sometimes,
            sees that we're only recording a portion, and they adjust some of
            their rectangle coordinates accordingly (e.g.
            RenderBoxModelObject::paintFillLayerExtended() which calls
            rect.intersect(paintInfo.rect) and then draws the bg with that
            rect. The result is that we end up drawing rects that are meant to
            seam together (one for each portion), but if the rects have
            fractional coordinates (e.g. we are zoomed by a fractional amount)
            we will double-draw those edges, resulting in visual cracks or
            artifacts.

            The fix seems to be to just turn off antialasing for rects (this
            entry-point in GraphicsContext seems to have been sufficient,
            though perhaps we'll find we need to do this as well in fillRect(r)
            as well.) Currently setup_paint_common() enables antialiasing.

            Since we never show the page rotated at a funny angle, disabling
            antialiasing seems to have no real down-side, and it does fix the
            bug when we're zoomed (and drawing portions that need to seam).
         */
        paint.setAntiAlias(false);

        GC2Canvas(this)->drawRect(rect, paint);
    }
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;
    
    GC2Canvas(this)->clipRect(rect);
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
    SkRect r(rect);

    path.addOval(r, SkPath::kCW_Direction);
    // only perform the inset if we won't invert r
    if (2*thickness < rect.width() && 2*thickness < rect.height())
    {
        r.inset(SkIntToScalar(thickness) ,SkIntToScalar(thickness));
        path.addOval(r, SkPath::kCCW_Direction);
    }
    GC2Canvas(this)->clipPath(path);
}

void GraphicsContext::canvasClip(const Path& path)
{
    clip(path);
}

void GraphicsContext::clipOut(const IntRect& r)
{
    if (paintingDisabled())
        return;
    
    GC2Canvas(this)->clipRect(r, SkRegion::kDifference_Op);
}

void GraphicsContext::clipOutEllipseInRect(const IntRect& r)
{
    if (paintingDisabled())
        return;
    
    SkPath path;

    path.addOval(r, SkPath::kCCW_Direction);
    GC2Canvas(this)->clipPath(path, SkRegion::kDifference_Op);
}

#if ENABLE(SVG)
void GraphicsContext::clipPath(WindRule clipRule)
{
    if (paintingDisabled())
        return;
    const SkPath* oldPath = m_data->getPath();
    SkPath path(*oldPath);
    path.setFillType(clipRule == RULE_EVENODD ? SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType);
    GC2Canvas(this)->clipPath(path);
}
#endif

void GraphicsContext::clipOut(const Path& p)
{
    if (paintingDisabled())
        return;
    
    GC2Canvas(this)->clipPath(*p.platformPath(), SkRegion::kDifference_Op);
}

void GraphicsContext::clipToImageBuffer(const FloatRect&, const ImageBuffer*) {
    SkDebugf("xxxxxxxxxxxxxxxxxx clipToImageBuffer not implemented\n");
}

//////////////////////////////////////////////////////////////////////////////////////////////////

#if SVG_SUPPORT
KRenderingDeviceContext* GraphicsContext::createRenderingDeviceContext()
{
    return new KRenderingDeviceContextQuartz(platformContext());
}
#endif

/*  These are the flags we need when we call saveLayer for transparency.
    Since it does not appear that webkit intends this to also save/restore
    the matrix or clip, I do not give those flags (for performance)
 */
#define TRANSPARENCY_SAVEFLAGS                                  \
    (SkCanvas::SaveFlags)(SkCanvas::kHasAlphaLayer_SaveFlag |   \
                          SkCanvas::kFullColorLayer_SaveFlag)
    
void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    SkCanvas* canvas = GC2Canvas(this);
    canvas->saveLayerAlpha(NULL, (int)(opacity * 255), TRANSPARENCY_SAVEFLAGS);
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    GC2Canvas(this)->restore();
}

    ///////////////////////////////////////////////////////////////////////////

    void GraphicsContext::setupBitmapPaint(SkPaint* paint) {
        m_data->setup_paint_bitmap(paint);
    }

    void GraphicsContext::setupFillPaint(SkPaint* paint) {
        m_data->setup_paint_fill(paint);
    }

    void GraphicsContext::setupStrokePaint(SkPaint* paint) {
        m_data->setup_paint_stroke(paint, NULL);
    }

    bool GraphicsContext::setupShadowPaint(SkPaint* paint, SkPoint* offset) {
        return m_data->mState->setupShadowPaint(paint, offset);
    }

    void GraphicsContext::setPlatformStrokeColor(const Color& c, ColorSpace) {
        m_data->setStrokeColor(c);
    }
    
    void GraphicsContext::setPlatformStrokeThickness(float f) {
        m_data->setStrokeThickness(f);
    }

    void GraphicsContext::setPlatformFillColor(const Color& c, ColorSpace) {
        m_data->setFillColor(c);
    }
    
void GraphicsContext::setPlatformShadow(const IntSize& size, int blur, const Color& color, ColorSpace)
{
    if (paintingDisabled())
        return;

    if (blur <= 0) {
        this->clearPlatformShadow();
    }

    SkColor c;
    if (color.isValid()) {
        c = color.rgb();
    } else {
        c = SkColorSetARGB(0xFF/3, 0, 0, 0);    // "std" Apple shadow color
    }
    m_data->mState->setShadow(blur, size.width(), size.height(), c);
}

void GraphicsContext::clearPlatformShadow()
{
    if (paintingDisabled())
        return;

    m_data->mState->setShadow(0, 0, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////

void GraphicsContext::drawFocusRing(const Vector<IntRect>&, int, int, const Color&)
{
    // Do nothing, since we draw the focus ring independently.
}

void GraphicsContext::drawFocusRing(const Vector<Path>&, int, int, const Color&)
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
    m_data->mState->mMode = WebCoreCompositeToSkiaComposite(op);
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;
    
    SkPaint paint;
    
    m_data->setup_paint_fill(&paint);
    paint.setXfermodeMode(SkXfermode::kClear_Mode);
    GC2Canvas(this)->drawRect(rect, paint);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (paintingDisabled())
        return;
    
    SkPaint paint;

    m_data->setup_paint_stroke(&paint, NULL);
    paint.setStrokeWidth(SkFloatToScalar(lineWidth));
    GC2Canvas(this)->drawRect(rect, paint);
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

#if ENABLE(SVG)
void GraphicsContext::setLineDash(const DashArray& dashes, float dashOffset)
{
    if (paintingDisabled())
        return;

    size_t dashLength = dashes.size();
    if (!dashLength)
        return;

    size_t count = (dashLength % 2) == 0 ? dashLength : dashLength * 2;
    SkScalar* intervals = new SkScalar[count];

    for (unsigned int i = 0; i < count; i++)
        intervals[i] = SkFloatToScalar(dashes[i % dashLength]);
    SkPathEffect **effectPtr = &m_data->mState->mPathEffect;
    (*effectPtr)->safeUnref();
    *effectPtr = new SkDashPathEffect(intervals, count, SkFloatToScalar(dashOffset));

    delete[] intervals;
}
#endif

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

void GraphicsContext::concatCTM(const AffineTransform& affine)
{
    if (paintingDisabled())
        return;
    GC2Canvas(this)->concat(affine);
}

/*  This is intended to round the rect to device pixels (through the CTM)
    and then invert the result back into source space, with the hope that when
    it is drawn (through the matrix), it will land in the "right" place (i.e.
    on pixel boundaries).

    For android, we record this geometry once and then draw it though various
    scale factors as the user zooms, without re-recording. Thus this routine
    should just leave the original geometry alone.

    If we instead draw into bitmap tiles, we should then perform this
    transform -> round -> inverse step.
 */
FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect)
{
    return rect;
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

void GraphicsContext::setPlatformShouldAntialias(bool useAA)
{
    if (paintingDisabled())
        return;
    m_data->mState->mUseAA = useAA;
}

AffineTransform GraphicsContext::getCTM() const
{
    const SkMatrix& m = GC2Canvas(this)->getTotalMatrix();
    return AffineTransform(SkScalarToDouble(m.getScaleX()),      // a
                           SkScalarToDouble(m.getSkewY()),       // b
                           SkScalarToDouble(m.getSkewX()),       // c
                           SkScalarToDouble(m.getScaleY()),      // d
                           SkScalarToDouble(m.getTranslateX()),  // e
                           SkScalarToDouble(m.getTranslateY())); // f
}

///////////////////////////////////////////////////////////////////////////////
    
void GraphicsContext::beginPath()
{
    m_data->beginPath();
}

void GraphicsContext::addPath(const Path& p)
{
    m_data->addPath(*p.platformPath());
}

void GraphicsContext::fillPath()
{
    SkPath* path = m_data->getPath();
    if (paintingDisabled() || !path)
        return;
    
    switch (this->fillRule()) {
        case RULE_NONZERO:
            path->setFillType(SkPath::kWinding_FillType);
            break;
        case RULE_EVENODD:
            path->setFillType(SkPath::kEvenOdd_FillType);
            break;
    }

    SkPaint paint;
    m_data->setup_paint_fill(&paint);

    extactShader(&paint,
                 m_common->state.fillPattern.get(),
                 m_common->state.fillGradient.get());

    GC2Canvas(this)->drawPath(*path, paint);
}

void GraphicsContext::strokePath()
{
    const SkPath* path = m_data->getPath();
    if (paintingDisabled() || !path)
        return;
    
    SkPaint paint;
    m_data->setup_paint_stroke(&paint, NULL);

    extactShader(&paint,
                 m_common->state.strokePattern.get(),
                 m_common->state.strokeGradient.get());
    
    GC2Canvas(this)->drawPath(*path, paint);
}

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality mode)
{
    /*
    enum InterpolationQuality {
        InterpolationDefault,
        InterpolationNone,
        InterpolationLow,
        InterpolationMedium,
        InterpolationHigh
    };
     
     TODO: record this, so we can know when to use bitmap-filtering when we draw
     ... not sure how meaningful this will be given our playback model.
     
     Certainly safe to do nothing for the present.
     */
}

} // namespace WebCore

///////////////////////////////////////////////////////////////////////////////

SkCanvas* android_gc2canvas(WebCore::GraphicsContext* gc)
{
    return gc->platformContext()->mCanvas;
}
