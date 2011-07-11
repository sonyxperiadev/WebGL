/*
 * Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Copyright 2010, The Android Open Source Project
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

#ifndef FormManagerAndroid_h
#define FormManagerAndroid_h

#include "ChromiumIncludes.h"

#include <map>
#include <vector>

// TODO: This file is taken from chromium/chrome/renderer/form_manager.h and
// customised to use WebCore types rather than WebKit API types. It would be
// nice and would ease future merge pain if the two could be combined.

namespace webkit_glue {
struct FormData;
struct FormField;
}  // namespace webkit_glue

namespace WebCore {
class Frame;
class HTMLFormControlElement;
class HTMLFormElement;
class Node;
}

using WebCore::Frame;
using WebCore::HTMLFormControlElement;
using WebCore::HTMLFormElement;
using WebCore::Node;

namespace android {

// Manages the forms in a Document.
class FormManager {
public:
    // A bit field mask for form requirements.
    enum RequirementsMask {
        REQUIRE_NONE         = 0,          // No requirements.
        REQUIRE_AUTOCOMPLETE = 1 << 0,     // Require that autocomplete != off.
        REQUIRE_ENABLED      = 1 << 1,     // Require that disabled attribute is off.
        REQUIRE_EMPTY        = 1 << 2,     // Require that the fields are empty.
    };

    // A bit field mask to extract data from HTMLFormControlElement.
    enum ExtractMask {
        EXTRACT_NONE        = 0,
        EXTRACT_VALUE       = 1 << 0, // Extract value from HTMLFormControlElement.
        EXTRACT_OPTION_TEXT = 1 << 1, // Extract option text from HTMLFormSelectElement. Only valid when |EXTRACT_VALUE| is set. This is used for form submission where humand readable value is captured.
        EXTRACT_OPTIONS     = 1 << 2, // Extract options from HTMLFormControlElement.
    };

    FormManager();
    virtual ~FormManager();

    // Fills out a FormField object from a given HTMLFormControlElement.
    // |extract_mask|: See the enum ExtractMask above for details.
    static void HTMLFormControlElementToFormField(HTMLFormControlElement* element, ExtractMask extract_mask, webkit_glue::FormField* field);

    // Returns the corresponding label for |element|.  WARNING: This method can
    // potentially be very slow.  Do not use during any code paths where the page
    // is loading.
    static string16 LabelForElement(const HTMLFormControlElement& element);

    // Fills out a FormData object from a given WebFormElement.  If |get_values|
    // is true, the fields in |form| will have the values filled out.  Returns
    // true if |form| is filled out; it's possible that |element| won't meet the
    // requirements in |requirements|.  This also returns false if there are no
    // fields in |form|.
    // TODO: Remove the user of this in RenderView and move this to
    // private.
    static bool HTMLFormElementToFormData(HTMLFormElement* element, RequirementsMask requirements, ExtractMask extract_mask, webkit_glue::FormData* form);

    // Scans the DOM in |frame| extracting and storing forms.
    void ExtractForms(Frame* frame);

    // Returns a vector of forms in |frame| that match |requirements|.
    void GetFormsInFrame(const Frame* frame, RequirementsMask requirements, std::vector<webkit_glue::FormData>* forms);

    // Finds the form that contains |element| and returns it in |form|. Returns
    // false if the form is not found.
    bool FindFormWithFormControlElement(HTMLFormControlElement* element, RequirementsMask requirements, webkit_glue::FormData* form);

    // Fills the form represented by |form|. |node| is the input element that
    // initiated the auto-fill process.
    bool FillForm(const webkit_glue::FormData& form, Node* node);

    // Previews the form represented by |form|.  |node| is the input element
    // that initiated the preview process.
    bool PreviewForm(const webkit_glue::FormData& form, Node* node);

    // Clears the values of all input elements in the form that contains |node|.
    // Returns false if the form is not found.
    bool ClearFormWithNode(Node* node);

    // Clears the placeholder values and the auto-filled background for any fields
    // in the form containing |node| that have been previewed. Resets the
    // autofilled state of |node| to |was_autofilled|. Returns false if the form
    // is not found.
    bool ClearPreviewedFormWithNode(Node* node, bool was_autofilled);

    // Resets the stored set of forms.
    void Reset();

    // Resets the forms for the specified |frame|.
    void ResetFrame(const Frame* frame);

    // Returns true if |form| has any auto-filled fields.
    bool FormWithNodeIsAutofilled(Node* node);

private:
    // Stores the HTMLFormElement and the form control elements for a form.
    // Original form values are stored so when we clear a form we can reset
    // <select> values to their original value.
    struct FormElement;

    // Type for cache of FormElement objects.
    typedef ScopedVector<FormElement> FormElementList;

    // The callback type used by ForEachMatchingFormField().
    typedef Callback3<HTMLFormControlElement*, const webkit_glue::FormField*, bool>::Type Callback;

    // Finds the cached FormElement that contains |node|.
    bool FindCachedFormElementWithNode(Node* node, FormElement** form_element);

    // Uses the data in |form| to find the cached FormElement.
    bool FindCachedFormElement(const webkit_glue::FormData& form, FormElement** form_element);

    // For each field in |data| that matches the corresponding field in |form|
    // and meets the |requirements|, |callback| is called with the actual
    // WebFormControlElement and the FormField data from |form|. The field that
    // matches |node| is not required to be empty if |requirements| includes
    // REQUIRE_EMPTY.  This method owns |callback|.
    void ForEachMatchingFormField(FormElement* form, Node* node, RequirementsMask requirements, const webkit_glue::FormData& data, Callback* callback);

    // A ForEachMatchingFormField() callback that sets |field|'s value using the
    // value in |data|.  This method also sets the autofill attribute, causing the
    // background to be yellow.
    void FillFormField(HTMLFormControlElement* field, const webkit_glue::FormField* data, bool is_initiating_node);

    // A ForEachMatchingFormField() callback that sets |field|'s placeholder value
    // using the value in |data|, causing the test to be greyed-out.  This method
    // also sets the autofill attribute, causing the background to be yellow.
    void PreviewFormField(HTMLFormControlElement* field, const webkit_glue::FormField* data, bool is_initiaiting_node);

    // The cached FormElement objects.
    FormElementList form_elements_;

    DISALLOW_COPY_AND_ASSIGN(FormManager);
};

} // namespace android

#endif  // FormManagerAndroid_h
