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

#define GC2CANVAS(ctx)  (ctx)->m_data->getPlatformGfxCtx()->mCanvas

namespace WebCore {

static int RoundToInt(float x)
{
    return (int)roundf(x);
}

template <typename T> T* deepCopyPtr(const T* src)
{
    return src ? new T(*src) : 0;
}

// Set a bitmap shader that mimics dashing by width-on, width-off.
// Returns false if it could not succeed (e.g. there was an existing shader)
static bool setBitmapDash(SkPaint* paint, int width) {
    if (width <= 0 || paint->getShader())
        return false;

    SkColor c = paint->getColor();

    SkBitmap bm;
    bm.setConfig(SkBitmap::kARGB_8888_Config, 2, 1);
    bm.allocPixels();
    bm.lockPixels();

    // set the ON pixel
    *bm.getAddr32(0, 0) = SkPreMultiplyARGB(0xFF, SkColorGetR(c),
                                            SkColorGetG(c), SkColorGetB(c));
    // set the OFF pixel
    *bm.getAddr32(1, 0) = 0;
    bm.unlockPixels();

    SkMatrix matrix;
    matrix.setScale(SkIntToScalar(width), SK_Scalar1);

    SkShader* s = SkShader::CreateBitmapShader(bm, SkShader::kRepeat_TileMode,
                                               SkShader::kClamp_TileMode);
    s->setLocalMatrix(matrix);

    paint->setShader(s)->unref();
    return true;
}

// TODO / questions

// alpha: how does this interact with the alpha in Color? multiply them together?
// mode: do I always respect this? If so, then
// the rgb() & 0xFF000000 check will abort drawing too often
// Is Color premultiplied or not? If it is, then I can't blindly pass it to paint.setColor()

struct ShadowRec {
    SkScalar blur;
    SkScalar dx;
    SkScalar dy;
    SkColor color;  // alpha>0 means valid shadow
    ShadowRec(SkScalar b = 0,
              SkScalar x = 0,
              SkScalar y = 0,
              SkColor c = 0) // by default, alpha=0, so no shadow
            : blur(b), dx(x), dy(y), color(c)
        {};
};

class GraphicsContextPlatformPrivate {
public:
    struct State {
        SkPathEffect* pathEffect;
        float miterLimit;
        float alpha;
        float strokeThickness;
        SkPaint::Cap lineCap;
        SkPaint::Join lineJoin;
        SkXfermode::Mode mode;
        int dashRatio; // Ratio of the length of a dash to its width
        ShadowRec shadow;
        SkColor fillColor;
        SkColor strokeColor;
        bool useAA;
        // This is a list of clipping paths which are currently active, in the
        // order in which they were pushed.
        WTF::Vector<SkPath> antiAliasClipPaths;

        State()
            : pathEffect(0)
            , miterLimit(4)
            , alpha(1)
            , strokeThickness(0) // Same as default in GraphicsContextPrivate.h
            , lineCap(SkPaint::kDefault_Cap)
            , lineJoin(SkPaint::kDefault_Join)
            , mode(SkXfermode::kSrcOver_Mode)
            , dashRatio(3)
            , fillColor(SK_ColorBLACK)
            , strokeColor(SK_ColorBLACK)
            , useAA(true)
        {
        }

        State(const State& other)
            : pathEffect(other.pathEffect)
            , miterLimit(other.miterLimit)
            , alpha(other.alpha)
            , strokeThickness(other.strokeThickness)
            , lineCap(other.lineCap)
            , lineJoin(other.lineJoin)
            , mode(other.mode)
            , dashRatio(other.dashRatio)
            , shadow(other.shadow)
            , fillColor(other.fillColor)
            , strokeColor(other.strokeColor)
            , useAA(other.useAA)
        {
            SkSafeRef(pathEffect);
        }

        ~State()
        {
            SkSafeUnref(pathEffect);
        }

        void setShadow(int radius, int dx, int dy, SkColor c)
        {
            // Cut the radius in half, to visually match the effect seen in
            // safari browser
            shadow.blur = SkScalarHalf(SkIntToScalar(radius));
            shadow.dx = SkIntToScalar(dx);
            shadow.dy = SkIntToScalar(dy);
            shadow.color = c;
        }

        bool setupShadowPaint(GraphicsContext* ctx, SkPaint* paint, SkPoint* offset)
        {
            paint->setAntiAlias(true);
            paint->setDither(true);
            paint->setXfermodeMode(mode);
            paint->setColor(shadow.color);
            offset->set(shadow.dx, shadow.dy);

            // Currently, only GraphicsContexts associated with the
            // HTMLCanvasElement have shadows ignore transforms set.  This
            // allows us to distinguish between CSS and Canvas shadows which
            // have different rendering specifications.
            uint32_t flags = SkBlurMaskFilter::kHighQuality_BlurFlag;
            if (ctx->shadowsIgnoreTransforms()) {
                offset->fY = -offset->fY;
                flags |= SkBlurMaskFilter::kIgnoreTransform_BlurFlag;
            }

            if (shadow.blur > 0) {
                paint->setMaskFilter(SkBlurMaskFilter::Create(shadow.blur,
                                     SkBlurMaskFilter::kNormal_BlurStyle))->unref();
            }
            return SkColorGetA(shadow.color) && (shadow.blur || shadow.dx || shadow.dy);
        }

        SkColor applyAlpha(SkColor c) const
        {
            int s = RoundToInt(alpha * 256);
            if (s >= 256)
                return c;
            if (s < 0)
                return 0;

            int a = SkAlphaMul(SkColorGetA(c), s);
            return (c & 0x00FFFFFF) | (a << 24);
        }
    };

    GraphicsContextPlatformPrivate(GraphicsContext* gfxCtx, PlatformGraphicsContext* platformGfxCtx)
        : m_parentGfxCtx(gfxCtx)
        , m_platformGfxCtx(platformGfxCtx)
        , m_stateStack(sizeof(State))
    {
        State* state = static_cast<State*>(m_stateStack.push_back());
        new (state) State();
        m_state = state;
    }

    ~GraphicsContextPlatformPrivate()
    {
        // We force restores so we don't leak any subobjects owned by our
        // stack of State records.
        while (m_stateStack.count() > 0)
            this->restore();

        if (m_platformGfxCtx && m_platformGfxCtx->deleteUs())
            delete m_platformGfxCtx;
    }

    void save()
    {
        State* newState = static_cast<State*>(m_stateStack.push_back());
        new (newState) State(*m_state);
        m_state = newState;
    }

    void restore()
    {
        if (!m_state->antiAliasClipPaths.isEmpty())
            applyAntiAliasedClipPaths(m_state->antiAliasClipPaths);

        m_state->~State();
        m_stateStack.pop_back();
        m_state = static_cast<State*>(m_stateStack.back());
    }

    void setFillColor(const Color& c)
    {
        m_state->fillColor = c.rgb();
    }

    void setStrokeColor(const Color& c)
    {
        m_state->strokeColor = c.rgb();
    }

    void setStrokeThickness(float f)
    {
        m_state->strokeThickness = f;
    }

    void setupPaintCommon(SkPaint* paint) const
    {
        paint->setAntiAlias(m_state->useAA);
        paint->setDither(true);
        paint->setXfermodeMode(m_state->mode);
        if (SkColorGetA(m_state->shadow.color) > 0) {

            // Currently, only GraphicsContexts associated with the
            // HTMLCanvasElement have shadows ignore transforms set.  This
            // allows us to distinguish between CSS and Canvas shadows which
            // have different rendering specifications.
            SkScalar dy = m_state->shadow.dy;
            uint32_t flags = SkBlurDrawLooper::kHighQuality_BlurFlag;
            if (m_parentGfxCtx->shadowsIgnoreTransforms()) {
                dy = -dy;
                flags |= SkBlurDrawLooper::kIgnoreTransform_BlurFlag;
                flags |= SkBlurDrawLooper::kOverrideColor_BlurFlag;
            }

            SkDrawLooper* looper = new SkBlurDrawLooper(m_state->shadow.blur,
                                                        m_state->shadow.dx,
                                                        dy,
                                                        m_state->shadow.color,
                                                        flags);
            paint->setLooper(looper)->unref();
        }
    }

    void setupPaintFill(SkPaint* paint) const
    {
        this->setupPaintCommon(paint);
        paint->setColor(m_state->applyAlpha(m_state->fillColor));
    }

    void setupPaintBitmap(SkPaint* paint) const
    {
        this->setupPaintCommon(paint);
        // We only want the global alpha for bitmaps,
        // so just give applyAlpha opaque black
        paint->setColor(m_state->applyAlpha(0xFF000000));
    }

    // Sets up the paint for stroking. Returns true if the style is really
    // just a dash of squares (the size of the paint's stroke-width.
    bool setupPaintStroke(SkPaint* paint, SkRect* rect, bool isHLine = false)
    {
        this->setupPaintCommon(paint);
        paint->setColor(m_state->applyAlpha(m_state->strokeColor));

        float width = m_state->strokeThickness;

        // This allows dashing and dotting to work properly for hairline strokes
        // FIXME: Should we only do this for dashed and dotted strokes?
        if (!width)
            width = 1;

        paint->setStyle(SkPaint::kStroke_Style);
        paint->setStrokeWidth(SkFloatToScalar(width));
        paint->setStrokeCap(m_state->lineCap);
        paint->setStrokeJoin(m_state->lineJoin);
        paint->setStrokeMiter(SkFloatToScalar(m_state->miterLimit));

        if (rect && (RoundToInt(width) & 1))
            rect->inset(-SK_ScalarHalf, -SK_ScalarHalf);

        SkPathEffect* pe = m_state->pathEffect;
        if (pe) {
            paint->setPathEffect(pe);
            return false;
        }
        switch (m_parentGfxCtx->strokeStyle()) {
        case NoStroke:
        case SolidStroke:
            width = 0;
            break;
        case DashedStroke:
            width = m_state->dashRatio * width;
            break;
            // No break
        case DottedStroke:
            break;
        }

        if (width > 0) {
            // Return true if we're basically a dotted dash of squares
            bool justSqrs = RoundToInt(width) == RoundToInt(paint->getStrokeWidth());

            if (justSqrs || !isHLine || !setBitmapDash(paint, width)) {
#if 0
                // this is slow enough that we just skip it for now
                // see http://b/issue?id=4163023
                SkScalar intervals[] = { width, width };
                pe = new SkDashPathEffect(intervals, 2, 0);
                paint->setPathEffect(pe)->unref();
#endif
            }
            return justSqrs;
        }
        return false;
    }

    void clipPathAntiAliased(const SkPath& clipPath)
    {
        // If we are currently tracking any anti-alias clip paths, then we already
        // have a layer in place and don't need to add another.
        bool haveLayerOutstanding = m_state->antiAliasClipPaths.size();

        // See comments in applyAntiAliasedClipPaths about how this works.
        m_state->antiAliasClipPaths.append(clipPath);
        if (!haveLayerOutstanding) {
            SkRect bounds = clipPath.getBounds();
            if (m_platformGfxCtx && m_platformGfxCtx->mCanvas) {
                m_platformGfxCtx->mCanvas->saveLayerAlpha(&bounds, 255,
                    static_cast<SkCanvas::SaveFlags>(SkCanvas::kHasAlphaLayer_SaveFlag
                        | SkCanvas::kFullColorLayer_SaveFlag
                        | SkCanvas::kClipToLayer_SaveFlag));
                m_platformGfxCtx->mCanvas->save();
            } else
                ASSERT(0);
        }
    }

    void applyAntiAliasedClipPaths(WTF::Vector<SkPath>& paths)
    {
        // Anti-aliased clipping:
        //
        // Refer to PlatformContextSkia.cpp's applyAntiAliasedClipPaths() for more details

        if (m_platformGfxCtx && m_platformGfxCtx->mCanvas)
            m_platformGfxCtx->mCanvas->restore();

        SkPaint paint;
        paint.setXfermodeMode(SkXfermode::kClear_Mode);
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kFill_Style);

        if (m_platformGfxCtx && m_platformGfxCtx->mCanvas)  {
            for (size_t i = paths.size() - 1; i < paths.size(); --i) {
                paths[i].setFillType(SkPath::kInverseWinding_FillType);
                m_platformGfxCtx->mCanvas->drawPath(paths[i], paint);
            }
            m_platformGfxCtx->mCanvas->restore();
        } else
            ASSERT(0);
    }

    PlatformGraphicsContext* getPlatformGfxCtx()
    {
        return m_platformGfxCtx;
    }

    State* getState()
    {
        return m_state;
    }
private:
    State* m_state;
    GraphicsContext* m_parentGfxCtx; // Back-ptr to our parent
    PlatformGraphicsContext* m_platformGfxCtx;
    SkDeque m_stateStack;
    // Not supported yet
    State& operator=(const State&);
};

static SkShader::TileMode SpreadMethod2TileMode(GradientSpreadMethod sm)
{
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

    SkBitmap bitmap;

    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    pgc->mCanvas->setBitmapDevice(bitmap);

    GraphicsContext* ctx = new GraphicsContext(pgc);
    return ctx;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void GraphicsContext::platformInit(PlatformGraphicsContext* gc)
{
    m_data = new GraphicsContextPlatformPrivate(this, gc);
    setPaintingDisabled(!gc || !gc->mCanvas);
}

void GraphicsContext::platformDestroy()
{
    delete m_data;
}

void GraphicsContext::savePlatformState()
{
    // Save our private State
    m_data->save();
    // Save our native canvas
    GC2CANVAS(this)->save();
}

void GraphicsContext::restorePlatformState()
{
    // Restore our private State
    m_data->restore();
    // Restore our native canvas
    GC2CANVAS(this)->restore();
}

bool GraphicsContext::willFill() const
{
    return m_data->getState()->fillColor;
}

bool GraphicsContext::willStroke() const
{
    return m_data->getState()->strokeColor;
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    SkRect r(rect);

    if (fillColor().alpha()) {
        m_data->setupPaintFill(&paint);
        GC2CANVAS(this)->drawRect(r, paint);
    }

    // According to GraphicsContext.h, stroking inside drawRect always means
    // a stroke of 1 inside the rect.
    if (strokeStyle() != NoStroke && strokeColor().alpha()) {
        paint.reset();
        m_data->setupPaintStroke(&paint, &r);
        paint.setPathEffect(0); // No dashing please
        paint.setStrokeWidth(SK_Scalar1); // Always just 1.0 width
        r.inset(SK_ScalarHalf, SK_ScalarHalf); // Ensure we're "inside"
        GC2CANVAS(this)->drawRect(r, paint);
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
    SkCanvas* canvas = GC2CANVAS(this);
    const int idx = SkAbs32(point2.x() - point1.x());
    const int idy = SkAbs32(point2.y() - point1.y());

    // Special-case horizontal and vertical lines that are really just dots
    if (m_data->setupPaintStroke(&paint, 0, !idy) && (!idx || !idy)) {
        const SkScalar diameter = paint.getStrokeWidth();
        const SkScalar radius = SkScalarHalf(diameter);
        SkScalar x = SkIntToScalar(SkMin32(point1.x(), point2.x()));
        SkScalar y = SkIntToScalar(SkMin32(point1.y(), point2.y()));
        SkScalar dx, dy;
        int count;
        SkRect bounds;

        if (!idy) { // Horizontal
            bounds.set(x, y - radius, x + SkIntToScalar(idx), y + radius);
            x += radius;
            dx = diameter * 2;
            dy = 0;
            count = idx;
        } else { // Vertical
            bounds.set(x - radius, y, x + radius, y + SkIntToScalar(idy));
            y += radius;
            dx = 0;
            dy = diameter * 2;
            count = idy;
        }

        // The actual count is the number of ONs we hit alternating
        // ON(diameter), OFF(diameter), ...
        {
            SkScalar width = SkScalarDiv(SkIntToScalar(count), diameter);
            // Now compute the number of cells (ON and OFF)
            count = SkScalarRound(width);
            // Now compute the number of ONs
            count = (count + 1) >> 1;
        }

        SkAutoMalloc storage(count * sizeof(SkPoint));
        SkPoint* verts = (SkPoint*)storage.get();
        // Now build the array of vertices to past to drawPoints
        for (int i = 0; i < count; i++) {
            verts[i].set(x, y);
            x += dx;
            y += dy;
        }

        paint.setStyle(SkPaint::kFill_Style);
        paint.setPathEffect(0);

        // Clipping to bounds is not required for correctness, but it does
        // allow us to reject the entire array of points if we are completely
        // offscreen. This is common in a webpage for android, where most of
        // the content is clipped out. If drawPoints took an (optional) bounds
        // parameter, that might even be better, as we would *just* use it for
        // culling, and not both wacking the canvas' save/restore stack.
        canvas->save(SkCanvas::kClip_SaveFlag);
        canvas->clipRect(bounds);
        canvas->drawPoints(SkCanvas::kPoints_PointMode, count, verts, paint);
        canvas->restore();
    } else {
        SkPoint pts[2] = { point1, point2 };
        canvas->drawLine(pts[0].fX, pts[0].fY, pts[1].fX, pts[1].fY, paint);
    }
}

static void setrectForUnderline(SkRect* r, GraphicsContext* context, const FloatPoint& point, int yOffset, float width)
{
    float lineThickness = context->strokeThickness();
#if 0
    if (lineThickness < 1) // Do we really need/want this?
        lineThickness = 1;
#endif
    r->fLeft    = point.x();
    r->fTop     = point.y() + yOffset;
    r->fRight   = r->fLeft + width;
    r->fBottom  = r->fTop + lineThickness;
}

void GraphicsContext::drawLineForText(const FloatPoint& pt, float width, bool)
{
    if (paintingDisabled())
        return;

    SkRect r;
    setrectForUnderline(&r, this, pt, 0, width);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(this->strokeColor().rgb());

    GC2CANVAS(this)->drawRect(r, paint);
}

// TODO: Should we draw different based on TextCheckingLineStyle?
void GraphicsContext::drawLineForTextChecking(const FloatPoint& pt, float width, TextCheckingLineStyle)
{
    if (paintingDisabled())
        return;

    SkRect r;
    setrectForUnderline(&r, this, pt, 0, width);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorRED); // Is this specified somewhere?

    GC2CANVAS(this)->drawRect(r, paint);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    SkRect oval(rect);

    if (fillColor().rgb() & 0xFF000000) {
        m_data->setupPaintFill(&paint);
        GC2CANVAS(this)->drawOval(oval, paint);
    }
    if (strokeStyle() != NoStroke) {
        paint.reset();
        m_data->setupPaintStroke(&paint, &oval);
        GC2CANVAS(this)->drawOval(oval, paint);
    }
}

static inline int fastMod(int value, int max)
{
    int sign = SkExtractSign(value);

    value = SkApplySign(value, sign);
    if (value >= max)
        value %= max;
    return SkApplySign(value, sign);
}

void GraphicsContext::strokeArc(const IntRect& r, int startAngle, int angleSpan)
{
    if (paintingDisabled())
        return;

    SkPath path;
    SkPaint paint;
    SkRect oval(r);

    if (strokeStyle() == NoStroke) {
        m_data->setupPaintFill(&paint); // We want the fill color
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(SkFloatToScalar(this->strokeThickness()));
    } else
        m_data->setupPaintStroke(&paint, 0);

    // We do this before converting to scalar, so we don't overflow SkFixed
    startAngle = fastMod(startAngle, 360);
    angleSpan = fastMod(angleSpan, 360);

    path.addArc(oval, SkIntToScalar(-startAngle), SkIntToScalar(-angleSpan));
    GC2CANVAS(this)->drawPath(path, paint);
}

void GraphicsContext::drawConvexPolygon(size_t numPoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    SkPaint paint;
    SkPath path;

    path.incReserve(numPoints);
    path.moveTo(SkFloatToScalar(points[0].x()), SkFloatToScalar(points[0].y()));
    for (size_t i = 1; i < numPoints; i++)
        path.lineTo(SkFloatToScalar(points[i].x()), SkFloatToScalar(points[i].y()));

    if (GC2CANVAS(this)->quickReject(path, shouldAntialias ?
            SkCanvas::kAA_EdgeType : SkCanvas::kBW_EdgeType)) {
        return;
    }

    if (fillColor().rgb() & 0xFF000000) {
        m_data->setupPaintFill(&paint);
        paint.setAntiAlias(shouldAntialias);
        GC2CANVAS(this)->drawPath(path, paint);
    }

    if (strokeStyle() != NoStroke) {
        paint.reset();
        m_data->setupPaintStroke(&paint, 0);
        paint.setAntiAlias(shouldAntialias);
        GC2CANVAS(this)->drawPath(path, paint);
    }
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
                                      const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color, ColorSpace)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    SkPath path;
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

    m_data->setupPaintFill(&paint);
    paint.setColor(color.rgb());
    GC2CANVAS(this)->drawPath(path, paint);
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    save();
    SkPaint paint;

    m_data->setupPaintFill(&paint);

    extactShader(&paint,
                 m_state.fillPattern.get(),
                 m_state.fillGradient.get());

    GC2CANVAS(this)->drawRect(rect, paint);
    restore();
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, ColorSpace)
{
    if (paintingDisabled())
        return;

    if (color.rgb() & 0xFF000000) {
        save();
        SkPaint paint;

        m_data->setupPaintCommon(&paint);
        paint.setColor(color.rgb()); // Punch in the specified color
        paint.setShader(0); // In case we had one set

        // Sometimes we record and draw portions of the page, using clips
        // for each portion. The problem with this is that webkit, sometimes,
        // sees that we're only recording a portion, and they adjust some of
        // their rectangle coordinates accordingly (e.g.
        // RenderBoxModelObject::paintFillLayerExtended() which calls
        // rect.intersect(paintInfo.rect) and then draws the bg with that
        // rect. The result is that we end up drawing rects that are meant to
        // seam together (one for each portion), but if the rects have
        // fractional coordinates (e.g. we are zoomed by a fractional amount)
        // we will double-draw those edges, resulting in visual cracks or
        // artifacts.

        // The fix seems to be to just turn off antialasing for rects (this
        // entry-point in GraphicsContext seems to have been sufficient,
        // though perhaps we'll find we need to do this as well in fillRect(r)
        // as well.) Currently setupPaintCommon() enables antialiasing.

        // Since we never show the page rotated at a funny angle, disabling
        // antialiasing seems to have no real down-side, and it does fix the
        // bug when we're zoomed (and drawing portions that need to seam).
        paint.setAntiAlias(false);

        GC2CANVAS(this)->drawRect(rect, paint);
        restore();
    }
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    GC2CANVAS(this)->clipRect(rect);
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;

    m_data->clipPathAntiAliased(*path.platformPath());
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    if (paintingDisabled())
        return;

    SkPath path;
    SkRect r(rect);

    path.addOval(r, SkPath::kCW_Direction);
    // Only perform the inset if we won't invert r
    if (2 * thickness < rect.width() && 2 * thickness < rect.height()) {
        // Adding one to the thickness doesn't make the border too thick as
        // it's painted over afterwards. But without this adjustment the
        // border appears a little anemic after anti-aliasing.
        r.inset(SkIntToScalar(thickness + 1), SkIntToScalar(thickness + 1));
        path.addOval(r, SkPath::kCCW_Direction);
    }
    m_data->clipPathAntiAliased(path);
}

void GraphicsContext::canvasClip(const Path& path)
{
    clip(path);
}

void GraphicsContext::clipOut(const IntRect& r)
{
    if (paintingDisabled())
        return;

    GC2CANVAS(this)->clipRect(r, SkRegion::kDifference_Op);
}

#if ENABLE(SVG)
void GraphicsContext::clipPath(const Path& pathToClip, WindRule clipRule)
{
    if (paintingDisabled())
        return;

    SkPath path = *pathToClip.platformPath();
    path.setFillType(clipRule == RULE_EVENODD ? SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType);
    GC2CANVAS(this)->clipPath(path);
}
#endif

void GraphicsContext::clipOut(const Path& p)
{
    if (paintingDisabled())
        return;

    GC2CANVAS(this)->clipPath(*p.platformPath(), SkRegion::kDifference_Op);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

#if SVG_SUPPORT
KRenderingDeviceContext* GraphicsContext::createRenderingDeviceContext()
{
    return new KRenderingDeviceContextQuartz(platformContext());
}
#endif

// These are the flags we need when we call saveLayer for transparency.
// Since it does not appear that webkit intends this to also save/restore
// the matrix or clip, I do not give those flags (for performance)
#define TRANSPARENCY_SAVEFLAGS                                  \
    (SkCanvas::SaveFlags)(SkCanvas::kHasAlphaLayer_SaveFlag |   \
                          SkCanvas::kFullColorLayer_SaveFlag)

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    SkCanvas* canvas = GC2CANVAS(this);
    canvas->saveLayerAlpha(0, (int)(opacity * 255), TRANSPARENCY_SAVEFLAGS);
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    GC2CANVAS(this)->restore();
}

///////////////////////////////////////////////////////////////////////////

void GraphicsContext::setupBitmapPaint(SkPaint* paint)
{
    m_data->setupPaintBitmap(paint);
}

void GraphicsContext::setupFillPaint(SkPaint* paint)
{
    m_data->setupPaintFill(paint);
}

void GraphicsContext::setupStrokePaint(SkPaint* paint)
{
    m_data->setupPaintStroke(paint, 0);
}

bool GraphicsContext::setupShadowPaint(SkPaint* paint, SkPoint* offset)
{
    return m_data->getState()->setupShadowPaint(this, paint, offset);
}

void GraphicsContext::setPlatformStrokeColor(const Color& c, ColorSpace)
{
    m_data->setStrokeColor(c);
}

void GraphicsContext::setPlatformStrokeThickness(float f)
{
    m_data->setStrokeThickness(f);
}

void GraphicsContext::setPlatformFillColor(const Color& c, ColorSpace)
{
    m_data->setFillColor(c);
}

void GraphicsContext::setPlatformShadow(const FloatSize& size, float blur, const Color& color, ColorSpace)
{
    if (paintingDisabled())
        return;

    if (blur <= 0)
        this->clearPlatformShadow();

    SkColor c;
    if (color.isValid())
        c = color.rgb();
    else
        c = SkColorSetARGB(0xFF / 3, 0, 0, 0); // "std" Apple shadow color
    m_data->getState()->setShadow(blur, size.width(), size.height(), c);
}

void GraphicsContext::clearPlatformShadow()
{
    if (paintingDisabled())
        return;

    m_data->getState()->setShadow(0, 0, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////

void GraphicsContext::drawFocusRing(const Vector<IntRect>&, int, int, const Color&)
{
    // Do nothing, since we draw the focus ring independently.
}

void GraphicsContext::drawFocusRing(const Path&, int, int, const Color&)
{
    // Do nothing, since we draw the focus ring independently.
}

PlatformGraphicsContext* GraphicsContext::platformContext() const
{
    ASSERT(!paintingDisabled());
    return m_data->getPlatformGfxCtx();
}

void GraphicsContext::setMiterLimit(float limit)
{
    m_data->getState()->miterLimit = limit;
}

void GraphicsContext::setAlpha(float alpha)
{
    m_data->getState()->alpha = alpha;
}

void GraphicsContext::setPlatformCompositeOperation(CompositeOperator op)
{
    m_data->getState()->mode = WebCoreCompositeToSkiaComposite(op);
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    SkPaint paint;

    m_data->setupPaintFill(&paint);
    paint.setXfermodeMode(SkXfermode::kClear_Mode);
    GC2CANVAS(this)->drawRect(rect, paint);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (paintingDisabled())
        return;

    SkPaint paint;

    m_data->setupPaintStroke(&paint, 0);
    paint.setStrokeWidth(SkFloatToScalar(lineWidth));
    GC2CANVAS(this)->drawRect(rect, paint);
}

void GraphicsContext::setLineCap(LineCap cap)
{
    switch (cap) {
    case ButtCap:
        m_data->getState()->lineCap = SkPaint::kButt_Cap;
        break;
    case RoundCap:
        m_data->getState()->lineCap = SkPaint::kRound_Cap;
        break;
    case SquareCap:
        m_data->getState()->lineCap = SkPaint::kSquare_Cap;
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

    size_t count = !(dashLength % 2) ? dashLength : dashLength * 2;
    SkScalar* intervals = new SkScalar[count];

    for (unsigned int i = 0; i < count; i++)
        intervals[i] = SkFloatToScalar(dashes[i % dashLength]);
    SkPathEffect **effectPtr = &m_data->getState()->pathEffect;
    SkSafeUnref(*effectPtr);
    *effectPtr = new SkDashPathEffect(intervals, count, SkFloatToScalar(dashOffset));

    delete[] intervals;
}
#endif

void GraphicsContext::setLineJoin(LineJoin join)
{
    switch (join) {
    case MiterJoin:
        m_data->getState()->lineJoin = SkPaint::kMiter_Join;
        break;
    case RoundJoin:
        m_data->getState()->lineJoin = SkPaint::kRound_Join;
        break;
    case BevelJoin:
        m_data->getState()->lineJoin = SkPaint::kBevel_Join;
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
    GC2CANVAS(this)->scale(SkFloatToScalar(size.width()), SkFloatToScalar(size.height()));
}

void GraphicsContext::rotate(float angleInRadians)
{
    if (paintingDisabled())
        return;
    GC2CANVAS(this)->rotate(SkFloatToScalar(angleInRadians * (180.0f / 3.14159265f)));
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;
    GC2CANVAS(this)->translate(SkFloatToScalar(x), SkFloatToScalar(y));
}

void GraphicsContext::concatCTM(const AffineTransform& affine)
{
    if (paintingDisabled())
        return;
    GC2CANVAS(this)->concat(affine);
}

// This is intended to round the rect to device pixels (through the CTM)
// and then invert the result back into source space, with the hope that when
// it is drawn (through the matrix), it will land in the "right" place (i.e.
// on pixel boundaries).

// For android, we record this geometry once and then draw it though various
// scale factors as the user zooms, without re-recording. Thus this routine
// should just leave the original geometry alone.

// If we instead draw into bitmap tiles, we should then perform this
// transform -> round -> inverse step.

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect, RoundingMode)
{
    return rect;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
// Appears to be PDF specific, so we ignore it
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
    m_data->getState()->useAA = useAA;
}

AffineTransform GraphicsContext::getCTM() const
{
    const SkMatrix& m = GC2CANVAS(this)->getTotalMatrix();
    return AffineTransform(SkScalarToDouble(m.getScaleX()), // a
                           SkScalarToDouble(m.getSkewY()), // b
                           SkScalarToDouble(m.getSkewX()), // c
                           SkScalarToDouble(m.getScaleY()), // d
                           SkScalarToDouble(m.getTranslateX()), // e
                           SkScalarToDouble(m.getTranslateY())); // f
}

void GraphicsContext::setCTM(const AffineTransform& transform)
{
    // The SkPicture mode of Skia does not support SkCanvas::setMatrix(), so we
    // can not simply use that method here. We could calculate the transform
    // required to achieve the desired matrix and use SkCanvas::concat(), but
    // there's currently no need for this.
    ASSERT_NOT_REACHED();
}

///////////////////////////////////////////////////////////////////////////////

void GraphicsContext::fillPath(const Path& pathToFill)
{
    SkPath* path = pathToFill.platformPath();
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
    m_data->setupPaintFill(&paint);

    extactShader(&paint,
                 m_state.fillPattern.get(),
                 m_state.fillGradient.get());

    GC2CANVAS(this)->drawPath(*path, paint);
}

void GraphicsContext::strokePath(const Path& pathToStroke)
{
    const SkPath* path = pathToStroke.platformPath();
    if (paintingDisabled() || !path)
        return;

    SkPaint paint;
    m_data->setupPaintStroke(&paint, 0);

    extactShader(&paint,
                 m_state.strokePattern.get(),
                 m_state.strokeGradient.get());

    GC2CANVAS(this)->drawPath(*path, paint);
}

InterpolationQuality GraphicsContext::imageInterpolationQuality() const
{
    notImplemented();
    return InterpolationDefault;
}

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality mode)
{
#if 0
    enum InterpolationQuality {
        InterpolationDefault,
        InterpolationNone,
        InterpolationLow,
        InterpolationMedium,
        InterpolationHigh
    };
#endif
    // TODO: record this, so we can know when to use bitmap-filtering when we draw
    // ... not sure how meaningful this will be given our playback model.

    // Certainly safe to do nothing for the present.
}

void GraphicsContext::clipConvexPolygon(size_t numPoints, const FloatPoint*, bool antialias)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    // FIXME: IMPLEMENT!
}

} // namespace WebCore

///////////////////////////////////////////////////////////////////////////////

SkCanvas* android_gc2canvas(WebCore::GraphicsContext* gc)
{
    return gc->platformContext()->mCanvas;
}
