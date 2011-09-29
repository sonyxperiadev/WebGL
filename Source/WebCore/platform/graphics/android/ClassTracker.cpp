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
#include "ClassTracker.h"

#include "LayerAndroid.h"

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "ClassTracker", __VA_ARGS__)

namespace WebCore {

ClassTracker* ClassTracker::instance()
{
    if (!gInstance)
        gInstance = new ClassTracker();
    return gInstance;
}

ClassTracker* ClassTracker::gInstance = 0;

void ClassTracker::increment(String name)
{
   android::Mutex::Autolock lock(m_lock);
   int value = 0;
   if (m_classes.contains(name))
       value = m_classes.get(name);

   m_classes.set(name, value + 1);
}

void ClassTracker::decrement(String name)
{
   android::Mutex::Autolock lock(m_lock);
   int value = 0;
   if (m_classes.contains(name))
       value = m_classes.get(name);

   m_classes.set(name, value - 1);
}


void ClassTracker::add(LayerAndroid* layer)
{
   android::Mutex::Autolock lock(m_lock);
   m_layers.append(layer);
}

void ClassTracker::remove(LayerAndroid* layer)
{
   android::Mutex::Autolock lock(m_lock);
   m_layers.remove(m_layers.find(layer));
}

void ClassTracker::show()
{
   android::Mutex::Autolock lock(m_lock);
   XLOG("*** Tracking %d classes ***", m_classes.size());
   for (HashMap<String, int>::iterator iter = m_classes.begin(); iter != m_classes.end(); ++iter) {
       XLOG("class %s has %d instances",
            iter->first.latin1().data(), iter->second);
   }
   XLOG("*** %d Layers ***", m_layers.size());
   for (unsigned int i = 0; i < m_layers.size(); i++) {
       LayerAndroid* layer = m_layers[i];
       XLOG("[%d/%d] layer %x (%.2f, %.2f) of type %d, refcount(%d) has texture %x has image ref %x (%x) root: %x parent: %x",
            i, m_layers.size(), layer,
            layer->getWidth(), layer->getHeight(),
            layer->type(), layer->getRefCnt(),
            layer->texture(), layer->imageRef(),
            layer->imageTexture(), (LayerAndroid*) layer->getRootLayer(),
            (LayerAndroid*) layer->getParent());
   }
}

} // namespace WebCore
