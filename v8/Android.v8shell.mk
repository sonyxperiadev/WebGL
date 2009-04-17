LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= samples/shell.cc
LOCAL_MODULE := v8shell 

LOCAL_CPP_EXTENSION := .cc

LOCAL_SHARED_LIBRARIES := libv8

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

include $(BUILD_EXECUTABLE)
