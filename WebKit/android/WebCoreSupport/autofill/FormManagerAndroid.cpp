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

#include "config.h"
#include "FormManagerAndroid.h"

#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "NodeList.h"
#include "HTMLCollection.h"
#include "FormFieldAndroid.h"
#include "QualifiedName.h"


// TODO: This file is taken from chromium/chrome/renderer/form_manager.cc and
// customised to use WebCore types rather than WebKit API types. It would be
// nice and would ease future merge pain if the two could be combined.

using webkit_glue::FormData;
using webkit_glue::FormField;
using WebCore::HTMLElement;
using WebCore::HTMLFormControlElement;
using WebCore::HTMLFormElement;
using WebCore::HTMLLabelElement;
using WebCore::HTMLSelectElement;
using WebCore::Node;
using WebCore::NodeList;

using namespace WebCore::HTMLNames;

namespace {

// The number of fields required by AutoFill.  Ideally we could send the forms
// to AutoFill no matter how many fields are in the forms; however, finding the
// label for each field is a costly operation and we can't spare the cycles if
// it's not necessary.
// Note the on ANDROID we reduce this from Chromium's 3 as it allows us to
// autofill simple name/email forms for example. This improves the mobile
// device experience where form filling can be time consuming and frustrating.
const size_t kRequiredAutoFillFields = 2;

string16 WTFStringToString16(const WTF::String& wtfString)
{
    WTF::String str = wtfString;

    if (str.charactersWithNullTermination())
        return string16(str.charactersWithNullTermination());
    else
        return string16();
}

string16 S(const WTF::AtomicString& str)
{
    return WTFStringToString16(str.string());
}

string16 S(const WTF::String& string)
{
    return WTFStringToString16(string);
}

// This is a helper function for the FindChildText() function.
// Returns the node value of the descendant or sibling of |node| that is a
// non-empty text node.  This is a faster alternative to |innerText()| for
// performance critical operations.  It does a full depth-first search so
// can be used when the structure is not directly known.  It does not aggregate
// the text of multiple nodes, it just returns the value of the first found.
// "Non-empty" in this case means non-empty after the whitespace has been
// stripped.
string16 FindChildTextInner(const Node* node) {
    string16 element_text;
    if (!node)
        return element_text;

    element_text = S(node->nodeValue());
    TrimWhitespace(element_text, TRIM_ALL, &element_text);
    if (!element_text.empty())
        return element_text;

    element_text = FindChildTextInner(node->firstChild());
    if (!element_text.empty())
        return element_text;

    element_text = FindChildTextInner(node->nextSibling());
    if (!element_text.empty())
        return element_text;

    return element_text;
}

// Returns the node value of the first decendant of |element| that is a
// non-empty text node.  "Non-empty" in this case means non-empty after the
// whitespace has been stripped.
string16 FindChildText(const HTMLElement* element) {
    Node* child = element->firstChild();
    return FindChildTextInner(child);
}

}  // namespace

namespace android {

FormManager::FormManager() {
}

FormManager::~FormManager() {
    Reset();
}

// static
void FormManager::HTMLFormControlElementToFormField(
    const HTMLFormControlElement& element, bool get_value, FormField* field) {
    ASSERT(field);

    // The label is not officially part of a HTMLFormControlElement; however, the
    // labels for all form control elements are scraped from the DOM and set in
    // WebFormElementToFormData.
    field->set_name(S(element.name()));
    field->set_form_control_type(S(element.type()));

    if (!get_value)
        return;

    string16 value;
    if (element.type() == WTF::AtomicString("text")) {
        const WebCore::HTMLTextAreaElement* input_element = static_cast<const WebCore::HTMLTextAreaElement*>(&element);
        if (input_element->renderer())
            value = S(input_element->value());
    } else if (element.type() == WTF::AtomicString("select-one")) {
        // TODO: This is ugly.  SelectElement::value() is a non-const
        // method. Look into fixing this on the WebKit side.
        HTMLFormControlElement& e = const_cast<HTMLFormControlElement&>(element);
        HTMLSelectElement& select_element = static_cast<HTMLSelectElement&>(e);
        value = S(select_element.value());
    }
    field->set_value(value);
}

// static
string16 FormManager::LabelForElement(const HTMLFormControlElement& element) {
    RefPtr<NodeList> labels = element.document()->getElementsByTagName("label");
    for (unsigned i = 0; i < labels->length(); ++i) {
        Node* e = labels->item(i);
        if (e->hasTagName(labelTag)) {
            HTMLLabelElement* label = static_cast<HTMLLabelElement*>(e);
            if (label->control() == &element)
                return FindChildText(label);
        }
    }

    // Infer the label from context if not found in label element.
    return FormManager::InferLabelForElement(element);
}

// static
bool FormManager::HTMLFormElementToFormData(HTMLFormElement& element,
                                           RequirementsMask requirements,
                                           bool get_values,
                                           FormData* form) {
    ASSERT(form);

    const WebCore::Document* document = element.document();
    if (!document)
        return false;

    if (requirements & REQUIRE_AUTOCOMPLETE && !element.autoComplete())
        return false;

    form->name = S(element.name());
    form->method = S(element.method());
    form->origin = GURL(S(document->documentURI()));
    form->action = GURL(S(document->completeURL(element.action())));
    // If the completed URL is not valid, just use the action we get from
    // WebKit.
    if (!form->action.is_valid())
        form->action = GURL(S(element.action()));

    // A map from a FormField's name to the FormField itself.
    std::map<string16, FormField*> name_map;

    // The extracted FormFields.  We use pointers so we can store them in
    // |name_map|.
    ScopedVector<FormField> form_fields;

    WTF::Vector<WebCore::HTMLFormControlElement*> control_elements = element.associatedElements();

    // A vector of bools that indicate whether each field in the form meets the
    // requirements and thus will be in the resulting |form|.
    std::vector<bool> fields_extracted(control_elements.size(), false);
    for (size_t i = 0; i < control_elements.size(); ++i) {
        const HTMLFormControlElement* control_element = control_elements[i];
        if (!(control_element->hasTagName(inputTag) || control_element->hasTagName(selectTag)))
            continue;

        if (requirements & REQUIRE_AUTOCOMPLETE &&
            control_element->type() == WTF::String("text")) {
            const WebCore::HTMLInputElement* input_element = static_cast<const WebCore::HTMLInputElement*>(control_element);
            if (!input_element->autoComplete())
                continue;
        }

        if (requirements & REQUIRE_ELEMENTS_ENABLED && !control_element->isEnabledFormControl())
            continue;

        // Create a new FormField, fill it out and map it to the field's name.
        FormField* field = new FormField;
        HTMLFormControlElementToFormField(*control_element, get_values, field);
        form_fields.push_back(field);
        // TODO: A label element is mapped to a form control element's id.
        // field->name() will contain the id only if the name does not exist.  Add
        // an id() method to HTMLFormControlElement and use that here.
        name_map[field->name()] = field;
        fields_extracted[i] = true;
    }

    // Don't extract field labels if we have no fields.
    if (form_fields.empty())
        return false;

    // Loop through the label elements inside the form element.  For each label
    // element, get the corresponding form control element, use the form control
    // element's name as a key into the <name, FormField> map to find the
    // previously created FormField and set the FormField's label to the
    // label.firstChild().nodeValue() of the label element.
    RefPtr<WebCore::NodeList> labels = element.getElementsByTagName("label");
    for (unsigned i = 0; i < labels->length(); ++i) {
        WebCore::HTMLLabelElement* label = static_cast<WebCore::HTMLLabelElement*>(labels->item(i));
        HTMLFormControlElement* field_element = label->control();
        if (!field_element || field_element->type() == "hidden")
            continue;

        std::map<string16, FormField*>::iterator iter =
            name_map.find(S(field_element->name()));
        if (iter != name_map.end())
            iter->second->set_label(FindChildText(label));
    }

    // Loop through the form control elements, extracting the label text from the
    // DOM.  We use the |fields_extracted| vector to make sure we assign the
    // extracted label to the correct field, as it's possible |form_fields| will
    // not contain all of the elements in |control_elements|.
    for (size_t i = 0, field_idx = 0;
         i < control_elements.size() && field_idx < form_fields.size(); ++i) {
        // This field didn't meet the requirements, so don't try to find a label for
        // it.
        if (!fields_extracted[i])
            continue;

        const HTMLFormControlElement* control_element = control_elements[i];
        if (form_fields[field_idx]->label().empty())
            form_fields[field_idx]->set_label(
                FormManager::InferLabelForElement(*control_element));

        ++field_idx;

    }
    // Copy the created FormFields into the resulting FormData object.
    for (ScopedVector<FormField>::const_iterator iter = form_fields.begin();
         iter != form_fields.end(); ++iter) {
        form->fields.push_back(**iter);
    }

    return true;
}

void FormManager::ExtractForms(WebCore::Document* document) {

    WTF::PassRefPtr<WebCore::HTMLCollection> collection = document->forms();

    WebCore::HTMLFormElement* form;
    WebCore::HTMLInputElement* input;
    for (Node* node = collection->firstItem();
         node && !node->namespaceURI().isNull() && !node->namespaceURI().isEmpty();
         node = collection->nextItem()) {
        FormElement* form_elements = new FormElement;
        form = static_cast<WebCore::HTMLFormElement*>(node);
        if (form->autoComplete()) {
            WTF::Vector<WebCore::HTMLFormControlElement*> elements = form->associatedElements();
            size_t size = elements.size();
            for (size_t i = 0; i < size; i++) {
                WebCore::HTMLFormControlElement* e = elements[i];
                if (e->hasTagName(inputTag) || e->hasTagName(selectTag))
                    form_elements->control_elements.push_back(e);
            }
            form_elements->form_element = form;
        }
        form_elements_map_[document].push_back(form_elements);
    }
}

void FormManager::GetForms(RequirementsMask requirements,
                           std::vector<FormData>* forms) {
    ASSERT(forms);

    for (DocumentFormElementMap::iterator iter = form_elements_map_.begin();
         iter != form_elements_map_.end(); ++iter) {
        for (std::vector<FormElement*>::iterator form_iter = iter->second.begin();
             form_iter != iter->second.end(); ++form_iter) {
            FormData form;
            if (HTMLFormElementToFormData(*(*form_iter)->form_element,
                                         requirements,
                                         true,
                                         &form))
                forms->push_back(form);
        }
    }
}

void FormManager::GetFormsInDocument(const WebCore::Document* document,
                                     RequirementsMask requirements,
                                     std::vector<FormData>* forms) {
    ASSERT(document);
    ASSERT(forms);

    DocumentFormElementMap::iterator iter = form_elements_map_.find(document);
    if (iter == form_elements_map_.end())
        return;

    // TODO: Factor this out and use it here and in GetForms.
    const std::vector<FormElement*>& form_elements = iter->second;
    for (std::vector<FormElement*>::const_iterator form_iter =
             form_elements.begin();
         form_iter != form_elements.end(); ++form_iter) {
        FormElement* form_element = *form_iter;

        // We need at least |kRequiredAutoFillFields| fields before appending this
        // form to |forms|.
        if (form_element->control_elements.size() < kRequiredAutoFillFields)
            continue;

        if (requirements & REQUIRE_AUTOCOMPLETE &&
            !form_element->form_element->autoComplete())
            continue;

        FormData form;
        FormElementToFormData(document, form_element, requirements, &form);
        if (form.fields.size() >= kRequiredAutoFillFields)
            forms->push_back(form);
    }
}

bool FormManager::FindForm(const HTMLFormElement& element,
                           RequirementsMask requirements,
                           FormData* form) {
    ASSERT(form);

    const WebCore::Document* document = element.document();
    DocumentFormElementMap::const_iterator document_iter =
        form_elements_map_.find(document);
    if (document_iter == form_elements_map_.end())
        return false;

    for (std::vector<FormElement*>::const_iterator iter =
             document_iter->second.begin();
         iter != document_iter->second.end(); ++iter) {
        if ((*iter)->form_element->name() != element.name())
            continue;
        return FormElementToFormData(document, *iter, requirements, form);
    }
    return false;
}

bool FormManager::FindFormWithFormControlElement(
    const HTMLFormControlElement& element,
    RequirementsMask requirements,
    FormData* form) {
    ASSERT(form);

    const WebCore::Document* document = element.document();
    if (form_elements_map_.find(document) == form_elements_map_.end())
        return false;

    const std::vector<FormElement*> forms = form_elements_map_[document];
    for (std::vector<FormElement*>::const_iterator iter = forms.begin();
         iter != forms.end(); ++iter) {
        const FormElement* form_element = *iter;

        for (std::vector<HTMLFormControlElement*>::const_iterator iter =
                 form_element->control_elements.begin();
             iter != form_element->control_elements.end(); ++iter) {
            if ((*iter)->name() == element.name()) {
                HTMLFormElementToFormData(
                    *form_element->form_element, requirements, true, form);
                return true;
            }
        }
    }

    return false;
}

bool FormManager::FillForm(const FormData& form) {
    FormElement* form_element = NULL;

    for (DocumentFormElementMap::iterator iter = form_elements_map_.begin();
         iter != form_elements_map_.end(); ++iter) {
        const WebCore::Document* document = iter->first;

        for (std::vector<FormElement*>::iterator form_iter = iter->second.begin();
             form_iter != iter->second.end(); ++form_iter) {
            // TODO: matching on form name here which is not guaranteed to
            // be unique for the page, nor is it guaranteed to be non-empty.  Need to
            // find a way to uniquely identify the form cross-process.  For now we'll
            // check form name and form action for identity.
            // http://crbug.com/37990 test file sample8.html.
            // Also note that WebString() == WebString(string16()) does not seem to
            // evaluate to |true| for some reason TBD, so forcing to string16.
            string16 element_name(S((*form_iter)->form_element->name()));
            GURL action(
                S(document->completeURL((*form_iter)->form_element->action()).string()));
            if (element_name == form.name && action == form.action) {
                form_element = *form_iter;
                break;
            }
        }
    }

    if (!form_element)
        return false;

    // It's possible that the site has injected fields into the form after the
    // page has loaded, so we can't assert that the size of the cached control
    // elements is equal to the size of the fields in |form|.  Fortunately, the
    // one case in the wild where this happens, paypal.com signup form, the fields
    // are appended to the end of the form and are not visible.

    for (size_t i = 0, j = 0;
         i < form_element->control_elements.size() && j < form.fields.size();
         ++i, ++j) {
        // Once again, empty WebString != empty string16, so we have to explicitly
        // check for this case.
        if (form_element->control_elements[i]->name().length() == 0 &&
            form.fields[j].name().empty())
            continue;

        // We assume that the intersection of the fields in
        // |form_element->control_elements| and |form.fields| is ordered, but it's
        // possible that one or the other sets may have more fields than the other,
        // so loop past non-matching fields in the set with more elements.
        while (S(form_element->control_elements[i]->name()) !=
               form.fields[j].name()) {
            if (form_element->control_elements.size() > form.fields.size()) {
                // We're at the end of the elements already.
                if (i + 1 == form_element->control_elements.size())
                    break;
                ++i;
            } else if (form.fields.size() > form_element->control_elements.size()) {
                // We're at the end of the elements already.
                if (j + 1 == form.fields.size())
                    break;
                ++j;
            } else {
                // Shouldn't get here.
                ASSERT(false);
            }

            continue;
        }

        HTMLFormControlElement* element = form_element->control_elements[i];
        if (!form.fields[j].value().empty() &&
            element->type() != WTF::String("submit")) {
            if (element->type() == WTF::String("text")) {
                WebCore::HTMLInputElement* input_element = static_cast<WebCore::HTMLInputElement*>(element);
                // If the maxlength attribute contains a negative value, maxLength()
                // returns the default maxlength value.
                input_element->setValue(
                    WTF::String(form.fields[j].value().substr(0, input_element->maxLength()).c_str()));
                input_element->setAutofilled(true);
            } else if (element->type() ==
                       WTF::String("select-one")) {
                WebCore::HTMLSelectElement* select_element = static_cast<WebCore::HTMLSelectElement*>(element);
                select_element->setValue(WTF::String(form.fields[j].value().c_str()));
            }
        }
    }

    return true;
}

void FormManager::FillForms(const std::vector<webkit_glue::FormData>& forms) {
    for (std::vector<webkit_glue::FormData>::const_iterator iter = forms.begin();
         iter != forms.end(); ++iter) {
        FillForm(*iter);
    }
}

void FormManager::Reset() {
    for (DocumentFormElementMap::iterator iter = form_elements_map_.begin();
         iter != form_elements_map_.end(); ++iter) {
        STLDeleteElements(&iter->second);
    }
    form_elements_map_.clear();
}

void FormManager::ResetFrame(const WebCore::Document* document) {

    DocumentFormElementMap::iterator iter = form_elements_map_.find(document);
    if (iter != form_elements_map_.end()) {
        STLDeleteElements(&iter->second);
        form_elements_map_.erase(iter);
    }
}

// static
bool FormManager::FormElementToFormData(const WebCore::Document* document,
                                        const FormElement* form_element,
                                        RequirementsMask requirements,
                                        FormData* form) {
    if (requirements & REQUIRE_AUTOCOMPLETE &&
        !form_element->form_element->autoComplete())
        return false;

    form->name = S(form_element->form_element->name());
    form->method = S(form_element->form_element->method());
    form->origin = GURL(S(document->documentURI()));
    form->action = GURL(S(document->completeURL(form_element->form_element->action())));

    // If the completed URL is not valid, just use the action we get from
    // WebKit.
    if (!form->action.is_valid())
        form->action = GURL(S(form_element->form_element->action()));

    // Form elements loop.
    for (std::vector<HTMLFormControlElement*>::const_iterator element_iter =
             form_element->control_elements.begin();
         element_iter != form_element->control_elements.end(); ++element_iter) {
        const HTMLFormControlElement* control_element = *element_iter;

        if (requirements & REQUIRE_AUTOCOMPLETE &&
            control_element->type() == WTF::String("text")) {
            const WebCore::HTMLInputElement* input_element =
                static_cast<const WebCore::HTMLInputElement*>(control_element);
            if (!input_element->autoComplete())
                continue;
        }

        if (requirements & REQUIRE_ELEMENTS_ENABLED && !control_element->isEnabledFormControl())
            continue;

        FormField field;
        HTMLFormControlElementToFormField(*control_element, false, &field);
        form->fields.push_back(field);
    }

    return true;
}

// static
string16 FormManager::InferLabelForElement(
    const HTMLFormControlElement& element) {
    string16 inferred_label;
    Node* previous = element.previousSibling();
    if (previous) {
        if (previous->isTextNode()) {
            inferred_label = S(previous->nodeValue());
            TrimWhitespace(inferred_label, TRIM_ALL, &inferred_label);
        }

        // If we didn't find text, check for previous paragraph.
        // Eg. <p>Some Text</p><input ...>
        // Note the lack of whitespace between <p> and <input> elements.
        if (inferred_label.empty()) {
            if (previous->isElementNode()) {
                if (previous->hasTagName(pTag)) {
                    inferred_label = FindChildText((HTMLElement*)previous);
                }
            }
        }

        // If we didn't find paragraph, check for previous paragraph to this.
        // Eg. <p>Some Text</p>   <input ...>
        // Note the whitespace between <p> and <input> elements.
        if (inferred_label.empty()) {
            previous = previous->previousSibling();
            if (previous) {
                if (previous->hasTagName(pTag)) {
                    inferred_label = FindChildText((HTMLElement*)previous);
                }
            }
        }
    }

    // If we didn't find paragraph, check for table cell case.
    // Eg. <tr><td>Some Text</td><td><input ...></td></tr>
    // Eg. <tr><td><b>Some Text</b></td><td><b><input ...></b></td></tr>
    if (inferred_label.empty()) {
        Node* parent = element.parentNode();
        while (parent &&
               !parent->hasTagName(tdTag))
            parent = parent->parentNode();

        if (parent) {
            if (parent->hasTagName(tdTag)) {
                previous = parent->previousSibling();

                // Skip by any intervening text nodes.
                while (previous && previous->isTextNode())
                    previous = previous->previousSibling();

                if (previous) {
                    if (previous->hasTagName(tdTag)) {
                        inferred_label = FindChildText((HTMLElement*)previous);
                    }
                }
            }
        }
    }
    return inferred_label;
}

}
