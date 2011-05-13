/*
 * Copyright 2010 The Android Open Source Project
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MemoryUsage_h
#define MemoryUsage_h

class MemoryUsage {
public:
    static int memoryUsageMb(bool forceFresh);
    static int lowMemoryUsageMb() { return m_lowMemoryUsageMb; }
    static int highMemoryUsageMb() { return m_highMemoryUsageMb; }
    static int highUsageDeltaMb() { return m_highUsageDeltaMb; }
    static void setHighMemoryUsageMb(int highMemoryUsageMb) { m_highMemoryUsageMb = highMemoryUsageMb; }
    static void setLowMemoryUsageMb(int lowMemoryUsageMb) { m_lowMemoryUsageMb = lowMemoryUsageMb; }
    static void setHighUsageDeltaMb(int highUsageDeltaMb) { m_highUsageDeltaMb = highUsageDeltaMb; }

private:
    static int m_lowMemoryUsageMb;
    static int m_highMemoryUsageMb;
    static int m_highUsageDeltaMb;
};

#endif
