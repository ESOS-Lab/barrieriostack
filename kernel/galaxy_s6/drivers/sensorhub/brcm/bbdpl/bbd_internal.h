/*
 * Copyright 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation (the "GPL").
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *  
 * A copy of the GPL is available at 
 * http://www.broadcom.com/licenses/GPLv2.php, or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The BBD (Broadcom Bridge Driver)
 *
 * tabstop = 8
 */

#ifndef BBD_INTERNAL_H__        /* { */
#define BBD_INTERNAL_H__

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/list.h>

#include <linux/kernel.h>	/* printk()		*/
#include <linux/slab.h>		/* kmalloc()		*/
#include <linux/fs.h>		/* everything		*/
#include <linux/errno.h>	/* error codes		*/
#include <linux/types.h>	/* size_t		*/
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE		*/
#include <linux/sysfs.h>
#include <linux/cdev.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>

#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/poll.h>

#include "bbd.h"
#include "bbd_ifc.h"
#include "bbd_debug.h"
#include "transport/bbd_engine.h"

#define BBD_DEVICE_MAJOR	239

#define BBD_PACKET_TIMEOUT	1000

enum {
    BBD_MINOR_SENSOR    = 0,
    BBD_MINOR_CONTROL   = 1,
    BBD_MINOR_PACKET    = 2,
    BBD_MINOR_RELIABLE  = 3,
    BBD_MINOR_PATCH     = 4,
    BBD_MINOR_SSI_SPI_DEBUG = 5,
    BBD_DEVICE_INDEX
};

struct bbd_dev;

struct bbd_buffer {
	struct mutex lock;		/* lock */
	wait_queue_head_t poll_inq;	/* wait queue for poll */
	wait_queue_head_t comp_inq;	/* wait queue for completion */
	unsigned int count;		/* total size count */
	unsigned int tout_cnt;		/* timeout count */

	unsigned char *pbuff;		/* buff pointer for tx */
	int buff_size;			/* buff size */
};

#define BBD_BUFFER_SIZE 1024

struct bbd_qitem {
        struct bbd_qitem *next;
        unsigned short    len;
        unsigned short    lenUsed;
        unsigned char     data[BBD_BUFFER_SIZE];
};

#define QITEM_OVERHEAD  (sizeof(struct bbd_qitem) - BBD_BUFFER_SIZE)


struct bbd_queue {
	struct mutex lock;		/* lock */
	wait_queue_head_t poll_inq;	/* wait queue for poll */
	wait_queue_head_t comp_inq;	/* wait queue for completion */
	unsigned int count;		/* total size count */
	unsigned int tout_cnt;		/* timeout count */

	struct bbd_qitem *qhead;	/* queue of data */
	struct bbd_qitem *qtail;	/* last item in q */
};

#define BBD_SERIAL_SPI  0               /* stacked ssi-spi driver for 4773 */
#define BBD_SERIAL_TTY  1               /* stacked tty driver for 477x */

#define SUPPORT_MCU_HOST_WAKE            /* JK temporary patch for new HW */
#define BBD_NEW_HW_REV  3               /* connnected mcu_req, mcu_resp from HW rev 8 */

/*
 * bbd device structure
 */
struct bbd_device {
        struct BbdEngine    bbd_engine;
	struct cdev dev    [BBD_DEVICE_INDEX];  /* device for bbd */
	void       *bbd_ptr[BBD_DEVICE_INDEX];  /* individual structures */

	struct mutex      qlock;
	struct bbd_qitem *qfree;	    /* list of free qitems */
        int               qcount;

        bool        db;                     /* debug flag */
        bool        shmd;                   /* enable shmd-->bridge flow */
        int         sio_type;               /* 0 [default] SPI, 1 TTY */
        unsigned long jiffies_to_wake;
        int           tty_baud;

        unsigned int hw_rev;   /* JK added temporary feature for old hw */
};

extern struct bbd_device *gpbbd_dev;

/*
 * bbd common device structure
 */
struct bbd_base {
	struct bbd_queue  txq;              /* tx buffer LHD<--BBD */
	struct bbd_buffer rxb;              /* rx buffer LHD-->BBD */
        const char* name;
};

/*      The overall data flow (e.g. packet, reliable, control) is like this:
 *      bbd_***_write()  --> bbd_write()
 *              Fills the internal bbd_base [rx side] with data from the user.
 *      It is OK for this RX buffer to be a single-entry deep because
 *      various routines then pull this data immediately for processing
 *         - packet data goes into the BbdBridge
 *         - reliable data goes into the BbdBridge
 *         - control data gets scanned and handled immediately
 *
 *   The reverse stream:
 *      When various data sources have packets for the user they call:
 *              bbd_on_read(int minor...)
 *      Which fills the TX queue.
 *      When the user code wakes up (as signalled by bbd_***_poll), it
 *      calls bbd_***_read() which calls bbd_read().
 */
ssize_t      bbd_write  (int minor, const          char *pbuff, size_t size, bool bUser);
ssize_t      bbd_on_read(int minor, const unsigned char *pbuff, size_t size);
ssize_t      bbd_read   (int minor, char __user         *pbuff, size_t size, bool bUser);
unsigned int bbd_poll   (int minor, struct file *filp, poll_table *wait);

/*
 * bbd sensor device structure
 */
struct bbd_sensor_device {
        struct bbd_base base;
	void           *priv_data;        /* private data pointer	*/
        bbd_callbacks  *callbacks;
};


/*
 * bbd control device structure
 */
struct bbd_control_device {
        struct bbd_base   base;
	bool              esw_ready;
};

/*
 * bbd packet device structure
 */
struct bbd_packet_device {
        struct bbd_base base;
};

/*
 * bbd reliable device structure
 */
struct bbd_reliable_device {
        struct bbd_base base;
};

struct Bcm477x_Debug_Xfer
{
        unsigned char a_mode;         // 0x80
        unsigned char a_cmd;          // 0xD1
        unsigned char a_spi_addr;     // 0x00
        unsigned char a_addr[4];
        unsigned char a_len [2];
        unsigned char a_dma;          // 0x03
        unsigned char b_mode;         // 0x80
        unsigned char b_cmd;          // 0xD1
        unsigned char b_spi_addr;     // 0x24
        unsigned char b_data[1];      // get actual size from a_len[]
};

/*
 * bbd ssi spi debug device structure
 */
struct bbd_ssi_spi_debug_device {
        struct bbd_base base;
        unsigned long numXfers;
        unsigned long numBytes;
        struct Bcm477x_Debug_Xfer trace;
};

void bbd_base_init_vars(struct bbd_device *dev, struct bbd_base *p, int* result);
int  bbd_base_uninit   (                        struct bbd_base *p             );
int  bbd_base_reinit   (int minor);
int  bbd_queue_uninit  (struct bbd_qitem *q);

void  bbd_free (void  *p   );
void* bbd_alloc(size_t size);
#define bbd_calloc bbd_alloc

struct bbd_sensor_device* bbd_sensor_ptr(void);

struct BbdEngine *bbd_engine(void);


#endif /* BBD_INTERNAL_H__        } */
