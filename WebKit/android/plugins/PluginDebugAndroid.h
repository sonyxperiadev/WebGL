#ifdef ANDROID_PLUGINS

/*
**
** Copyright 2008, The Android Open Source Project
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

#ifndef PLUGIN_DEBUG_ANDROID_H__
#define PLUGIN_DEBUG_ANDROID_H__

// Define PLUGIN_DEBUG_LOCAL in an individual C++ file to enable for
// that file only.

// Define PLUGIN_DEBUG_GLOBAL to 1 to turn plug-in debug for all
// Android plug-in code in this direectory.
#define PLUGIN_DEBUG_GLOBAL     0

#if PLUGIN_DEBUG_GLOBAL || defined(PLUGIN_DEBUG_LOCAL)
# define PLUGIN_LOG(A, B...)    do { LOGI( A , ## B ); } while(0)
#else
# define PLUGIN_LOG(A, B...)    do { } while(0)
#endif

#endif // defined(PLUGIN_DEBUG_ANDROID_H__)

#endif
