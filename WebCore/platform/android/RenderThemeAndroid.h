/* libs/WebKitLib/WebKit/WebCore/rendering/RenderThemeAndroid.h
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

#ifndef RenderThemeAndroid_h
#define RenderThemeAndroid_h

#include "RenderTheme.h"

namespace WebCore {

class PopupMenu;
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

    virtual short baselinePosition(const RenderObject*) const;

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

    virtual void adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const;
    virtual bool paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);

    virtual void adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const;
    virtual bool paintMenuListButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);
    
    virtual void adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);


private:
    void addIntrinsicMargins(RenderStyle* style) const;
    void close();

    bool supportsFocus(EAppearance appearance);
    // FIXME: There should be a way to use one RenderSkinRadio for both radio and checkbox
    RenderSkinRadio*    m_radio;
    RenderSkinRadio*    m_checkbox;
    RenderSkinCombo*    m_combo;
};

};

#endif

