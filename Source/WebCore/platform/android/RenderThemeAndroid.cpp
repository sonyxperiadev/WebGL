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

#include "config.h"
#include "RenderThemeAndroid.h"

#include "Color.h"
#include "Element.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "Node.h"
#include "PlatformGraphicsContext.h"
#if ENABLE(VIDEO)
#include "RenderMediaControls.h"
#endif
#include "RenderSkinAndroid.h"
#include "RenderSkinButton.h"
#include "RenderSkinCombo.h"
#include "RenderSkinMediaButton.h"
#include "RenderSkinRadio.h"
#include "SkCanvas.h"
#include "UserAgentStyleSheets.h"
#include "WebCoreFrameBridge.h"

namespace WebCore {

// Add padding to the fontSize of ListBoxes to get their maximum sizes.
// Listboxes often have a specified size.  Since we change them into
// dropdowns, we want a much smaller height, which encompasses the text.
const int listboxPadding = 5;

// This is the color of selection in a textfield.  It was computed from
// frameworks/base/core/res/res/values/colors.xml, which uses #9983CC39
// (decimal a = 153, r = 131, g = 204, b = 57)
// for all four highlighted text values. Blending this with white yields:
// R = (131 * 153 + 255 * (255 - 153)) / 255  -> 180.6
// G = (204 * 153 + 255 * (255 - 153)) / 255  -> 224.4
// B = ( 57 * 153 + 255 * (255 - 153)) / 255  -> 136.2

const RGBA32 selectionColor = makeRGB(181, 224, 136);

static SkCanvas* getCanvasFromInfo(const PaintInfo& info)
{
    return info.context->platformContext()->mCanvas;
}

static android::WebFrame* getWebFrame(const Node* node)
{
    if (!node)
        return 0;
    return android::WebFrame::getWebFrame(node->document()->frame());
}

RenderTheme* theme()
{
    DEFINE_STATIC_LOCAL(RenderThemeAndroid, androidTheme, ());
    return &androidTheme;
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* rt = RenderThemeAndroid::create().releaseRef();
    return rt;
}

PassRefPtr<RenderTheme> RenderThemeAndroid::create()
{
    return adoptRef(new RenderThemeAndroid());
}

RenderThemeAndroid::RenderThemeAndroid()
{
}

RenderThemeAndroid::~RenderThemeAndroid()
{
}

void RenderThemeAndroid::close()
{
}

bool RenderThemeAndroid::stateChanged(RenderObject* obj, ControlState state) const
{
    if (CheckedState == state) {
        obj->repaint();
        return true;
    }
    return false;
}

Color RenderThemeAndroid::platformActiveSelectionBackgroundColor() const
{
    return Color(selectionColor);
}

Color RenderThemeAndroid::platformInactiveSelectionBackgroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformActiveSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeAndroid::platformInactiveSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeAndroid::platformTextSearchHighlightColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformActiveListBoxSelectionBackgroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformInactiveListBoxSelectionBackgroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformActiveListBoxSelectionForegroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformInactiveListBoxSelectionForegroundColor() const
{
    return Color(Color::transparent);
}

int RenderThemeAndroid::baselinePosition(const RenderObject* obj) const
{
    // From the description of this function in RenderTheme.h:
    // A method to obtain the baseline position for a "leaf" control.  This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.
    //
    // Our checkboxes and radio buttons need to be offset to line up properly.
    return RenderTheme::baselinePosition(obj) - 2;
}

void RenderThemeAndroid::addIntrinsicMargins(RenderStyle* style) const
{
    // Cut out the intrinsic margins completely if we end up using a small font size
    if (style->fontSize() < 11)
        return;
    
    // Intrinsic margin value.
    const int m = 2;
    
    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    if (style->width().isIntrinsicOrAuto()) {
        if (style->marginLeft().quirk())
            style->setMarginLeft(Length(m, Fixed));
        if (style->marginRight().quirk())
            style->setMarginRight(Length(m, Fixed));
    }

    if (style->height().isAuto()) {
        if (style->marginTop().quirk())
            style->setMarginTop(Length(m, Fixed));
        if (style->marginBottom().quirk())
            style->setMarginBottom(Length(m, Fixed));
    }
}

bool RenderThemeAndroid::supportsFocus(ControlPart appearance)
{
    switch (appearance) {
    case PushButtonPart:
    case ButtonPart:
    case TextFieldPart:
        return true;
    default:
        return false;
    }

    return false;
}

void RenderThemeAndroid::adjustButtonStyle(CSSStyleSelector*, RenderStyle* style, WebCore::Element*) const
{
    // Code is taken from RenderThemeSafari.cpp
    // It makes sure we have enough space for the button text.
    const int paddingHoriz = 12;
    const int paddingVert = 8;
    style->setPaddingLeft(Length(paddingHoriz, Fixed));
    style->setPaddingRight(Length(paddingHoriz, Fixed));
    style->setPaddingTop(Length(paddingVert, Fixed));
    style->setPaddingBottom(Length(paddingVert, Fixed));

    // Set a min-height so that we can't get smaller than the mini button.
    style->setMinHeight(Length(15, Fixed));
}

bool RenderThemeAndroid::paintCheckbox(RenderObject* obj, const PaintInfo& info, const IntRect& rect)
{
    RenderSkinRadio::Draw(getCanvasFromInfo(info), obj->node(), rect, true);
    return false;
}

bool RenderThemeAndroid::paintButton(RenderObject* obj, const PaintInfo& info, const IntRect& rect)
{
    // If it is a disabled button, simply paint it to the master picture.
    Node* node = obj->node();
    Element* formControlElement = static_cast<Element*>(node);
    if (formControlElement) {
        android::WebFrame* webFrame = getWebFrame(node);
        if (webFrame) {
            RenderSkinAndroid* skins = webFrame->renderSkins();
            if (skins) {
                RenderSkinAndroid::State state = RenderSkinAndroid::kNormal;
                if (!formControlElement->isEnabledFormControl())
                    state = RenderSkinAndroid::kDisabled;
                skins->renderSkinButton()->draw(getCanvasFromInfo(info), rect, state);
            }
        }
    }

    // We always return false so we do not request to be redrawn.
    return false;
}

#if ENABLE(VIDEO)

String RenderThemeAndroid::extraMediaControlsStyleSheet()
{
      return String(mediaControlsAndroidUserAgentStyleSheet, sizeof(mediaControlsAndroidUserAgentStyleSheet));
}

bool RenderThemeAndroid::shouldRenderMediaControlPart(ControlPart part, Element* e)
{
      HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(e);
      switch (part) {
      case MediaMuteButtonPart:
          return false;
      case MediaSeekBackButtonPart:
      case MediaSeekForwardButtonPart:
          return false;
      case MediaRewindButtonPart:
          return mediaElement->movieLoadType() != MediaPlayer::LiveStream;
      case MediaReturnToRealtimeButtonPart:
          return mediaElement->movieLoadType() == MediaPlayer::LiveStream;
      case MediaFullscreenButtonPart:
          return mediaElement->supportsFullscreen();
      case MediaToggleClosedCaptionsButtonPart:
          return mediaElement->hasClosedCaptions();
      default:
          return true;
      }
}

bool RenderThemeAndroid::paintMediaFullscreenButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      RenderSkinMediaButton::Draw(getCanvasFromInfo(paintInfo), rect, RenderSkinMediaButton::FULLSCREEN, translucent);
      return false;
}

bool RenderThemeAndroid::paintMediaMuteButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      RenderSkinMediaButton::Draw(getCanvasFromInfo(paintInfo), rect, RenderSkinMediaButton::MUTE, translucent);
      return false;
}

bool RenderThemeAndroid::paintMediaPlayButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      if (MediaControlPlayButtonElement* btn = static_cast<MediaControlPlayButtonElement*>(o->node())) {
          if (btn->displayType() == MediaPlayButton)
              RenderSkinMediaButton::Draw(getCanvasFromInfo(paintInfo), rect, RenderSkinMediaButton::PLAY, translucent);
          else
              RenderSkinMediaButton::Draw(getCanvasFromInfo(paintInfo), rect, RenderSkinMediaButton::PAUSE, translucent);
          return false;
      }
      return true;
}

bool RenderThemeAndroid::paintMediaSeekBackButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      RenderSkinMediaButton::Draw(getCanvasFromInfo(paintInfo), rect, RenderSkinMediaButton::REWIND, translucent);
      return false;
}

bool RenderThemeAndroid::paintMediaSeekForwardButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      RenderSkinMediaButton::Draw(getCanvasFromInfo(paintInfo), rect, RenderSkinMediaButton::FORWARD, translucent);
      return false;
}

bool RenderThemeAndroid::paintMediaControlsBackground(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      RenderSkinMediaButton::Draw(getCanvasFromInfo(paintInfo), rect, RenderSkinMediaButton::BACKGROUND_SLIDER, translucent);
      return false;
}

bool RenderThemeAndroid::paintMediaSliderTrack(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      RenderSkinMediaButton::Draw(getCanvasFromInfo(paintInfo), rect,
                                  RenderSkinMediaButton::SLIDER_TRACK, translucent, o);
      return false;
}

bool RenderThemeAndroid::paintMediaSliderThumb(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      RenderSkinMediaButton::Draw(getCanvasFromInfo(paintInfo), rect, RenderSkinMediaButton::SLIDER_THUMB, translucent);
      return false;
}

void RenderThemeAndroid::adjustSliderThumbSize(RenderObject* o) const
{
    static const int sliderThumbWidth = RenderSkinMediaButton::sliderThumbWidth();
    static const int sliderThumbHeight = RenderSkinMediaButton::sliderThumbHeight();
    if (o->style()->appearance() == MediaSliderThumbPart) {
        o->style()->setWidth(Length(sliderThumbWidth, Fixed));
        o->style()->setHeight(Length(sliderThumbHeight, Fixed));
    }
}

#endif

bool RenderThemeAndroid::paintRadio(RenderObject* obj, const PaintInfo& info, const IntRect& rect)
{
    RenderSkinRadio::Draw(getCanvasFromInfo(info), obj->node(), rect, false);
    return false;
}

void RenderThemeAndroid::setCheckboxSize(RenderStyle* style) const
{
    style->setWidth(Length(19, Fixed));
    style->setHeight(Length(19, Fixed));
}

void RenderThemeAndroid::setRadioSize(RenderStyle* style) const
{
    // This is the same as checkboxes.
    setCheckboxSize(style);
}

void RenderThemeAndroid::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle* style, WebCore::Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintTextField(RenderObject*, const PaintInfo&, const IntRect&)
{
    return true;    
}

void RenderThemeAndroid::adjustTextAreaStyle(CSSStyleSelector*, RenderStyle* style, WebCore::Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintTextArea(RenderObject* obj, const PaintInfo& info, const IntRect& rect)
{
    if (obj->isMenuList())
        paintCombo(obj, info, rect);
    return true;
}

void RenderThemeAndroid::adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintSearchField(RenderObject*, const PaintInfo&, const IntRect&)
{
    return true;    
}

static void adjustMenuListStyleCommon(RenderStyle* style)
{
    // Added to make room for our arrow and make the touch target less cramped.
    style->setPaddingLeft(Length(RenderSkinCombo::padding(), Fixed));
    style->setPaddingTop(Length(RenderSkinCombo::padding(), Fixed));
    style->setPaddingBottom(Length(RenderSkinCombo::padding(), Fixed));
    style->setPaddingRight(Length(RenderSkinCombo::extraWidth(), Fixed));
    style->setMinHeight(Length(RenderSkinCombo::minHeight(), Fixed));
}

void RenderThemeAndroid::adjustListboxStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    adjustMenuListButtonStyle(0, style, 0);
}

void RenderThemeAndroid::adjustMenuListStyle(CSSStyleSelector*, RenderStyle* style, Element* e) const
{
    adjustMenuListStyleCommon(style);
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintCombo(RenderObject* obj, const PaintInfo& info,  const IntRect& rect)
{
  if (obj->style() && !obj->style()->visitedDependentColor(CSSPropertyBackgroundColor).alpha())
        return true;
    return RenderSkinCombo::Draw(getCanvasFromInfo(info), obj->node(), rect.x(), rect.y(), rect.width(), rect.height());
}

bool RenderThemeAndroid::paintMenuList(RenderObject* obj, const PaintInfo& info, const IntRect& rect) 
{ 
    return paintCombo(obj, info, rect);
}

void RenderThemeAndroid::adjustMenuListButtonStyle(CSSStyleSelector*,
        RenderStyle* style, Element*) const
{
    // Copied from RenderThemeSafari.
    const float baseFontSize = 11.0f;
    const int baseBorderRadius = 5;
    float fontScale = style->fontSize() / baseFontSize;
    
    style->resetPadding();
    style->setBorderRadius(IntSize(int(baseBorderRadius + fontScale - 1), int(baseBorderRadius + fontScale - 1))); // FIXME: Round up?

    const int minHeight = 15;
    style->setMinHeight(Length(minHeight, Fixed));
    
    style->setLineHeight(RenderStyle::initialLineHeight());
    // Found these padding numbers by trial and error.
    const int padding = 4;
    style->setPaddingTop(Length(padding, Fixed));
    style->setPaddingLeft(Length(padding, Fixed));
    adjustMenuListStyleCommon(style);
}

bool RenderThemeAndroid::paintMenuListButton(RenderObject* obj, const PaintInfo& info, const IntRect& rect) 
{
    return paintCombo(obj, info, rect);
}

bool RenderThemeAndroid::supportsFocusRing(const RenderStyle* style) const
{
    return style->opacity() > 0
        && style->hasAppearance() 
        && style->appearance() != TextFieldPart 
        && style->appearance() != SearchFieldPart 
        && style->appearance() != TextAreaPart 
        && style->appearance() != CheckboxPart
        && style->appearance() != RadioPart
        && style->appearance() != PushButtonPart
        && style->appearance() != SquareButtonPart
        && style->appearance() != ButtonPart
        && style->appearance() != ButtonBevelPart
        && style->appearance() != MenulistPart
        && style->appearance() != MenulistButtonPart;
}

} // namespace WebCore
