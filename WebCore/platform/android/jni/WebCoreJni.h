/* Copyright 2008, The Android Open Source Project
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

#ifndef WEBCOREJNI_H
#define WEBCOREJNI_H

#include <jni.h>

namespace android {

class WebCoreJni {
    static JavaVM *mJavaVM;
  public:
    static void setJavaVM(JavaVM *vm) { mJavaVM = vm; }
    static JavaVM *getJavaVM() { return mJavaVM; }
};

}   // namespace android

#endif // WEBCOREJNI_H
