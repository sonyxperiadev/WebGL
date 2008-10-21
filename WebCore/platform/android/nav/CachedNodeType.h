/*
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

#ifndef CachedNodeType_H
#define CachedNodeType_H

namespace android {

enum CachedNodeType {
    NORMAL_CACHEDNODETYPE = 0,
    ADDRESS_CACHEDNODETYPE = 1,
    EMAIL_CACHEDNODETYPE = 2,
    PHONE_CACHEDNODETYPE = 4,
    ALL_CACHEDNODETYPES = 7 
};

}

#endif
