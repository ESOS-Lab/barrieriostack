/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef _ET310_LINUX_DIRVER_H_
#define _ET310_LINUX_DIRVER_H_

#include <linux/module.h>
#include <linux/spi/spi.h>

/*#define ET310_SPI_DEBUG*/

#ifdef ET310_SPI_DEBUG
#define DEBUG_PRINT(fmt, args...) pr_err(fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...)
#endif

#define VENDOR						"EGISTEC"
#define CHIP_ID						"ET310"

/* assigned */
#define ET310_MAJOR					153
/* ... up to 256 */
#define N_SPI_MINORS					32

#define ET310_ADDRESS_0					0x00
#define ET310_WRITE_ADDRESS				0xAC
#define ET310_READ_DATA					0xAF
#define ET310_WRITE_DATA				0xAE
#define FP_EEPROM_WREN_OP				0x06
#define FP_EEPROM_WRDI_OP				0x04
#define FP_EEPROM_RDSR_OP				0x05
#define FP_EEPROM_WRSR_OP				0x01
#define FP_EEPROM_READ_OP				0x03
#define FP_EEPROM_WRITE_OP				0x02
#define BITS_PER_WORD					8

#define SLOW_BAUD_RATE					16000000

#define DRDY_IRQ_ENABLE					1
#define DRDY_IRQ_DISABLE				0

#define ET310_INT_DETECTION_PERIOD			10
#define ET310_DETECTION_THRESHOLD			10

#define FP_REGISTER_READ				0x01
#define FP_REGISTER_WRITE				0x02
#define FP_GET_ONE_IMG					0x03
#define FP_SENSOR_RESET					0x04
#define FP_POWER_CONTROL				0x05
#define FP_SET_SPI_CLOCK				0x06
#define FP_RESET_SET					0x07

#define FP_EEPROM_WREN					0x90
#define FP_EEPROM_WRDI					0x91
#define FP_EEPROM_RDSR					0x92
#define FP_EEPROM_WRSR					0x93
#define FP_EEPROM_READ					0x94
#define FP_EEPROM_WRITE					0x95

/* trigger signal initial routine*/
#define INT_TRIGGER_INIT				0xa4
/* trigger signal close routine*/
#define INT_TRIGGER_CLOSE				0xa5
/* read trigger status*/
#define INT_TRIGGER_READ				0xa6
/* polling trigger status*/
#define INT_TRIGGER_POLLING				0xa7
/* polling abort*/
#define INT_TRIGGER_ABORT				0xa8
/* Sensor Registers */
#define FDATA_ET310_ADDR				0x00
#define FSTATUS_ET310_ADDR				0x01
/* Detect Define */
#define FRAME_READY_MASK				0x01

struct egis_ioc_transfer {
	u8 *tx_buf;
	u8 *rx_buf;

	__u32 len;
	__u32 speed_hz;

	__u16 delay_usecs;
	__u8 bits_per_word;
	__u8 cs_change;
	__u8 opcode;
	__u8 pad[3];

};

#define EGIS_IOC_MAGIC			'k'
#define EGIS_MSGSIZE(N) \
	((((N)*(sizeof(struct egis_ioc_transfer))) < (1 << _IOC_SIZEBITS)) \
		? ((N)*(sizeof(struct egis_ioc_transfer))) : 0)
#define EGIS_IOC_MESSAGE(N) _IOW(EGIS_IOC_MAGIC, 0, char[EGIS_MSGSIZE(N)])

struct etspi_data {
	dev_t devt;
	spinlock_t spi_lock;
	struct spi_device *spi;
	struct list_head device_entry;

	/* buffer is NULL unless this device is open (users > 0) */
	struct mutex buf_lock;
	unsigned users;
	u8 *buffer;

	unsigned int ocp_en;	/* ocp enable GPIO pin number */
	unsigned int drdyPin;	/* DRDY GPIO pin number */
	unsigned int sleepPin;	/* Sleep GPIO pin number */
	unsigned int ldo_pin;	/* Ldo GPIO pin number */
	unsigned int ldo_pin2;	/* Ldo2 GPIO pin number */

	unsigned int spi_cs;	/* spi cs pin <temporary gpio setting> */

	unsigned int drdy_irq_flag;	/* irq flag */
	bool ldo_onoff;

	/* For polling interrupt */
	int int_count;
	struct timer_list timer;
	struct work_struct work_debug;
	struct workqueue_struct *wq_dbg;
	struct timer_list dbg_timer;
	unsigned int sensortype;
#ifdef CONFIG_SENSORS_FINGERPRINT_SYSFS
	struct device *fp_device;
#endif
	bool tz_mode;
	int detect_period;
	int detect_threshold;
	bool finger_on;
};

int etspi_io_read_register(struct etspi_data *etspi, u8 *addr, u8 *buf);
int etspi_io_write_register(struct etspi_data *etspi, u8 *buf);
int etspi_io_get_one_image(struct etspi_data *etspi, u8 *buf, u8 *image_buf);
int etspi_read_register(struct etspi_data *etspi, u8 addr, u8 *buf);
int etspi_mass_read(struct etspi_data *etspi, u8 addr, u8 *buf, int read_len);
int etspi_eeprom_wren(struct etspi_data *etspi);
int etspi_eeprom_wrdi(struct etspi_data *etspi);
int etspi_eeprom_rdsr(struct etspi_data *etspi, u8 *buf);
int etspi_eeprom_wrsr(struct etspi_data *etspi, u8 *buf);
int etspi_eeprom_read(struct etspi_data *etspi,
		u8 *addr, u8 *buf, int read_len);
int etspi_eeprom_write(struct etspi_data *etspi, u8 *buf, int write_len);

#ifdef CONFIG_SENSORS_FINGERPRINT_SYSFS
extern int fingerprint_register(struct device *dev, void *drvdata,
	struct device_attribute *attributes[], char *name);
extern void fingerprint_unregister(struct device *dev,
	struct device_attribute *attributes[]);
#endif

#endif
