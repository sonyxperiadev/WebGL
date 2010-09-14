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

#ifndef FormFieldAndroid_h
#define FormFieldAndroid_h

#include <base/string16.h>
#include <vector>

// TODO: This file is taken from chromium/webkit/glue/form_field.cc and
// customised to use WebCore types rather than WebKit API types. It would
// be nice and would ease future merge pain if the two could be combined.

namespace WebCore {
class HTMLFormControlElement;
}

namespace webkit_glue {

// Stores information about a field in a form.
class FormField {
public:
    FormField();
    explicit FormField(WebCore::HTMLFormControlElement& element);
    FormField(const string16& label,
              const string16& name,
              const string16& value,
              const string16& form_control_type,
              int size);

    const string16& label() const { return label_; }
    const string16& name() const { return name_; }
    const string16& value() const { return value_; }
    const string16& form_control_type() const { return form_control_type_; }
    int size() const { return size_; }
    // Returns option string for elements for which they make sense (select-one,
    // for example) for the rest of elements return an empty array.
    const std::vector<string16>& option_strings() const {
      return option_strings_;
    }


    void set_label(const string16& label) { label_ = label; }
    void set_name(const string16& name) { name_ = name; }
    void set_value(const string16& value) { value_ = value; }
    void set_form_control_type(const string16& form_control_type) {
        form_control_type_ = form_control_type;
    }
    void set_size(int size) { size_ = size; }
    void set_option_strings(const std::vector<string16>& strings) {
        option_strings_ = strings;
    }

    bool operator==(const FormField& field) const;
    bool operator!=(const FormField& field) const;

private:
    string16 label_;
    string16 name_;
    string16 value_;
    string16 form_control_type_;
    int size_;
    std::vector<string16> option_strings_;
};

inline bool FormField::operator==(const FormField& field) const {
    // A FormField stores a value, but the value is not part of the identity of
    // the field, so we don't want to compare the values.
    return (label_ == field.label_ &&
            name_ == field.name_ &&
            form_control_type_ == field.form_control_type_);

}

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FORM_FIELD_H_
