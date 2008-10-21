/* 
**
** Copyright 2007, The Android Open Source Project
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

#include "config.h"
#include "JavaSharedClient.h"
#include "CookieClient.h"

#define LOG_TAG "Cookies"
#undef LOG
#include "utils/Log.h"

namespace WebCore {

    class Document;

    void setCookies(Document* , const KURL& url, const KURL& policyBaseURL, const String& value)
    {
        if (JavaSharedClient::GetCookieClient())
            JavaSharedClient::GetCookieClient()->setCookies(url, policyBaseURL, value);
    }

    String cookies(const Document* , const KURL& url)
    { 
        if (JavaSharedClient::GetCookieClient())
            return JavaSharedClient::GetCookieClient()->cookies(url);
        return String();
    }

    bool cookiesEnabled(const Document* )
    {
        if (JavaSharedClient::GetCookieClient())
            return JavaSharedClient::GetCookieClient()->cookiesEnabled();
        return false;
    }

}

