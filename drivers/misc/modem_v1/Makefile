# Makefile of modem_v1

EXTRA_CFLAGS += -Idrivers/misc/modem_v1

obj-y += modem_main.o modem_variation.o modem_io_device.o modem_utils.o

obj-$(CONFIG_LINK_DEVICE_MEMORY) += link_device_memory_main.o \
				    link_device_memory_snapshot.o \
				    link_device_memory_flow_control.o \
				    link_device_memory_debug.o

obj-$(CONFIG_LINK_CONTROL_MSG_IOSM) += link_ctrlmsg_iosm.o
obj-$(CONFIG_LINK_CONTROL_MSG_COMMAND) += link_ctrlmsg_command.o
obj-$(CONFIG_LINK_DEVICE_WITH_SBD_ARCH) += link_device_memory_sbd.o

obj-$(CONFIG_LINK_POWER_MANAGEMENT_WITH_FSM) += link_device_pm.o

obj-$(CONFIG_LINK_DEVICE_C2C) += link_device_c2c.o
obj-$(CONFIG_LINK_DEVICE_LLI) += link_device_lli.o
obj-$(CONFIG_LINK_DEVICE_SHMEM) += link_device_shmem.o
obj-$(CONFIG_LINK_DEVICE_HSIC) += link_device_hsic.o

obj-$(CONFIG_BOOT_DEVICE_SPI) += boot_device_spi.o

obj-$(CONFIG_UMTS_MODEM_SS222) += modem_ctrl_ss222.o
obj-$(CONFIG_UMTS_MODEM_SS300) += modem_ctrl_ss300.o
obj-$(CONFIG_UMTS_MODEM_SS333) += modem_ctrl_ss333.o
obj-$(CONFIG_UMTS_MODEM_SH222AP) += modem_ctrl_sh222ap.o
obj-$(CONFIG_LTE_MODEM_XMM7260) += modem_ctrl_xmm7260.o
