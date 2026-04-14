
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libmedia-buffers-memory
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Media buffers memory library
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
# Public API headers - top level headers first
# This header list is currently used to generate a python binding
LOCAL_EXPORT_CUSTOM_VARIABLES := LIBMEDIABUFFERSMEMORY_HEADERS=$\
	$(LOCAL_PATH)/include/media-buffers/mbuf_mem.h;
LOCAL_CFLAGS := -DMBUF_API_EXPORTS -fvisibility=hidden -std=gnu11
LOCAL_SRC_FILES := \
	src/mbuf_mem_implem.c \
	src/mbuf_mem.c
LOCAL_LIBRARIES := \
	libfutils \
	libpomp \
	libulog

ifeq ($(CONFIG_ALCHEMY_BUILD_LIBMEDIA_BUFFERS_MEMORY_ION),y)
LOCAL_CFLAGS += -DBUILD_LIBMEDIA_BUFFERS_MEMORY_ION
endif
ifeq ($(CONFIG_ALCHEMY_BUILD_LIBMEDIA_BUFFERS_MEMORY_VACQ),y)
LOCAL_CFLAGS += -DBUILD_LIBMEDIA_BUFFERS_MEMORY_VACQ
endif
ifeq ($(CONFIG_ALCHEMY_BUILD_LIBMEDIA_BUFFERS_MEMORY_HISI),y)
LOCAL_CFLAGS += -DBUILD_LIBMEDIA_BUFFERS_MEMORY_HISI
endif
ifeq ($(CONFIG_ALCHEMY_BUILD_LIBMEDIA_BUFFERS_MEMORY_GENERIC),y)
LOCAL_CFLAGS += -DBUILD_LIBMEDIA_BUFFERS_MEMORY_GENERIC
endif

include $(BUILD_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE := libmedia-buffers-memory-internal
LOCAL_DESCRIPTION := Media buffers memory internal headers, for custom implementations
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/src/internal/

include $(BUILD_CUSTOM)


include $(CLEAR_VARS)

LOCAL_MODULE := libmedia-buffers-memory-generic
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Media buffers generic memory implementation
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/implem/generic/include
# Public API headers - top level headers first
# This header list is currently used to generate a python binding
LOCAL_EXPORT_CUSTOM_VARIABLES := LIBMEDIABUFFERSMEMORYGENERIC_HEADERS=$\
	$(LOCAL_PATH)/implem/generic/include/media-buffers/mbuf_mem_generic.h;
LOCAL_CFLAGS := -DMBUF_API_EXPORTS -fvisibility=hidden -std=gnu11
LOCAL_SRC_FILES := \
	implem/generic/src/mbuf_mem_generic.c
LOCAL_LIBRARIES := \
	libmedia-buffers-memory \
	libmedia-buffers-memory-internal \
	libulog

include $(BUILD_LIBRARY)


ifeq ($(TARGET_OS),$(filter %$(TARGET_OS),linux darwin))
ifneq ("$(TARGET_OS_FLAVOUR)", "android")

include $(CLEAR_VARS)

LOCAL_MODULE := libmedia-buffers-memory-shm
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Media buffers SHM memory implementation
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/implem/shm/include
LOCAL_CFLAGS := -DMBUF_API_EXPORTS -fvisibility=hidden -std=gnu11
LOCAL_SRC_FILES := \
	implem/shm/src/mbuf_mem_shm.c
LOCAL_LIBRARIES := \
	libmedia-buffers-memory \
	libmedia-buffers-memory-internal \
	libulog \

include $(BUILD_LIBRARY)

endif
endif


include $(CLEAR_VARS)

LOCAL_MODULE := libmedia-buffers-memory-pbo
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Media buffers Pixel Buffer Object memory implementation based on OpenGL
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/implem/pbo/include
LOCAL_CFLAGS := -DMBUF_API_EXPORTS -fvisibility=hidden -std=gnu11
LOCAL_SRC_FILES := \
	implem/pbo/src/mbuf_mem_pbo.c
LOCAL_LIBRARIES := \
	libmedia-buffers-memory \
	libmedia-buffers-memory-internal \
	libulog
ifeq ($(TARGET_CPU),$(filter %$(TARGET_CPU),s905d3 s905x3))
  LOCAL_LDLIBS += -lGLESv2 -lEGL
  LOCAL_LIBRARIES += \
	am-gpu
else ifeq ($(TARGET_CPU),$(filter %$(TARGET_CPU),qrb5165 qcs405))
  LOCAL_LIBRARIES += \
	glesv2
else ifeq ("$(TARGET_OS)-$(TARGET_OS_FLAVOUR)","linux-native")
  LOCAL_LIBRARIES += \
	opengles
else ifeq ("$(TARGET_OS)-$(TARGET_OS_FLAVOUR)","linux-native-chroot")
  LOCAL_LIBRARIES += \
	opengles
else ifeq ("$(TARGET_OS)-$(TARGET_OS_FLAVOUR)","linux-android")
  LOCAL_LDLIBS += -lGLESv2 -lEGL -landroid
else ifeq ("$(TARGET_OS)-$(TARGET_OS_FLAVOUR)","darwin-native")
  LOCAL_CFLAGS += \
	-DGL_SILENCE_DEPRECATION
  LOCAL_LDLIBS += \
	-framework OpenGL
else ifeq ($(TARGET_OS_FLAVOUR),$(filter %$(TARGET_OS_FLAVOUR),iphoneos iphonesimulator))
  LOCAL_CFLAGS += \
	-DGLES_SILENCE_DEPRECATION
  LOCAL_LDLIBS += \
	-framework OpenGLES
else ifeq ("$(TARGET_OS)","windows")
  LOCAL_CFLAGS += -DEPOXY_SHARED
  LOCAL_LDLIBS += -lepoxy
endif

include $(BUILD_LIBRARY)


ifeq ("${TARGET_OS}", "darwin")
include $(CLEAR_VARS)

LOCAL_MODULE := libmedia-buffers-memory-cvpixelbuffer
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Media buffers CVPixelBuffer memory implementation for Apple targets
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/implem/cvpixelbuffer/include
LOCAL_CFLAGS := -DMBUF_API_EXPORTS -fvisibility=hidden -std=gnu11
LOCAL_SRC_FILES := \
	implem/cvpixelbuffer/src/mbuf_mem_cvpixelbuffer.c
LOCAL_LIBRARIES := \
	libmedia-buffers-memory \
	libmedia-buffers-memory-internal \
	libulog
  LOCAL_LDLIBS += \
	-framework Foundation \
	-framework CoreVideo

include $(BUILD_LIBRARY)
endif


include $(CLEAR_VARS)

LOCAL_MODULE := libmedia-buffers
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Media buffers library
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
# Public API headers - top level headers first
# This header list is currently used to generate a python binding
LOCAL_EXPORT_CUSTOM_VARIABLES := LIBMEDIABUFFERS_HEADERS=$\
	$(LOCAL_PATH)/include/media-buffers/mbuf_ancillary_data.h:$\
	$(LOCAL_PATH)/include/media-buffers/mbuf_audio_frame.h:$\
	$(LOCAL_PATH)/include/media-buffers/mbuf_coded_video_frame.h:$\
	$(LOCAL_PATH)/include/media-buffers/mbuf_raw_video_frame.h;
LOCAL_CFLAGS := -DMBUF_API_EXPORTS -fvisibility=hidden -std=gnu11 -D_GNU_SOURCE
LOCAL_SRC_FILES := \
	src/mbuf_ancillary_data.c \
	src/mbuf_audio_frame.c \
	src/mbuf_base_frame.c \
	src/mbuf_coded_video_frame.c \
	src/mbuf_raw_video_frame.c \
	src/mbuf_utils.c
LOCAL_LIBRARIES := \
	libaudio-defs \
	libfutils \
	libmedia-buffers-memory \
	libpomp \
	libulog \
	libvideo-defs \
	libvideo-metadata

include $(BUILD_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE := libmedia-buffers-cpp
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Media buffers library (c++)
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
# Public API headers - top level headers first
# This header list is currently used to generate a python binding
LOCAL_EXPORT_CUSTOM_VARIABLES := LIBMEDIABUFFERS_HEADERS=$\
	$(LOCAL_PATH)/include/media-buffers/mbuf_queue.hpp;
LOCAL_EXPORT_CXXFLAGS := -std=c++11
LOCAL_CFLAGS := -DMBUF_API_EXPORTS -fvisibility=hidden -std=gnu11 -D_GNU_SOURCE
LOCAL_SRC_FILES := \
	src/mbuf_queue.cpp
LOCAL_LIBRARIES := \
	libaudio-defs \
	libfutils \
	libmedia-buffers \
	libmedia-buffers-memory \
	libpomp \
	libulog \
	libvideo-defs \
	libvideo-metadata

include $(BUILD_LIBRARY)


ifdef TARGET_TEST

include $(CLEAR_VARS)

LOCAL_MODULE := tst-libmedia-buffers
LOCAL_LIBRARIES := \
	libaudio-defs \
	libcunit\
	libmedia-buffers \
	libmedia-buffers-cpp \
	libmedia-buffers-memory \
	libmedia-buffers-memory-generic \
	libpomp \
	libulog \
	libvideo-defs \
	libvideo-metadata
LOCAL_CFLAGS := -std=gnu11 -D_GNU_SOURCE
LOCAL_SRC_FILES := \
	tests/mbuf_ancillary_test.c \
	tests/mbuf_audio_frame_test.c \
	tests/mbuf_coded_video_frame_test.c \
	tests/mbuf_implem_test.c \
	tests/mbuf_pool_test.c \
	tests/mbuf_queue_test.cpp \
	tests/mbuf_raw_video_frame_test.c \
	tests/mbuf_test.c \
	tests/mbuf_wrap_test.c

include $(BUILD_EXECUTABLE)

endif
