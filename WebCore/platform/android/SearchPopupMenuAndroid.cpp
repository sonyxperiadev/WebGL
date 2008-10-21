/*
** libs/WebKitLib/WebKit/WebCore/platform/android/SearchPopupMenuAndroid.cpp
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

#ifndef SearchPopupMenuAndroid_h
#define SearchPopupMenuAndroid_h

#include "config.h"
#include "SearchPopupMenu.h"

namespace WebCore {

// Save the past searches stored in 'searchItems' to a database associated with 'name'
void SearchPopupMenu::saveRecentSearches(const AtomicString& name, const Vector<String>& searchItems) 
{ 
    
    //ASSERT(0); //notImplemented(); 

}

// Load past searches associated with 'name' from the database to 'searchItems'
void SearchPopupMenu::loadRecentSearches(const AtomicString& name, Vector<String>& searchItems) 
{ 

    //ASSERT(0); //notImplemented(); 

}

// Create a search popup menu - not sure what else we have to do here
SearchPopupMenu::SearchPopupMenu(PopupMenuClient* client) : PopupMenu(client) 
{ 
    
    //ASSERT(0); //notImplemented(); 
    
}
        
// functions new to Jun-07 tip of tree merge:
bool SearchPopupMenu::enabled() { return false; }

}   // WebCore

#endif

