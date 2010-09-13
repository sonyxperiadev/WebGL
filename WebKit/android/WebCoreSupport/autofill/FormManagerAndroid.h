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
class FormField;
}  // namespace webkit_glue

namespace WebCore {
    class HTMLFormControlElement;
    class HTMLFormElement;
    class Document;
}

namespace android {

// Manages the forms in a Document.
class FormManager {
 public:
  // A bit field mask for form requirements.
  enum RequirementsMask {
    REQUIRE_NONE = 0x0,             // No requirements.
    REQUIRE_AUTOCOMPLETE = 0x1,     // Require that autocomplete != off.
    REQUIRE_ELEMENTS_ENABLED = 0x2  // Require that disabled attribute is off.
  };

  FormManager();
  virtual ~FormManager();

  // Fills out a FormField object from a given WebFormControlElement.
  // If |get_value| is true, |field| will have the value set from |element|.
  static void HTMLFormControlElementToFormField(
      const WebCore::HTMLFormControlElement& element,
      bool get_value,
      webkit_glue::FormField* field);

  // Returns the corresponding label for |element|.  WARNING: This method can
  // potentially be very slow.  Do not use during any code paths where the page
  // is loading.
  static string16 LabelForElement(const WebCore::HTMLFormControlElement& element);

  // Fills out a FormData object from a given WebFormElement.  If |get_values|
  // is true, the fields in |form| will have the values filled out.  Returns
  // true if |form| is filled out; it's possible that |element| won't meet the
  // requirements in |requirements|.  This also returns false if there are no
  // fields in |form|.
  // TODO: Remove the user of this in RenderView and move this to
  // private.
  static bool HTMLFormElementToFormData(WebCore::HTMLFormElement& element,
                                       RequirementsMask requirements,
                                       bool get_values,
                                       webkit_glue::FormData* form);

  // Scans the DOM in |document| extracting and storing forms.
  void ExtractForms(WebCore::Document* document);

  // Returns a vector of forms that match |requirements|.
  void GetForms(RequirementsMask requirements,
                std::vector<webkit_glue::FormData>* forms);

  // Returns a vector of forms in |document| that match |requirements|.
  void GetFormsInDocument(const WebCore::Document* document,
                       RequirementsMask requirements,
                       std::vector<webkit_glue::FormData>* forms);

  // Returns the cached FormData for |element|.  Returns true if the form was
  // found in the cache.
  bool FindForm(const WebCore::HTMLFormElement& element,
                RequirementsMask requirements,
                webkit_glue::FormData* form);

  // Finds the form that contains |element| and returns it in |form|. Returns
  // false if the form is not found.
  bool FindFormWithFormControlElement(
      const WebCore::HTMLFormControlElement& element,
      RequirementsMask requirements,
      webkit_glue::FormData* form);

  // Fills the form represented by |form|.  |form| should have the name set to
  // the name of the form to fill out, and the number of elements and values
  // must match the number of stored elements in the form.
  // TODO: Is matching on name alone good enough?  It's possible to
  // store multiple forms with the same names from different frames.
  bool FillForm(const webkit_glue::FormData& form);

  // Fills all of the forms in the cache with form data from |forms|.
  void FillForms(const std::vector<webkit_glue::FormData>& forms);

  // Resets the stored set of forms.
  void Reset();

  // Resets the forms for the specified |document|.
  void ResetFrame(const WebCore::Document* document);

 private:
  // Stores the HTMLFormElement and the form control elements for a form.
  struct FormElement {
      WebCore::HTMLFormElement* form_element;
      std::vector<WebCore::HTMLFormControlElement*> control_elements;
  };

  // A map of vectors of FormElements keyed by the Document containing each
  // form.
  typedef std::map<const WebCore::Document*, std::vector<FormElement*> >
      DocumentFormElementMap;

  // Converts a FormElement to FormData storage.  Returns false if the form does
  // not meet all the requirements in the requirements mask.
  static bool FormElementToFormData(const WebCore::Document* document,
                                    const FormElement* form_element,
                                    RequirementsMask requirements,
                                    webkit_glue::FormData* form);

  // Infers corresponding label for |element| from surrounding context in the
  // DOM.  Contents of preceeding <p> tag or preceeding text element found in
  // the form.
  static string16 InferLabelForElement(
      const WebCore::HTMLFormControlElement& element);

  // The map of form elements.
  DocumentFormElementMap form_elements_map_;

  DISALLOW_COPY_AND_ASSIGN(FormManager);
};

} // namespace android

#endif  // FormManagerAndroid_h
