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

#include "KWQLineEdit.h"
#include "IntPoint.h"

#define LOG_TAG "WebCore"
#undef LOG
#include <utils/Log.h>

using namespace WebCore;

class KWQTextFieldController {
public:
    int     m_maxLength;
    int     m_cursorPosition;
    bool    m_edited;
    String  m_placeholderString;
    String  m_text;
};

QLineEdit::QLineEdit(Type type) : m_type(type)
{
    printf("============ QLineEdit %d\n", type);
    
    m_controller = new KWQTextFieldController;

    m_controller->m_maxLength = 32; // ???
    m_controller->m_cursorPosition = 0;
    m_controller->m_edited = false;
}

QLineEdit::~QLineEdit()
{
    delete m_controller;
}

int QLineEdit::maxLength() const
{
    return m_controller->m_maxLength;
}

void QLineEdit::setMaxLength(int ml)
{
    m_controller->m_maxLength = ml;
}

String QLineEdit::text() const
{
    return m_controller->m_text;
}

void QLineEdit::setText(String const& text)
{
    m_controller->m_text = text;
}

int QLineEdit::cursorPosition() const
{
    return m_controller->m_cursorPosition;
}

void QLineEdit::setCursorPosition(int cp)
{
    m_controller->m_cursorPosition = cp;
}

void QLineEdit::setPlaceholderString(String const& ph)
{
    m_controller->m_placeholderString = ph;
}

bool QLineEdit::edited() const
{
    return m_controller->m_edited;
}

void QLineEdit::setEdited(bool edited)
{
    m_controller->m_edited = edited;
}

void QLineEdit::setFont(Font const&) { }
void QLineEdit::setAlignment(HorizontalAlignment) { }
void QLineEdit::setWritingDirection(TextDirection) { }
void QLineEdit::setReadOnly(bool) { }
void QLineEdit::setColors(Color const&, Color const&) { }
IntSize QLineEdit::sizeForCharacterWidth(int) const { return IntSize(); }
int QLineEdit::baselinePosition(int) const { return 0; }
void QLineEdit::setLiveSearch(bool) { }

#define notImplemented() { LOGV("%s: Not Implemented", __FUNCTION__); }

void QLineEdit::selectAll() { notImplemented(); }
void QLineEdit::addSearchResult() { notImplemented(); }
int QLineEdit::selectionStart() const { notImplemented(); return 0; }
bool QLineEdit::hasSelectedText() const { notImplemented(); return 0; }
String QLineEdit::selectedText() const { notImplemented(); return String(); }
void QLineEdit::setAutoSaveName(String const&) { notImplemented(); }
bool QLineEdit::checksDescendantsForFocus() const { notImplemented(); return false; }
void QLineEdit::setSelection(int,int) { notImplemented(); }
void QLineEdit::setMaxResults(int) { notImplemented(); }

Widget::FocusPolicy QLineEdit::focusPolicy() const { notImplemented(); return NoFocus; }
