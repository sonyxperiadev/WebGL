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

#ifndef RenderThemeAndroid_h
#define RenderThemeAndroid_h

#include "RenderTheme.h"

namespace WebCore {

class RenderSkinButton;
class RenderSkinRadio;
class RenderSkinCombo;

struct ThemeData {
    ThemeData() :m_part(0), m_state(0) {}

    unsigned m_part;
    unsigned m_state;
};

class RenderThemeAndroid : public RenderTheme {
public:
    RenderThemeAndroid();
    ~RenderThemeAndroid();
    
    virtual bool stateChanged(RenderObject*, ControlState) const;
       
    virtual bool supportsFocusRing(const RenderStyle* style) const;
    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle* style) const { return style->affectedByHoverRules(); }

    virtual int baselinePosition(const RenderObject*) const;

    virtual Color platformActiveSelectionBackgroundColor() const;
    virtual Color platformInactiveSelectionBackgroundColor() const;
    virtual Color platformActiveSelectionForegroundColor() const;
    virtual Color platformInactiveSelectionForegroundColor() const;
    virtual Color platformTextSearchHighlightColor() const;
    
    virtual void systemFont(int, WebCore::FontDescription&) const {}

    virtual int minimumMenuListSize(RenderStyle*) const { return 0; }

protected:
    virtual bool paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);
    virtual void setCheckboxSize(RenderStyle* style) const;

    virtual bool paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);
    virtual void setRadioSize(RenderStyle* style) const;

    virtual void adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const;
    virtual bool paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);

    virtual void adjustTextFieldStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const;
    virtual bool paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);

    virtual void adjustTextAreaStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const;
    virtual bool paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);
    
    bool paintCombo(RenderObject* o, const RenderObject::PaintInfo& i,  const IntRect& ir);

    virtual void adjustListboxStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const;
    virtual void adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const;
    virtual bool paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);

    virtual void adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const;
    virtual bool paintMenuListButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);
    
    virtual void adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

private:
    void addIntrinsicMargins(RenderStyle* style) const;
    void close();

    bool supportsFocus(ControlPart appearance);
};

} // namespace WebCore

#endif // RenderThemeAndroid_h

