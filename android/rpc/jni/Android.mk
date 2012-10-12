
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := test

LOCAL_C_INCLUDES := .

LOCAL_LDLIBS := -lz

LOCAL_SRC_FILES := ../../../src/and_rpc_clnt.c \
                   ../../../src/tpl.c \
                   ../../../src/ocl_utils.c \
                   ../../../src/hmm.c

include $(BUILD_EXECUTABLE)
