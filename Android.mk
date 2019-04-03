#
# Copyright 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_OWNER := nexell

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += ./
LOCAL_CFLAGS += -Wno-unused-parameter
LOCAL_CPPFLAGS += -DLOG_TAG=\"hal_bootctrl\"
LOCAL_CPPFLAGS += -DMISC_PARTITION=\"${NEXELL_MISC_PARTITION}\"
LOCAL_SHARED_LIBRARIES := liblog libcutils libz
LOCAL_HEADER_LIBRARIES := libhardware_headers libsystem_headers
LOCAL_SRC_FILES := bootctrl.cpp
LOCAL_MODULE := bootctrl.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)
