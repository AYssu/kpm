LOCAL_PATH := $(call my-dir)

# ========== FastScan KPM 驱动用户态静态库 (ioctl 版本) ==========
include $(CLEAR_VARS)

LOCAL_MODULE := kpmdriver
LOCAL_SRC_FILES := KPmTearGame.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_CFLAGS := -Wall -O2
LOCAL_CPPFLAGS := -std=c++11 -fexceptions -frtti
LOCAL_LDLIBS := -llog

include $(BUILD_STATIC_LIBRARY)

# ========== 预编译静态库: libDriverTear.a (可选，如果文件存在则启用) ==========
# 如果 libDriverTear.a 不存在，请注释掉以下部分
# include $(CLEAR_VARS)
# LOCAL_MODULE := tear
# LOCAL_SRC_FILES := libDriverTear.a
# include $(PREBUILT_STATIC_LIBRARY)

# ========== 可执行文件: teartest (可选，如果 tearusermain.cpp 存在则启用) ==========
# 如果 tearusermain.cpp 不存在，请注释掉以下部分
# include $(CLEAR_VARS)
# LOCAL_MODULE := teartest
# LOCAL_SRC_FILES := tearusermain.cpp
# LOCAL_C_INCLUDES := $(LOCAL_PATH) \
#                     $(LOCAL_PATH)/../../

# LOCAL_CFLAGS := -w -O3
# LOCAL_CPPFLAGS := -std=c++20
# LOCAL_STATIC_LIBRARIES := kpmdriver
# LOCAL_LDLIBS := -llog

# include $(BUILD_EXECUTABLE)

# ========== 示例可执行文件: kpm_example (ioctl 版本) ==========
include $(CLEAR_VARS)

LOCAL_MODULE := kpm_example
LOCAL_SRC_FILES := KPmTearGame.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_CFLAGS := -Wall -O2 -DBUILD_EXAMPLE_PROGRAM
LOCAL_CPPFLAGS := -std=c++11 -fexceptions -frtti
LOCAL_LDLIBS := -llog

include $(BUILD_EXECUTABLE)
