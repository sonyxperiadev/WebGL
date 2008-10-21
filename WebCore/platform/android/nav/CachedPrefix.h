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

#ifndef CachedPrefix_H
#define CachedPrefix_H

#ifndef LOG_TAG
#define LOG_TAG "navcache"
#endif

#include "config.h"
#include "CachedDebug.h"

#ifdef LOG
#undef LOG
#endif

#include <utils/Log.h>

#define OFFSETOF(type, field) ((char*)&(((type*)1)->field) - (char*)1) // avoids gnu warning

#endif
