ifeq ($(TARGET_USE_MM_OPEN_DAEMON),true)
  LOCAL_PATH:= $(call my-dir)

  include $(CLEAR_VARS)

  MMDAEMON_FILES:=	\
	mm_daemon.c	\
	mm_daemon_config.c \
	mm_daemon_sensor.c

  LOCAL_SRC_FILES := $(MMDAEMON_FILES)

  LOCAL_SHARED_LIBRARIES := \
	libutils	\
	libcutils

  LOCAL_C_INCLUDES += \
	hardware/qcom/camera/QCamera2/stack/common

  LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include

  LOCAL_MODULE := mm_daemon

  LOCAL_MODULE_TAGS := optional

  LOCAL_CFLAGS = -Wall -Werror

  include $(BUILD_EXECUTABLE)
endif
