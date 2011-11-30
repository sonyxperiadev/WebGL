/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "EmojiFont.h"
#include "Font.h"
#include "FontData.h"
#include "FontFallbackList.h"
#include "GraphicsContext.h"
#include "GlyphBuffer.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "PlatformGraphicsContext.h"
#include "SkCanvas.h"
#include "SkColorFilter.h"
#include "SkLayerDrawLooper.h"
#include "SkPaint.h"
#include "SkTemplates.h"
#include "SkTypeface.h"
#include "SkUtils.h"
#include "TextRun.h"

#ifdef SUPPORT_COMPLEX_SCRIPTS
#include "HarfbuzzSkia.h"
#include <unicode/normlzr.h>
#include <unicode/uchar.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnArrayPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/unicode/Unicode.h>
#endif

using namespace android;

namespace WebCore {

typedef std::pair<int, float> FallbackFontKey;
typedef HashMap<FallbackFontKey, FontPlatformData*> FallbackHash;

static void updateForFont(SkPaint* paint, const SimpleFontData* font) {
    font->platformData().setupPaint(paint);
    paint->setTextEncoding(SkPaint::kGlyphID_TextEncoding);
}

static SkPaint* setupFill(SkPaint* paint, GraphicsContext* gc,
                          const SimpleFontData* font) {
    gc->setupFillPaint(paint);
    updateForFont(paint, font);
    return paint;
}

static SkPaint* setupStroke(SkPaint* paint, GraphicsContext* gc,
                            const SimpleFontData* font) {
    gc->setupStrokePaint(paint);
    updateForFont(paint, font);
    return paint;
}

static bool setupForText(SkPaint* paint, GraphicsContext* gc,
                         const SimpleFontData* font) {
    int mode = gc->textDrawingMode() & (TextModeFill | TextModeStroke);
    if (!mode)
        return false;

    FloatSize shadowOffset;
    float shadowBlur;
    Color shadowColor;
    ColorSpace shadowColorSpace;
    bool hasShadow = gc->getShadow(shadowOffset, shadowBlur, shadowColor, shadowColorSpace);
    bool hasBothStrokeAndFill =
        (mode & (TextModeStroke | TextModeFill)) == (TextModeStroke | TextModeFill);
    if (hasShadow || hasBothStrokeAndFill) {
        SkLayerDrawLooper* looper = new SkLayerDrawLooper;
        paint->setLooper(looper)->unref();

        // The layerDrawLooper uses at the root paint to determine the text
        // encoding so we need to make sure it is properly configured.
        updateForFont(paint, font);

        // Specify the behavior of the looper
        SkLayerDrawLooper::LayerInfo info;
        info.fPaintBits = SkLayerDrawLooper::kEntirePaint_Bits;
        info.fColorMode = SkXfermode::kSrc_Mode;
        info.fFlagsMask = SkPaint::kAllFlags;

        // The paint is only valid until the looper receives another call to
        // addLayer(). Therefore, we must cache certain state for later use.
        bool hasFillPaint = false;
        bool hasStrokePaint = false;
        SkScalar strokeWidth;

        if ((mode & TextModeStroke) && gc->willStroke()) {
            strokeWidth = setupStroke(looper->addLayer(info), gc, font)->getStrokeWidth();
            hasStrokePaint = true;
        }
        if ((mode & TextModeFill) && gc->willFill()) {
            setupFill(looper->addLayer(info), gc, font);
            hasFillPaint = true;
        }

        if (hasShadow) {
            SkPaint shadowPaint;
            SkPoint offset;
            if (gc->setupShadowPaint(&shadowPaint, &offset)) {

                // add an offset to the looper when creating a shadow layer
                info.fOffset.set(offset.fX, offset.fY);

                SkPaint* p = looper->addLayer(info);
                *p = shadowPaint;

                // Currently, only GraphicsContexts associated with the
                // HTMLCanvasElement have shadows ignore transforms set.  This
                // allows us to distinguish between CSS and Canvas shadows which
                // have different rendering specifications.
                if (gc->shadowsIgnoreTransforms()) {
                    SkColorFilter* cf = SkColorFilter::CreateModeFilter(p->getColor(),
                            SkXfermode::kSrcIn_Mode);
                    p->setColorFilter(cf)->unref();
                } else { // in CSS
                    p->setShader(NULL);
                }

                if (hasStrokePaint && !hasFillPaint) {
                    // stroke the shadow if we have stroke but no fill
                    p->setStyle(SkPaint::kStroke_Style);
                    p->setStrokeWidth(strokeWidth);
                }
                updateForFont(p, font);
            }
        }
    } else if (mode & TextModeFill) {
        (void)setupFill(paint, gc, font);
    } else if (mode & TextModeStroke) {
        (void)setupStroke(paint, gc, font);
    } else {
        return false;
    }
    return true;
}

bool Font::canReturnFallbackFontsForComplexText()
{
    return false;
}

bool Font::canExpandAroundIdeographsInComplexText()
{
    return false;
}

void Font::drawGlyphs(GraphicsContext* gc, const SimpleFontData* font,
                      const GlyphBuffer& glyphBuffer,  int from, int numGlyphs,
                      const FloatPoint& point) const
{
    // compile-time assert
    SkASSERT(sizeof(GlyphBufferGlyph) == sizeof(uint16_t));

    SkPaint paint;
    if (!setupForText(&paint, gc, font)) {
        return;
    }

    SkScalar                    x = SkFloatToScalar(point.x());
    SkScalar                    y = SkFloatToScalar(point.y());
    const GlyphBufferGlyph*     glyphs = glyphBuffer.glyphs(from);
    const GlyphBufferAdvance*   adv = glyphBuffer.advances(from);
    SkAutoSTMalloc<32, SkPoint> storage(numGlyphs), storage2(numGlyphs), storage3(numGlyphs);
    SkPoint*                    pos = storage.get();
    SkPoint*                    vPosBegin = storage2.get();
    SkPoint*                    vPosEnd = storage3.get();

    SkCanvas* canvas = gc->platformContext()->mCanvas;

    /*  We need an array of [x,y,x,y,x,y,...], but webkit is giving us
        point.xy + [width, height, width, height, ...], so we have to convert
     */

    if (EmojiFont::IsAvailable()) {
        // set filtering, to make scaled images look nice(r)
        paint.setFilterBitmap(true);

        int localIndex = 0;
        int localCount = 0;
        for (int i = 0; i < numGlyphs; i++) {
            if (EmojiFont::IsEmojiGlyph(glyphs[i])) {
                if (localCount)
                    canvas->drawPosText(&glyphs[localIndex],
                                        localCount * sizeof(uint16_t),
                                        &pos[localIndex], paint);
                EmojiFont::Draw(canvas, glyphs[i], x, y, paint);
                // reset local index/count track for "real" glyphs
                localCount = 0;
                localIndex = i + 1;
            } else {
                pos[i].set(x, y);
                localCount += 1;
            }
            x += SkFloatToScalar(adv[i].width());
            y += SkFloatToScalar(adv[i].height());
        }
        // draw the last run of glyphs (if any)
        if (localCount)
            canvas->drawPosText(&glyphs[localIndex],
                                localCount * sizeof(uint16_t),
                                &pos[localIndex], paint);
    } else {
        bool isVertical = font->platformData().orientation() == Vertical;
        for (int i = 0; i < numGlyphs; i++) {
            pos[i].set(x, y);
            y += SkFloatToScalar(adv[i].height());
            if (isVertical) {
                SkScalar myWidth = SkFloatToScalar(adv[i].width());
                vPosBegin[i].set(x + myWidth, y);
                vPosEnd[i].set(x + myWidth, y - myWidth);
                x += myWidth;

                SkPath path;
                path.reset();
                path.moveTo(vPosBegin[i]);
                path.lineTo(vPosEnd[i]);
                canvas->drawTextOnPath(glyphs + i, 2, path, 0, paint);
            }
            else
                x += SkFloatToScalar(adv[i].width());
        }
        if (!isVertical)
            canvas->drawPosText(glyphs, numGlyphs * sizeof(uint16_t), pos, paint);
    }
}

void Font::drawEmphasisMarksForComplexText(WebCore::GraphicsContext*, WebCore::TextRun const&, WTF::AtomicString const&, WebCore::FloatPoint const&, int, int) const
{
    notImplemented();
}

#ifndef SUPPORT_COMPLEX_SCRIPTS

FloatRect Font::selectionRectForComplexText(const TextRun& run,
                                const FloatPoint& point, int h, int, int) const
{
    SkPaint              paint;
    SkScalar             width, left;
    SkPaint::FontMetrics metrics;

    primaryFont()->platformData().setupPaint(&paint);

    width = paint.measureText(run.characters(), run.length() << 1);
    SkScalar spacing = paint.getFontMetrics(&metrics);

    return FloatRect(point.x(),
                     point.y(),
                     roundf(SkScalarToFloat(width)),
                     roundf(SkScalarToFloat(spacing)));
}

void Font::drawComplexText(GraphicsContext* gc, TextRun const& run,
                           FloatPoint const& point, int, int) const
{
    SkCanvas*   canvas = gc->platformContext()->mCanvas;
    SkPaint     paint;

    if (!setupForText(&paint, gc, primaryFont())) {
        return;
    }

    // go to chars, instead of glyphs, which was set by setupForText()
    paint.setTextEncoding(SkPaint::kUTF16_TextEncoding);

    canvas->drawText(run.characters(), run.length() << 1,
                     SkFloatToScalar(point.x()), SkFloatToScalar(point.y()),
                     paint);
}

float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>*, GlyphOverflow*) const
{
    SkPaint paint;

    primaryFont()->platformData().setupPaint(&paint);

//printf("--------- complext measure %d chars\n", run.to() - run.from());

    SkScalar width = paint.measureText(run.characters(), run.length() << 1);
    return SkScalarToFloat(width);
}

int Font::offsetForPositionForComplexText(const TextRun& run, float x,
                                          bool includePartialGlyphs) const
{
    SkPaint                         paint;
    int                             count = run.length();
    SkAutoSTMalloc<64, SkScalar>    storage(count);
    SkScalar*                       widths = storage.get();

    primaryFont()->platformData().setupPaint(&paint);

    count = paint.getTextWidths(run.characters(), count << 1, widths);

    if (count > 0)
    {
        SkScalar pos = 0;
        for (int i = 0; i < count; i++)
        {
            if (x < SkScalarRound(pos + SkScalarHalf(widths[i])))
                return i;
            pos += widths[i];
        }
    }
    return count;
}

#else

// TODO Should we remove the multilayer support?
// If yes. remove isCanvasMultiLayered() and adjustTextRenderMode().
static bool isCanvasMultiLayered(SkCanvas* canvas)
{
    SkCanvas::LayerIter layerIterator(canvas, false);
    layerIterator.next();
    return !layerIterator.done();
}

static void adjustTextRenderMode(SkPaint* paint, bool isCanvasMultiLayered)
{
    // Our layers only have a single alpha channel. This means that subpixel
    // rendered text cannot be compositied correctly when the layer is
    // collapsed. Therefore, subpixel text is disabled when we are drawing
    // onto a layer.
    if (isCanvasMultiLayered)
        paint->setLCDRenderText(false);
}

// Harfbuzz uses 26.6 fixed point values for pixel offsets. However, we don't
// handle subpixel positioning so this function is used to truncate Harfbuzz
// values to a number of pixels.
static int truncateFixedPointToInteger(HB_Fixed value)
{
    return value >> 6;
}

// TextRunWalker walks a TextRun and presents each script run in sequence. A
// TextRun is a sequence of code-points with the same embedding level (i.e. they
// are all left-to-right or right-to-left). A script run is a subsequence where
// all the characters have the same script (e.g. Arabic, Thai etc). Shaping is
// only ever done with script runs since the shapers only know how to deal with
// a single script.
//
// After creating it, the script runs are either iterated backwards or forwards.
// It defaults to backwards for RTL and forwards otherwise (which matches the
// presentation order), however you can set it with |setBackwardsIteration|.
//
// Once you have setup the object, call |nextScriptRun| to get the first script
// run. This will return false when the iteration is complete. At any time you
// can call |reset| to start over again.
class TextRunWalker {
public:
    TextRunWalker(const TextRun&, unsigned, const Font*);
    ~TextRunWalker();

    bool isWordBreak(unsigned, bool);
    // setPadding sets a number of pixels to be distributed across the TextRun.
    // WebKit uses this to justify text.
    void setPadding(int);
    void reset();
    void setBackwardsIteration(bool);
    // Advance to the next script run, returning false when the end of the
    // TextRun has been reached.
    bool nextScriptRun();
    float widthOfFullRun();

    // setWordSpacingAdjustment sets a delta (in pixels) which is applied at
    // each word break in the TextRun.
    void setWordSpacingAdjustment(int wordSpacingAdjustment)
    {
        m_wordSpacingAdjustment = wordSpacingAdjustment;
    }

    // setLetterSpacingAdjustment sets an additional number of pixels that is
    // added to the advance after each output cluster. This matches the behaviour
    // of WidthIterator::advance.
    //
    // (NOTE: currently does nothing because I don't know how to get the
    // cluster information from Harfbuzz.)
    void setLetterSpacingAdjustment(int letterSpacingAdjustment)
    {
        m_letterSpacing = letterSpacingAdjustment;
    }

    // setWordAndLetterSpacing calls setWordSpacingAdjustment() and
    // setLetterSpacingAdjustment() to override m_wordSpacingAdjustment
    // and m_letterSpacing.
    void setWordAndLetterSpacing(int wordSpacingAdjustment, int letterSpacingAdjustment);

    // Set the x offset for the next script run. This affects the values in
    // |xPositions|
    void setXOffsetToZero() { m_offsetX = 0; }
    bool rtl() const { return m_run.rtl(); }
    const uint16_t* glyphs() const { return m_glyphs16; }

    // Return the length of the array returned by |glyphs|
    unsigned length() const { return m_item.num_glyphs; }

    // Return the x offset for each of the glyphs. Note that this is translated
    // by the current x offset and that the x offset is updated for each script
    // run.
    const SkScalar* xPositions() const { return m_xPositions; }

    // Get the advances (widths) for each glyph.
    const HB_Fixed* advances() const { return m_item.advances; }

    // Return the width (in px) of the current script run.
    unsigned width() const { return m_pixelWidth; }

    // Return the cluster log for the current script run. For example:
    //   script run: f i a n c é  (fi gets ligatured)
    //   log clutrs: 0 0 1 2 3 4
    // So, for each input code point, the log tells you which output glyph was
    // generated for it.
    const unsigned short* logClusters() const { return m_item.log_clusters; }

    // return the number of code points in the current script run
    unsigned numCodePoints() const { return m_numCodePoints; }

    const FontPlatformData* fontPlatformDataForScriptRun() {
        return reinterpret_cast<FontPlatformData*>(m_item.font->userData);
    }

private:
    enum CustomScript {
        Bengali,
        Devanagari,
        Hebrew,
        HebrewBold,
        Naskh,
        Tamil,
        Thai,
        NUM_SCRIPTS
    };

    static const char* paths[NUM_SCRIPTS];

    void setupFontForScriptRun();
    const FontPlatformData* setupComplexFont(CustomScript script,
                const FontPlatformData& platformData);
    HB_FontRec* allocHarfbuzzFont();
    void deleteGlyphArrays();
    void createGlyphArrays(int);
    void resetGlyphArrays();
    void shapeGlyphs();
    void setGlyphXPositions(bool);

    static void normalizeSpacesAndMirrorChars(const UChar* source, bool rtl,
        UChar* destination, int length);
    static const TextRun& getNormalizedTextRun(const TextRun& originalRun,
        OwnPtr<TextRun>& normalizedRun, OwnArrayPtr<UChar>& normalizedBuffer);

    // This matches the logic in RenderBlock::findNextLineBreak
    static bool isCodepointSpace(HB_UChar16 c) { return c == ' ' || c == '\t'; }

    const Font* const m_font;
    HB_ShaperItem m_item;
    uint16_t* m_glyphs16; // A vector of 16-bit glyph ids.
    SkScalar* m_xPositions; // A vector of x positions for each glyph.
    ssize_t m_indexOfNextScriptRun; // Indexes the script run in |m_run|.
    const unsigned m_startingX; // Offset in pixels of the first script run.
    unsigned m_offsetX; // Offset in pixels to the start of the next script run.
    unsigned m_pixelWidth; // Width (in px) of the current script run.
    unsigned m_numCodePoints; // Code points in current script run.
    unsigned m_glyphsArrayCapacity; // Current size of all the Harfbuzz arrays.

    OwnPtr<TextRun> m_normalizedRun;
    OwnArrayPtr<UChar> m_normalizedBuffer; // A buffer for normalized run.
    const TextRun& m_run;
    bool m_iterateBackwards;
    int m_wordSpacingAdjustment; // delta adjustment (pixels) for each word break.
    float m_padding; // pixels to be distributed over the line at word breaks.
    float m_padPerWordBreak; // pixels to be added to each word break.
    float m_padError; // |m_padPerWordBreak| might have a fractional component.
                      // Since we only add a whole number of padding pixels at
                      // each word break we accumulate error. This is the
                      // number of pixels that we are behind so far.
    unsigned m_letterSpacing; // pixels to be added after each glyph.
};


// Indexed using enum CustomScript
const char* TextRunWalker::paths[] = {
    "/system/fonts/Lohit-Bengali.ttf",
    "/system/fonts/Lohit-Devanagari.ttf",
    "/system/fonts/DroidSansHebrew-Regular.ttf",
    "/system/fonts/DroidSansHebrew-Bold.ttf",
    "/system/fonts/DroidNaskh-Regular.ttf",
    "/system/fonts/Lohit-Tamil.ttf",
    "/system/fonts/DroidSansThai.ttf"
};

TextRunWalker::TextRunWalker(const TextRun& run, unsigned startingX, const Font* font)
    : m_font(font)
    , m_startingX(startingX)
    , m_offsetX(m_startingX)
    , m_run(getNormalizedTextRun(run, m_normalizedRun, m_normalizedBuffer))
    , m_iterateBackwards(m_run.rtl())
    , m_wordSpacingAdjustment(0)
    , m_padding(0)
    , m_padPerWordBreak(0)
    , m_padError(0)
    , m_letterSpacing(0)
{
    // Do not use |run| inside this constructor. Use |m_run| instead.

    memset(&m_item, 0, sizeof(m_item));
    // We cannot know, ahead of time, how many glyphs a given script run
    // will produce. We take a guess that script runs will not produce more
    // than twice as many glyphs as there are code points plus a bit of
    // padding and fallback if we find that we are wrong.
    createGlyphArrays((m_run.length() + 2) * 2);

    m_item.log_clusters = new unsigned short[m_run.length()];

    m_item.face = 0;
    m_item.font = allocHarfbuzzFont();

    m_item.item.bidiLevel = m_run.rtl();

    m_item.string = m_run.characters();
    m_item.stringLength = m_run.length();

    reset();
}

TextRunWalker::~TextRunWalker()
{
    fastFree(m_item.font);
    deleteGlyphArrays();
    delete[] m_item.log_clusters;
}

bool TextRunWalker::isWordBreak(unsigned index, bool isRTL)
{
    if (!isRTL)
        return index && isCodepointSpace(m_item.string[index])
            && !isCodepointSpace(m_item.string[index - 1]);
    return index != m_item.stringLength - 1 && isCodepointSpace(m_item.string[index])
        && !isCodepointSpace(m_item.string[index + 1]);
}

// setPadding sets a number of pixels to be distributed across the TextRun.
// WebKit uses this to justify text.
void TextRunWalker::setPadding(int padding)
{
    m_padding = padding;
    if (!m_padding)
        return;

    // If we have padding to distribute, then we try to give an equal
    // amount to each space. The last space gets the smaller amount, if
    // any.
    unsigned numWordBreaks = 0;
    bool isRTL = m_iterateBackwards;

    for (unsigned i = 0; i < m_item.stringLength; i++) {
        if (isWordBreak(i, isRTL))
            numWordBreaks++;
    }

    if (numWordBreaks)
        m_padPerWordBreak = m_padding / numWordBreaks;
    else
        m_padPerWordBreak = 0;
}

void TextRunWalker::reset()
{
    if (m_iterateBackwards)
        m_indexOfNextScriptRun = m_run.length() - 1;
    else
        m_indexOfNextScriptRun = 0;
    m_offsetX = m_startingX;
}

void TextRunWalker::setBackwardsIteration(bool isBackwards)
{
    m_iterateBackwards = isBackwards;
    reset();
}

// Advance to the next script run, returning false when the end of the
// TextRun has been reached.
bool TextRunWalker::nextScriptRun()
{
    if (m_iterateBackwards) {
        // In right-to-left mode we need to render the shaped glyph backwards and
        // also render the script runs themselves backwards. So given a TextRun:
        //    AAAAAAACTTTTTTT   (A = Arabic, C = Common, T = Thai)
        // we render:
        //    TTTTTTCAAAAAAA
        // (and the glyphs in each A, C and T section are backwards too)
        if (!hb_utf16_script_run_prev(&m_numCodePoints, &m_item.item, m_run.characters(),
            m_run.length(), &m_indexOfNextScriptRun))
            return false;
    } else {
        if (!hb_utf16_script_run_next(&m_numCodePoints, &m_item.item, m_run.characters(),
            m_run.length(), &m_indexOfNextScriptRun))
            return false;

        // It is actually wrong to consider script runs at all in this code.
        // Other WebKit code (e.g. Mac) segments complex text just by finding
        // the longest span of text covered by a single font.
        // But we currently need to call hb_utf16_script_run_next anyway to fill
        // in the harfbuzz data structures to e.g. pick the correct script's shaper.
        // So we allow that to run first, then do a second pass over the range it
        // found and take the largest subregion that stays within a single font.
        const FontData* glyphData = m_font->glyphDataForCharacter(
           m_item.string[m_item.item.pos], false).fontData;
        unsigned endOfRun;
        for (endOfRun = 1; endOfRun < m_item.item.length; ++endOfRun) {
            const FontData* nextGlyphData = m_font->glyphDataForCharacter(
                m_item.string[m_item.item.pos + endOfRun], false).fontData;
            if (nextGlyphData != glyphData)
                break;
        }
        m_item.item.length = endOfRun;
        m_indexOfNextScriptRun = m_item.item.pos + endOfRun;
    }

    setupFontForScriptRun();
    shapeGlyphs();
    setGlyphXPositions(rtl());

    return true;
}

float TextRunWalker::widthOfFullRun()
{
    float widthSum = 0;
    while (nextScriptRun())
        widthSum += width();

    return widthSum;
}

void TextRunWalker::setWordAndLetterSpacing(int wordSpacingAdjustment,
                                            int letterSpacingAdjustment)
{
    setWordSpacingAdjustment(wordSpacingAdjustment);
    setLetterSpacingAdjustment(letterSpacingAdjustment);
}

const FontPlatformData* TextRunWalker::setupComplexFont(
        CustomScript script,
        const FontPlatformData& platformData)
{
    static FallbackHash fallbackPlatformData;

    FallbackFontKey key(script, platformData.size());
    FontPlatformData* newPlatformData = 0;

    if (!fallbackPlatformData.contains(key)) {
        SkTypeface* typeface = SkTypeface::CreateFromFile(paths[script]);
        newPlatformData = new FontPlatformData(platformData, typeface);
        SkSafeUnref(typeface);
        fallbackPlatformData.set(key, newPlatformData);
    }

    if (!newPlatformData)
        newPlatformData = fallbackPlatformData.get(key);

    // If we couldn't allocate a new FontPlatformData, revert to the one passed
    return newPlatformData ? newPlatformData : &platformData;
}

void TextRunWalker::setupFontForScriptRun()
{
    const FontData* fontData = m_font->glyphDataForCharacter(m_run[0], false).fontData;
    const FontPlatformData& platformData =
        fontData->fontDataForCharacter(' ')->platformData();
    const FontPlatformData* complexPlatformData = &platformData;

    switch (m_item.item.script) {
      case HB_Script_Bengali:
          complexPlatformData = setupComplexFont(Bengali, platformData);
          break;
        case HB_Script_Devanagari:
            complexPlatformData = setupComplexFont(Devanagari, platformData);
            break;
        case HB_Script_Hebrew:
            switch (platformData.typeface()->style()) {
                case SkTypeface::kBold:
                case SkTypeface::kBoldItalic:
                    complexPlatformData = setupComplexFont(HebrewBold, platformData);
                    break;
                case SkTypeface::kNormal:
                case SkTypeface::kItalic:
                default:
                    complexPlatformData = setupComplexFont(Hebrew, platformData);
                    break;
            }
            break;
        case HB_Script_Arabic:
            complexPlatformData = setupComplexFont(Naskh, platformData);
            break;
        case HB_Script_Tamil:
            complexPlatformData = setupComplexFont(Tamil, platformData);
            break;
        case HB_Script_Thai:
            complexPlatformData = setupComplexFont(Thai, platformData);
            break;
        default:
            // HB_Script_Common; includes Ethiopic
            complexPlatformData = &platformData;
            break;
    }
    m_item.face = complexPlatformData->harfbuzzFace();
    m_item.font->userData = const_cast<FontPlatformData*>(complexPlatformData);
}

HB_FontRec* TextRunWalker::allocHarfbuzzFont()
{
    HB_FontRec* font = reinterpret_cast<HB_FontRec*>(fastMalloc(sizeof(HB_FontRec)));
    memset(font, 0, sizeof(HB_FontRec));
    font->klass = &harfbuzzSkiaClass;
    font->userData = 0;
    // The values which harfbuzzSkiaClass returns are already scaled to
    // pixel units, so we just set all these to one to disable further
    // scaling.
    font->x_ppem = 1;
    font->y_ppem = 1;
    font->x_scale = 1;
    font->y_scale = 1;

    return font;
}

void TextRunWalker::deleteGlyphArrays()
{
    delete[] m_item.glyphs;
    delete[] m_item.attributes;
    delete[] m_item.advances;
    delete[] m_item.offsets;
    delete[] m_glyphs16;
    delete[] m_xPositions;
}

void TextRunWalker::createGlyphArrays(int size)
{
    m_item.glyphs = new HB_Glyph[size];
    m_item.attributes = new HB_GlyphAttributes[size];
    m_item.advances = new HB_Fixed[size];
    m_item.offsets = new HB_FixedPoint[size];

    m_glyphs16 = new uint16_t[size];
    m_xPositions = new SkScalar[size];

    m_item.num_glyphs = size;
    m_glyphsArrayCapacity = size; // Save the GlyphArrays size.
}

void TextRunWalker::resetGlyphArrays()
{
    int size = m_item.num_glyphs;
    // All the types here don't have pointers. It is safe to reset to
    // zero unless Harfbuzz breaks the compatibility in the future.
    memset(m_item.glyphs, 0, size * sizeof(m_item.glyphs[0]));
    memset(m_item.attributes, 0, size * sizeof(m_item.attributes[0]));
    memset(m_item.advances, 0, size * sizeof(m_item.advances[0]));
    memset(m_item.offsets, 0, size * sizeof(m_item.offsets[0]));
    memset(m_glyphs16, 0, size * sizeof(m_glyphs16[0]));
    memset(m_xPositions, 0, size * sizeof(m_xPositions[0]));
}

void TextRunWalker::shapeGlyphs()
{
    // HB_ShapeItem() resets m_item.num_glyphs. If the previous call to
    // HB_ShapeItem() used less space than was available, the capacity of
    // the array may be larger than the current value of m_item.num_glyphs.
    // So, we need to reset the num_glyphs to the capacity of the array.
    m_item.num_glyphs = m_glyphsArrayCapacity;
    resetGlyphArrays();
    while (!HB_ShapeItem(&m_item)) {
        // We overflowed our arrays. Resize and retry.
        // HB_ShapeItem fills in m_item.num_glyphs with the needed size.
        deleteGlyphArrays();
        createGlyphArrays(m_item.num_glyphs << 1);
        resetGlyphArrays();
    }
}

void TextRunWalker::setGlyphXPositions(bool isRTL)
{
    int position = 0;
    // logClustersIndex indexes logClusters for the first (or last when
    // RTL) codepoint of the current glyph.  Each time we advance a glyph,
    // we skip over all the codepoints that contributed to the current
    // glyph.
    unsigned logClustersIndex = isRTL && m_item.num_glyphs ? m_item.num_glyphs - 1 : 0;

    for (unsigned iter = 0; iter < m_item.num_glyphs; ++iter) {
        // Glyphs are stored in logical order, but for layout purposes we
        // always go left to right.
        int i = isRTL ? m_item.num_glyphs - iter - 1 : iter;

        m_glyphs16[i] = m_item.glyphs[i];
        m_xPositions[i] = SkIntToScalar(m_offsetX + position);

        int advance = truncateFixedPointToInteger(m_item.advances[i]);
        // The first half of the conjunction works around the case where
        // output glyphs aren't associated with any codepoints by the
        // clusters log.
        if (logClustersIndex < m_item.item.length
            && isWordBreak(m_item.item.pos + logClustersIndex, isRTL)) {
            advance += m_wordSpacingAdjustment;

            if (m_padding > 0) {
                int toPad = roundf(m_padPerWordBreak + m_padError);
                m_padError += m_padPerWordBreak - toPad;

                if (m_padding < toPad)
                    toPad = m_padding;
                m_padding -= toPad;
                advance += toPad;
            }
        }

        // TODO We would like to add m_letterSpacing after each cluster, but I
        // don't know where the cluster information is. This is typically
        // fine for Roman languages, but breaks more complex languages
        // terribly.
        // advance += m_letterSpacing;

        if (isRTL) {
            while (logClustersIndex > 0 && logClusters()[logClustersIndex] == i)
                logClustersIndex--;
        } else {
            while (logClustersIndex < m_item.item.length
                   && logClusters()[logClustersIndex] == i)
                logClustersIndex++;
        }

        position += advance;
    }

    m_pixelWidth = position;
    m_offsetX += m_pixelWidth;
}

void TextRunWalker::normalizeSpacesAndMirrorChars(const UChar* source, bool rtl,
    UChar* destination, int length)
{
    int position = 0;
    bool error = false;
    // Iterate characters in source and mirror character if needed.
    while (position < length) {
        UChar32 character;
        int nextPosition = position;
        U16_NEXT(source, nextPosition, length, character);
        if (Font::treatAsSpace(character))
            character = ' ';
        else if (rtl)
            character = u_charMirror(character);
        U16_APPEND(destination, position, length, character, error);
        ASSERT(!error);
        position = nextPosition;
    }
}

const TextRun& TextRunWalker::getNormalizedTextRun(const TextRun& originalRun,
    OwnPtr<TextRun>& normalizedRun, OwnArrayPtr<UChar>& normalizedBuffer)
{
    // Normalize the text run in three ways:
    // 1) Convert the |originalRun| to NFC normalized form if combining diacritical marks
    // (U+0300..) are used in the run. This conversion is necessary since most OpenType
    // fonts (e.g., Arial) don't have substitution rules for the diacritical marks in
    // their GSUB tables.
    //
    // Note that we don't use the icu::Normalizer::isNormalized(UNORM_NFC) API here since
    // the API returns FALSE (= not normalized) for complex runs that don't require NFC
    // normalization (e.g., Arabic text). Unless the run contains the diacritical marks,
    // Harfbuzz will do the same thing for us using the GSUB table.
    // 2) Convert spacing characters into plain spaces, as some fonts will provide glyphs
    // for characters like '\n' otherwise.
    // 3) Convert mirrored characters such as parenthesis for rtl text.

    // Convert to NFC form if the text has diacritical marks.
    icu::UnicodeString normalizedString;
    UErrorCode error = U_ZERO_ERROR;

    for (int16_t i = 0; i < originalRun.length(); ++i) {
        UChar ch = originalRun[i];
        if (::ublock_getCode(ch) == UBLOCK_COMBINING_DIACRITICAL_MARKS) {
            icu::Normalizer::normalize(icu::UnicodeString(originalRun.characters(),
                                       originalRun.length()), UNORM_NFC, 0 /* no options */,
                                       normalizedString, error);
            if (U_FAILURE(error))
                return originalRun;
            break;
        }
    }

    // Normalize space and mirror parenthesis for rtl text.
    int normalizedBufferLength;
    const UChar* sourceText;
    if (normalizedString.isEmpty()) {
        normalizedBufferLength = originalRun.length();
        sourceText = originalRun.characters();
    } else {
        normalizedBufferLength = normalizedString.length();
        sourceText = normalizedString.getBuffer();
    }

    normalizedBuffer = adoptArrayPtr(new UChar[normalizedBufferLength + 1]);

    normalizeSpacesAndMirrorChars(sourceText, originalRun.rtl(), normalizedBuffer.get(),
                                  normalizedBufferLength);

    normalizedRun = adoptPtr(new TextRun(originalRun));
    normalizedRun->setText(normalizedBuffer.get(), normalizedBufferLength);
    return *normalizedRun;
}

FloatRect Font::selectionRectForComplexText(const TextRun& run,
    const FloatPoint& point, int height, int from, int to) const
{

    int fromX = -1, toX = -1, fromAdvance = -1, toAdvance = -1;
    TextRunWalker walker(run, 0, this);
    walker.setWordAndLetterSpacing(wordSpacing(), letterSpacing());

    // Base will point to the x offset for the current script run. Note that, in
    // the LTR case, width will be 0.
    int base = walker.rtl() ? walker.widthOfFullRun() : 0;
    const int leftEdge = base;

    // We want to enumerate the script runs in code point order in the following
    // code. This call also resets |walker|.
    walker.setBackwardsIteration(false);

    while (walker.nextScriptRun() && (fromX == -1 || toX == -1)) {
        // TextRunWalker will helpfully accumulate the x offsets for different
        // script runs for us. For this code, however, we always want the x
        // offsets to start from zero so we call this before each script run.
        walker.setXOffsetToZero();

        if (walker.rtl())
            base -= walker.width();

        int numCodePoints = static_cast<int>(walker.numCodePoints());
        if (fromX == -1 && from < numCodePoints) {
            // |from| is within this script run. So we index the clusters log to
            // find which glyph this code-point contributed to and find its x
            // position.
            int glyph = walker.logClusters()[from];
            fromX = base + walker.xPositions()[glyph];
            fromAdvance = walker.advances()[glyph];
        } else
            from -= numCodePoints;

        if (toX == -1 && to < numCodePoints) {
            int glyph = walker.logClusters()[to];
            toX = base + walker.xPositions()[glyph];
            toAdvance = walker.advances()[glyph];
        } else
            to -= numCodePoints;

        if (!walker.rtl())
            base += walker.width();
    }

    // The position in question might be just after the text.
    const int rightEdge = base;
    if (fromX == -1 && !from)
        fromX = leftEdge;
    else if (walker.rtl())
       fromX += truncateFixedPointToInteger(fromAdvance);

    if (toX == -1 && !to)
        toX = rightEdge;

    ASSERT(fromX != -1 && toX != -1);

    if (fromX < toX)
        return FloatRect(point.x() + fromX, point.y(), toX - fromX, height);

    return FloatRect(point.x() + toX, point.y(), fromX - toX, height);
}

void Font::drawComplexText(GraphicsContext* gc, TextRun const& run,
                           FloatPoint const& point, int, int) const
{
    if (!run.length())
        return;

    int mode = gc->textDrawingMode();
    bool fill = mode & TextModeFill;
    bool stroke = mode & TextModeStroke;
    if (!fill && !stroke)
        return;

    SkPaint fillPaint, strokePaint;
    if (fill)
        setupFill(&fillPaint, gc, primaryFont());
    if (stroke)
        setupStroke(&strokePaint, gc, primaryFont());

    SkCanvas* canvas = gc->platformContext()->mCanvas;
    bool haveMultipleLayers = isCanvasMultiLayered(canvas);
    TextRunWalker walker(run, point.x(), this);
    walker.setWordAndLetterSpacing(wordSpacing(), letterSpacing());
    walker.setPadding(run.expansion());

    while (walker.nextScriptRun()) {
        if (fill) {
            walker.fontPlatformDataForScriptRun()->setupPaint(&fillPaint);
            adjustTextRenderMode(&fillPaint, haveMultipleLayers);
            canvas->drawPosTextH(walker.glyphs(), walker.length() << 1,
                                 walker.xPositions(), point.y(), fillPaint);
        }
        if (stroke) {
            walker.fontPlatformDataForScriptRun()->setupPaint(&strokePaint);
            adjustTextRenderMode(&strokePaint, haveMultipleLayers);
            canvas->drawPosTextH(walker.glyphs(), walker.length() << 1,
                                 walker.xPositions(), point.y(), strokePaint);
        }
    }
}

float Font::floatWidthForComplexText(const TextRun& run,
            HashSet<const SimpleFontData*>*, GlyphOverflow*) const
{
    TextRunWalker walker(run, 0, this);
    walker.setWordAndLetterSpacing(wordSpacing(), letterSpacing());
    return walker.widthOfFullRun();
}

static int glyphIndexForXPositionInScriptRun(const TextRunWalker& walker, int x)
{
    const HB_Fixed* advances = walker.advances();
    int glyphIndex;
    if (walker.rtl()) {
        for (glyphIndex = walker.length() - 1; glyphIndex >= 0; --glyphIndex) {
            if (x < truncateFixedPointToInteger(advances[glyphIndex]))
                break;
            x -= truncateFixedPointToInteger(advances[glyphIndex]);
        }
    } else {
        for (glyphIndex = 0; glyphIndex < static_cast<int>(walker.length());
             ++glyphIndex) {
            if (x < truncateFixedPointToInteger(advances[glyphIndex]))
                break;
            x -= truncateFixedPointToInteger(advances[glyphIndex]);
        }
    }

    return glyphIndex;
}

int Font::offsetForPositionForComplexText(const TextRun& run, float x,
                                          bool includePartialGlyphs) const
{
    // (Mac code ignores includePartialGlyphs, and they don't know what it's
    // supposed to do, so we just ignore it as well.)
    TextRunWalker walker(run, 0, this);
    walker.setWordAndLetterSpacing(wordSpacing(), letterSpacing());

    // If this is RTL text, the first glyph from the left is actually the last
    // code point. So we need to know how many code points there are total in
    // order to subtract. This is different from the length of the TextRun
    // because UTF-16 surrogate pairs are a single code point, but 32-bits long.
    // In LTR we leave this as 0 so that we get the correct value for
    // |basePosition|, below.
    unsigned totalCodePoints = 0;
    if (walker.rtl()) {
        ssize_t offset = 0;
        while (offset < run.length()) {
            utf16_to_code_point(run.characters(), run.length(), &offset);
            totalCodePoints++;
        }
    }

    unsigned basePosition = totalCodePoints;

    // For RTL:
    //   code-point order:  abcd efg hijkl
    //   on screen:         lkjih gfe dcba
    //                                ^   ^
    //                                |   |
    //                  basePosition--|   |
    //                 totalCodePoints----|
    // Since basePosition is currently the total number of code-points, the
    // first thing we do is decrement it so that it's pointing to the start of
    // the current script-run.
    //
    // For LTR, basePosition is zero so it already points to the start of the
    // first script run.
    while (walker.nextScriptRun()) {
        if (walker.rtl())
            basePosition -= walker.numCodePoints();

        if (x >= 0 && x < static_cast<int>(walker.width())) {
            // The x value in question is within this script run. We consider
            // each glyph in presentation order and stop when we find the one
            // covering this position.
            const int glyphIndex = glyphIndexForXPositionInScriptRun(walker, x);

            // Now that we have a glyph index, we have to turn that into a
            // code-point index. Because of ligatures, several code-points may
            // have gone into a single glyph. We iterate over the clusters log
            // and find the first code-point which contributed to the glyph.

            // Some shapers (i.e. Khmer) will produce cluster logs which report
            // that /no/ code points contributed to certain glyphs. Because of
            // this, we take any code point which contributed to the glyph in
            // question, or any subsequent glyph. If we run off the end, then
            // we take the last code point.
            const unsigned short* log = walker.logClusters();
            for (unsigned j = 0; j < walker.numCodePoints(); ++j) {
                if (log[j] >= glyphIndex)
                    return basePosition + j;
            }

            return basePosition + walker.numCodePoints() - 1;
        }

        x -= walker.width();

        if (!walker.rtl())
            basePosition += walker.numCodePoints();
    }

    return basePosition;
}
#endif

}
