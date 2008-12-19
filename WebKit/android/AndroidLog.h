/*
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

#ifndef ANDROIDLOG_H_
#define ANDROIDLOG_H_

#ifdef ANDROID_DOM_LOGGING
#include <stdio.h>
extern FILE* gDomTreeFile;
#define DOM_TREE_LOG_FILE "/sdcard/domTree.txt"
#define DUMP_DOM_LOGD(...) { if (gDomTreeFile) \
    fprintf(gDomTreeFile, __VA_ARGS__); else LOGD(__VA_ARGS__); }

extern FILE* gRenderTreeFile;
#define RENDER_TREE_LOG_FILE "/sdcard/renderTree.txt"
#define DUMP_RENDER_LOGD(...) { if (gRenderTreeFile) \
    fprintf(gRenderTreeFile, __VA_ARGS__); else LOGD(__VA_ARGS__); }
#else
#define DUMP_DOM_LOGD(...) ((void)0)
#define DUMP_RENDER_LOGD(...) ((void)0)
#endif /* ANDROID_DOM_LOGGING */

#define DISPLAY_TREE_LOG_FILE "/data/data/com.android.browser/displayTree.txt"

#endif /* ANDROIDLOG_H_ */
