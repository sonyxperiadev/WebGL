/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2008 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#ifndef PluginTimer_H
#define PluginTimer_H

#include "Timer.h"
#include "npapi.h"

namespace WebCore {

    class PluginTimerList;

    class PluginTimer : public TimerBase {
    public:
        PluginTimer(PluginTimer** list, NPP instance, bool repeat,
                    void (*proc)(NPP npp, uint32 timerID));
        virtual ~PluginTimer();
    
        uint32 timerID() const { return m_timerID; }

        static PluginTimer* Find(PluginTimer* list, uint32 timerID);

    private:
        // override from TimerBase
        virtual void fired();
        
        PluginTimer* next() const { return m_next; }
        friend class PluginTimerList;

        PluginTimer**   m_list;
        PluginTimer*    m_prev;
        PluginTimer*    m_next;
        NPP             m_instance;
        void            (*m_timerFunc)(NPP, uint32);
        uint32          m_timerID;
        bool            m_repeat;
    };
    
    class PluginTimerList {
    public:
        PluginTimerList() : m_list(0) {}
        ~PluginTimerList();
        
        uint32 schedule(NPP instance, uint32 interval, bool repeat,
                        void (*proc)(NPP npp, uint32 timerID));
        void unschedule(NPP instance, uint32 timerID);
        
    private:
        PluginTimer* m_list;
    };

} // namespace WebCore

#endif
