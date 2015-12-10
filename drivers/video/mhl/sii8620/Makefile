# drivers/video/mhl/sii8620/Makefile
# Makefile for the Silicon Image 8620 MHL TX device driver
#
# example invocations:
#	For regular Linux builds
#	make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- clean debug
#	make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- clean release
#	make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- clean clean
#
#	For Android driver builds - Specify different tool-chain and kernel revision
#	export PATH=~/rowboat-android/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin:$PATH
#	make ARCH=arm KERNELPATH=~/rowboat-android/kernel CROSS_COMPILE=arm-eabi- clean debug
#	make ARCH=arm KERNELPATH=~/rowboat-android/kernel CROSS_COMPILE=arm-eabi- clean release
#	make ARCH=arm KERNELPATH=~/rowboat-android/kernel CROSS_COMPILE=arm-eabi- clean clean
#
#	For Android 4.0.3 (ICS):
#	export PATH=~/rowboat-android/TI-Android-ICS-4.0.3_AM37x_3.0.0/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin:$PATH
#	make ARCH=arm KERNELPATH=~/rowboat-android/TI-Android-ICS-4.0.3_AM37x_3.0.0/kernel CROSS_COMPILE=arm-eabi- clean debug
#	make ARCH=arm KERNELPATH=~/rowboat-android/TI-Android-ICS-4.0.3_AM37x_3.0.0/kernel CROSS_COMPILE=arm-eabi- clean release
#	make ARCH=arm KERNELPATH=~/rowboat-android/TI-Android-ICS-4.0.3_AM37x_3.0.0/kernel CROSS_COMPILE=arm-eabi- clean clean
#
#	For Android 4.2.2 (JB) Linaro release 13.04 for PandaBoard:
#	export PATH=$PATH:~/Linaro-13.04/android/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.7-linaro/bin
#	make ARCH=arm KERNELPATH=~/Linaro-13.04/android/kernel/linaro/pandaboard CROSS_COMPILE=arm-eabi- clean debug
#	make ARCH=arm KERNELPATH=~/Linaro-13.04/android/kernel/linaro/pandaboard CROSS_COMPILE=arm-eabi- clean release
#	make ARCH=arm KERNELPATH=~/Linaro-13.04/android/kernel/linaro/pandaboard CROSS_COMPILE=arm-eabi- clean clean
#

#ifneq ($(KERNELRELEASE),)
ifeq ($(CONFIG_SEC_MHL_SII8620),y)
CONFIG_DEBUG_DRIVER=y
MHL_PRODUCT_NUM=8620
# kbuild part of makefile

#
# color annotations to use instead of leading newline chars
#ccflags-y += -DANSI_COLORS

ccflags-y += -DBUILD_NUM_STRING=\"$(MHL_BUILD_NUM)$(DEVELOPER_BUILD_ID)\"
ccflags-y += -DMHL_PRODUCT_NUM=$(MHL_PRODUCT_NUM)
ifeq ($(CONFIG_SEC_MHL_SII8620),y)
ccflags-y += -DMHL_DRIVER_NAME=\"sii$(MHL_PRODUCT_NUM)\"
else
ccflags-y += -DMHL_DRIVER_NAME=\"sii$(MHL_PRODUCT_NUM)drv\"
endif
ccflags-y += -DMHL_DEVICE_NAME=\"sii-$(MHL_PRODUCT_NUM)\"
ccflags-y += $(MHL_HOST_PLATFORM)
#ccflags-y += -DINPUT_DEV_ACCESSORY
#
# RCP_INPUTDEV_SUPPORT supports android key mapping for incoming RCP keys.
# Disable this support if key mapping is not required.
ccflags-y += -DRCP_INPUTDEV_SUPPORT
#
# FORCE_OCBUS_FOR_ECTS is used to identify code added for ECTS temporary fix that prohibits use of eCBUS.
# in addition module parameter force_ocbus_for_ects needs to be set as 1 to achieve ECTS operation.
ccflags-y += -DFORCE_OCBUS_FOR_ECTS
#
# PC_MODE_VIDEO_TIMING_SUPPORT is for cases where no VIC is available from either AVIF or VSIF.
ccflags-y += -DPC_MODE_VIDEO_TIMING_SUPPORT
#
# MANUAL_INFO_FRAME_CLEAR_AT_HPD_DRIVEN_HIGH is to clear all infoframes
#	upon driving HPD high instead of when SCDT goes low.
ccflags-y += -DMANUAL_INFO_FRAME_CLEAR_AT_HPD_DRIVEN_HIGH
#
# MEDIA_DATA_TUNNEL_SUPPORT is for MDT support in chip independent code
ccflags-y += -DMEDIA_DATA_TUNNEL_SUPPORT
#
# MANUAL_EDID_FETCH uses DDC master directly, instead of h/w automated method.
ccflags-y += -DMANUAL_EDID_FETCH

# PRINT_ALL_INTR enables additional verbosity in the main driver level interrupt handler.
#ccflags-y += -DPRINT_ALL_INTR

# PRINT_DDC_ABORTS enables logging of all DDC_ABORTs. Default "disabled" - helps MHL2 hot plug.
# Enable only if you must for debugging.
#ccflags-y += -DPRINT_DDC_ABORTS

# For si_emsc_hid-mt.c
ccflags-y += -Idrivers/hid

# apq8074 SPI support
#ccflags-y += -DGCS_USE_DTS

# apq8074 SPI optimzations or workaround
#ccflags-y += -DGCS_USE_SPIOPTIMIZE
#ccflags-y+= -DGCS_USE_CSWORKAROUND

# sii8620 FIFO syncrhonization
#ccflags-y += -DGCS_USE_FIFO_SYNC

ccflags-$(CONFIG_DEBUG_DRIVER) += -DDEBUG

# Enable VBUS sense and related operations
ccflags-$(CONFIG_DEBUG_DRIVER) += -DENABLE_VBUS_SENSE

#
# the next three lines are optional - they enable greater verbosity in debug output
#ccflags-$(CONFIG_DEBUG_DRIVER)  += -DENABLE_EDID_INFO_PRINT
#ccflags-$(CONFIG_DEBUG_DRIVER) += -DENABLE_EDID_DEBUG_PRINT
#ccflags-$(CONFIG_DEBUG_DRIVER) += -DENABLE_DUMP_INFOFRAME
#
# uncomment next line to prevent EDID parsing. Useful for debugging.
#ccflags-$(CONFIG_DEBUG_DRIVER) += -DEDID_PASSTHROUGH

#SAMSUNG FEATURES
ccflags-y += -DCONFIG_MHL3_SEC_FEATURE
#ccflags-y += -DCONFIG_MHL_SPI
ccflags-y += -DCONFIG_SII8620_HDCP_SUPPORT
ccflags-y += -DCONFIG_SS_FACTORY
ccflags-y += -DCONFIG_MHL_CONFIG_TOOLS
#ccflags-y += -DCONFIG_TESTONLY_SYSFS_SW_REG_TUNING
ccflags-y += -DCONFIG_SII8620_CHECK_MONITOR
ccflags-y += -DCONFIG_MHL3_DVI_WR

obj-$(CONFIG_SEC_MHL_SII8620)  += platform.o
obj-$(CONFIG_SEC_MHL_SII8620)  += mhl_linux_tx.o
obj-$(CONFIG_SEC_MHL_SII8620)  += mhl_rcp_inputdev.o
obj-$(CONFIG_SEC_MHL_SII8620)  += mhl_rbp_inputdev.o
obj-$(CONFIG_SEC_MHL_SII8620)  += mhl_supp.o
obj-$(CONFIG_SEC_MHL_SII8620)  += si_8620_drv.o
obj-$(CONFIG_SEC_MHL_SII8620)  += si_mhl2_edid_3d.o
obj-$(CONFIG_SEC_MHL_SII8620)  += si_mdt_inputdev.o
obj-$(CONFIG_SEC_MHL_SII8620)  += si_emsc_hid.o
obj-$(CONFIG_SEC_MHL_SII8620)  += si_emsc_hid-mt.o
obj-$(CONFIG_SEC_MHL_SII8620)  += mhl_test_case.o
else

# Normal Makefile

# If a specific kernel is not specified, default to the kernel
# used with Android Ice Cream Sandwich
ifneq ($(KERNELPATH),)
KERNELDIR=$(KERNELPATH)
else
KERNELDIR=~/src/linux-2.6.36
#KERNELDIR=~/src/Android_ICS/kernel
endif
ARCH=arm

PWD := $(shell pwd)


.PHONY: clean


release:
	make -C $(KERNELDIR) M=$(PWD) CONFIG_SII$(MHL_PRODUCT_NUM)_MHL_TX=m CONFIG_MEDIA_DATA_TUNNEL_SUPPORT=y modules
	$(CROSS_COMPILE)strip --strip-debug sii$(MHL_PRODUCT_NUM)drv.ko
	$(DEVELOPER_BUILD_COPY)

debug:
	make -C $(KERNELDIR) M=$(PWD) CONFIG_SII$(MHL_PRODUCT_NUM)_MHL_TX=m CONFIG_MEDIA_DATA_TUNNEL_SUPPORT=y CONFIG_DEBUG_DRIVER=y modules
	$(DEVELOPER_BUILD_COPY)

clean:
	make -C $(KERNELDIR) M=$(PWD) CONFIG_SII$(MHL_PRODUCT_NUM)_MHL_TX=m clean

endif
