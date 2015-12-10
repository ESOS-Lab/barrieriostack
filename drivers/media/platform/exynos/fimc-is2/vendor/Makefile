obj-$(CONFIG_USE_VENDER_FEATURE) := fimc-is-spi.o \
				fimc-is-i2c.o \
				fimc-is-sec-define.o \
				crc32.o \
				fimc-is-sysfs.o

obj-$(CONFIG_COMPANION_USE) += fimc-is-companion-dt.o

obj-$(CONFIG_COMPANION_DCDC_USE) += fimc-is-pmic.o

obj-$(CONFIG_COMPANION_C2_USE) += fimc-is-companion_c2.o

obj-$(CONFIG_OIS_USE)	+= fimc-is-device-ois.o
obj-$(CONFIG_AF_HOST_CONTROL)	+= fimc-is-device-af.o
obj-$(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)	+= fimc-is-device-eeprom.o
obj-$(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)	+= fimc-is-device-eeprom.o

EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/include
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/vendor