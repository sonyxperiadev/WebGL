/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8DOMMap.h"

#include "DOMData.h"
#include "DOMDataStore.h"
#include "DOMObjectsInclude.h"
#include "ScopedDOMDataStore.h"

namespace WebCore {

#ifdef MANUAL_MERGE_REQUIRED
// DOM binding algorithm:
//
// There are two kinds of DOM objects:
// 1. DOM tree nodes, such as Document, HTMLElement, ...
//    there classes implement TreeShared<T> interface;
// 2. Non-node DOM objects, such as CSSRule, Location, etc.
//    these classes implement a ref-counted scheme.
//
// A DOM object may have a JS wrapper object. If a tree node
// is alive, its JS wrapper must be kept alive even it is not
// reachable from JS roots.
// However, JS wrappers of non-node objects can go away if
// not reachable from other JS objects. It works like a cache.
//
// DOM objects are ref-counted, and JS objects are traced from
// a set of root objects. They can create a cycle. To break
// cycles, we do following:
//   Handles from DOM objects to JS wrappers are always weak,
// so JS wrappers of non-node object cannot create a cycle.
//   Before starting a global GC, we create a virtual connection
// between nodes in the same tree in the JS heap. If the wrapper
// of one node in a tree is alive, wrappers of all nodes in
// the same tree are considered alive. This is done by creating
// object groups in GC prologue callbacks. The mark-compact
// collector will remove these groups after each GC.
//
// DOM objects should be deref-ed from the owning thread, not the GC thread
// that does not own them. In V8, GC can kick in from any thread. To ensure
// that DOM objects are always deref-ed from the owning thread when running
// V8 in multi-threading environment, we do following:
// 1. Maintain a thread specific DOM wrapper map for each object map.
//    (We're using TLS support from WTF instead of base since V8Bindings
//     does not depend on base. We further assume that all child threads
//     running V8 instances are created by WTF and thus a destructor will
//     be called to clean up all thread specific data.)
// 2. When GC happens:
//    2.1. If the dead object is in GC thread's map, remove the JS reference
//         and deref the DOM object.
//    2.2. Otherwise, go through all thread maps to find the owning thread.
//         Remove the JS reference from the owning thread's map and move the
//         DOM object to a delayed queue. Post a task to the owning thread
//         to have it deref-ed from the owning thread at later time.
// 3. When a thread is tearing down, invoke a cleanup routine to go through
//    all objects in the delayed queue and the thread map and deref all of
//    them.

static void weakDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject);
static void weakNodeCallback(v8::Persistent<v8::Value> v8Object, void* domObject);

#if ENABLE(SVG)
static void weakSVGElementInstanceCallback(v8::Persistent<v8::Value> v8Object, void* domObject);

// SVG non-node elements may have a reference to a context node which should be notified when the element is change.
static void weakSVGObjectWithContextCallback(v8::Persistent<v8::Value> v8Object, void* domObject);
#endif

// This is to ensure that we will deref DOM objects from the owning thread, not the GC thread.
// The helper function will be scheduled by the GC thread to get called from the owning thread.
static void derefDelayedObjectsInCurrentThread(void*);

// A list of all ThreadSpecific DOM Data objects. Traversed during GC to find a thread-specific map that
// contains the object - so we can schedule the object to be deleted on the thread which created it.
class ThreadSpecificDOMData;
typedef WTF::Vector<ThreadSpecificDOMData*> DOMDataList;
static DOMDataList& domDataList()
{
    DEFINE_STATIC_LOCAL(DOMDataList, staticDOMDataList, ());
    return staticDOMDataList;
}

// Mutex to protect against concurrent access of DOMDataList.
static WTF::Mutex& domDataListMutex()
{
    DEFINE_STATIC_LOCAL(WTF::Mutex, staticDOMDataListMutex, ());
    return staticDOMDataListMutex;
}

class ThreadSpecificDOMData : Noncopyable {
public:
    enum DOMWrapperMapType {
        DOMNodeMap,
        DOMObjectMap,
        ActiveDOMObjectMap,
#if ENABLE(SVG)
        DOMSVGElementInstanceMap,
        DOMSVGObjectWithContextMap
#endif
    };

    typedef WTF::HashMap<void*, V8ClassIndex::V8WrapperType> DelayedObjectMap;

    template <class KeyType>
    class InternalDOMWrapperMap : public DOMWrapperMap<KeyType> {
    public:
        InternalDOMWrapperMap(v8::WeakReferenceCallback callback)
            : DOMWrapperMap<KeyType>(callback) { }

        virtual void forget(KeyType*);

        void forgetOnly(KeyType* object)
        {
            DOMWrapperMap<KeyType>::forget(object);
        }
    };

    ThreadSpecificDOMData()
        : m_domNodeMap(0)
        , m_domObjectMap(0)
        , m_activeDomObjectMap(0)
#if ENABLE(SVG)
        , m_domSvgElementInstanceMap(0)
        , m_domSvgObjectWithContextMap(0)
#endif
        , m_delayedProcessingScheduled(false)
        , m_isMainThread(WTF::isMainThread())
    {
        WTF::MutexLocker locker(domDataListMutex());
        domDataList().append(this);
    }

    virtual ~ThreadSpecificDOMData()
    {
        WTF::MutexLocker locker(domDataListMutex());
        domDataList().remove(domDataList().find(this));
    }

    void* getDOMWrapperMap(DOMWrapperMapType type)
    {
        switch (type) {
        case DOMNodeMap:
            return m_domNodeMap;
        case DOMObjectMap:
            return m_domObjectMap;
        case ActiveDOMObjectMap:
            return m_activeDomObjectMap;
#if ENABLE(SVG)
        case DOMSVGElementInstanceMap:
            return m_domSvgElementInstanceMap;
        case DOMSVGObjectWithContextMap:
            return m_domSvgObjectWithContextMap;
#endif
        }

        ASSERT_NOT_REACHED();
        return 0;
    }

    InternalDOMWrapperMap<Node>& domNodeMap() { return *m_domNodeMap; }
    InternalDOMWrapperMap<void>& domObjectMap() { return *m_domObjectMap; }
    InternalDOMWrapperMap<void>& activeDomObjectMap() { return *m_activeDomObjectMap; }
#if ENABLE(SVG)
    InternalDOMWrapperMap<SVGElementInstance>& domSvgElementInstanceMap() { return *m_domSvgElementInstanceMap; }
    InternalDOMWrapperMap<void>& domSvgObjectWithContextMap() { return *m_domSvgObjectWithContextMap; }
#endif

    DelayedObjectMap& delayedObjectMap() { return m_delayedObjectMap; }
    bool delayedProcessingScheduled() const { return m_delayedProcessingScheduled; }
    void setDelayedProcessingScheduled(bool value) { m_delayedProcessingScheduled = value; }
    bool isMainThread() const { return m_isMainThread; }

protected:
    InternalDOMWrapperMap<Node>* m_domNodeMap;
    InternalDOMWrapperMap<void>* m_domObjectMap;
    InternalDOMWrapperMap<void>* m_activeDomObjectMap;
#if ENABLE(SVG)
    InternalDOMWrapperMap<SVGElementInstance>* m_domSvgElementInstanceMap;
    InternalDOMWrapperMap<void>* m_domSvgObjectWithContextMap;
#endif

    // Stores all the DOM objects that are delayed to be processed when the owning thread gains control.
    DelayedObjectMap m_delayedObjectMap;

    // The flag to indicate if the task to do the delayed process has already been posted.
    bool m_delayedProcessingScheduled;

    bool m_isMainThread;
};

// This encapsulates thread-specific DOM data for non-main thread. All the maps in it are created dynamically.
class NonMainThreadSpecificDOMData : public ThreadSpecificDOMData {
public:
    NonMainThreadSpecificDOMData()
    {
        m_domNodeMap = new InternalDOMWrapperMap<Node>(&weakNodeCallback);
        m_domObjectMap = new InternalDOMWrapperMap<void>(weakDOMObjectCallback);
        m_activeDomObjectMap = new InternalDOMWrapperMap<void>(weakActiveDOMObjectCallback);
#if ENABLE(SVG)
        m_domSvgElementInstanceMap = new InternalDOMWrapperMap<SVGElementInstance>(weakSVGElementInstanceCallback);
        m_domSvgObjectWithContextMap = new InternalDOMWrapperMap<void>(weakSVGObjectWithContextCallback);
#endif
    }

    // This is called when WTF thread is tearing down.
    // We assume that all child threads running V8 instances are created by WTF.
    virtual ~NonMainThreadSpecificDOMData()
    {
        delete m_domNodeMap;
        delete m_domObjectMap;
        delete m_activeDomObjectMap;
#if ENABLE(SVG)
        delete m_domSvgElementInstanceMap;
        delete m_domSvgObjectWithContextMap;
#endif
    }
};

// This encapsulates thread-specific DOM data for the main thread. All the maps in it are static.
// This is because we are unable to rely on WTF::ThreadSpecificThreadExit to do the cleanup since
// the place that tears down the main thread can not call any WTF functions.
class MainThreadSpecificDOMData : public ThreadSpecificDOMData {
public:
    MainThreadSpecificDOMData()
        : m_staticDomNodeMap(weakNodeCallback)
        , m_staticDomObjectMap(weakDOMObjectCallback)
        , m_staticActiveDomObjectMap(weakActiveDOMObjectCallback)
#if ENABLE(SVG)
        , m_staticDomSvgElementInstanceMap(weakSVGElementInstanceCallback)
        , m_staticDomSvgObjectWithContextMap(weakSVGObjectWithContextCallback)
#endif
    {
        m_domNodeMap = &m_staticDomNodeMap;
        m_domObjectMap = &m_staticDomObjectMap;
        m_activeDomObjectMap = &m_staticActiveDomObjectMap;
#if ENABLE(SVG)
        m_domSvgElementInstanceMap = &m_staticDomSvgElementInstanceMap;
        m_domSvgObjectWithContextMap = &m_staticDomSvgObjectWithContextMap;
#endif
    }

private:
    InternalDOMWrapperMap<Node> m_staticDomNodeMap;
    InternalDOMWrapperMap<void> m_staticDomObjectMap;
    InternalDOMWrapperMap<void> m_staticActiveDomObjectMap;
#if ENABLE(SVG)
    InternalDOMWrapperMap<SVGElementInstance> m_staticDomSvgElementInstanceMap;
    InternalDOMWrapperMap<void> m_staticDomSvgObjectWithContextMap;
#endif
};

DEFINE_STATIC_LOCAL(WTF::ThreadSpecific<NonMainThreadSpecificDOMData>, threadSpecificDOMData, ());

template<typename T>
static void handleWeakObjectInOwningThread(ThreadSpecificDOMData::DOMWrapperMapType mapType, V8ClassIndex::V8WrapperType objectType, T*);

ThreadSpecificDOMData& getThreadSpecificDOMData()
#else // MANUAL_MERGE_REQUIRED
DOMDataStoreHandle::DOMDataStoreHandle()
    : m_store(new ScopedDOMDataStore(DOMData::getCurrent()))
#endif // MANUAL_MERGE_REQUIRED
{
}

DOMDataStoreHandle::~DOMDataStoreHandle()
{
}

DOMWrapperMap<Node>& getDOMNodeMap()
{
    // Nodes only exist on the main thread.
    return DOMData::getCurrentMainThread()->getStore().domNodeMap();
}

DOMWrapperMap<void>& getDOMObjectMap()
{
    return DOMData::getCurrent()->getStore().domObjectMap();
}

DOMWrapperMap<void>& getActiveDOMObjectMap()
{
    return DOMData::getCurrent()->getStore().activeDomObjectMap();
}

#if ENABLE(SVG)

DOMWrapperMap<SVGElementInstance>& getDOMSVGElementInstanceMap()
{
    return DOMData::getCurrent()->getStore().domSvgElementInstanceMap();
}

// Map of SVG objects with contexts to V8 objects
DOMWrapperMap<void>& getDOMSVGObjectWithContextMap()
{
    return DOMData::getCurrent()->getStore().domSvgObjectWithContextMap();
}

#endif // ENABLE(SVG)

static void removeAllDOMObjectsInCurrentThreadHelper()
{
    v8::HandleScope scope;

    // Deref all objects in the delayed queue.
    DOMData::getCurrent()->derefDelayedObjects();

    // The DOM objects with the following types only exist on the main thread.
    if (WTF::isMainThread()) {
        // Remove all DOM nodes.
        DOMData::removeObjectsFromWrapperMap<Node>(getDOMNodeMap());

#if ENABLE(SVG)
        // Remove all SVG element instances in the wrapper map.
        DOMData::removeObjectsFromWrapperMap<SVGElementInstance>(getDOMSVGElementInstanceMap());

        // Remove all SVG objects with context in the wrapper map.
        DOMData::removeObjectsFromWrapperMap<void>(getDOMSVGObjectWithContextMap());
#endif
    }

    // Remove all DOM objects in the wrapper map.
    DOMData::removeObjectsFromWrapperMap<void>(getDOMObjectMap());

    // Remove all active DOM objects in the wrapper map.
    DOMData::removeObjectsFromWrapperMap<void>(getActiveDOMObjectMap());
}

void removeAllDOMObjectsInCurrentThread()
{
    // Use the locker only if it has already been invoked before, as by worker thread.
    if (v8::Locker::IsActive()) {
        v8::Locker locker;
        removeAllDOMObjectsInCurrentThreadHelper();
    } else
        removeAllDOMObjectsInCurrentThreadHelper();
}


void visitDOMNodesInCurrentThread(DOMWrapperMap<Node>::Visitor* visitor)
{
    v8::HandleScope scope;

    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());
    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];
        if (!store->domData()->owningThread() == WTF::currentThread())
            continue;

        HashMap<Node*, v8::Object*>& map = store->domNodeMap().impl();
        for (HashMap<Node*, v8::Object*>::iterator it = map.begin(); it != map.end(); ++it)
            visitor->visitDOMWrapper(it->first, v8::Persistent<v8::Object>(it->second));
    }
}

void visitDOMObjectsInCurrentThread(DOMWrapperMap<void>::Visitor* visitor)
{
    v8::HandleScope scope;

    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());
    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];
        if (!store->domData()->owningThread() == WTF::currentThread())
            continue;

        HashMap<void*, v8::Object*> & map = store->domObjectMap().impl();
        for (HashMap<void*, v8::Object*>::iterator it = map.begin(); it != map.end(); ++it)
            visitor->visitDOMWrapper(it->first, v8::Persistent<v8::Object>(it->second));
    }
}

void visitActiveDOMObjectsInCurrentThread(DOMWrapperMap<void>::Visitor* visitor)
{
    v8::HandleScope scope;

    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());
    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];
        if (!store->domData()->owningThread() == WTF::currentThread())
            continue;

        HashMap<void*, v8::Object*>& map = store->activeDomObjectMap().impl();
        for (HashMap<void*, v8::Object*>::iterator it = map.begin(); it != map.end(); ++it)
            visitor->visitDOMWrapper(it->first, v8::Persistent<v8::Object>(it->second));
    }
}

#if ENABLE(SVG)

void visitDOMSVGElementInstancesInCurrentThread(DOMWrapperMap<SVGElementInstance>::Visitor* visitor)
{
    v8::HandleScope scope;

    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());
    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];
        if (!store->domData()->owningThread() == WTF::currentThread())
            continue;

        HashMap<SVGElementInstance*, v8::Object*> & map = store->domSvgElementInstanceMap().impl();
        for (HashMap<SVGElementInstance*, v8::Object*>::iterator it = map.begin(); it != map.end(); ++it)
            visitor->visitDOMWrapper(it->first, v8::Persistent<v8::Object>(it->second));
    }
}

void visitSVGObjectsInCurrentThread(DOMWrapperMap<void>::Visitor* visitor)
{
    v8::HandleScope scope;

    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());
    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];
        if (!store->domData()->owningThread() == WTF::currentThread())
            continue;

        HashMap<void*, v8::Object*>& map = store->domSvgObjectWithContextMap().impl();
        for (HashMap<void*, v8::Object*>::iterator it = map.begin(); it != map.end(); ++it)
            visitor->visitDOMWrapper(it->first, v8::Persistent<v8::Object>(it->second));
    }
}

#endif

} // namespace WebCore
