
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := test

LOCAL_C_INCLUDES := .

LOCAL_SRC_FILES := ../../../src/and_rpc_clnt.c \
                   ../../../src/sobel_test.c \
                   ../../../src/tpl.c

include $(BUILD_EXECUTABLE)
