snd-soc-es705-objs := es705.o es705-platform.o es705-cdev.o es705-uart.o es705-uart-common.o

snd-soc-es705-slim-objs += es705-slim.o
snd-soc-es705-i2s-objs += es705-i2s.o
snd-soc-es705-i2c-objs += es705-i2c.o
snd-soc-es705-spi-objs += es705-spi.o

obj-$(CONFIG_SND_SOC_ES705)	+= snd-soc-es705.o
obj-$(CONFIG_SND_SOC_ES804)	+= snd-soc-es705.o

obj-$(CONFIG_SND_SOC_ES_SLIM) += snd-soc-es705-slim.o
obj-$(CONFIG_SND_SOC_ES_I2S) += snd-soc-es705-i2s.o
obj-$(CONFIG_SND_SOC_ES_I2C) += snd-soc-es705-i2c.o
obj-$(CONFIG_SND_SOC_ES_SPI) += snd-soc-es705-spi.o