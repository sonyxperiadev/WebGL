/* libs/WebKitLib/WebKit/WebCore/platform/android/FileChooserAndroid.cpp
**
** Copyright 2006, The Android Open Source Project
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
#include "FileChooser.h"
#include "Font.h"
#include "Icon.h"
#include "LocalizedStrings.h"
#include "PlatformString.h"

namespace WebCore {

FileChooser::FileChooser(FileChooserClient* client, const String& initialFilename)
{
    m_client = client;
    if (initialFilename.length() == 0)
        m_filename = fileButtonNoFileSelectedLabel();
    else
        m_filename = initialFilename;
}

FileChooser::~FileChooser() 
{
}

void FileChooser::openFileChooser(Document* doc) 
{ 
    // FIXME: NEED TO OPEN A FILE CHOOSER OF SOME SORT!!
    // When it's chosen, set m_filename, call chooseFile(m_filename) and call chooseIcon(m_filename)
}

String FileChooser::basenameForWidth(const Font& font, int width) const 
{ 
    // FIXME: This could be a lot faster, but assuming the data will not often be 
    // much longer than the provided width, this may be fast enough.
    String output = m_filename.copy();
    while (font.width(TextRun(output.impl())) > width && output.length() > 4) {
        output = output.replace(output.length() - 4, 4, String("..."));
    }
    return output;
    
}


// The following two strings are used for File Upload form control, ie
// <input type="file">. The first is the text that appears on the button
// that when pressed, the user can browse for and select a file. The
// second string is rendered on the screen when no file has been selected.
String fileButtonChooseFileLabel() { return String("Uploads Disabled"); }
String fileButtonNoFileSelectedLabel() { return String("No file selected"); }

}   // WebCore

