
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := /opt/AMDAPP/include/

LOCAL_MODULE := test

LOCAL_SRC_FILES := and_rpc_clnt.c tpl.c
include $(BUILD_EXECUTABLE)
