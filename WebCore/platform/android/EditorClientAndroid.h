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

#ifndef EditorClientAndroid_h
#define EditorClientAndroid_h

#include "EditorClient.h"
#include "Page.h"

namespace WebCore {

class EditorClientAndroid : public EditorClient {
public:
    EditorClientAndroid() { m_notFromClick = true; }
    virtual void pageDestroyed();
    
    virtual bool shouldDeleteRange(Range*);
    virtual bool shouldShowDeleteInterface(HTMLElement*);
    virtual bool smartInsertDeleteEnabled(); 
    virtual bool isContinuousSpellCheckingEnabled();
    virtual void toggleContinuousSpellChecking();
    virtual bool isGrammarCheckingEnabled();
    virtual void toggleGrammarChecking();
    virtual int spellCheckerDocumentTag();
    
    virtual bool isEditable();
    
    virtual bool shouldBeginEditing(Range*);
    virtual bool shouldEndEditing(Range*);
    virtual bool shouldInsertNode(Node*, Range*, EditorInsertAction);
    virtual bool shouldInsertText(String, Range*, EditorInsertAction);
    virtual bool shouldChangeSelectedRange(Range* fromRange, Range* toRange, EAffinity, bool stillSelecting);

    virtual bool shouldApplyStyle(CSSStyleDeclaration*, Range*);
//  virtual bool shouldChangeTypingStyle(CSSStyleDeclaration* fromStyle, CSSStyleDeclaration* toStyle);
//  virtual bool doCommandBySelector(SEL selector);

    virtual void didBeginEditing();
    virtual void respondToChangedContents();
    virtual void respondToChangedSelection();
    virtual void didEndEditing();
    virtual void didWriteSelectionToPasteboard();
    virtual void didSetSelectionTypesForPasteboard();
//  virtual void didChangeTypingStyle:(NSNotification *)notification;
//  virtual void didChangeSelection:(NSNotification *)notification;
//  virtual NSUndoManager* undoManager:(WebView *)webView;

    virtual void registerCommandForUndo(PassRefPtr<EditCommand>);
    virtual void registerCommandForRedo(PassRefPtr<EditCommand>);
    virtual void clearUndoRedoOperations();
    
    virtual bool canUndo() const;
    virtual bool canRedo() const;
    
    virtual void undo();
    virtual void redo();

    virtual void textFieldDidBeginEditing(Element*);
    virtual void textFieldDidEndEditing(Element*);
    virtual void textDidChangeInTextField(Element*);
    virtual bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*);
    virtual void textWillBeDeletedInTextField(Element*);
    virtual void textDidChangeInTextArea(Element*);

    virtual void ignoreWordInSpellDocument(const String&);
    virtual void learnWord(const String&);
    virtual void checkSpellingOfString(const UChar*, int length, int* misspellingLocation, int* misspellingLength);
    virtual void checkGrammarOfString(const UChar*, int length, Vector<GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength);
    virtual void updateSpellingUIWithGrammarString(const String&, const GrammarDetail& detail);
    virtual void updateSpellingUIWithMisspelledWord(const String&);
    virtual void showSpellingUI(bool show);
    virtual bool spellingUIIsShowing();
    virtual void getGuessesForWord(const String&, Vector<String>& guesses);
    virtual bool shouldMoveRangeAfterDelete(Range*, Range*);
    virtual void setInputMethodState(bool);

    virtual void handleKeyboardEvent(KeyboardEvent*);
    virtual void handleInputMethodKeydown(KeyboardEvent*);
    // Android specific:
    void setPage(Page* page) { m_page = page; }
    void setFromClick(bool fromClick) { m_notFromClick = !fromClick; }
private:
    Page* m_page;
    bool  m_notFromClick;
};

}

#endif
