/*
 * Copyright 2007, The Android Open Source Project
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

#ifndef JavaSharedClient_h
#define JavaSharedClient_h

namespace android {

    class TimerClient;
    class CookieClient;
    class PluginClient;
    class KeyGeneratorClient;
    class FileSystemClient;

    class JavaSharedClient
    {
    public:
        static TimerClient* GetTimerClient(); 
        static CookieClient* GetCookieClient();
        static PluginClient* GetPluginClient();
        static KeyGeneratorClient* GetKeyGeneratorClient();
        static FileSystemClient* GetFileSystemClient();

        static void SetTimerClient(TimerClient* client);
        static void SetCookieClient(CookieClient* client);
        static void SetPluginClient(PluginClient* client);
        static void SetKeyGeneratorClient(KeyGeneratorClient* client);
        static void SetFileSystemClient(FileSystemClient* client);

        // can be called from any thread, to be executed in webkit thread
        static void EnqueueFunctionPtr(void (*proc)(void*), void* payload);
        // only call this from webkit thread
        static void ServiceFunctionPtrQueue();

    private:
        static TimerClient* gTimerClient;
        static CookieClient* gCookieClient;
        static PluginClient* gPluginClient;
        static KeyGeneratorClient* gKeyGeneratorClient;
        static FileSystemClient* gFileSystemClient;
    };
}
#endif
