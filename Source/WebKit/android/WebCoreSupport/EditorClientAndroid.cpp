/*
 * Copyright 2007, The Android Open Source Project
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

#define LOG_TAG "WebCore"

#include "config.h"
#include "Editor.h"
#include "EditorClientAndroid.h"
#include "Event.h"
#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "NotImplemented.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformString.h"
#include "WebViewCore.h"
#include "WindowsKeyboardCodes.h"

using namespace WebCore::HTMLNames;

namespace android {
    
void EditorClientAndroid::pageDestroyed() {
    delete this;
}

bool EditorClientAndroid::shouldDeleteRange(Range*) { return true; }
bool EditorClientAndroid::shouldShowDeleteInterface(HTMLElement*) { notImplemented(); return false; }
bool EditorClientAndroid::smartInsertDeleteEnabled() { notImplemented(); return false; } 
bool EditorClientAndroid::isSelectTrailingWhitespaceEnabled(){ notImplemented(); return false; }
bool EditorClientAndroid::isContinuousSpellCheckingEnabled() { notImplemented(); return false; }
void EditorClientAndroid::toggleContinuousSpellChecking() { notImplemented(); }
bool EditorClientAndroid::isGrammarCheckingEnabled() { notImplemented(); return false; }
void EditorClientAndroid::toggleGrammarChecking() { notImplemented(); }
int EditorClientAndroid::spellCheckerDocumentTag() { notImplemented(); return -1; }

bool EditorClientAndroid::isEditable() { /* notImplemented(); */ return false; }

// Following Qt's implementation. For shouldBeginEditing and shouldEndEditing.
// Returning true for these fixes issue http://b/issue?id=735185
bool EditorClientAndroid::shouldBeginEditing(Range*) 
{ 
    return true;
}

bool EditorClientAndroid::shouldEndEditing(Range*) 
{  
    return true;
}

bool EditorClientAndroid::shouldInsertNode(Node*, Range*, EditorInsertAction) { notImplemented(); return true; }
bool EditorClientAndroid::shouldInsertText(const String&, Range*, EditorInsertAction) { return true; }
bool EditorClientAndroid::shouldApplyStyle(CSSStyleDeclaration*, Range*) { notImplemented(); return true; }

void EditorClientAndroid::didBeginEditing() { notImplemented(); }

// This function is called so that the platform can handle changes to content. It is called
// after the contents have been edited or unedited (ie undo)
void EditorClientAndroid::respondToChangedContents() { notImplemented(); }

void EditorClientAndroid::didEndEditing() { notImplemented(); }
void EditorClientAndroid::didWriteSelectionToPasteboard() { notImplemented(); }
void EditorClientAndroid::didSetSelectionTypesForPasteboard() { notImplemented(); }

// Copied from the Window's port of WebKit.
static const unsigned AltKey = 1 << 0;
static const unsigned ShiftKey = 1 << 1;

struct KeyDownEntry {
    unsigned virtualKey;
    unsigned modifiers;
    const char* name;
};

struct KeyPressEntry {
    unsigned charCode;
    unsigned modifiers;
    const char* name;
};

static const KeyDownEntry keyDownEntries[] = {
    { VK_LEFT,   0,                  "MoveLeft"                                    },
    { VK_LEFT,   ShiftKey,           "MoveLeftAndModifySelection"                  },
    { VK_LEFT,   AltKey,             "MoveWordLeft"                                },
    { VK_LEFT,   AltKey | ShiftKey,  "MoveWordLeftAndModifySelection"              },
    { VK_RIGHT,  0,                  "MoveRight"                                   },
    { VK_RIGHT,  ShiftKey,           "MoveRightAndModifySelection"                 },
    { VK_RIGHT,  AltKey,             "MoveWordRight"                               },
    { VK_RIGHT,  AltKey | ShiftKey,  "MoveWordRightAndModifySelection"             },
    { VK_UP,     0,                  "MoveUp"                                      },
    { VK_UP,     ShiftKey,           "MoveUpAndModifySelection"                    },
    { VK_DOWN,   0,                  "MoveDown"                                    },
    { VK_DOWN,   ShiftKey,           "MoveDownAndModifySelection"                  },

    { VK_BACK,   0,                  "BackwardDelete"                              },
    { VK_BACK,   ShiftKey,           "ForwardDelete"                               },
    { VK_BACK,   AltKey,             "DeleteWordBackward"                          },
    { VK_BACK,   AltKey | ShiftKey,  "DeleteWordForward"                           },

    { VK_ESCAPE, 0,                  "Cancel"                                      },
    { VK_TAB,    0,                  "InsertTab"                                   },
    { VK_TAB,    ShiftKey,           "InsertBacktab"                               },
    { VK_RETURN, 0,                  "InsertNewline"                               },
    { VK_RETURN, AltKey,             "InsertNewline"                               },
    { VK_RETURN, AltKey | ShiftKey,  "InsertNewline"                               }
};

static const KeyPressEntry keyPressEntries[] = {
    { '\t', 0,                  "InsertTab"     },
    { '\t', ShiftKey,           "InsertBackTab" },
    { '\r', 0,                  "InsertNewline" },
    { '\r', AltKey,             "InsertNewline" },
    { '\r', AltKey | ShiftKey,  "InsertNewline" }
};

static const char* interpretKeyEvent(const KeyboardEvent* evt)
{
    const PlatformKeyboardEvent* keyEvent = evt->keyEvent();

    static HashMap<int, const char*>* keyDownCommandsMap = 0;
    static HashMap<int, const char*>* keyPressCommandsMap = 0;

    if (!keyDownCommandsMap) {
        keyDownCommandsMap = new HashMap<int, const char*>;
        keyPressCommandsMap = new HashMap<int, const char*>;

        for (unsigned i = 0; i < sizeof(keyDownEntries)/sizeof(KeyDownEntry); i++)
            keyDownCommandsMap->set(keyDownEntries[i].modifiers << 16 | keyDownEntries[i].virtualKey, keyDownEntries[i].name);

        for (unsigned i = 0; i < sizeof(keyPressEntries)/sizeof(KeyPressEntry); i++)
            keyPressCommandsMap->set(keyPressEntries[i].modifiers << 16 | keyPressEntries[i].charCode, keyPressEntries[i].name);
    }

    unsigned modifiers = 0;
    if (keyEvent->shiftKey())
        modifiers |= ShiftKey;
    if (keyEvent->altKey())
        modifiers |= AltKey;

    if (evt->type() == eventNames().keydownEvent) {
        int mapKey = modifiers << 16 | evt->keyCode();
        return mapKey ? keyDownCommandsMap->get(mapKey) : 0;
    }

    int mapKey = modifiers << 16 | evt->charCode();
    return mapKey ? keyPressCommandsMap->get(mapKey) : 0;
}

void EditorClientAndroid::handleKeyboardEvent(KeyboardEvent* event) {
    ASSERT(m_page);
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    const PlatformKeyboardEvent* keyEvent = event->keyEvent();
    // TODO: If the event is not coming from Android Java, e.g. from JavaScript,
    // PlatformKeyboardEvent is null. We should support this later.
    if (!keyEvent)
        return;

    Editor::Command command = frame->editor()->command(interpretKeyEvent(event));
    if (keyEvent->type() == PlatformKeyboardEvent::RawKeyDown) {
        if (!command.isTextInsertion() && command.execute(event)) {
            // This function mimics the Windows version.  However, calling event->setDefaultHandled()
            // prevents the javascript key events for the delete key from happening.
            // Update: Safari doesn't send delete key events to javascript so
            // we will mimic that behavior.
            event->setDefaultHandled();
        }
        return;
    }
    
    if (command.execute(event)) {
        event->setDefaultHandled();
        return;
    }
    
    // Don't insert null or control characters as they can result in unexpected behaviour
    if (event->charCode() < ' ')
        return;
    
    if (frame->editor()->insertText(keyEvent->text(), event))
        event->setDefaultHandled();
}

////////////////////////////////////////////////////////////////////////////////////////////////
// we just don't support Undo/Redo at the moment

void EditorClientAndroid::registerCommandForUndo(PassRefPtr<EditCommand>) {}
void EditorClientAndroid::registerCommandForRedo(PassRefPtr<EditCommand>) {}
void EditorClientAndroid::clearUndoRedoOperations() {}
bool EditorClientAndroid::canUndo() const { return false; }
bool EditorClientAndroid::canRedo() const { return false; }
void EditorClientAndroid::undo() {}
void EditorClientAndroid::redo() {}
bool EditorClientAndroid::canCopyCut(bool defaultValue) const { return defaultValue; }
bool EditorClientAndroid::canPaste(bool defaultValue) const { return defaultValue; }

// functions new to Jun-07 tip of tree merge:
void EditorClientAndroid::showSpellingUI(bool) {}
void EditorClientAndroid::getGuessesForWord(String const&, const String&, WTF::Vector<String>&) {}
bool EditorClientAndroid::spellingUIIsShowing() { return false; }
void EditorClientAndroid::checkGrammarOfString(unsigned short const*, int, WTF::Vector<GrammarDetail>&, int*, int*) {}
void EditorClientAndroid::checkSpellingOfString(unsigned short const*, int, int*, int*) {}
String EditorClientAndroid::getAutoCorrectSuggestionForMisspelledWord(const String&) { return String(); }
void EditorClientAndroid::textFieldDidEndEditing(Element*) {}
void EditorClientAndroid::textDidChangeInTextArea(Element*) {}
void EditorClientAndroid::textDidChangeInTextField(Element*) {}
void EditorClientAndroid::textFieldDidBeginEditing(Element*) {}
void EditorClientAndroid::ignoreWordInSpellDocument(String const&) {}

// We need to pass the selection up to the WebTextView
void EditorClientAndroid::respondToChangedSelection() {
    if (m_uiGeneratedSelectionChange)
        return;
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame || !frame->view())
        return;
    WebViewCore* webViewCore = WebViewCore::getWebViewCore(frame->view());
    webViewCore->updateTextSelection();
}

bool EditorClientAndroid::shouldChangeSelectedRange(Range*, Range*, EAffinity,
        bool) {
    return m_shouldChangeSelectedRange;
}

bool EditorClientAndroid::doTextFieldCommandFromEvent(Element*, KeyboardEvent*) { return false; }
void EditorClientAndroid::textWillBeDeletedInTextField(Element*) {}
void EditorClientAndroid::updateSpellingUIWithGrammarString(String const&, GrammarDetail const&) {}
void EditorClientAndroid::updateSpellingUIWithMisspelledWord(String const&) {}
void EditorClientAndroid::learnWord(String const&) {}

// functions new to the Nov-16-08 tip of tree merge:
bool EditorClientAndroid::shouldMoveRangeAfterDelete(Range*, Range*) { return true; }
void EditorClientAndroid::setInputMethodState(bool) {}

// functions new to Feb-19 tip of tree merge:
void EditorClientAndroid::handleInputMethodKeydown(KeyboardEvent*) {}

void EditorClientAndroid::willSetInputMethodState()
{
    notImplemented();
}

void EditorClientAndroid::requestCheckingOfString(SpellChecker*, int, TextCheckingTypeMask, const String&) {}

#if ENABLE(WEB_AUTOFILL)
WebAutofill* EditorClientAndroid::getAutofill()
{
    if (!m_autoFill)
        m_autoFill.set(new WebAutofill());

    return m_autoFill.get();
}
#endif

}
