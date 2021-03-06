ifeq ($(PRODUCT_MANUFACTURER), $(filter $(PRODUCT_MANUFACTURER), freescale nxp))
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := true

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_PACKAGE_NAME := CactusPlayer

LOCAL_SDK_VERSION := system_current

LOCAL_CERTIFICATE := platform

include $(BUILD_PACKAGE)

# Use the folloing include to make our test apk.
include $(call all-makefiles-under,$(LOCAL_PATH))

endif
