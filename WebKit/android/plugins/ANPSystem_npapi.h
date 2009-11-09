#ifndef ANPSystem_npapi_H
#define ANPSystem_npapi_H

#include "android_npapi.h"
#include <jni.h>

struct ANPSystemInterfaceV0 : ANPInterface {
    /** Return the path name for the current Application's plugin data directory,
        or NULL if not supported
     */
    const char* (*getApplicationDataDirectory)();

    /** A helper function to load java classes from the plugin's apk.  The
        function looks for a class given the fully qualified and null terminated
        string representing the className. For example,

        const char* className = "com.android.mypackage.MyClass";

        If the class cannot be found or there is a problem loading the class
        NULL will be returned.
     */
    jclass (*loadJavaClass)(NPP instance, const char* className);
};

#endif //ANPSystem_npapi_H
