/*
 * Copyright (C) 2003, 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#ifndef GraphicsContext_h
#define GraphicsContext_h

#include "FloatRect.h"
#include "Image.h"
#include "IntRect.h"
#include "Path.h"
#include "TextDirection.h"
#include <wtf/Noncopyable.h>
#include <wtf/Platform.h>

#ifdef ANDROID_CANVAS_IMPL
    #include "PlatformGraphics.h"
#endif

#if PLATFORM(CG)
typedef struct CGContext PlatformGraphicsContext;
#elif PLATFORM(CAIRO)
typedef struct _cairo PlatformGraphicsContext;
#elif PLATFORM(QT)
class QPainter;
typedef QPainter PlatformGraphicsContext;
#elif PLATFORM(SGL)
namespace WebCore {
class PlatformGraphicsContext;
}
#elif PLATFORM(WX)
class wxGCDC;
class wxWindowDC;

// wxGraphicsContext allows us to support Path, etc. 
// but on some platforms, e.g. Linux, it requires fairly 
// new software.
#if USE(WXGC)
// On OS X, wxGCDC is just a typedef for wxDC, so use wxDC explicitly to make
// the linker happy.
#ifdef __APPLE__
    class wxDC;
    typedef wxDC PlatformGraphicsContext;
#else
    typedef wxGCDC PlatformGraphicsContext;
#endif
#else
    typedef wxWindowDC PlatformGraphicsContext;
#endif
#else
typedef void PlatformGraphicsContext;
#endif

#if PLATFORM(GTK)
typedef struct _GdkDrawable GdkDrawable;
typedef struct _GdkEventExpose GdkEventExpose;
#endif

#if PLATFORM(WIN)
typedef struct HDC__* HDC;
#endif

namespace WebCore {

    const int cMisspellingLineThickness = 3;
    const int cMisspellingLinePatternWidth = 4;
    const int cMisspellingLinePatternGapWidth = 1;

    class AffineTransform;
    class Font;
    class GraphicsContextPrivate;
    class GraphicsContextPlatformPrivate;
    class KURL;
    class Path;
    class TextRun;

    // These bits can be ORed together for a total of 8 possible text drawing modes.
    const int cTextInvisible = 0;
    const int cTextFill = 1;
    const int cTextStroke = 2;
    const int cTextClip = 4;
    
    enum StrokeStyle {
        NoStroke,
        SolidStroke,
        DottedStroke,
        DashedStroke
    };

    class GraphicsContext : Noncopyable {
    public:
        GraphicsContext(PlatformGraphicsContext*);
        ~GraphicsContext();
       
        PlatformGraphicsContext* platformContext() const;

        const Font& font() const;
        void setFont(const Font&);
        
        float strokeThickness() const;
        void setStrokeThickness(float);
        StrokeStyle strokeStyle() const;
        void setStrokeStyle(const StrokeStyle& style);
        Color strokeColor() const;
        void setStrokeColor(const Color&);

        Color fillColor() const;
        void setFillColor(const Color&);
        
        void save();
        void restore();
        
        // These draw methods will do both stroking and filling.
        void drawRect(const IntRect&);
        void drawLine(const IntPoint&, const IntPoint&);
        void drawEllipse(const IntRect&);
        void drawConvexPolygon(size_t numPoints, const FloatPoint*, bool shouldAntialias = false);

#ifdef ANDROID_CANVAS_IMPL
        /** Fill the specified path using the optional gradient or pattern, using the following
            precedence. If/when gradients/patterns are added to the graphics context, these
            parameters can go away
            1) use gradient if gradient != null
            2) use pattern if pattern != null
            3) use color in the graphics context
        */
        void fillPath(const Path&, PlatformGradient*, PlatformPattern*);
        /** Stroke the specified path using the optional gradient or pattern, using the following
            precedence. If/when gradients/patterns are added to the graphics context, these
            parameters can go away
            1) use gradient if gradient != null
            2) use pattern if pattern != null
            3) use color in the graphics context
        */
        void strokePath(const Path&, PlatformGradient*, PlatformPattern*);
        /** Fill the specified rect using the optional gradient or pattern, using the following
            precedence. If/when gradients/patterns are added to the graphics context, these
            parameters can go away
            1) use gradient if gradient != null
            2) use pattern if pattern != null
            3) use color in the graphics context
        */
        void fillRect(const FloatRect&, PlatformGradient*, PlatformPattern*);
        /** Stroke the specified rect using the optional gradient or pattern, using the following
            precedence. If/when gradients/patterns are added to the graphics context, these
            parameters can go away
            1) use gradient if gradient != null
            2) use pattern if pattern != null
            3) use color in the graphics context
        */
        void strokeRect(const FloatRect&, float lineWidth, PlatformGradient*, PlatformPattern*);
        
        /** Return a platform specific linear-gradient. Use freePlatformGradient() when you are
            done with it.
            stopData is { stop, red, green, blue, alpha } per entry
        */
        static PlatformGradient* newPlatformLinearGradient(const FloatPoint& p0, const FloatPoint& p1,
                                                           const float stopData[5], int count);

        /** Return a platform specific radial-gradient. Use freePlatformGradient() when you are
            done with it.
            stopData is { stop, red, green, blue, alpha } per entry
        */
        static PlatformGradient* newPlatformRadialGradient(const FloatPoint& p0, float r0,
                                                           const FloatPoint& p1, float r1,
                                                           const float stopData[5], int count);
        static void freePlatformGradient(PlatformGradient*);

        /** Return a platform specific pattern. Use freePlatformPattern() when you are
            done with it.
        */
        static PlatformPattern* newPlatformPattern(Image* image,
                                                   Image::TileRule hRule, Image::TileRule vRule);
        static void freePlatformPattern(PlatformPattern*);

        /** platform-specific factory method to return a bitmap graphicscontext,
            called by <canvas> when we need to draw offscreen. Caller is responsible for
            deleting the context. Use drawOffscreenContext() to draw the context's image
            onto another graphics context.
        */
        static GraphicsContext* createOffscreenContext(int width, int height);
        /** Called with a context returned by createOffscreenContext. Draw the underlying
            bitmap to the current context. Similar to drawImage(), but this hides how
            to extract the bitmap from ctx from the portable code.
            If srcRect is NULL, it is assumed that we want to draw the entire bitmap represented
            by the GraphicsContext.
        */
        void drawOffscreenContext(GraphicsContext* ctx, const WebCore::FloatRect* srcRect,
                                  const WebCore::FloatRect& dstRect);
        
        /** Return the clip bounds in local coordinates. It can be an approximation, as long as
            the returned bounds completely enclose the actual clip.
        */
        FloatRect getClipLocalBounds() const;
#endif

        // Arc drawing (used by border-radius in CSS) just supports stroking at the moment.
        void strokeArc(const IntRect&, int startAngle, int angleSpan);
        
        void fillRect(const IntRect&, const Color&);
        void fillRect(const FloatRect&, const Color&);
        void fillRoundedRect(const IntRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color&);
        void clearRect(const FloatRect&);
        void strokeRect(const FloatRect&, float lineWidth);

        void drawImage(Image*, const IntPoint&, CompositeOperator = CompositeSourceOver);
        void drawImage(Image*, const IntRect&, CompositeOperator = CompositeSourceOver, bool useLowQualityScale = false);
        void drawImage(Image*, const IntPoint& destPoint, const IntRect& srcRect, CompositeOperator = CompositeSourceOver);
        void drawImage(Image*, const IntRect& destRect, const IntRect& srcRect, CompositeOperator = CompositeSourceOver, bool useLowQualityScale = false);
        void drawImage(Image*, const FloatRect& destRect, const FloatRect& srcRect = FloatRect(0, 0, -1, -1),
                       CompositeOperator = CompositeSourceOver, bool useLowQualityScale = false);
        void drawTiledImage(Image*, const IntRect& destRect, const IntPoint& srcPoint, const IntSize& tileSize,
                       CompositeOperator = CompositeSourceOver);
        void drawTiledImage(Image*, const IntRect& destRect, const IntRect& srcRect, 
                            Image::TileRule hRule = Image::StretchTile, Image::TileRule vRule = Image::StretchTile,
                            CompositeOperator = CompositeSourceOver);
#if PLATFORM(CG)
        void setUseLowQualityImageInterpolation(bool = true);
        bool useLowQualityImageInterpolation() const;
#else
        void setUseLowQualityImageInterpolation(bool = true) {}
        bool useLowQualityImageInterpolation() const { return false; }
#endif

        void clip(const IntRect&);
        void addRoundedRectClip(const IntRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight);
        void addInnerRoundedRectClip(const IntRect&, int thickness);
        void clipOut(const IntRect&);
        void clipOutEllipseInRect(const IntRect&);
        void clipOutRoundedRect(const IntRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight);

        int textDrawingMode();
        void setTextDrawingMode(int);

        void drawText(const TextRun&, const IntPoint&, int from = 0, int to = -1);
        void drawBidiText(const TextRun&, const IntPoint&);
        void drawHighlightForText(const TextRun&, const IntPoint&, int h, const Color& backgroundColor, int from = 0, int to = -1);

        FloatRect roundToDevicePixels(const FloatRect&);
        
        void drawLineForText(const IntPoint&, int width, bool printing);
        void drawLineForMisspellingOrBadGrammar(const IntPoint&, int width, bool grammar);
        
        bool paintingDisabled() const;
        void setPaintingDisabled(bool);
        
        bool updatingControlTints() const;
        void setUpdatingControlTints(bool);

        void beginTransparencyLayer(float opacity);
        void endTransparencyLayer();

        void setShadow(const IntSize&, int blur, const Color&);
        void clearShadow();

        void initFocusRing(int width, int offset);
        void addFocusRingRect(const IntRect&);
        void drawFocusRing(const Color&);
        void clearFocusRing();
        IntRect focusRingBoundingRect();

        void setLineCap(LineCap);
        void setLineJoin(LineJoin);
        void setMiterLimit(float);

        void setAlpha(float);

        void setCompositeOperation(CompositeOperator);

        void beginPath();
        void addPath(const Path&);

        void clip(const Path&);
        void clipOut(const Path&);

        void scale(const FloatSize&);
        void rotate(float angleInRadians);
        void translate(float x, float y);
        IntPoint origin();
        
        void setURLForRect(const KURL&, const IntRect&);

        void concatCTM(const AffineTransform&);
        AffineTransform getCTM() const;

        void setUseAntialiasing(bool = true);

#if PLATFORM(WIN)
        GraphicsContext(HDC); // FIXME: To be removed.
        bool inTransparencyLayer() const;
        HDC getWindowsContext(const IntRect&, bool supportAlphaBlend = true); // The passed in rect is used to create a bitmap for compositing inside transparency layers.
        void releaseWindowsContext(HDC, const IntRect&, bool supportAlphaBlend = true);    // The passed in HDC should be the one handed back by getWindowsContext.
#endif

#if PLATFORM(QT)
        void setFillRule(WindRule);
        PlatformPath* currentPath();
#endif

#if PLATFORM(GTK)
        void setGdkExposeEvent(GdkEventExpose*);
        GdkDrawable* gdkDrawable() const;
        GdkEventExpose* gdkExposeEvent() const;
        IntPoint translatePoint(const IntPoint&) const;
#endif

    private:
        void savePlatformState();
        void restorePlatformState();
        void setPlatformTextDrawingMode(int);
        void setPlatformStrokeColor(const Color&);
        void setPlatformStrokeStyle(const StrokeStyle&);
        void setPlatformStrokeThickness(float);
        void setPlatformFillColor(const Color&);
        void setPlatformFont(const Font& font);

        int focusRingWidth() const;
        int focusRingOffset() const;
        const Vector<IntRect>& focusRingRects() const;

        static GraphicsContextPrivate* createGraphicsContextPrivate();
        static void destroyGraphicsContextPrivate(GraphicsContextPrivate*);

        GraphicsContextPrivate* m_common;
        GraphicsContextPlatformPrivate* m_data;
    };

} // namespace WebCore

#endif // GraphicsContext_h
