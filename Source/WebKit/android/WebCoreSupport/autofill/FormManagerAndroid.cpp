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

#include "DocumentLoader.h"
#include "Element.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLCollection.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "InputElement.h"
#include "Node.h"
#include "NodeList.h"
#include "HTMLCollection.h"
#include "FormAssociatedElement.h"
#include "QualifiedName.h"
#include "StringUtils.h"

// TODO: This file is taken from chromium/chrome/renderer/form_manager.cc and
// customised to use WebCore types rather than WebKit API types. It would be
// nice and would ease future merge pain if the two could be combined.

using webkit_glue::FormData;
using webkit_glue::FormField;
using WebCore::Element;
using WebCore::FormAssociatedElement;
using WebCore::HTMLCollection;
using WebCore::HTMLElement;
using WebCore::HTMLFormControlElement;
using WebCore::HTMLFormElement;
using WebCore::HTMLInputElement;
using WebCore::HTMLLabelElement;
using WebCore::HTMLOptionElement;
using WebCore::HTMLSelectElement;
using WebCore::InputElement;
using WebCore::Node;
using WebCore::NodeList;

using namespace WebCore::HTMLNames;

namespace {

// Android helper function.
HTMLInputElement* HTMLFormControlElementToHTMLInputElement(const HTMLFormControlElement& element) {
    Node* node = const_cast<Node*>(static_cast<const Node*>(&element));
    InputElement* input_element = node->toInputElement();
    if (node && node->isHTMLElement())
        return static_cast<HTMLInputElement*>(input_element);

    return 0;
}

// The number of fields required by Autofill.  Ideally we could send the forms
// to Autofill no matter how many fields are in the forms; however, finding the
// label for each field is a costly operation and we can't spare the cycles if
// it's not necessary.
// Note the on ANDROID we reduce this from Chromium's 3 as it allows us to
// Autofill simple name/email forms for example. This improves the mobile
// device experience where form filling can be time consuming and frustrating.
const size_t kRequiredAutofillFields = 2;

// The maximum number of form fields we are willing to parse, due to
// computational costs. This is a very conservative upper bound.
const size_t kMaxParseableFields = 1000;

// The maximum length allowed for form data.
const size_t kMaxDataLength = 1024;

// In HTML5, all text fields except password are text input fields to
// autocomplete.
bool IsTextInput(const HTMLInputElement* element) {
    if (!element)
        return false;

    return element->isTextField() && !element->isPasswordField();
}

bool IsSelectElement(const HTMLFormControlElement& element) {
    return formControlType(element) == kSelectOne;
}

bool IsOptionElement(Element& element) {
    return element.hasTagName(optionTag);
}

bool IsAutofillableElement(const HTMLFormControlElement& element) {
    HTMLInputElement* html_input_element = HTMLFormControlElementToHTMLInputElement(element);
    return (html_input_element && IsTextInput(html_input_element)) || IsSelectElement(element);
}

// This is a helper function for the FindChildText() function (see below).
// Search depth is limited with the |depth| parameter.
string16 FindChildTextInner(Node* node, int depth) {
    string16 element_text;
    if (!node || depth <= 0)
        return element_text;

    string16 node_text = WTFStringToString16(node->nodeValue());
    TrimWhitespace(node_text, TRIM_ALL, &node_text);
    if (!node_text.empty())
        element_text = node_text;

    string16 child_text = FindChildTextInner(node->firstChild(), depth-1);
    if (!child_text.empty())
        element_text = element_text + child_text;

    string16 sibling_text = FindChildTextInner(node->nextSibling(), depth-1);
    if (!sibling_text.empty())
        element_text = element_text + sibling_text;

    return element_text;
}

// Returns the aggregated values of the decendants or siblings of |element| that
// are non-empty text nodes. This is a faster alternative to |innerText()| for
// performance critical operations. It does a full depth-first search so can be
// used when the structure is not directly known. Whitesapce is trimmed from
// text accumulated at descendant and sibling. Search is limited to within 10
// siblings and/or descendants.
string16 FindChildText(Element* element) {
    Node* child = element->firstChild();

    const int kChildSearchDepth = 10;
    return FindChildTextInner(child, kChildSearchDepth);
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a previous node of |element|.
string16 InferLabelFromPrevious(const HTMLFormControlElement& element) {
    string16 inferred_label;
    Node* previous = element.previousSibling();
    if (!previous)
        return string16();

    if (previous->isTextNode()) {
        inferred_label = WTFStringToString16(previous->nodeValue());
        TrimWhitespace(inferred_label, TRIM_ALL, &inferred_label);
    }

    // If we didn't find text, check for previous paragraph.
    // Eg. <p>Some Text</p><input ...>
    // Note the lack of whitespace between <p> and <input> elements.
    if (inferred_label.empty() && previous->isElementNode()) {
        Element* element = static_cast<Element*>(previous);
        if (element->hasTagName(pTag))
            inferred_label = FindChildText(element);
    }

    // If we didn't find paragraph, check for previous paragraph to this.
    // Eg. <p>Some Text</p>   <input ...>
    // Note the whitespace between <p> and <input> elements.
    if (inferred_label.empty()) {
        Node* sibling = previous->previousSibling();
        if (sibling && sibling->isElementNode()) {
            Element* element = static_cast<Element*>(sibling);
            if (element->hasTagName(pTag))
                inferred_label = FindChildText(element);
        }
    }

    // Look for text node prior to <img> tag.
    // Eg. Some Text<img/><input ...>
    if (inferred_label.empty()) {
        while (inferred_label.empty() && previous) {
            if (previous->isTextNode()) {
                inferred_label = WTFStringToString16(previous->nodeValue());
                TrimWhitespace(inferred_label, TRIM_ALL, &inferred_label);
            } else if (previous->isElementNode()) {
                Element* element = static_cast<Element*>(previous);
                if (!element->hasTagName(imgTag))
                    break;
            } else
                break;
            previous = previous->previousSibling();
        }
    }

    // Look for label node prior to <input> tag.
    // Eg. <label>Some Text</label><input ...>
    if (inferred_label.empty()) {
        while (inferred_label.empty() && previous) {
            if (previous->isTextNode()) {
                inferred_label = WTFStringToString16(previous->nodeValue());
                TrimWhitespace(inferred_label, TRIM_ALL, &inferred_label);
            } else if (previous->isElementNode()) {
                Element* element = static_cast<Element*>(previous);
                if (element->hasTagName(labelTag)) {
                    inferred_label = FindChildText(element);
                } else {
                    break;
                }
            } else {
                break;
            }

            previous = previous->previousSibling();
         }
    }

    return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// surrounding table structure.
// Eg. <tr><td>Some Text</td><td><input ...></td></tr>
// Eg. <tr><td><b>Some Text</b></td><td><b><input ...></b></td></tr>
string16 InferLabelFromTable(const HTMLFormControlElement& element) {
    string16 inferred_label;
    Node* parent = element.parentNode();
    while (parent && parent->isElementNode() && !static_cast<Element*>(parent)->hasTagName(tdTag))
        parent = parent->parentNode();

    // Check all previous siblings, skipping non-element nodes, until we find a
    // non-empty text block.
    Node* previous = parent;
    while(previous) {
        if (previous->isElementNode()) {
            Element* e = static_cast<Element*>(previous);
            if (e->hasTagName(tdTag)) {
                inferred_label = FindChildText(e);
                if (!inferred_label.empty())
                    break;
            }
        }
        previous = previous->previousSibling();
    }
    return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a surrounding div table.
// Eg. <div>Some Text<span><input ...></span></div>
string16 InferLabelFromDivTable(const HTMLFormControlElement& element) {
    Node* parent = element.parentNode();
    while (parent && parent->isElementNode() && !static_cast<Element*>(parent)->hasTagName(divTag))
        parent = parent->parentNode();

    if (!parent || !parent->isElementNode())
        return string16();

    Element* e = static_cast<Element*>(parent);
    if (!e || !e->hasTagName(divTag))
        return string16();

    return FindChildText(e);
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a surrounding definition list.
// Eg. <dl><dt>Some Text</dt><dd><input ...></dd></dl>
// Eg. <dl><dt><b>Some Text</b></dt><dd><b><input ...></b></dd></dl>
string16 InferLabelFromDefinitionList(const HTMLFormControlElement& element) {
    string16 inferred_label;
    Node* parent = element.parentNode();
    while (parent && parent->isElementNode() && !static_cast<Element*>(parent)->hasTagName(ddTag))
        parent = parent->parentNode();

    if (parent && parent->isElementNode()) {
        Element* element = static_cast<Element*>(parent);
        if (element->hasTagName(ddTag)) {
            Node* previous = parent->previousSibling();

            // Skip by any intervening text nodes.
            while (previous && previous->isTextNode())
                previous = previous->previousSibling();

            if (previous && previous->isElementNode()) {
                element = static_cast<Element*>(previous);
                if (element->hasTagName(dtTag))
                    inferred_label = FindChildText(element);
            }
        }
    }
    return inferred_label;
}

// Infers corresponding label for |element| from surrounding context in the DOM.
// Contents of preceding <p> tag or preceding text element found in the form.
string16 InferLabelForElement(const HTMLFormControlElement& element) {
    string16 inferred_label = InferLabelFromPrevious(element);

    // If we didn't find a label, check for table cell case.
    if (inferred_label.empty())
        inferred_label = InferLabelFromTable(element);

    // If we didn't find a label, check for div table case.
    if (inferred_label.empty())
        inferred_label = InferLabelFromDivTable(element);

    // If we didn't find a label, check for definition list case.
    if (inferred_label.empty())
        inferred_label = InferLabelFromDefinitionList(element);

    return inferred_label;
}

void GetOptionStringsFromElement(const HTMLSelectElement& select_element, std::vector<string16>* option_strings) {
    DCHECK(option_strings);
    option_strings->clear();
    WTF::Vector<Element*> list_items = select_element.listItems();
    option_strings->reserve(list_items.size());
    for (size_t i = 0; i < list_items.size(); ++i) {
        if (IsOptionElement(*list_items[i])) {
                option_strings->push_back(WTFStringToString16(static_cast<HTMLOptionElement*>(list_items[i])->value()));
        }
    }
}

// Returns the form's |name| attribute if non-empty; otherwise the form's |id|
// attribute.
const string16 GetFormIdentifier(const HTMLFormElement& form) {
    string16 identifier = WTFStringToString16(form.name());
    if (identifier.empty())
        identifier = WTFStringToString16(form.getIdAttribute());
    return identifier;
}

}  // namespace

namespace android {

struct FormManager::FormElement {
    RefPtr<HTMLFormElement> form_element;
    std::vector<RefPtr<HTMLFormControlElement> > control_elements;
    std::vector<string16> control_values;
};

FormManager::FormManager() {
}

FormManager::~FormManager() {
    Reset();
}

// static
void FormManager::HTMLFormControlElementToFormField(HTMLFormControlElement* element, ExtractMask extract_mask, FormField* field) {
    DCHECK(field);
    DCHECK(element);

    // The label is not officially part of a HTMLFormControlElement; however, the
    // labels for all form control elements are scraped from the DOM and set in
    // WebFormElementToFormData.
    field->name = nameForAutofill(*element);
    field->form_control_type = formControlType(*element);

    if (!IsAutofillableElement(*element))
        return;

    HTMLInputElement* input_element = HTMLFormControlElementToHTMLInputElement(*element);

    if (IsTextInput(input_element)) {
        field->max_length = input_element->maxLength();
        field->is_autofilled = input_element->isAutofilled();
    } else if (extract_mask & EXTRACT_OPTIONS) {
        // Set option strings on the field is available.
        DCHECK(IsSelectElement(*element));
        HTMLSelectElement* select_element = static_cast<HTMLSelectElement*>(element);
        std::vector<string16> option_strings;
        GetOptionStringsFromElement(*select_element, &option_strings);
        field->option_strings = option_strings;
    }

    if (!(extract_mask & EXTRACT_VALUE))
        return;

    string16 value;
    if (IsTextInput(input_element)) {
        value = WTFStringToString16(input_element->value());
    } else {
        DCHECK(IsSelectElement(*element));
        HTMLSelectElement* select_element = static_cast<HTMLSelectElement*>(element);
        value = WTFStringToString16(select_element->value());

        // Convert the |select_element| value to text if requested.
        if (extract_mask & EXTRACT_OPTION_TEXT) {
            Vector<Element*> list_items = select_element->listItems();
            for (size_t i = 0; i < list_items.size(); ++i) {
                if (IsOptionElement(*list_items[i]) &&
                    WTFStringToString16(static_cast<HTMLOptionElement*>(list_items[i])->value()) == value) {
                    value = WTFStringToString16(static_cast<HTMLOptionElement*>(list_items[i])->text());
                    break;
                }
            }
        }
    }

    // TODO: This is a temporary stop-gap measure designed to prevent
    // a malicious site from DOS'ing the browser with extremely large profile
    // data.  The correct solution is to parse this data asynchronously.
    // See http://crbug.com/49332.
    if (value.size() > kMaxDataLength)
        value = value.substr(0, kMaxDataLength);

    field->value = value;
}

// static
string16 FormManager::LabelForElement(const HTMLFormControlElement& element) {
    // Don't scrape labels for elements we can't possible autofill anyway.
    if (!IsAutofillableElement(element))
        return string16();

    RefPtr<NodeList> labels = element.document()->getElementsByTagName("label");
    for (unsigned i = 0; i < labels->length(); ++i) {
        Node* e = labels->item(i);
        DCHECK(e->hasTagName(labelTag));
        HTMLLabelElement* label = static_cast<HTMLLabelElement*>(e);
        if (label->control() == &element)
            return FindChildText(label);
    }

    // Infer the label from context if not found in label element.
    return InferLabelForElement(element);
}

// static
bool FormManager::HTMLFormElementToFormData(HTMLFormElement* element, RequirementsMask requirements, ExtractMask extract_mask, FormData* form) {
    DCHECK(form);

    Frame* frame = element->document()->frame();
    if (!frame)
        return false;

    if (requirements & REQUIRE_AUTOCOMPLETE && !element->autoComplete())
        return false;

    form->name = GetFormIdentifier(*element);
    form->method = WTFStringToString16(element->method());
    form->origin = GURL(WTFStringToString16(frame->loader()->documentLoader()->url().string()));
    form->action = GURL(WTFStringToString16(frame->document()->completeURL(element->action())));
    form->user_submitted = element->wasUserSubmitted();

    // If the completed URL is not valid, just use the action we get from
    // WebKit.
    if (!form->action.is_valid())
        form->action = GURL(WTFStringToString16(element->action()));

    // A map from a FormField's name to the FormField itself.
    std::map<string16, FormField*> name_map;

    // The extracted FormFields.  We use pointers so we can store them in
    // |name_map|.
    ScopedVector<FormField> form_fields;

    WTF::Vector<WebCore::FormAssociatedElement*> control_elements = element->associatedElements();

    // A vector of bools that indicate whether each field in the form meets the
    // requirements and thus will be in the resulting |form|.
    std::vector<bool> fields_extracted(control_elements.size(), false);

    for (size_t i = 0; i < control_elements.size(); ++i) {
        if (!control_elements[i]->isFormControlElement())
            continue;

        HTMLFormControlElement* control_element = static_cast<HTMLFormControlElement*>(control_elements[i]);

        if(!IsAutofillableElement(*control_element))
            continue;

        HTMLInputElement* input_element = HTMLFormControlElementToHTMLInputElement(*control_element);
        if (requirements & REQUIRE_AUTOCOMPLETE && IsTextInput(input_element) &&
                !input_element->autoComplete())
                continue;

        if (requirements & REQUIRE_ENABLED && !control_element->isEnabledFormControl())
            continue;

        // Create a new FormField, fill it out and map it to the field's name.
        FormField* field = new FormField;
        HTMLFormControlElementToFormField(control_element, extract_mask, field);
        form_fields.push_back(field);
        // TODO: A label element is mapped to a form control element's id.
        // field->name() will contain the id only if the name does not exist.  Add
        // an id() method to HTMLFormControlElement and use that here.
        name_map[field->name] = field;
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
    RefPtr<WebCore::NodeList> labels = element->getElementsByTagName("label");
    for (unsigned i = 0; i < labels->length(); ++i) {
        HTMLLabelElement* label = static_cast<WebCore::HTMLLabelElement*>(labels->item(i));
        HTMLFormControlElement* field_element = label->control();
        if (!field_element || field_element->type() == "hidden")
            continue;

        std::map<string16, FormField*>::iterator iter =
            name_map.find(nameForAutofill(*field_element));
        // Concatenate labels because some sites might have multiple label
        // candidates.
        if (iter != name_map.end())
            iter->second->label += FindChildText(label);
    }

    // Loop through the form control elements, extracting the label text from the
    // DOM.  We use the |fields_extracted| vector to make sure we assign the
    // extracted label to the correct field, as it's possible |form_fields| will
    // not contain all of the elements in |control_elements|.
    for (size_t i = 0, field_idx = 0; i < control_elements.size() && field_idx < form_fields.size(); ++i) {
        // This field didn't meet the requirements, so don't try to find a label for
        // it.
        if (!fields_extracted[i])
            continue;

        if (!control_elements[i]->isFormControlElement())
            continue;

        const HTMLFormControlElement* control_element = static_cast<HTMLFormControlElement*>(control_elements[i]);
        if (form_fields[field_idx]->label.empty())
            form_fields[field_idx]->label = InferLabelForElement(*control_element);

        ++field_idx;

    }
    // Copy the created FormFields into the resulting FormData object.
    for (ScopedVector<FormField>::const_iterator iter = form_fields.begin(); iter != form_fields.end(); ++iter)
        form->fields.push_back(**iter);

    return true;
}

void FormManager::ExtractForms(Frame* frame) {

    ResetFrame(frame);

    WTF::PassRefPtr<HTMLCollection> web_forms = frame->document()->forms();

    for (size_t i = 0; i < web_forms->length(); ++i) {
        // Owned by |form_elements|.
        FormElement* form_element = new FormElement;
        HTMLFormElement* html_form_element = static_cast<HTMLFormElement*>(web_forms->item(i));
        form_element->form_element = html_form_element;

        WTF::Vector<FormAssociatedElement*> control_elements = html_form_element->associatedElements();
        for (size_t j = 0; j < control_elements.size(); ++j) {
            if (!control_elements[j]->isFormControlElement())
                continue;

            HTMLFormControlElement* element = static_cast<HTMLFormControlElement*>(control_elements[j]);

            if (!IsAutofillableElement(*element))
                continue;

            form_element->control_elements.push_back(element);

            // Save original values of <select> elements so we can restore them
            // when |ClearFormWithNode()| is invoked.
            if (IsSelectElement(*element)) {
                HTMLSelectElement* select_element = static_cast<HTMLSelectElement*>(element);
                form_element->control_values.push_back(WTFStringToString16(select_element->value()));
            } else
                form_element->control_values.push_back(string16());
        }

        form_elements_.push_back(form_element);
    }
}

void FormManager::GetFormsInFrame(const Frame* frame, RequirementsMask requirements, std::vector<FormData>* forms) {
    DCHECK(frame);
    DCHECK(forms);

    size_t num_fields_seen = 0;
    for (FormElementList::const_iterator form_iter = form_elements_.begin(); form_iter != form_elements_.end(); ++form_iter) {
        FormElement* form_element = *form_iter;

        if (form_element->form_element->document()->frame() != frame)
            continue;

        // To avoid overly expensive computation, we impose both a minimum and a
        // maximum number of allowable fields.
        if (form_element->control_elements.size() < kRequiredAutofillFields ||
            form_element->control_elements.size() > kMaxParseableFields)
            continue;

        if (requirements & REQUIRE_AUTOCOMPLETE && !form_element->form_element->autoComplete())
            continue;

        FormData form;
        HTMLFormElementToFormData(form_element->form_element.get(), requirements, EXTRACT_VALUE, &form);

        num_fields_seen += form.fields.size();
        if (num_fields_seen > kMaxParseableFields)
            break;

        if (form.fields.size() >= kRequiredAutofillFields)
            forms->push_back(form);
    }
}

bool FormManager::FindFormWithFormControlElement(HTMLFormControlElement* element, RequirementsMask requirements, FormData* form) {
    DCHECK(form);

    const Frame* frame = element->document()->frame();
    if (!frame)
        return false;

    for (FormElementList::const_iterator iter = form_elements_.begin(); iter != form_elements_.end(); ++iter) {
        const FormElement* form_element = *iter;

        if (form_element->form_element->document()->frame() != frame)
            continue;

        for (std::vector<RefPtr<HTMLFormControlElement> >::const_iterator iter = form_element->control_elements.begin(); iter != form_element->control_elements.end(); ++iter) {
            HTMLFormControlElement* candidate = iter->get();
            if (nameForAutofill(*candidate) == nameForAutofill(*element)) {
                ExtractMask extract_mask = static_cast<ExtractMask>(EXTRACT_VALUE | EXTRACT_OPTIONS);
                return HTMLFormElementToFormData(form_element->form_element.get(), requirements, extract_mask, form);
            }
        }
    }
    return false;
}

bool FormManager::FillForm(const FormData& form, Node* node) {
    FormElement* form_element = NULL;
    if (!FindCachedFormElement(form, &form_element))
        return false;

    RequirementsMask requirements = static_cast<RequirementsMask>(REQUIRE_AUTOCOMPLETE | REQUIRE_ENABLED | REQUIRE_EMPTY);
    ForEachMatchingFormField(form_element, node, requirements, form, NewCallback(this, &FormManager::FillFormField));

    return true;
}

bool FormManager::PreviewForm(const FormData& form, Node* node) {
    FormElement* form_element = NULL;
    if (!FindCachedFormElement(form, &form_element))
        return false;

    RequirementsMask requirements = static_cast<RequirementsMask>(REQUIRE_AUTOCOMPLETE | REQUIRE_ENABLED | REQUIRE_EMPTY);
    ForEachMatchingFormField(form_element, node, requirements, form, NewCallback(this, &FormManager::PreviewFormField));

    return true;
}

bool FormManager::ClearFormWithNode(Node* node) {
    FormElement* form_element = NULL;
    if (!FindCachedFormElementWithNode(node, &form_element))
        return false;

    for (size_t i = 0; i < form_element->control_elements.size(); ++i) {
        HTMLFormControlElement* element = form_element->control_elements[i].get();
        HTMLInputElement* input_element = HTMLFormControlElementToHTMLInputElement(*element);
        if (IsTextInput(input_element)) {

            // We don't modify the value of disabled fields.
            if (!input_element->isEnabledFormControl())
                continue;

            input_element->setValue("");
            input_element->setAutofilled(false);
             // Clearing the value in the focused node (above) can cause selection
             // to be lost. We force selection range to restore the text cursor.
             if (node == input_element) {
                 int length = input_element->value().length();
                 input_element->setSelectionRange(length, length);
             }
        } else {
            DCHECK(IsSelectElement(*element));
            HTMLSelectElement* select_element = static_cast<HTMLSelectElement*>(element);
            if (WTFStringToString16(select_element->value()) != form_element->control_values[i]) {
                select_element->setValue(form_element->control_values[i].c_str());
                select_element->dispatchFormControlChangeEvent();
            }
        }
    }

    return true;
}

bool FormManager::ClearPreviewedFormWithNode(Node* node, bool was_autofilled) {
    FormElement* form_element = NULL;
    if (!FindCachedFormElementWithNode(node, &form_element))
        return false;

    for (size_t i = 0; i < form_element->control_elements.size(); ++i) {
        HTMLFormControlElement* element = form_element->control_elements[i].get();
        HTMLInputElement* input_element = HTMLFormControlElementToHTMLInputElement(*element);

        // Only input elements can be previewed.
        if (!IsTextInput(input_element))
            continue;

        // If the input element has not been auto-filled, FormManager has not
        // previewed this field, so we have nothing to reset.
        if (!input_element->isAutofilled())
            continue;

        // There might be unrelated elements in this form which have already been
        // auto-filled. For example, the user might have already filled the address
        // part of a form and now be dealing with the credit card section. We only
        // want to reset the auto-filled status for fields that were previewed.
        if (input_element->suggestedValue().isEmpty())
            continue;

        // Clear the suggested value. For the initiating node, also restore the
        // original value.
        input_element->setSuggestedValue("");
        bool is_initiating_node = (node == input_element);
        if (is_initiating_node) {
            input_element->setAutofilled(was_autofilled);
        } else {
            input_element->setAutofilled(false);
        }

        // Clearing the suggested value in the focused node (above) can cause
        // selection to be lost. We force selection range to restore the text
        // cursor.
        if (is_initiating_node) {
            int length = input_element->value().length();
            input_element->setSelectionRange(length, length);
        }
    }

    return true;
}

void FormManager::Reset() {
    form_elements_.reset();
}

void FormManager::ResetFrame(const Frame* frame) {
    FormElementList::iterator iter = form_elements_.begin();
    while (iter != form_elements_.end()) {
        if ((*iter)->form_element->document()->frame() == frame)
            iter = form_elements_.erase(iter);
        else
            ++iter;
    }
}

bool FormManager::FormWithNodeIsAutofilled(Node* node) {
    FormElement* form_element = NULL;
    if (!FindCachedFormElementWithNode(node, &form_element))
        return false;

    for (size_t i = 0; i < form_element->control_elements.size(); ++i) {
        HTMLFormControlElement* element = form_element->control_elements[i].get();
        HTMLInputElement* input_element = HTMLFormControlElementToHTMLInputElement(*element);
        if (!IsTextInput(input_element))
            continue;

        if (input_element->isAutofilled())
            return true;
    }

    return false;
}

bool FormManager::FindCachedFormElementWithNode(Node* node, FormElement** form_element) {
    for (FormElementList::const_iterator form_iter = form_elements_.begin(); form_iter != form_elements_.end(); ++form_iter) {
        for (std::vector<RefPtr<HTMLFormControlElement> >::const_iterator iter = (*form_iter)->control_elements.begin(); iter != (*form_iter)->control_elements.end(); ++iter) {
            if (iter->get() == node) {
                *form_element = *form_iter;
                return true;
            }
        }
    }

    return false;
}

bool FormManager::FindCachedFormElement(const FormData& form, FormElement** form_element) {
    for (FormElementList::iterator form_iter = form_elements_.begin(); form_iter != form_elements_.end(); ++form_iter) {
        // TODO: matching on form name here which is not guaranteed to
        // be unique for the page, nor is it guaranteed to be non-empty.  Need to
        // find a way to uniquely identify the form cross-process.  For now we'll
        // check form name and form action for identity.
        // http://crbug.com/37990 test file sample8.html.
        // Also note that WebString() == WebString(string16()) does not evaluate to
        // |true| -- WebKit distinguisges between a "null" string (lhs) and an
        // "empty" string (rhs). We don't want that distinction, so forcing to
        // string16.
        string16 element_name = GetFormIdentifier(*(*form_iter)->form_element);
        GURL action(WTFStringToString16((*form_iter)->form_element->document()->completeURL((*form_iter)->form_element->action()).string()));
        if (element_name == form.name && action == form.action) {
            *form_element = *form_iter;
            return true;
        }
    }

    return false;
}


void FormManager::ForEachMatchingFormField(FormElement* form, Node* node, RequirementsMask requirements, const FormData& data, Callback* callback) {
    // It's possible that the site has injected fields into the form after the
    // page has loaded, so we can't assert that the size of the cached control
    // elements is equal to the size of the fields in |form|.  Fortunately, the
    // one case in the wild where this happens, paypal.com signup form, the fields
    // are appended to the end of the form and are not visible.
    for (size_t i = 0, j = 0; i < form->control_elements.size() && j < data.fields.size(); ++i) {
        HTMLFormControlElement* element = form->control_elements[i].get();
        string16 element_name = nameForAutofill(*element);

        // Search forward in the |form| for a corresponding field.
        size_t k = j;
        while (k < data.fields.size() && element_name != data.fields[k].name)
               k++;

        if (k >= data.fields.size())
            continue;

        DCHECK_EQ(data.fields[k].name, element_name);

        bool is_initiating_node = false;

        HTMLInputElement* input_element = HTMLFormControlElementToHTMLInputElement(*element);
        if (IsTextInput(input_element)) {
            // TODO: WebKit currently doesn't handle the autocomplete
            // attribute for select control elements, but it probably should.
            if (!input_element->autoComplete())
                continue;

            is_initiating_node = (input_element == node);

            // Only autofill empty fields and the firls that initiated the filling,
            // i.e. the field the user is currently editing and interacting with.
            if (!is_initiating_node && !input_element->value().isEmpty())
               continue;
        }

        if (!element->isEnabledFormControl() || element->isReadOnlyFormControl() || !element->isFocusable())
            continue;

        callback->Run(element, &data.fields[k], is_initiating_node);

        // We found a matching form field so move on to the next.
        ++j;
    }

    delete callback;
}

void FormManager::FillFormField(HTMLFormControlElement* field, const FormField* data, bool is_initiating_node) {
    // Nothing to fill.
    if (data->value.empty())
        return;

    HTMLInputElement* input_element = HTMLFormControlElementToHTMLInputElement(*field);

    if (IsTextInput(input_element)) {
        // If the maxlength attribute contains a negative value, maxLength()
        // returns the default maxlength value.
        input_element->setValue(data->value.substr(0, input_element->maxLength()).c_str(), true);
        input_element->setAutofilled(true);
        if (is_initiating_node) {
            int length = input_element->value().length();
            input_element->setSelectionRange(length, length);
        }
    } else {
        DCHECK(IsSelectElement(*field));
        HTMLSelectElement* select_element = static_cast<HTMLSelectElement*>(field);
        if (WTFStringToString16(select_element->value()) != data->value) {
            select_element->setValue(data->value.c_str());
            select_element->dispatchFormControlChangeEvent();
        }
    }
}

void FormManager::PreviewFormField(HTMLFormControlElement* field, const FormField* data, bool is_initiating_node) {
    // Nothing to preview.
    if (data->value.empty())
        return;

    // Only preview input fields.
    HTMLInputElement* input_element = HTMLFormControlElementToHTMLInputElement(*field);
    if (!IsTextInput(input_element))
        return;

    // If the maxlength attribute contains a negative value, maxLength()
    // returns the default maxlength value.
    input_element->setSuggestedValue(data->value.substr(0, input_element->maxLength()).c_str());
    input_element->setAutofilled(true);
    if (is_initiating_node) {
        // Select the part of the text that the user didn't type.
        input_element->setSelectionRange(input_element->value().length(), input_element->suggestedValue().length());
    }
}

}
