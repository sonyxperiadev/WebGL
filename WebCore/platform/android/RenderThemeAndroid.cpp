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
#include "RenderThemeAndroid.h"

#include "RenderSkinAndroid.h"
#include "RenderSkinButton.h"
#include "RenderSkinCombo.h"
#include "RenderSkinRadio.h"

#include "GraphicsContext.h"
#include "PlatformGraphicsContext.h"

#include "SkCanvas.h"

#define MAX_COMBO_HEIGHT 20

// Add a constant amount of padding to the textsize to get the final height of buttons,
// so that our button images are large enough to properly fit the text.
#define BUTTON_PADDING 18

// Add padding to the fontSize of ListBoxes to get their maximum sizes.
// Listboxes often have a specified size.  Since we change them into dropdowns,
// we want a much smaller height, which encompasses the text.
#define LISTBOX_PADDING 5

namespace WebCore {

static SkCanvas* getCanvasFromInfo(const RenderObject::PaintInfo& info)
{
    return info.context->platformContext()->mCanvas;
}

RenderTheme* theme()
{
    static RenderThemeAndroid androidTheme;
    return &androidTheme;
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

bool RenderThemeAndroid::stateChanged(RenderObject* o, ControlState state) const
{
    if (CheckedState == state) {
        o->repaint();
        return true;
    }
    return false;
}

Color RenderThemeAndroid::platformActiveSelectionBackgroundColor() const
{
    return Color(46, 251, 0);
}

Color RenderThemeAndroid::platformInactiveSelectionBackgroundColor() const
{
    return Color(255, 255, 0, 255);
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
    return Color(192, 192, 192);
}

int RenderThemeAndroid::baselinePosition(const RenderObject* obj) const
{
    // From the description of this function in RenderTheme.h:
    // A method to obtain the baseline position for a "leaf" control.  This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.
    //
    // Our checkboxes and radio buttons need to be offset to line up properly.
    return RenderTheme::baselinePosition(obj) - 5;
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

void RenderThemeAndroid::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    // Padding code is taken from RenderThemeSafari.cpp
    // It makes sure we have enough space for the button text.
    const int padding = 8;
    style->setPaddingLeft(Length(padding, Fixed));
    style->setPaddingRight(Length(padding, Fixed));
    style->setMinHeight(Length(style->fontSize() + BUTTON_PADDING, Fixed));
}

bool RenderThemeAndroid::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& ir)
{
    RenderSkinRadio::Draw(getCanvasFromInfo(i), o->element(), ir, true);
    return false;
}

bool RenderThemeAndroid::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& ir)
{
    // If it is a disabled button, simply paint it to the master picture.
    Node* element = o->element();
    if (!element->isEnabled()) {
        RenderSkinButton::Draw(getCanvasFromInfo(i), ir, RenderSkinAndroid::kDisabled);
    } else {
        // Store all the important information in the platform context.
        i.context->platformContext()->storeButtonInfo(element, ir);
    }
    // We always return false so we do not request to be redrawn.
    return false;
}

bool RenderThemeAndroid::paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& ir)
{
    RenderSkinRadio::Draw(getCanvasFromInfo(i), o->element(), ir, false);
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

void RenderThemeAndroid::adjustTextFieldStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& ir)
{
    return true;    
}

void RenderThemeAndroid::adjustTextAreaStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& ir)
{
    if (o->isMenuList()) {
        return paintCombo(o, i, ir);
    }
    return true;    
}

void RenderThemeAndroid::adjustSearchFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintSearchField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& ir)
{
    return true;    
}

void RenderThemeAndroid::adjustListboxStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->setPaddingRight(Length(RenderSkinCombo::extraWidth(), Fixed));
    style->setMaxHeight(Length(style->fontSize() + LISTBOX_PADDING, Fixed));
    addIntrinsicMargins(style);
}

void RenderThemeAndroid::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->setPaddingRight(Length(RenderSkinCombo::extraWidth(), Fixed));
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintCombo(RenderObject* o, const RenderObject::PaintInfo& i,  const IntRect& ir)
{
    if (o->style() && o->style()->backgroundColor().alpha() == 0)
        return true;
    Node* element = o->element();
    int height = ir.height();
    int y = ir.y();
    // If the combo box is too large, leave it at its max height, and center it.
    if (height > MAX_COMBO_HEIGHT) {
        y += (height - MAX_COMBO_HEIGHT) >> 1;
        height = MAX_COMBO_HEIGHT;
    }
    return RenderSkinCombo::Draw(getCanvasFromInfo(i), element, ir.x(), y,
            ir.width(), height);
}

bool RenderThemeAndroid::paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& ir) 
{ 
    return paintCombo(o, i, ir);
}

void RenderThemeAndroid::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
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
    // Added to make room for our arrow.
    style->setPaddingRight(Length(RenderSkinCombo::extraWidth(), Fixed));
}

bool RenderThemeAndroid::paintMenuListButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& ir) 
{
    return paintCombo(o, i, ir);
}

bool RenderThemeAndroid::supportsFocusRing(const RenderStyle* style) const
{
    return (style->opacity() > 0 && style->hasAppearance() 
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
                    && style->appearance() != MenulistButtonPart
                    );
}

}
