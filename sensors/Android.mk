LOCAL_PATH:= $(call my-dir)

mm_sensor_includes:= \
	hardware/qcom/camera/QCamera2/stack/common \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include

include $(CLEAR_VARS)
LOCAL_MODULE := libmmdaemon_s5k4e1gx
LOCAL_SRC_FILES := mm_daemon_sensor_s5k4e1gx.c
LOCAL_C_INCLUDES := $(mm_sensor_includes)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libmmdaemon_mt9v113
LOCAL_SRC_FILES := mm_daemon_sensor_mt9v113.c
LOCAL_C_INCLUDES := $(mm_sensor_includes)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libmmdaemon_imx105
LOCAL_SRC_FILES := mm_daemon_sensor_imx105.c
LOCAL_C_INCLUDES := $(mm_sensor_includes)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
