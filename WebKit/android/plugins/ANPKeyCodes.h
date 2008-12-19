/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANPKeyCodes_DEFINED
#define ANPKeyCodes_DEFINED

/*  List the key codes that are set to a plugin in the ANPKeyEvent.
 
    These exactly match the values in android/view/KeyEvent.java and the
    corresponding .h file include/ui/KeycodeLabels.h, which contains more than
    I want to publish to plugin authors.
*/
enum ANPKeyCodes {
    kUnknown_ANPKeyCode = 0,

    kSoftLeft_ANPKeyCode = 1,
    kSoftRight_ANPKeyCode = 2,
    kHome_ANPKeyCode = 3,
    kBack_ANPKeyCode = 4,
    kCall_ANPKeyCode = 5,
    kEndCall_ANPKeyCode = 6,
    k0_ANPKeyCode = 7,
    k1_ANPKeyCode = 8,
    k2_ANPKeyCode = 9,
    k3_ANPKeyCode = 10,
    k4_ANPKeyCode = 11,
    k5_ANPKeyCode = 12,
    k6_ANPKeyCode = 13,
    k7_ANPKeyCode = 14,
    k8_ANPKeyCode = 15,
    k9_ANPKeyCode = 16,
    kStar_ANPKeyCode = 17,
    kPound_ANPKeyCode = 18,
    kDpadUp_ANPKeyCode = 19,
    kDpadDown_ANPKeyCode = 20,
    kDpadLeft_ANPKeyCode = 21,
    kDpadRight_ANPKeyCode = 22,
    kDpadCenter_ANPKeyCode = 23,
    kVolumeUp_ANPKeyCode = 24,
    kVolumeDown_ANPKeyCode = 25,
    kPower_ANPKeyCode = 26,
    kCamera_ANPKeyCode = 27,
    kClear_ANPKeyCode = 28,
    kA_ANPKeyCode = 29,
    kB_ANPKeyCode = 30,
    kC_ANPKeyCode = 31,
    kD_ANPKeyCode = 32,
    kE_ANPKeyCode = 33,
    kF_ANPKeyCode = 34,
    kG_ANPKeyCode = 35,
    kH_ANPKeyCode = 36,
    kI_ANPKeyCode = 37,
    kJ_ANPKeyCode = 38,
    kK_ANPKeyCode = 39,
    kL_ANPKeyCode = 40,
    kM_ANPKeyCode = 41,
    kN_ANPKeyCode = 42,
    kO_ANPKeyCode = 43,
    kP_ANPKeyCode = 44,
    kQ_ANPKeyCode = 45,
    kR_ANPKeyCode = 46,
    kS_ANPKeyCode = 47,
    kT_ANPKeyCode = 48,
    kU_ANPKeyCode = 49,
    kV_ANPKeyCode = 50,
    kW_ANPKeyCode = 51,
    kX_ANPKeyCode = 52,
    kY_ANPKeyCode = 53,
    kZ_ANPKeyCode = 54,
    kComma_ANPKeyCode = 55,
    kPeriod_ANPKeyCode = 56,
    kAltLeft_ANPKeyCode = 57,
    kAltRight_ANPKeyCode = 58,
    kShiftLeft_ANPKeyCode = 59,
    kShiftRight_ANPKeyCode = 60,
    kTab_ANPKeyCode = 61,
    kSpace_ANPKeyCode = 62,
    kSym_ANPKeyCode = 63,
    kExplorer_ANPKeyCode = 64,
    kEnvelope_ANPKeyCode = 65,
    kNewline_ANPKeyCode = 66,
    kDel_ANPKeyCode = 67,
    kGrave_ANPKeyCode = 68,
    kMinus_ANPKeyCode = 69,
    kEquals_ANPKeyCode = 70,
    kLeftBracket_ANPKeyCode = 71,
    kRightBracket_ANPKeyCode = 72,
    kBackslash_ANPKeyCode = 73,
    kSemicolon_ANPKeyCode = 74,
    kApostrophe_ANPKeyCode = 75,
    kSlash_ANPKeyCode = 76,
    kAt_ANPKeyCode = 77,
    kNum_ANPKeyCode = 78,
    kHeadSetHook_ANPKeyCode = 79,
    kFocus_ANPKeyCode = 80,
    kPlus_ANPKeyCode = 81,
    kMenu_ANPKeyCode = 82,
    kNotification_ANPKeyCode = 83,
    kSearch_ANPKeyCode = 84
};

#endif

