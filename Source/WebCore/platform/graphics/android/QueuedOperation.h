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

#ifndef QueuedOperation_h
#define QueuedOperation_h

namespace WebCore {

class TiledPage;

class QueuedOperation {
public:
    enum OperationType { Undefined, PaintTile, PaintLayer, DeleteTexture };
    QueuedOperation(OperationType type, TiledPage* page)
        : m_type(type)
        , m_page(page) {}
    virtual ~QueuedOperation() {}
    virtual void run() = 0;
    virtual bool operator==(const QueuedOperation* operation) = 0;
    virtual int priority() { return -1; }
    OperationType type() const { return m_type; }
    TiledPage* page() const { return m_page; }
private:
    OperationType m_type;
    TiledPage* m_page;
};

class OperationFilter {
public:
    virtual ~OperationFilter() {}
    virtual bool check(QueuedOperation* operation) = 0;
};

class PageFilter : public OperationFilter {
public:
    PageFilter(TiledPage* page) : m_page(page) {}
    virtual bool check(QueuedOperation* operation)
    {
        if (operation->page() == m_page)
            return true;
        return false;
    }
private:
    TiledPage* m_page;
};

class PagePaintFilter : public OperationFilter {
public:
    PagePaintFilter(TiledPage* page) : m_page(page) {}
    virtual bool check(QueuedOperation* operation)
    {
        if (operation->type() == QueuedOperation::PaintTile
                && operation->page() == m_page)
            return true;
        return false;
    }
private:
    TiledPage* m_page;
};

}

#endif // QueuedOperation_h
