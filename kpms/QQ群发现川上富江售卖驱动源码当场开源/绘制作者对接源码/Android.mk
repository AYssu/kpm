LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := AmeliaPro_Driver.sh

LOCAL_CFLAGS := -w -s -Wno-error=format-security -fvisibility=hidden -fpermissive -fexceptions
LOCAL_LDFLAGS += -Wl,--gc-sections,--strip-all,-llog
LOCAL_CPPFLAGS += -w -s -Wno-error=format-security -fvisibility=hidden -Werror -std=c++17 -O2
LOCAL_CPPFLAGS += -Wno-error=c++11-narrowing -fpermissive -Wall -fexceptions
LOCAL_CFLAGS := -std=c++17

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

FILE_LIST += $(wildcard $(LOCAL_PATH)/src/*.c*)

LOCAL_SRC_FILES := $(FILE_LIST:$(LOCAL_PATH)/%=%)

LOCAL_LDLIBS += $(LOCAL_PATH)/lib/libAmeliaPro.a

include $(BUILD_EXECUTABLE)