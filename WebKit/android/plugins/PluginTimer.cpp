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

#include "config.h"
#include "PluginTimer.h"
#include "RefPtr.h"

namespace WebCore {

    static uint32_t gTimerID;

    PluginTimer::PluginTimer(PluginTimer** list, NPP instance, bool repeat,
                             void (*timerFunc)(NPP npp, uint32_t timerID))
                : m_list(list),
                  m_instance(instance),
                  m_timerFunc(timerFunc),
                  m_repeat(repeat),
                  m_unscheduled(false)
    {
        m_timerID = ++gTimerID;

        m_next = *list;
        if (m_next) {
            m_next->m_prev = this;
        }
        m_prev = 0;
        *list = this;
        relaxAdoptionRequirement();
    }
    
    PluginTimer::~PluginTimer()
    {
        if (m_next) {
            m_next->m_prev = m_prev;
        }
        if (m_prev) {
            m_prev->m_next = m_next;
        } else {
            *m_list = m_next;
        }
    }
        
    void PluginTimer::fired()
    {
        // ensure the timer cannot be deleted until this method completes
        RefPtr<PluginTimer> protector(this);

        if (!m_unscheduled)
            m_timerFunc(m_instance, m_timerID);

        // remove the timer if it is a one-shot timer (!m_repeat) or if is a
        // repeating timer that has been unscheduled. In either case we must
        // ensure that the refcount is 2 or greater since the PluginTimerList
        // could have been deleted by the timerFunc and we must ensure that we
        // do not double delete.
        if ((!m_repeat || m_unscheduled) && refCount() > 1)
            deref(); // mark the timer for deletion as it is no longer needed
    }
    
    // may return null if timerID is not found
    PluginTimer* PluginTimer::Find(PluginTimer* list, uint32_t timerID)
    {
        PluginTimer* curr = list;
        while (curr) {
            if (curr->m_timerID == timerID) {
                break;
            }
            curr = curr->m_next;
        }
        return curr;
    }

    ///////////////////////////////////////////////////////////////////////////

    PluginTimerList::~PluginTimerList()
    {
        PluginTimer* curr = m_list;
        PluginTimer* next;
        while (curr) {
            next = curr->next();
            curr->deref();
            curr = next;
        }
    }

    uint32_t PluginTimerList::schedule(NPP instance, uint32_t interval, bool repeat,
                                     void (*proc)(NPP npp, uint32_t timerID))
    {        
        PluginTimer* timer = new PluginTimer(&m_list, instance, repeat, proc);
        
        double dinterval = interval * 0.001;    // milliseconds to seconds
        if (repeat) {
            timer->startRepeating(dinterval);
        } else {
            timer->startOneShot(dinterval);
        }
        return timer->timerID();
    }
    
    void PluginTimerList::unschedule(NPP instance, uint32_t timerID)
    {
        // Although it looks like simply deleting the timer would work here
        // (stop() will be executed by the dtor), we cannot do this, as
        // the plugin can call us while we are in the fired() method,
        // (when we execute the timerFunc callback). Deleting the object
        // we are in would then be a rather bad move...
        PluginTimer* timer = PluginTimer::Find(m_list, timerID);
        if (timer)
            timer->unschedule();
    }
    
} // namespace WebCore
