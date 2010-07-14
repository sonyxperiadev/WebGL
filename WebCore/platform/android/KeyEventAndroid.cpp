/*
 * Copyright 2007, The Android Open Source Project
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Collabora, Ltd.  All rights reserved.
 * All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformKeyboardEvent.h"

#include "NotImplemented.h"
#include "WindowsKeyboardCodes.h"
#include <ui/KeycodeLabels.h>

namespace WebCore {

// compare to same function in gdk/KeyEventGdk.cpp
static int windowsKeyCodeForKeyEvent(unsigned int keyCode)
{
    // Does not provide all key codes, and does not handle all keys.
    switch (keyCode) {
    case AKEYCODE_DEL:
        return VK_BACK;
    case AKEYCODE_TAB:
        return VK_TAB;
    case AKEYCODE_CLEAR:
        return VK_CLEAR;
    case AKEYCODE_DPAD_CENTER:
    case AKEYCODE_ENTER:
        return VK_RETURN;
    case AKEYCODE_SHIFT_LEFT:
    case AKEYCODE_SHIFT_RIGHT:
        return VK_SHIFT;
    // back will serve as escape, although we probably do not have access to it
    case AKEYCODE_BACK:
        return VK_ESCAPE;
    case AKEYCODE_SPACE:
        return VK_SPACE;
    case AKEYCODE_HOME:
        return VK_HOME;
    case AKEYCODE_DPAD_LEFT:
        return VK_LEFT;
    case AKEYCODE_DPAD_UP:
        return VK_UP;
    case AKEYCODE_DPAD_RIGHT:
        return VK_RIGHT;
    case AKEYCODE_DPAD_DOWN:
        return VK_DOWN;
    case AKEYCODE_0:
        return VK_0;
    case AKEYCODE_1:
        return VK_1;
    case AKEYCODE_2:
        return VK_2;
    case AKEYCODE_3:
        return VK_3;
    case AKEYCODE_4:
        return VK_4;
    case AKEYCODE_5:
        return VK_5;
    case AKEYCODE_6:
        return VK_6;
    case AKEYCODE_7:
        return VK_7;
    case AKEYCODE_8:
        return VK_8;
    case AKEYCODE_9:
        return VK_9;
    case AKEYCODE_A:
        return VK_A;
    case AKEYCODE_B:
        return VK_B;
    case AKEYCODE_C:
        return VK_C;
    case AKEYCODE_D:
        return VK_D;
    case AKEYCODE_E:
        return VK_E;
    case AKEYCODE_F:
        return VK_F;
    case AKEYCODE_G:
        return VK_G;
    case AKEYCODE_H:
        return VK_H;
    case AKEYCODE_I:
        return VK_I;
    case AKEYCODE_J:
        return VK_J;
    case AKEYCODE_K:
        return VK_K;
    case AKEYCODE_L:
        return VK_L;
    case AKEYCODE_M:
        return VK_M;
    case AKEYCODE_N:
        return VK_N;
    case AKEYCODE_O:
        return VK_O;
    case AKEYCODE_P:
        return VK_P;
    case AKEYCODE_Q:
        return VK_Q;
    case AKEYCODE_R:
        return VK_R;
    case AKEYCODE_S:
        return VK_S;
    case AKEYCODE_T:
        return VK_T;
    case AKEYCODE_U:
        return VK_U;
    case AKEYCODE_V:
        return VK_V;
    case AKEYCODE_W:
        return VK_W;
    case AKEYCODE_X:
        return VK_X;
    case AKEYCODE_Y:
        return VK_Y;
    case AKEYCODE_Z:
        return VK_Z;
    // colon
    case AKEYCODE_SEMICOLON:
        return VK_OEM_1;
    case AKEYCODE_COMMA:
        return VK_OEM_COMMA;
    case AKEYCODE_MINUS:
        return VK_OEM_MINUS;
    case AKEYCODE_EQUALS:
        return VK_OEM_PLUS;
    case AKEYCODE_PERIOD:
        return VK_OEM_PERIOD;
    case AKEYCODE_SLASH:
        return VK_OEM_2;
    // maybe not the right choice
    case AKEYCODE_LEFT_BRACKET:
        return VK_OEM_4;
    case AKEYCODE_BACKSLASH:
        return VK_OEM_5;
    case AKEYCODE_RIGHT_BRACKET:
        return VK_OEM_6;
    default:
        return 0;
    }
}

static String keyIdentifierForAndroidKeyCode(int keyCode)
{
    // Does not return all of the same key identifiers, and
    // does not handle all the keys.
    switch (keyCode) {
    case AKEYCODE_CLEAR:
        return "Clear";
    case AKEYCODE_ENTER:
    case AKEYCODE_DPAD_CENTER:
        return "Enter";
    case AKEYCODE_HOME:
        return "Home";
    case AKEYCODE_DPAD_DOWN:
        return "Down";
    case AKEYCODE_DPAD_LEFT:
        return "Left";
    case AKEYCODE_DPAD_RIGHT:
        return "Right";
    case AKEYCODE_DPAD_UP:
        return "Up";
    // Standard says that DEL becomes U+00007F.
    case AKEYCODE_DEL:
        return "U+00007F";
    default:
        char upper[16];
        sprintf(upper, "U+%06X", windowsKeyCodeForKeyEvent(keyCode));
        return String(upper);
    }
}

static inline String singleCharacterString(UChar32 c) 
{
    if (!c)
        return String();
    if (c > 0xffff) {
        UChar lead = U16_LEAD(c);
        UChar trail = U16_TRAIL(c);
        UChar utf16[2] = {lead, trail};
        return String(utf16, 2);
    }
    UChar n = (UChar)c;
    return String(&n, 1);
}

PlatformKeyboardEvent::PlatformKeyboardEvent(int keyCode, UChar32 unichar,
        int repeatCount, bool down, bool cap, bool alt, bool sym)
    : m_type(down ? KeyDown : KeyUp)
    , m_text(singleCharacterString(unichar))
    , m_unmodifiedText(singleCharacterString(unichar))
    , m_keyIdentifier(keyIdentifierForAndroidKeyCode(keyCode))
    , m_autoRepeat(repeatCount > 0)
    , m_windowsVirtualKeyCode(windowsKeyCodeForKeyEvent(keyCode))
    , m_nativeVirtualKeyCode(keyCode)
    , m_isKeypad(false)
    , m_shiftKey(cap ? ShiftKey : 0)
    , m_ctrlKey(sym ? CtrlKey : 0)
    , m_altKey(alt ? AltKey : 0)
    , m_metaKey(0)
    // added for android
    , m_repeatCount(repeatCount)
    , m_unichar(unichar)
{
    // Copied from the mac port
    if (m_windowsVirtualKeyCode == '\r') {
        m_text = "\r";
        m_unmodifiedText = "\r";
    }

    if (m_text == "\x7F")
        m_text = "\x8";
    if (m_unmodifiedText == "\x7F")
        m_unmodifiedText = "\x8";

    if (m_windowsVirtualKeyCode == 9) {
        m_text = "\x9";
        m_unmodifiedText = "\x9";
    }
}

bool PlatformKeyboardEvent::currentCapsLockState()
{
    notImplemented();
    return false;
}

void PlatformKeyboardEvent::getCurrentModifierState(bool& shiftKey, bool& ctrlKey, bool& altKey, bool& metaKey)
{
    notImplemented();
    shiftKey = false;
    ctrlKey = false;
    altKey = false;
    metaKey = false;
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool backwardCompatibilityMode)
{
    // Copied with modification from the mac port.
    ASSERT(m_type == KeyDown);
    ASSERT(type == RawKeyDown || type == Char);
    m_type = type;
    if (backwardCompatibilityMode)
        return;

    if (type == RawKeyDown) {
        m_text = String();
        m_unmodifiedText = String();
    } else {
        m_keyIdentifier = String();
        m_windowsVirtualKeyCode = 0;
    }
}

} // namespace WebCore
