/*
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#define LOG_TAG "favicons"

#include <config.h>
#include <wtf/Platform.h>

#include "WebIconDatabase.h"

#include "IconDatabase.h"
#include "Image.h"
#include "IntRect.h"
#include "JavaSharedClient.h"
#include "jni_utility.h"
#include "KURL.h"
#include "WebCoreJni.h"

#include <pthread.h>
#include "GraphicsJNI.h"
#include <SkBitmap.h>
#include <SkImageDecoder.h>
#include <SkTemplates.h>
#include <utils/misc.h>
#include <JNIHelp.h>

namespace android {

jobject webcoreImageToJavaBitmap(JNIEnv* env, WebCore::Image* icon)
{
    if (!icon)
        return NULL;
    SkBitmap bm;
    WebCore::SharedBuffer* buffer = icon->data();
    if (!buffer || !SkImageDecoder::DecodeMemory(buffer->data(), buffer->size(),
                                                 &bm, SkBitmap::kNo_Config,
                                            SkImageDecoder::kDecodePixels_Mode))
        return NULL;

    return GraphicsJNI::createBitmap(env, new SkBitmap(bm), false, NULL);
}

static WebIconDatabase* gIconDatabaseClient = new WebIconDatabase();

// XXX: Called by the IconDatabase thread
void WebIconDatabase::dispatchDidAddIconForPageURL(const WebCore::String& pageURL)
{
    // If there are no clients currently, drop this message.
    if (mClients.size() == 0)
        return;

    // Attempt to attach to the current vm.
    JavaVM* vm = JSC::Bindings::getJavaVM();
    JavaVMAttachArgs args;

    args.version = JNI_VERSION_1_4;
    args.name = "IconDatabase";
    args.group = NULL;

    JNIEnv* env;
    bool threadIsAttached = true;
    if (vm->AttachCurrentThread(&env, (void*) &args) != JNI_OK) {
        LOGE("Could not attach IconDatabase thread to the VM");
        threadIsAttached = false;
    }

    mNotificationsMutex.lock();
    mNotifications.append(pageURL);
    if (!mDeliveryRequested) {
        if (threadIsAttached) {
            mDeliveryRequested = true;
            WebCore::JavaSharedClient::EnqueueFunctionPtr(DeliverNotifications, this);
        }
    }
    mNotificationsMutex.unlock();
}

// Called in the WebCore thread
void WebIconDatabase::RegisterForIconNotification(WebIconDatabaseClient* client)
{
    gIconDatabaseClient->mClientsMutex.lock();
    gIconDatabaseClient->mClients.append(client);
    gIconDatabaseClient->mClientsMutex.unlock();
}

// Called in the WebCore thread
void WebIconDatabase::UnregisterForIconNotification(WebIconDatabaseClient* client)
{
    WebIconDatabase* db = gIconDatabaseClient;
    db->mClientsMutex.lock();
    for (unsigned i = 0; i < db->mClients.size(); ++i) {
        if (db->mClients[i] == client) {
            db->mClients.remove(i);
            break;
        }
    }
    db->mClientsMutex.unlock();
}

// Called in the WebCore thread
void WebIconDatabase::DeliverNotifications(void* v)
{
    ASSERT(v);
    ((WebIconDatabase*)v)->deliverNotifications();
}

// Called in the WebCore thread
void WebIconDatabase::deliverNotifications()
{
    ASSERT(mDeliveryRequested);

    // Swap the notifications queue
    Vector<WebCore::String> queue;
    mNotificationsMutex.lock();
    queue.swap(mNotifications);
    mDeliveryRequested = false;
    mNotificationsMutex.unlock();

    // Swap the clients queue
    Vector<WebIconDatabaseClient*> clients;
    mClientsMutex.lock();
    clients.swap(mClients);
    mClientsMutex.unlock();

    for (unsigned i = 0; i < queue.size(); ++i) {
        for (unsigned j = 0; j < clients.size(); ++j) {
            clients[j]->didAddIconForPageUrl(queue[i]);
        }
    }
}

static void Open(JNIEnv* env, jobject obj, jstring path)
{
    WebCore::IconDatabase* iconDb = WebCore::iconDatabase();
    if (iconDb->isOpen())
        return;
    iconDb->setEnabled(true);
    iconDb->setClient(gIconDatabaseClient);
    LOG_ASSERT(path, "No path given to nativeOpen");
    WebCore::String pathStr = to_string(env, path);
    LOGV("Opening WebIconDatabase file '%s'", pathStr.latin1().data());
    bool res = iconDb->open(pathStr);
    if (!res)
        LOGE("Open failed!");
}

static void Close(JNIEnv* env, jobject obj)
{
    WebCore::iconDatabase()->close();
}

static void RemoveAllIcons(JNIEnv* env, jobject obj)
{
    LOGV("Removing all icons");
    WebCore::iconDatabase()->removeAllIcons();
}

static jobject IconForPageUrl(JNIEnv* env, jobject obj, jstring url)
{
    LOG_ASSERT(url, "No url given to iconForPageUrl");
    WebCore::String urlStr = to_string(env, url);

    WebCore::Image* icon = WebCore::iconDatabase()->iconForPageURL(urlStr,
            WebCore::IntSize(16, 16));
    LOGV("Retrieving icon for '%s' %p", urlStr.latin1().data(), icon);
    return webcoreImageToJavaBitmap(env, icon);
}

static void RetainIconForPageUrl(JNIEnv* env, jobject obj, jstring url)
{
    LOG_ASSERT(url, "No url given to retainIconForPageUrl");
    WebCore::String urlStr = to_string(env, url);

    LOGV("Retaining icon for '%s'", urlStr.latin1().data());
    WebCore::iconDatabase()->retainIconForPageURL(urlStr);
}

static void ReleaseIconForPageUrl(JNIEnv* env, jobject obj, jstring url)
{
    LOG_ASSERT(url, "No url given to releaseIconForPageUrl");
    WebCore::String urlStr = to_string(env, url);

    LOGV("Releasing icon for '%s'", urlStr.latin1().data());
    WebCore::iconDatabase()->releaseIconForPageURL(urlStr);
}

/*
 * JNI registration
 */
static JNINativeMethod gWebIconDatabaseMethods[] = {
    { "nativeOpen", "(Ljava/lang/String;)V",
        (void*) Open },
    { "nativeClose", "()V",
        (void*) Close },
    { "nativeRemoveAllIcons", "()V",
        (void*) RemoveAllIcons },
    { "nativeIconForPageUrl", "(Ljava/lang/String;)Landroid/graphics/Bitmap;",
        (void*) IconForPageUrl },
    { "nativeRetainIconForPageUrl", "(Ljava/lang/String;)V",
        (void*) RetainIconForPageUrl },
    { "nativeReleaseIconForPageUrl", "(Ljava/lang/String;)V",
        (void*) ReleaseIconForPageUrl }
};

int register_webicondatabase(JNIEnv* env)
{
    jclass webIconDB = env->FindClass("android/webkit/WebIconDatabase");
    LOG_ASSERT(webIconDB, "Unable to find class android.webkit.WebIconDatabase");

    return jniRegisterNativeMethods(env, "android/webkit/WebIconDatabase",
            gWebIconDatabaseMethods, NELEM(gWebIconDatabaseMethods));
}

}
