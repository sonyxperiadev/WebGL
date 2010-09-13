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
#include "FormFieldAndroid.h"

#include "ChromiumIncludes.h"
#include "HTMLFormControlElement.h"
#include "HTMLInputElement.h"
#include "HTMLSelectElement.h"

// TODO: This file is taken from chromium/webkit/glue/form_field.h and
// customised to use WebCore types rather than WebKit API types. It would
// be nice and would ease future merge pain if the two could be combined.

namespace webkit_glue {
FormField::FormField()
    : size_(0) {
}

FormField::FormField(WebCore::HTMLFormControlElement& element)
    : size_(0) {
    name_ = string16(element.name().characters());

    // TODO: Extract the field label.  For now we just use the field
    // name.
    label_ = name_;

    form_control_type_ = string16(element.type().characters());
    if (form_control_type_ == ASCIIToUTF16("text")) {
        WebCore::HTMLInputElement* input_element = static_cast<WebCore::HTMLInputElement*>(&element);
        value_ = string16(input_element->value().characters());
        size_ = input_element->size();
    } else if (form_control_type_ == ASCIIToUTF16("select-one")) {
        WebCore::HTMLSelectElement* select_element = static_cast<WebCore::HTMLSelectElement*>(&element);
        value_ = string16(select_element->value().characters());
    }

    TrimWhitespace(value_, TRIM_LEADING, &value_);
}

FormField::FormField(const string16& label,
                     const string16& name,
                     const string16& value,
                     const string16& form_control_type,
                     int size)
    : label_(label),
      name_(name),
      value_(value),
      form_control_type_(form_control_type),
      size_(size) {
}

bool FormField::operator!=(const FormField& field) const {
    return !operator==(field);
}
}  // namespace webkit_glue
