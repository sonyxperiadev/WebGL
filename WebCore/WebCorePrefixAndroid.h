/*
**
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

// This prefix file is for Android only. It should contain only:
//    - the special trick to catch us using new or delete without including "config.h"
// Things that need to be defined globally should go into "config.h".

#include <limits.h>
#include <math.h>   // !!! is this allowed as a standard include?
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef __cplusplus
#define PREFIX_FOR_WEBCORE 1
#define EXPORT __attribute__((visibility("default")))

#include <algorithm>
#include "stl_iterator_base.h"
#include "heap.h"
#include <memory>
#include <new>
#include <wtf/Vector.h>

#endif

#ifdef __cplusplus
#define new ("if you use new/delete make sure to include config.h at the top of the file"()) 
#define delete ("if you use new/delete make sure to include config.h at the top of the file"()) 
#endif

typedef short int   flex_int16_t;
typedef int     flex_int32_t;
typedef signed char     flex_int8_t;
typedef unsigned short int  flex_uint16_t;
typedef unsigned int    flex_uint32_t;
typedef unsigned char   flex_uint8_t;

#define FLATTEN_FRAMESET
#define FLATTEN_IFRAME
#define ANDROID_CANVAS_IMPL
#define ANDROID_DRAW_OWN_CARET
#define ANDROID_EXPOSE_COLUMN_GAP
#define ANDROID_NAVIGATE_LISTBOX
#define ANDROID_NAVIGATE_AREAMAPS
#define ANDROID_LAYOUT
#define ANDROID_SELECT_TEXT_AREAS
#define ANDROID_KEYBOARD_NAVIGATION
#define ANDROID_NO_BODY_INNER_HTML
#define ANDROID_BRIDGE
#define ANDROID_FIX
//#define ANDROID_INSTRUMENT
#define ANDROID_IGNORE_BLUR
#define ANDROID_ACCEPT_CHANGES_TO_FOCUSED_TEXTFIELDS
#define ANDROID_SCROLL_FIX
#define ANDROID_DO_NOT_RESTORE_PREVIOUS_SELECTION
#define ANDROID_REDIRECT_IMAGE_INVALIDATES
#define ANDROID_RESET_SELECTION
#define ANDROID_PLUGINS
#define ANDROID_CHOOSE_HIGHLIGHT_COLOR_FOR_TEXT_MATCHES
#define ANDROID_META_SUPPORT
#define ANDROID_FILE_SECURITY
#define ANDROID_DESELECT_SELECT
#define ANDROID_LISTBOX_USES_MENU_LIST
#define ANDROID_ALLOW_TRANSPARENT_BACKGROUNDS
#define ANDROID_LOG
#define ANDROID_HISTORY_CLIENT
#define ANDROID_MULTIPLE_WINDOWS
#define ANDROID_CSS_TAP_HIGHLIGHT_COLOR
#define ANDROID_JAVASCRIPT_SECURITY
#define ANDROID_DISABLE_UPLOAD
#define ANDROID_BLOCK_NETWORK_IMAGE
#define ANDROID_PRELOAD_CHANGES

// Prevent Webkit from drawing the selection in textfields/textareas, since we 
// draw it ourselves in the UI thread.
#define ANDROID_DO_NOT_DRAW_TEXTFIELD_SELECTION

// This should possibly be patched back to WebKit since they seem to lose the
// user gesture hint. If we do decide to patch this back, the user gesture flag
// should probably be passed in the NavigationAction rather than the
// ResourceRequest.
#define ANDROID_USER_GESTURE

// Inform webkit (Font.cpp) that we NEVER want to perform rounding hacks for
// text, since we always measure/draw in subpixel mode (performance)
#define ANDROID_NEVER_ROUND_FONT_METRICS

// Add bool to GlyphBuffer as a drawing hint, marking buffers whose array of
// widths happen to exactly match the values returned from the font. This allows
// the drawing code to ignore the position array if they choose (performace)
#define ANDROID_GLYPHBUFFER_HAS_ADJUSTED_WIDTHS

// Add support for the orientation window property
#define ANDROID_ORIENTATION_SUPPORT
