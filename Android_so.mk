LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := cgamearm
LOCAL_SRC_FILES := armeabi/libcgamearm.so

include $(PREBUILT_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE := qaamearm
LOCAL_SRC_FILES := armeabi/libqagamearm.so

include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := uiarm
LOCAL_SRC_FILES := armeabi/libuiarm.so

include $(PREBUILT_SHARED_LIBRARY)