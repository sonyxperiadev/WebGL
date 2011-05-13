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
#include "PackageNotifier.h"

#if ENABLE(APPLICATION_INSTALLED)

#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

PackageNotifier::PackageNotifier()
        : m_onResultAvailable(0), m_isInitialized(false), m_timer(this, &PackageNotifier::timerFired)  { }

void PackageNotifier::setOnResultAvailable(Callback callback)
{
    m_onResultAvailable = callback;
}

void PackageNotifier::addPackageNames(const HashSet<String>& packageNames)
{
    if (!m_isInitialized)
        m_isInitialized = true;

    typedef HashSet<String>::const_iterator NamesIterator;
    for (NamesIterator iter = packageNames.begin(); iter != packageNames.end(); ++iter)
        m_packageNames.add(*iter);

    if (m_onResultAvailable)
        m_onResultAvailable();
}

void PackageNotifier::addPackageName(const String& packageName)
{
    ASSERT(m_isInitialized);
    m_packageNames.add(packageName);
}

void PackageNotifier::removePackageName(const String& packageName)
{
    ASSERT(m_isInitialized);
    m_packageNames.remove(packageName);
}

void PackageNotifier::requestPackageResult()
{
    if (!m_isInitialized || m_timer.isActive())
        return;

    m_timer.startOneShot(0);
}

void PackageNotifier::timerFired(Timer<PackageNotifier>*)
{
    m_timer.stop();
    if (m_onResultAvailable)
        m_onResultAvailable();
}

bool PackageNotifier::isPackageInstalled(const String& packageName)
{
    return m_packageNames.contains(packageName);
}

PackageNotifier& packageNotifier()
{
    AtomicallyInitializedStatic(PackageNotifier*, packageNotifier = new PackageNotifier);
    return *packageNotifier;
}

}

#endif // ENABLE(APPLICATION_INSTALLED)
