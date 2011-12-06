/*
 * Copyright 2010, The Android Open Source Project
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
#include "TexturesGenerator.h"

#if USE(ACCELERATED_COMPOSITING)

#include "BaseLayerAndroid.h"
#include "GLUtils.h"
#include "PaintTileOperation.h"
#include "TilesManager.h"

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "TexturesGenerator", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

void TexturesGenerator::scheduleOperation(QueuedOperation* operation)
{
    {
        android::Mutex::Autolock lock(mRequestedOperationsLock);
        mRequestedOperations.append(operation);
    }
    mRequestedOperationsCond.signal();
}

void TexturesGenerator::removeOperationsForPage(TiledPage* page)
{
    removeOperationsForFilter(new PageFilter(page));
}

void TexturesGenerator::removePaintOperationsForPage(TiledPage* page, bool waitForRunning)
{
    removeOperationsForFilter(new PagePaintFilter(page), waitForRunning);
}

void TexturesGenerator::removeOperationsForFilter(OperationFilter* filter)
{
    removeOperationsForFilter(filter, true);
}

void TexturesGenerator::removeOperationsForFilter(OperationFilter* filter, bool waitForRunning)
{
    if (!filter)
        return;

    android::Mutex::Autolock lock(mRequestedOperationsLock);
    for (unsigned int i = 0; i < mRequestedOperations.size();) {
        QueuedOperation* operation = mRequestedOperations[i];
        if (filter->check(operation)) {
            mRequestedOperations.remove(i);
            delete operation;
        } else {
            i++;
        }
    }

    if (waitForRunning && m_currentOperation) {
        QueuedOperation* operation = m_currentOperation;

        if (operation && filter->check(operation)) {
            m_waitForCompletion = true;
            // The reason we are signaling the transferQueue is :
            // TransferQueue may be waiting a slot to work on, but now UI
            // thread is waiting for Tex Gen thread to finish first before the
            // UI thread can free a slot for the transferQueue.
            // Therefore, it could be a deadlock.
            // The solution is use this as a flag to tell Tex Gen thread that
            // UI thread is waiting now, Tex Gen thread should not wait for the
            // queue any more.
            TilesManager::instance()->transferQueue()->interruptTransferQueue(true);
        }

        delete filter;

        // At this point, it means that we are currently executing an operation that
        // we want to be removed -- we should wait until it is done, so that
        // when we return our caller can be sure that there is no more operations
        // in the queue matching the given filter.
        while (m_waitForCompletion)
            mRequestedOperationsCond.wait(mRequestedOperationsLock);
    } else {
        delete filter;
    }
}

status_t TexturesGenerator::readyToRun()
{
    TilesManager::instance()->markGeneratorAsReady();
    XLOG("Thread ready to run");
    return NO_ERROR;
}

// Must be called from within a lock!
QueuedOperation* TexturesGenerator::popNext()
{
    // Priority can change between when it was added and now
    // Hence why the entire queue is rescanned
    QueuedOperation* current = mRequestedOperations.last();
    int currentPriority = current->priority();
    if (currentPriority < 0) {
        mRequestedOperations.removeLast();
        return current;
    }
    int currentIndex = mRequestedOperations.size() - 1;
    // Scan from the back to make removing faster (less items to copy)
    for (int i = mRequestedOperations.size() - 2; i >= 0; i--) {
        QueuedOperation *next = mRequestedOperations[i];
        int nextPriority = next->priority();
        if (nextPriority < 0) {
            // Found a very high priority item, go ahead and just handle it now
            mRequestedOperations.remove(i);
            return next;
        }
        // pick items preferrably by priority, or if equal, by order of
        // insertion (as we add items at the back of the queue)
        if (nextPriority <= currentPriority) {
            current = next;
            currentPriority = nextPriority;
            currentIndex = i;
        }
    }
    mRequestedOperations.remove(currentIndex);
    return current;
}

bool TexturesGenerator::threadLoop()
{
    // Check if we have any pending operations.
    mRequestedOperationsLock.lock();
    while (!mRequestedOperations.size())
        mRequestedOperationsCond.wait(mRequestedOperationsLock);

    XLOG("threadLoop, got signal");
    mRequestedOperationsLock.unlock();

    m_currentOperation = 0;
    bool stop = false;
    while (!stop) {
        mRequestedOperationsLock.lock();
        XLOG("threadLoop, %d operations in the queue", mRequestedOperations.size());
        if (mRequestedOperations.size())
            m_currentOperation = popNext();
        mRequestedOperationsLock.unlock();

        if (m_currentOperation) {
            XLOG("threadLoop, painting the request with priority %d", m_currentOperation->priority());
            m_currentOperation->run();
        }

        QueuedOperation* oldOperation = m_currentOperation;
        mRequestedOperationsLock.lock();
        if (m_currentOperation)
            m_currentOperation = 0;
        if (!mRequestedOperations.size())
            stop = true;
        if (m_waitForCompletion) {
            m_waitForCompletion = false;
            TilesManager::instance()->transferQueue()->interruptTransferQueue(false);
            mRequestedOperationsCond.signal();
        }
        mRequestedOperationsLock.unlock();
        if (oldOperation)
            delete oldOperation; // delete outside lock
    }
    XLOG("threadLoop empty");

    return true;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
