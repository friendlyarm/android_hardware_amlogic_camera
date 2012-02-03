ifneq ($(strip $(USE_CAMERA_STUB)),true)

LOCAL_PATH:= $(call my-dir)

CAMERA_HAL_SRC := \
	CameraHal_Module.cpp \
	CameraHal.cpp \
	CameraHalUtilClasses.cpp \
	AppCallbackNotifier.cpp \
	ANativeWindowDisplayAdapter.cpp \
	CameraProperties.cpp \
	MemoryManager.cpp \
	Encoder_libjpeg.cpp \
	SensorListener.cpp  \
	NV12_resize.c

CAMERA_COMMON_SRC:= \
	CameraParameters.cpp \
	TICameraParameters.cpp \
	CameraHalCommon.cpp

CAMERA_V4L_SRC:= \
	BaseCameraAdapter.cpp \
	V4LCameraAdapter/V4LCameraAdapter.cpp

CAMERA_UTILS_SRC:= \
	utils/ErrorUtils.cpp \
	utils/MessageQueue.cpp \
	utils/Semaphore.cpp

ifeq ($(BOARD_USE_USB_CAMERA),true)
    CAMERA_UTILS_SRC += utils/util.cpp
endif

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	$(CAMERA_HAL_SRC) \
	$(CAMERA_V4L_SRC) \
	$(CAMERA_COMMON_SRC) \
	$(CAMERA_UTILS_SRC)

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/inc/ \
    $(LOCAL_PATH)/utils \
    $(LOCAL_PATH)/inc/V4LCameraAdapter \
    frameworks/base/include/ui \
    frameworks/base/include/utils \
    frameworks/base/include/media/stagefright \
    external/jhead/ \
    external/jpeg/ \
    hardware/libhardware/modules/gralloc/

LOCAL_SHARED_LIBRARIES:= \
    libui \
    libbinder \
    libutils \
    libcutils \
    libcamera_client \
    libexif \
    libjpeg \
    libgui

LOCAL_CFLAGS := -fno-short-enums -DCOPY_IMAGE_BUFFER

ifeq ($(BOARD_HAVE_FRONT_CAM),true)
    LOCAL_CFLAGS += -DAMLOGIC_FRONT_CAMERA_SUPPORT
endif

ifeq ($(BOARD_HAVE_BACK_CAM),true)
    LOCAL_CFLAGS += -DAMLOGIC_BACK_CAMERA_SUPPORT
endif

ifeq ($(BOARD_USE_USB_CAMERA),true)
    LOCAL_CFLAGS += -DAMLOGIC_USB_CAMERA_SUPPORT
else
    ifeq ($(BOARD_HAVE_MULTI_CAMERAS),true)
        LOCAL_CFLAGS += -DAMLOGIC_MULTI_CAMERA_SUPPORT
    endif
    ifeq ($(BOARD_HAVE_FLASHLIGHT),true)
        LOCAL_CFLAGS += -DAMLOGIC_FLASHLIGHT_SUPPORT
    endif
endif

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE:= camera.amlogic
LOCAL_MODULE_TAGS:= optional

#include $(BUILD_HEAPTRACKED_SHARED_LIBRARY)
include $(BUILD_SHARED_LIBRARY)
endif
