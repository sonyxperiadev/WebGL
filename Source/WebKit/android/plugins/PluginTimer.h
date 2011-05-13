/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2008 Google Inc. All rights reserved.
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

#ifndef PluginTimer_H
#define PluginTimer_H

#include "RefCounted.h"
#include "Timer.h"
#include "npapi.h"

namespace WebCore {

    class PluginTimerList;

    class PluginTimer : public TimerBase, public RefCounted<PluginTimer> {
    public:
        PluginTimer(PluginTimer** list, NPP instance, bool repeat,
                    void (*proc)(NPP npp, uint32_t timerID));
        virtual ~PluginTimer();
    
        uint32_t timerID() const { return m_timerID; }

        void unschedule() { m_unscheduled = true; }

        static PluginTimer* Find(PluginTimer* list, uint32_t timerID);

    private:
        // override from TimerBase
        virtual void fired();
        
        PluginTimer* next() const { return m_next; }
        friend class PluginTimerList;

        PluginTimer**   m_list;
        PluginTimer*    m_prev;
        PluginTimer*    m_next;
        NPP             m_instance;
        void            (*m_timerFunc)(NPP, uint32_t);
        uint32_t          m_timerID;
        bool            m_repeat;
        bool            m_unscheduled;
    };
    
    class PluginTimerList {
    public:
        PluginTimerList() : m_list(0) {}
        ~PluginTimerList();
        
        uint32_t schedule(NPP instance, uint32_t interval, bool repeat,
                        void (*proc)(NPP npp, uint32_t timerID));
        void unschedule(NPP instance, uint32_t timerID);
        
    private:
        PluginTimer* m_list;
    };

} // namespace WebCore

#endif
