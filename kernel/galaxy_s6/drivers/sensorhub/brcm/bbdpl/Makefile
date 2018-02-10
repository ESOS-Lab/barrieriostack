#
# Makefile for the Broadcom Bridge Driver
#

# Each configuration option enables a list of files.
obj-$(CONFIG_SENSORS_SSP_BBD)		+=	bbd_common.o bbd_control.o bbd_main.o bbd_packet.o \
						bbd_reliable.o bbd_sensor.o bbd_sysfs.o \
						bbd_ssi_spi.o bbd_tty.o bcm_gps_spi.o bbd_patch.o
obj-$(CONFIG_SENSORS_SSP_BBD)		+= 	transport/
obj-$(CONFIG_SENSORS_SSP_BBD)		+= 	utils/
