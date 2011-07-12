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

// TODO: This file is taken from chromium/webkit/glue/form_field.h and
// customised to use WebCore types rather than WebKit API types. It would
// be nice and would ease future merge pain if the two could be combined.

namespace WebCore {
class HTMLFormControlElement;
}

namespace webkit_glue {

// Stores information about a field in a form.
struct FormField {
    FormField();
    explicit FormField(const WebCore::HTMLFormControlElement& element);
    FormField(const string16& label, const string16& name, const string16& value, const string16& form_control_type, int max_length, bool is_autofilled);
    virtual ~FormField();

    // Equality tests for identity which does not include |value_| or |size_|.
    // Use |StrictlyEqualsHack| method to test all members.
    // TODO: These operators need to be revised when we implement field
    // ids.
    bool operator==(const FormField& field) const;
    bool operator!=(const FormField& field) const;

    // Test equality of all data members.
    // TODO: This will be removed when we implement field ids.
    bool StrictlyEqualsHack(const FormField& field) const;

    string16 label;
    string16 name;
    string16 value;
    string16 form_control_type;
    int max_length;
    bool is_autofilled;
    std::vector<string16> option_strings;
};

// So we can compare FormFields with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const FormField& field);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FORM_FIELD_H_
