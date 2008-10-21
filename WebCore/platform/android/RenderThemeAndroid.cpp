/* libs/WebKitLib/WebKit/WebCore/rendering/RenderThemeAndroid.cpp
**
** Copyright 2006, The Android Open Source Project
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
#include "RenderThemeAndroid.h"
#include "PopupMenu.h"
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

namespace WebCore {
static SkCanvas* getCanvasFromInfo(const RenderObject::PaintInfo& info)
{
    return info.context->platformContext()->mCanvas;
}

/*  Helper function that paints the RenderObject
 *  paramters:
 *      the skin to use, 
 *      the object to be painted, the PaintInfo, from which we get the canvas, the bounding rectangle, 
 *  returns false, meaning no one else has to paint it
*/

static bool paintBrush(RenderSkinAndroid* rSkin, RenderObject* o, const RenderObject::PaintInfo& i,  const IntRect& ir)
{
    Node* element = o->element();
    SkCanvas* canvas = getCanvasFromInfo(i);
    canvas->save();
    canvas->translate(SkIntToScalar(ir.x()), SkIntToScalar(ir.y()));
    rSkin->setDim(ir.width(), ir.height());
    rSkin->notifyState(element);
    rSkin->draw(i.context->platformContext());
    canvas->restore();
    return false;
}


RenderTheme* theme()
{
    static RenderThemeAndroid androidTheme;
    return &androidTheme;
}

RenderThemeAndroid::RenderThemeAndroid()
{
    m_radio = new RenderSkinRadio(false);
    m_checkbox = new RenderSkinRadio(true);
    m_combo = new RenderSkinCombo();
}

RenderThemeAndroid::~RenderThemeAndroid()
{
    delete m_radio;
    delete m_checkbox;
    delete m_combo;
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

short RenderThemeAndroid::baselinePosition(const RenderObject* obj) const
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

bool RenderThemeAndroid::supportsFocus(EAppearance appearance)
{
    switch (appearance) {
        case PushButtonAppearance:
        case ButtonAppearance:
        case TextFieldAppearance:
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
    const int padding = 12;
    style->setPaddingLeft(Length(padding, Fixed));
    style->setPaddingRight(Length(padding, Fixed));
    style->setMinHeight(Length(style->fontSize() + BUTTON_PADDING, Fixed));
}

bool RenderThemeAndroid::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& ir)
{
    return paintBrush(m_checkbox, o, i, ir);
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
    return paintBrush(m_radio, o, i, ir);
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
    SkCanvas* canvas = getCanvasFromInfo(i);
    m_combo->notifyState(element);
    canvas->save();
    int height = ir.height();
    int y = ir.y();
    // If the combo box is too large, leave it at its max height, and center it.
    if (height > MAX_COMBO_HEIGHT) {
        y += (height - MAX_COMBO_HEIGHT) >> 1;
        height = MAX_COMBO_HEIGHT;
    }
    canvas->translate(SkIntToScalar(ir.x()), SkIntToScalar(y));
    m_combo->setDim(ir.width(), height);
    m_combo->draw(i.context->platformContext());
    canvas->restore();
    return false;
}

bool RenderThemeAndroid::paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& ir) 
{ 
    return paintCombo(o, i, ir);
}

void RenderThemeAndroid::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->setPaddingRight(Length(RenderSkinCombo::extraWidth(), Fixed));
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintMenuListButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& ir) 
{ 
    return paintCombo(o, i, ir);
}

bool RenderThemeAndroid::supportsFocusRing(const RenderStyle* style) const
{
    return (style->opacity() > 0 && style->hasAppearance() 
                    && style->appearance() != TextFieldAppearance 
                    && style->appearance() != SearchFieldAppearance 
                    && style->appearance() != TextAreaAppearance 
                    && style->appearance() != CheckboxAppearance
                    && style->appearance() != RadioAppearance
                    && style->appearance() != PushButtonAppearance
                    && style->appearance() != SquareButtonAppearance
                    && style->appearance() != ButtonAppearance
                    && style->appearance() != ButtonBevelAppearance
                    && style->appearance() != MenulistAppearance
                    && style->appearance() != MenulistButtonAppearance
                    );
}

}
