/* 
**
** Copyright 2007, The Android Open Source Project
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
#include "KWQTextEdit.h"
#include "Font.h"
#include "IntSize.h"
#include "WebCoreViewBridge.h"
#include "GraphicsContext.h"
#include "PlatformGraphicsContext.h"

#include "SkCanvas.h"
#include "SkString.h"
#include "SkUtils.h"
#include "SkPaint.h"

#undef LOG
#define LOG_TAG "WebCore"
#undef LOGI
#undef LOG
#include "utils/Log.h"

static void setstr(SkString* dst, const WebCore::String& src)
{
    if (src.length()) {
        const uint16_t* uni = (const uint16_t*)src.characters();
        size_t          size = SkUTF16_ToUTF8(uni, src.length(), 0);

        dst->resize(size);
        SkUTF16_ToUTF8(uni, src.length(), dst->writable_str());
    }
}

class TextEditViewBridge : public WebCoreViewBridge {
public:
    TextEditViewBridge(QTextEdit* te);

    virtual void draw(WebCore::GraphicsContext* ctx, const IntRect& rect);
    virtual bool isEnabled() const;
    virtual void setEnabled(bool);    
    virtual bool hasFocus() const;
    virtual void setFocus(bool);

private:
    QTextEdit*      m_te;
    bool            m_enabled;
    bool            m_focused;
};

TextEditViewBridge::TextEditViewBridge(QTextEdit* te)
{
    m_te = te;
    this->setBounds(0,0,0,0);
    m_enabled = true;
    m_focused = false;
}

void TextEditViewBridge::draw(WebCore::GraphicsContext* ctx, const IntRect& rect)
{
    SkCanvas* canvas = ctx->platformContext()->mCanvas;
    SkPaint paint;
    SkRect  r;
    
    paint.setColor(m_focused ? SK_ColorGREEN : SK_ColorBLUE);
    r.set(  SkIntToScalar(rect.x()), SkIntToScalar(rect.y()),
            SkIntToScalar(rect.right()), SkIntToScalar(rect.bottom()));
    canvas->drawRect(r, paint);
}

bool TextEditViewBridge::isEnabled() const { return m_enabled; }
void TextEditViewBridge::setEnabled(bool e) { m_enabled = e; }

bool TextEditViewBridge::hasFocus() const { return m_focused; }
void TextEditViewBridge::setFocus(bool f) { m_focused = f; }

/////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace WebCore;

QTextEdit::QTextEdit(Widget *parent)
{
    this->setWebCoreViewBridge(new TextEditViewBridge(this));

#if 0
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    WebCoreTextArea *textView = [[WebCoreTextArea alloc] initWithQTextEdit:this];
    setView(textView);
    [textView release];
    END_BLOCK_OBJC_EXCEPTIONS;
#endif
}

QTextEdit::~QTextEdit()
{
    delete this->getWebCoreViewBridge();

#if 0
    WebCoreTextArea *textArea = (WebCoreTextArea *)getView();
    [textArea detachQTextEdit]; 
#endif
}

void QTextEdit::setText(const String& string)
{
    SkString    str;
    
    setstr(&str, string);
    LOGI("%p QTextEdit::setText(%s)\n", this, str.c_str());
}

String QTextEdit::text() const
{
    return String("ATextEdit::text");
}

String QTextEdit::textWithHardLineBreaks() const
{
    return String("ATextEdit::textWithHardLineBreaks");
}

void QTextEdit::getCursorPosition(int *paragraph, int *index) const
{
//    WebCoreTextArea *textView = (WebCoreTextArea *)getView();
    if (index)
        *index = 0;
    if (paragraph)
        *paragraph = 0;
}

void QTextEdit::setCursorPosition(int paragraph, int index)
{
LOGI("QTextEdit::setCursorPosition(%d, %d)\n", paragraph, index);
}

QTextEdit::WrapStyle QTextEdit::wordWrap() const
{
//    return [textView wordWrap] ? WidgetWidth : NoWrap;
    return NoWrap;
}

void QTextEdit::setWordWrap(WrapStyle style)
{
}

void QTextEdit::setScrollBarModes(ScrollBarMode hMode, ScrollBarMode vMode)
{
#if 0
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // this declaration must be inside the BEGIN_BLOCK_OBJC_EXCEPTIONS block or the deployment build fails
    bool autohides = hMode == ScrollBarAuto || vMode == ScrollBarAuto;
    
    ASSERT(!autohides || hMode != ScrollBarAlwaysOn);
    ASSERT(!autohides || vMode != ScrollBarAlwaysOn);

    [textView setHasHorizontalScroller:hMode != ScrollBarAlwaysOff];
    [textView setHasVerticalScroller:vMode != ScrollBarAlwaysOff];
    [textView setAutohidesScrollers:autohides];

    END_BLOCK_OBJC_EXCEPTIONS;
#endif
}

bool QTextEdit::isReadOnly() const
{
    return false;
}

void QTextEdit::setReadOnly(bool flag)
{
}

bool QTextEdit::isDisabled() const
{
    return false;
}

void QTextEdit::setDisabled(bool flag)
{
}

int QTextEdit::selectionStart()
{
    return 0;
}

int QTextEdit::selectionEnd()
{
    return 0;
}

void QTextEdit::setSelectionStart(int start)
{
#if 0
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSRange range = [textView selectedRange];
    if (range.location == NSNotFound) {
        range.location = 0;
        range.length = 0;
    }
    
    // coerce start to a valid value
    int maxLength = [[textView text] length];
    int newStart = start;
    if (newStart < 0)
        newStart = 0;
    if (newStart > maxLength)
        newStart = maxLength;
    
    if ((unsigned)newStart < range.location + range.length) {
        // If we're expanding or contracting, but not collapsing the selection
        range.length += range.location - newStart;
        range.location = newStart;
    } else {
        // ok, we're collapsing the selection
        range.location = newStart;
        range.length = 0;
    }
    
    [textView setSelectedRange:range];
    END_BLOCK_OBJC_EXCEPTIONS;
#endif
}

void QTextEdit::setSelectionEnd(int end)
{
#if 0
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSRange range = [textView selectedRange];
    if (range.location == NSNotFound) {
        range.location = 0;
        range.length = 0;
    }
    
    // coerce end to a valid value
    int maxLength = [[textView text] length];
    int newEnd = end;
    if (newEnd < 0)
        newEnd = 0;
    if (newEnd > maxLength)
        newEnd = maxLength;
    
    if ((unsigned)newEnd >= range.location) {
        // If we're just changing the selection length, but not location..
        range.length = newEnd - range.location;
    } else {
        // ok, we've collapsed the selection and are moving it
        range.location = newEnd;
        range.length = 0;
    }
    
    [textView setSelectedRange:range];
    END_BLOCK_OBJC_EXCEPTIONS;
#endif
}

bool QTextEdit::hasSelectedText() const
{
    return false;
}

void QTextEdit::selectAll()
{
}

void QTextEdit::setSelectionRange(int start, int length)
{
#if 0
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    int newStart = start;
    int newLength = length;
    if (newStart < 0) {
        // truncate the length by the negative start
        newLength = length + newStart;
        newStart = 0;
    }
    if (newLength < 0) {
        newLength = 0;
    }
    int maxlen = [[textView text] length];
    if (newStart > maxlen) {
        newStart = maxlen;
    }
    if (newStart + newLength > maxlen) {
        newLength = maxlen - newStart;
    }
    NSRange tempRange = {newStart, newLength}; // 4213314
    [textView setSelectedRange:tempRange];
    END_BLOCK_OBJC_EXCEPTIONS;
#endif
}

void QTextEdit::setFont(const Font& font)
{
    Widget::setFont(font);
#if 0
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [textView setFont:font.getNSFont()];
    END_BLOCK_OBJC_EXCEPTIONS;
#endif
}

void QTextEdit::setAlignment(HorizontalAlignment alignment)
{
}

void QTextEdit::setLineHeight(int lineHeight)
{
}

void QTextEdit::setWritingDirection(TextDirection direction)
{
#if 0
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebCoreTextArea *textArea = static_cast<WebCoreTextArea *>(getView());
    [textArea setBaseWritingDirection:(direction == RTL ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight)];

    END_BLOCK_OBJC_EXCEPTIONS;
#endif
}
 
IntSize QTextEdit::sizeWithColumnsAndRows(int numColumns, int numRows) const
{
#if 0
    WebCoreTextArea *textArea = static_cast<WebCoreTextArea *>(getView());
    NSSize size = {0,0};

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    size = [textArea sizeWithColumns:numColumns rows:numRows];
    END_BLOCK_OBJC_EXCEPTIONS;

    return IntSize((int)ceil(size.width), (int)ceil(size.height));
#else
    return IntSize(0, 0);
#endif
}

Widget::FocusPolicy QTextEdit::focusPolicy() const
{
    FocusPolicy policy = ScrollView::focusPolicy();
    return policy == TabFocus ? StrongFocus : policy;
}

bool QTextEdit::checksDescendantsForFocus() const
{
    return true;
}

void QTextEdit::setColors(const Color& background, const Color& foreground)
{
}

